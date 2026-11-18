"""Exercise screenshots and raw frame delivery with the deterministic CLI fixture."""

import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


binary = str(Path(sys.argv[1]).resolve())


def run(*args, mode='normal', code=0):
    result = subprocess.run([binary, *map(str, args), '--json'],
                            env={**os.environ, 'MC_TEST_SCENARIO': mode},
                            capture_output=True, encoding='utf-8', timeout=30)
    assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
    assert not result.stderr, (args, result.stderr)
    return [json.loads(line) for line in result.stdout.splitlines()]


def packets(path):
    with path.open('rb') as stream:
        assert stream.readline() == b'MediaCap frames 1\n'
        result = []
        for line in iter(stream.readline, b''):
            metadata = json.loads(line)
            data = stream.read(metadata['payload_bytes'])
            assert len(data) == metadata['payload_bytes']
            result.append((metadata, data))
        return result


with tempfile.TemporaryDirectory() as temp:
    directory = Path(temp)
    image = directory / 'preview é.BMP'
    image_args = ('screenshot', 'screen', '--monitor', 7, '-o', image)
    events = run(*image_args, '--region', '1,2,3,4', '--size', '2x2', '--timeout', 456, mode='screenshot-config')
    assert events == [{'event': 'screenshot', 'output': str(image), 'width': 2, 'height': 2, 'format': 'bmp'}]
    data = image.read_bytes()
    assert data[:2] == b'BM' and struct.unpack_from('<I', data, 2)[0] == len(data)
    assert struct.unpack_from('<iiHH', data, 18) == (2, 2, 1, 24)
    assert data[54:] == bytes([7, 8, 9, 10, 11, 12, 0, 0, 1, 2, 3, 4, 5, 6, 0, 0])
    assert run(*image_args, code=2)[-1]['event'] == 'error'
    assert run(*image_args, '--overwrite', mode='screenshot-error', code=1)[-1]['code'] == 4
    assert image.read_bytes() == data
    assert run('screenshot', 'window', '--title', 'Window fixture', '-o', image, '--overwrite')[-1]['event'] == 'screenshot'
    assert run(*image_args, '--region', '1,2,0,4', '--overwrite', code=2)[-1]['event'] == 'error'
    assert not list(directory.glob('.mediacap-image-*'))

    video = directory / 'video.frames'
    audio = directory / 'audio.frames'
    capture_args = ('capture', 'screen', '--duration', '.001', '--audio', 'input',
                    '--video-frames', video, '--audio-samples', audio)
    final = run(*capture_args)[-1]
    assert (final['event'], final['video_frames'], final['audio_blocks'], final['sample_frames']) == ('finished', 1, 1, 2)
    [(info, data)] = packets(video)
    assert info == {'type': 'video', 'source_index': 0, 'source_type': 1, 'timestamp_ns': 1010,
                    'frame_index': 4, 'width': 2, 'height': 2, 'pixel_format': 'bgra', 'orientation': 'top-down',
                    'planes': [{'row_bytes': 8, 'rows': 2}], 'payload_bytes': 16}
    assert data == bytes([1, 2, 3, 255, 4, 5, 6, 255, 7, 8, 9, 255, 10, 11, 12, 255])
    [(info, data)] = packets(audio)
    assert info == {'type': 'audio', 'source_index': 1, 'source_type': 4, 'timestamp_ns': 2020,
                    'sample_frames': 2, 'channels': 2, 'sample_rate': 48000, 'sample_format': 's16le', 'payload_bytes': 8}
    assert struct.unpack('<hhhh', data) == (1, 2, 3, 4)
    assert run('capture', 'screen', '--duration', '.001')[-1]['video_frames'] == 1
    assert run(*capture_args, code=2)[-1]['event'] == 'error'
    original_video = video.read_bytes()
    assert run(*capture_args, '--overwrite', mode='capture-error', code=1)[-1]['code'] == 6
    assert video.read_bytes() == original_video
    target = directory / 'new.frames'
    assert run('capture', 'screen', '--duration', '.001', '--video-frames', target, mode='bad-frame', code=1)[-1]['event'] == 'error'
    assert not target.exists()
    assert run('capture', 'screen', '--audio', 'input', '--duration', '.001', '--video-frames', target,
               '--audio-samples', target, '--overwrite', code=2)[-1]['event'] == 'error'
    assert not target.exists()
    recording = directory / 'video.mp4'
    assert run('record', 'screen', '-o', recording, '--duration', '.001', '--video-frames', target,
               mode='record-frames')[-1]['video_frames'] == 1
    assert recording.read_bytes() == b'fixture media' and len(packets(target)) == 1

print('Passed screenshot and raw frame checks.')
