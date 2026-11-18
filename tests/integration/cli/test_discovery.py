"""Black-box CLI output/error tests against a deterministic, test-only API."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
checks = 0

def run(args, *, mode='normal', code=0, merged=False, env_extra=None):
    global checks
    env = os.environ.copy()
    env['MC_TEST_SCENARIO'] = mode
    env.update(env_extra or {})
    result = subprocess.run([binary, *args], env=env, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT if merged else subprocess.PIPE,
                            text=True, encoding='utf-8', timeout=30)
    assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
    checks += 1
    return result

for group, caption in [('monitors', 'Monitors (1)'), ('windows', 'Windows (1)'), ('cameras', 'Cameras (1)'),
                       ('audio input', 'Audio inputs (2)'), ('audio output', 'Audio outputs (1)'),
                       ('audio processes', 'Audio processes (2)')]:
    args = ['list', *group.split()]
    for merged in (False, True):
        r = run(args, merged=merged)
        assert caption in r.stdout and 'INTERNAL' not in r.stdout
        assert not r.stderr
    r = run(args, mode='empty')
    assert 'No ' in r.stdout and '(0)' in r.stdout and not r.stderr
    r = run([*args, '--json'], mode='empty')
    assert json.loads(r.stdout) == [] and not r.stderr
    r = run(args, mode='error', code=1)
    assert not r.stdout and r.stderr.startswith('Error: ') and len(r.stderr.splitlines()) == 1
    assert 'INTERNAL' not in r.stderr
    r = run([*args, '--json'], mode='error', code=1)
    assert json.loads(r.stdout)['event'] == 'error' and not r.stderr

r = run(['list', 'audio', 'input', '--json'])
assert [d['kind'] for d in json.loads(r.stdout)] == ['input', 'input']
r = run(['list', 'audio', 'output', '--json'])
assert [d['kind'] for d in json.loads(r.stdout)] == ['output']
r = run(['list', 'audio', 'processes', '--active', '--json'])
assert [d['pid'] for d in json.loads(r.stdout)] == [101]
r = run(['list', 'audio', 'input', '--name', 'USB', '--json'])
assert [d['index'] for d in json.loads(r.stdout)] == [1], 'Filtering renumbered a device'
r = run(['list', 'audio', 'input', '--name', 'missing'])
assert 'No audio inputs matching' in r.stdout
for args, fields in [
    (['monitor', '7'], {'id': 7, 'width': 1920, 'name': 'Display fixture'}),
    (['window', '0x7b'], {'handle': 123, 'title': 'Window fixture'}),
    (['camera', '0'], {'index': 0, 'name': 'Camera fixture', 'details_available': True,
                        'resolutions': [{'width': 640, 'height': 480}, {'width': 1280, 'height': 720}],
                        'pixel_formats': ['bgra', 'nv12']}),
    (['audio-input', '1'], {'index': 1, 'id': 'input-two', 'sample_rate': 48000}),
    (['audio-output', 'id:output-one'], {'index': 0, 'kind': 'output', 'is_default': True}),
    (['audio-process', '101'], {'pid': 101, 'output_active': 1, 'executable': 'music.exe'}),
    (['compat-audio', '0'], {'index': 0, 'kind': 'input', 'name': 'Compatibility input'}),
]:
    r = run(['info', *args, '--json'])
    data = json.loads(r.stdout)
    assert all(data.get(key) == value for key, value in fields.items()), data
    assert not r.stderr
r = run(['info', 'audio-input', '0'])
assert 'Desk microphone' in r.stdout and 'Sample rate: 48000' in r.stdout and 'USB microphone' not in r.stdout
for args in (['info', 'monitor', '999'], ['info', 'window', '0xnot-a-handle']):
    r = run([*args, '--json'], code=2)
    assert json.loads(r.stdout)['exit_code'] == 2 and not r.stderr
r = run(['capabilities', 'formats', '--json'])
formats = json.loads(r.stdout)
assert 'webm' in formats['containers'] and 'av1' in formats['video_codecs'] and 'i420' in formats['camera_pixel_formats']
r = run(['capabilities', 'audio', '--json'])
assert json.loads(r.stdout)['microphone'] == 'available' and not r.stderr
r = run(['list', 'cameras'], mode='partial', code=1)
assert 'Camera fixture' in r.stdout and r.stderr.startswith('Warning: ')
r = run(['list', '--json'], mode='windows-error', code=1)
all_sources = json.loads(r.stdout)
assert all_sources['failed_lists'] == ['windows'] and len(all_sources['monitors']) == 1
assert all_sources['errors']['windows']['code']
r = run(['list'])
assert 'Monitors (1)' in r.stdout and 'Audio processes (2)' in r.stdout and not r.stderr
r = run(['list', 'audio'])
assert 'Audio devices (3)' in r.stdout and not r.stderr
if os.name == 'nt':
    r = run(['list', 'cameras'], mode='slow-details')
    assert 'Camera fixture' in r.stdout and not r.stderr
    r = run(['list', 'cameras', '--details'], mode='slow-details', code=1)
    assert 'Camera fixture' in r.stdout and 'timed out' in r.stderr
    r = run(['list', '--details', '--json'], mode='slow-details', code=1)
    data = json.loads(r.stdout)
    assert data['cameras'][0]['name'] == 'Camera fixture' and data['failed_lists'] == ['cameras']
    assert data['audio-input'] and data['processes'] and 'timed out' in r.stderr
r = run(['list', 'audio', 'input', '--verbose'])
assert 'Desk microphone' in r.stdout and 'INTERNAL' not in r.stdout and 'INTERNAL' in r.stderr
for args in (['list', 'audio', 'bogus', '--json'], ['--json', 'record', 'audio'], ['record', 'audio', '--json']):
    r = run(args, code=2)
    assert json.loads(r.stdout)['exit_code'] == 2 and not r.stderr
with tempfile.TemporaryDirectory() as temp:
    target = Path(temp) / 'capture.wav'
    for source, expected in [('input:1', 'input-two'), ('output:0', 'output-one')]:
        r = run(['record', 'audio', '--audio', source, '-o', str(target), '--duration', '1', '--json'], code=1,
                env_extra={'MC_TEST_EXPECT_DEVICE': expected})
        events = [json.loads(line) for line in r.stdout.splitlines()]
        assert events[-1]['code'] == 7, events # Selection succeeded; injected encoder failure.
        assert not r.stderr and not target.exists()
    r = run(['record', 'audio', '--audio', 'input:99', '-o', str(target)], code=2)
    assert 'device ID not found' in r.stderr and not target.exists()
    r = run(['record', 'audio', '--audio', 'input:0', '-o', str(target), '--json'], mode='slow-start', code=1)
    events = [json.loads(line) for line in r.stdout.splitlines()]
    assert events[-1]['event'] == 'error' and 'preparation timed out' in events[-1]['message']
    assert not target.exists() and not r.stderr
print(f'Passed {checks} discovery/output/error checks.')
