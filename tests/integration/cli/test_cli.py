"""CLI contracts: no discovery or capture is started by this suite."""
import json
import re
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
checks = 0

def run(args, status=0, contains=None, input_text=None):
    global checks
    result = subprocess.run([binary, *args], input=input_text, capture_output=True, text=True, encoding="utf-8", timeout=15)
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    if contains:
        assert contains in result.stdout + result.stderr, (args, result.stdout, result.stderr)
    checks += 1
    return result

run(["--help"], contains="record")
run(["--version"], contains="MediaCap 1.0.0")
run([], contains="Record your screen")
requirements = json.loads(run(["--json", "capabilities", "requirements"]).stdout)
assert requirements["platform"] in ("windows", "macos", "linux")
features = requirements["features"]
assert features["screen"]["included_in_build"] and "display_session" in features["screen"]["prerequisites"]
assert "camera_device" in features["camera"]["prerequisites"]
assert "microphone_permission" in features["microphone"]["prerequisites"] or requirements["platform"] == "linux"
if features["process_audio"]["included_in_build"]:
    process = features["process_audio"]
    if requirements["platform"] == "windows":
        assert process["minimum_os"] == {"major": 10, "minor": 0, "build": 20348}
        assert features["system_audio"]["minimum_os"] is None
    elif requirements["platform"] == "macos":
        assert process["minimum_os"] == {"major": 14, "minor": 4, "build": 0}
        assert features["system_audio"]["minimum_os"] == process["minimum_os"]
    else:
        assert "pipewire" in process["prerequisites"] and "x11" in features["screen"]["prerequisites"]
        assert process["minimum_os"] is None
run(["record"], contains="record screen")
run(["--help-all"], contains="--capture-size")
for path in [[], ["list"], ["record"], ["capabilities"], *[["list", x] for x in ("monitors", "windows", "cameras", "audio", "audio-classic", "processes")], ["capabilities", "audio"], *[["record", x] for x in ("screen", "window", "camera", "audio")]]:
    run([*path, "--help"])
for topic in ("audio", "audio-options", "codecs", "controls", "examples", "record", "list"):
    run(["help", topic])
run(["help", "record", "camera"], contains="--capture-size")
run(["help", "missing"], 2)
for args in (["missing"], ["record", "screen"], ["record", "screen", "-o", "a.mp4", "--bogus"], ["list", "monitors", "windows"]):
    run(args, 2)

with tempfile.TemporaryDirectory(prefix="mediacap-cli-test-") as temp:
    target = Path(temp) / "space and unicode-été-画面-🎙.wav"
    base = ["--json", "record", "audio", "-o", str(target), "--dry-run"]
    valid = ["input", "input:0", "output", "output:1", "compat:0", "mic", "system", "mix", "process:123", "exclude-process:456", "classic:0",
             "mic;device=some%3Bdevice%25;gain=0.25;rate=44100;channels=1;queue=16",
             "system;exclude-self=true;exclude-pids=123,456;gain=0;channels=2",
             "mix;mic-device=input;system-device=output;mic-gain=0.4;system-gain=2;rate=48000;queue=0",
             "classic:1;format=u8;rate=16000;channels=1", "classic:2;format=s16", "classic:3;format=s32", "classic:4;format=f32"]
    for source in valid:
        result = json.loads(run([*base, "--audio", source]).stdout)
        assert result["event"] == "plan" and result["audio_sources"] == [source]
    mixed = json.loads(run([*base, "--audio", "mic", "--audio", "system"]).stdout)
    assert mixed["audio_sources"] == ["mic", "system"]
    assert mixed["output"] == str(target)
    assert not target.exists(), "dry-run created an output"
    invalid = ["input:-1", "input:abc", "output:", "input:0;device=another", "unknown", "process", "process:0", "process:-1", "process:4294967296", "mic:abc", "classic", "classic:-1",
               "mic;gain=-1", "mic;gain=nan", "mic;gain=inf", "mic;gain=1oops", "mic;gain=1;gain=2",
               "mic;channels=3", "mic;rate=999999999999999999999", "classic:0;rate=0", "classic:0;channels=0",
               "classic:0;format=f64", "classic:0;gain=1", "mic;exclude-self=true", "system;mic-gain=0.5",
               "mix;device=wrong", "process:123;device=wrong", "system;exclude-self=maybe", "system;exclude-pids=0",
               "mic;device=", "mic;", "mic;unknown=1"]
    for source in invalid:
        run([*base, "--audio", source], 2)
    run(base, 2, "requires at least one")
    for args in (["--duration", "0"], ["--duration", "-1"], ["--duration", "nan"], ["--duration", "0.0001"],
                 ["--status-interval", "inf"], ["--channels", "0"], ["--channels", "256"], ["--sample-rate", "0"],
                 ["--audio-bitrate", "-1"], ["--audio-codec", "unknown"], ["--container", "bad"]):
        run([*base, "--audio", "mic", *args], 2)
    for container, codec in [("mp4", "aac"), ("mkv", "pcm"), ("webm", "opus"), ("m4a", "aac"), ("flac", "flac"), ("mp3", "mp3"), ("wav", "pcm"), ("opus", "opus")]:
        run([*base, "--audio", "mic", "--container", container, "--audio-codec", codec])
    run([*base, "--audio", "mic", "--container", "opus", "--audio-codec", "aac"], 2)
    run([*base, "--audio", "mic", "--container", "mp3", "--audio-codec", "aac"], 2, "Audio codec")
    run([*base, "--audio", "mic", "--container", "opus", "--sample-rate", "44100"], 2, "sample rate")
    run([*base, "--audio", "mic", "--container", "mp3", "--channels", "6"], 2, "6-channel layout")
    screen = ["--json", "record", "screen", "-o", str(Path(temp) / "video.mp4"), "--dry-run"]
    run([*screen, "--audio-codec", "aac"], 2, "--audio")
    run([*screen, "--container", "webm", "--video-codec", "h264"], 2, "Video codec")
    run([*screen, "--container", "webm", "--audio", "mic", "--audio-codec", "aac"], 2, "Audio codec")
    run(["record", "screen", "-o", str(target), "--dry-run"], 2, "audio-only container")
    run(["record", "screen", "-o", "capture.mediacap-unknown", "--dry-run"], 2, "Cannot infer")
    for codec in ("auto", "h264", "vp9", "av1"):
        run([*screen, "--video-codec", codec, "--size", "1920x1080", "--fps", "60", "--capture-interval", "16"])
    for args in (["--size", "1919x1080"], ["--size", "bogus"], ["--size", "0x2"], ["--fps", "0"], ["--capture-interval", "0"], ["--container", "wav"], ["--container", "webm", "--video-codec", "h264"]):
        run([*screen, *args], 2)
    window = ["record", "window", "-o", "window.mp4", "--dry-run"]
    run(window, 2)
    run([*window, "--window", "0x123"])
    run([*window, "--title", "A title"])
    unicode_title = "Écran 画面 🎬"
    result = run(["--json", *window, "--title", unicode_title])
    assert json.loads(result.stdout)["window_title"] == unicode_title
    run([*window, "--window", "123", "--title", "A title"], 2)
    for pixel in ("bgra", "bgr24", "rgba", "rgb24", "nv12", "i420"):
        run(["record", "camera", "-o", "camera.mp4", "--dry-run", "--pixel-format", pixel, "--capture-size", "1280x720"])
    # File protection must fail before camera/device discovery, preserving contents.
    target.write_bytes(b"keep this file")
    run(["record", "camera", "-o", str(target), "--container", "mp4", "--duration", "1"], 2, "already exists")
    assert target.read_bytes() == b"keep this file"
    run(["record", "screen", "-o", temp, "--container", "mp4", "--overwrite", "--duration", "1"], 2, "regular file")
    request_path = Path(temp) / "options-été.json"
    request = {"command": ["record", "audio"], "options": {
        "output": str(target), "dry-run": True, "duration": 1.5, "audio": ["input", "output"]}}
    request_path.write_text(json.dumps(request, ensure_ascii=False), encoding="utf-8")
    plan = json.loads(run(["run", str(request_path)]).stdout)
    assert plan["output"] == str(target) and plan["duration_seconds"] == 1.5
    assert plan["audio_sources"] == ["input", "output"]
    request["options"]["json"] = False
    request_path.write_text(json.dumps(request), encoding="utf-8")
    run(["run", str(request_path)], contains="Ready: audio")
    assert json.loads(run(["--json", "run", str(request_path)]).stdout)["event"] == "plan"
    assert target.read_bytes() == b"keep this file"

    text_file = Path(temp) / "cli-reference.txt"
    reference = run(["docs"]).stdout
    assert reference == run(["--help-all"]).stdout
    assert ".. code-block::" not in reference and "**Example**" not in reference
    run(["docs", "--output", str(text_file)])
    assert text_file.read_text(encoding="utf-8") == reference
    reference_file = Path(temp) / "cli-reference.rst"
    reference = run(["docs", "--format", "rst"]).stdout
    run(["docs", "--output", str(reference_file)])
    assert reference_file.read_text(encoding="utf-8") == reference
    assert "MediaCap screenshot window" in reference and "MediaCap capture audio" in reference
    assert reference.count("``--json``") == 1
    assert ".. code-block:: console" in reference
    run(["docs", "--format", "markdown"], 2)

for payload in (
    '{"command":', '{"command":["list"],"command":["list"]}',
    '{"command":["run"]}', '{"command":["--_camera-worker"]}',
    '{"command":["list","monitors"],"options":{"_exact-id":42}}',
    '{"command":["list","monitors"],"options":{"json":1}}',
    '{"command":["record","audio"],"options":{"stdin-control":true}}',
    '{"command":["info","camera"],"arguments":["0\\u0000"]}',
):
    result = run(["run", "-"], 2, input_text=payload)
    assert json.loads(result.stdout)["exit_code"] == 2 and not result.stderr

result = run(["run", "-"], input_text='{"command":["help"],"arguments":["record","camera"]}')
assert "--capture-size" in json.loads(result.stdout)["text"]
result = run(["--json", "--help"])
assert json.loads(result.stdout)["event"] == "help"
print(f"Passed {checks} CLI contract checks; no devices opened.")

# The full reference must explain shared options once, with clear command scope.
reference = run(['--help-all']).stdout
definitions = re.findall(r'^  (?:-\w, )?(--[\w-]+)\b', reference, re.MULTILINE)
assert len(definitions) == len(set(definitions)), 'Repeated option definitions in full help'
assert max(map(len, reference.splitlines())) <= 80, 'Help wraps past a standard terminal'
for text in ['record screen', 'record window', 'record camera', 'record audio',
             'list audio input', 'list audio output', 'list audio processes',
             'info camera', 'capabilities formats', 'screenshot window', 'capture audio']:
    assert text in reference, text
for scope in [['record'], ['list'], ['list', 'audio']]:
    scoped = run([*scope, '--help-all']).stdout
    options = re.findall(r'^  (?:-\w, )?(--[\w-]+)\b', scoped, re.MULTILINE)
    assert len(options) == len(set(options)), scope
    if scope[0] == 'list': assert '--video-codec' not in scoped
    else: assert 'list audio processes' not in scoped
direct = run(['list', 'audio', 'input', '--help']).stdout
assert direct == run(['help', 'list', 'audio', 'input']).stdout
assert 'Usage: MediaCap list audio input [options]' in direct
assert '--name' in direct and 'Global options include --json' in direct
assert 'Print this help message' not in direct

for path in (["info", "camera"], ["capabilities", "formats"], ["screenshot", "screen"],
             ["screenshot", "window"], ["capture", "screen"], ["capture", "window"],
             ["capture", "camera"], ["capture", "audio"], ["docs"], ["run"]):
    focused = run([*path, "--help"]).stdout
    assert "Example" in focused and "MediaCap " + " ".join(path) in focused

# Every visible help path must stay free of dependency names and raw backend terms.
for path in [[], ["list"], ["list", "audio"], *[["list", "audio", n] for n in ("input", "output", "processes")], *[["record", n] for n in ("screen", "window", "camera", "audio")]]:
    r = run([*path, "--help"])
    assert not any(name in r.stdout.lower() for name in ("flexaudio", "miniaudio", "cli11", "ffmpeg", "libvpx", "libaom", "openh264")), r.stdout
assert not any(name in reference.lower() for name in ("flexaudio", "miniaudio", "cli11", "ffmpeg", "libvpx", "libaom", "openh264"))
assert 'FLOAT:NUMBER' not in reference and 'Needs:' not in reference
for topic in ("audio", "audio-options", "codecs", "controls", "examples"):
    r = run(["help", topic])
    assert len(r.stdout.splitlines()) <= 18, (topic, r.stdout)
    assert not any(name in r.stdout.lower() for name in ("flexaudio", "miniaudio", "cli11", "ffmpeg", "libvpx", "libaom", "openh264")), r.stdout
r = run(["record", "audio", "--audio", "input", "--duration", "0", "-o", "unused.wav"], 2)
assert '0.001 seconds' in r.stderr and 'e-324' not in r.stderr and len(r.stderr.splitlines()) == 1
r = run(["record", "screen", "--json", "-o", "unused.mp4", "--dry-run"])
assert json.loads(r.stdout)['event'] == 'plan'
print(f"Passed {checks} total CLI contract checks.")
