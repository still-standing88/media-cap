#include "help.hpp"

#include <utility>

namespace mccli {
namespace {

using Entry = std::pair<std::string_view, std::string_view>;

constexpr Entry guides[]{
    {"audio", R"(Choose an audio source with --audio.
  input[:ID]           Microphone or recording input
  output[:ID]          Speakers or playback output
  process:PID          One application's audio
  exclude-process:PID  All output except that application
  mix                  Default input and output together
  compat:ID            Alternate device capture

Omit the input or output ID to select the default device. Repeat --audio to
mix sources. Use list audio input, list audio output or list audio processes
to look up IDs. See MediaCap help audio-options for source settings.

  MediaCap record audio --audio input:0 -o voice.wav --duration 10
)"},
    {"audio-options", R"(Append key=value settings to a source, separated by semicolons. Quote the
entire source in your shell.

  MediaCap record audio --audio "input;gain=0.8;channels=1" -o voice.wav
  MediaCap record audio --audio "mix;mic-gain=1;system-gain=0.5" -o mix.wav

Input/output/process/mix: gain, rate, channels (0/default, 1 or 2), queue.
Input/output: device (full device ID in place of a numeric source ID).
Mix: mic-device, system-device, mic-gain, system-gain.
Output/mix: exclude-self=true/false, exclude-pids=PID,PID.
Compatibility capture: format=u8/s16/s32/f32, rate, channels (1..32).
Gains default to 1; 0 mutes. Rate and queue 0 use backend defaults.
Use %25 for '%' and %3B for ';' inside device IDs.
Check routing support with MediaCap capabilities audio --details.
)"},
    {"codecs", R"(Run MediaCap capabilities formats for recording and capture formats.
Auto selects a container from the output file extension.
Common pairs include MP4 with H.264/AAC, WebM with VP9/Opus, WAV with PCM,
M4A with AAC, FLAC with FLAC, MP3 with MP3, .opus with Opus, and .ogg with
Vorbis. Bitrates are in bits per second. Video sizes must have even dimensions.

  MediaCap record screen -o desktop.webm --video-codec vp9 --duration 10
)"},
    {"controls", R"(Press p or Space to toggle pause, r to resume, s for status, or q to stop.
Ctrl+C saves and stops. --duration SECONDS stops automatically. The duration
starts after preparation and includes pauses.
--stdin-control accepts pause/resume/toggle/status/stop, one command per line.
Reaching EOF on stdin stops the session. --no-interactive disables keys.
--status-interval SECONDS prints periodic status. Set it to 0 to disable it.

  MediaCap record screen -o desktop.mp4 --duration 10 --status-interval 1

Exit status: 0 success, 1 runtime failure, 2 invalid command or settings.
)"},
    {"examples", R"(Use IDs, window handles and PIDs from the source lists.

  MediaCap list monitors
  MediaCap list audio processes --active
  MediaCap info audio-input 0
  MediaCap record screen -o desktop.mp4 --duration 10
  MediaCap record audio --audio output -o sound.wav --duration 10
  MediaCap record audio --audio process:1234 -o app.opus --duration 10
  MediaCap screenshot screen -o desktop.bmp
  MediaCap capture audio --audio input --duration 2 --audio-samples mic.frames

Use MediaCap COMMAND --help for examples tailored to a command.
)"},
    {"json", R"(Run a command from a UTF-8 JSON request file with MediaCap run FILE.
Use MediaCap run - to read a request from stdin. Keys in options may omit
the leading '--'. Array values repeat the option. Supply positional values
in arguments. Use a file for the request if recording reads control lines
from stdin. A request cannot run another request.

  {"command":["record","audio"],"options":{"output":"voice.wav",
   "audio":["input"],"duration":10,"json":true}}

  MediaCap run request.json

Pass --json for machine-readable results and errors on stdout. Session events
use one object per line. --verbose adds diagnostics on stderr.
)"},
};

constexpr Entry examples[]{
    {"", "list monitors"},
    {"list", "list"},
    {"list all", "list all"},
    {"list monitors", "list monitors"},
    {"list windows", "list windows --name browser"},
    {"list cameras", "list cameras"},
    {"list audio", "list audio"},
    {"list audio devices", "list audio devices"},
    {"list audio input", "list audio input"},
    {"list audio output", "list audio output"},
    {"list audio processes", "list audio processes --active"},
    {"list audio compat", "list audio compat"},
    {"info", "info camera 0"},
    {"info monitor", "info monitor 0"},
    {"info window", "info window 0x12345"},
    {"info camera", "info camera 0"},
    {"info audio-input", "info audio-input 0"},
    {"info audio-output", "info audio-output 0"},
    {"info audio-process", "info audio-process 1234"},
    {"info compat-audio", "info compat-audio 0"},
    {"capabilities", "capabilities audio"},
    {"capabilities audio", "capabilities audio --details"},
    {"capabilities formats", "capabilities formats --json"},
    {"record", "record screen -o desktop.mp4 --duration 10"},
    {"record screen", "record screen -o desktop.mp4 --duration 10"},
    {"record window", "record window --window 0x12345 -o window.mp4 --duration 10"},
    {"record camera", "record camera --camera 0 -o camera.mp4 --duration 10"},
    {"record audio", "record audio --audio input -o voice.wav --duration 10"},
    {"screenshot", "screenshot screen -o desktop.bmp"},
    {"screenshot screen", "screenshot screen -o desktop.bmp --size 1280x720"},
    {"screenshot window", "screenshot window --window 0x12345 -o window.bmp"},
    {"capture", "capture screen --duration 2 --video-frames screen.frames"},
    {"capture screen", "capture screen --duration 2 --video-frames screen.frames"},
    {"capture window", "capture window --window 123 --duration 2 --video-frames w.raw"},
    {"capture camera", "capture camera --camera 0 --duration 2 --video-frames c.raw"},
    {"capture audio", "capture audio --audio input --duration 2 --audio-samples a.frames"},
    {"run", "run request.json"},
    {"docs", "docs --output reference.txt"},
    {"help", "help record camera"},
};

constexpr Entry notes[]{
    {"list", "Run without a subcommand to list all source categories. Use info for one\n"
             "source's details."},
    {"list audio", "Input and output indices are separate. Filtering keeps their original IDs."},
    {"info", "Use IDs from list. For audio input or output, prefix a full device ID with\n"
             "id: to force a literal match."},
    {"info camera", "The query can open the camera briefly to inspect its supported formats."},
    {"record", "Repeat --audio to mix sources into one track. See MediaCap help audio-options\n"
               "for routing and gain, or MediaCap help controls for pause and stop."},
    {"record window", "Choose --window HANDLE or --title TITLE. Use list windows to find a handle."},
    {"screenshot screen", "The region uses source pixels. Resizing applies after the crop."},
    {"screenshot window", "Choose --window HANDLE or --title TITLE. Crop coordinates are relative\n"
                          "to that window."},
    {"capture", "Use --video-frames or --audio-samples to save raw data. Without either\n"
                "option, the final summary reports the frame and sample counts."},
    {"run", "Use - to read JSON from stdin. JSON output is the default for requests.\n"
            "See MediaCap help json for the request fields and an example."},
};

}

std::string_view guide_help(std::string_view topic) {
    for (const auto& guide : guides) {
        if (guide.first == topic) return guide.second;
    }
    return {};
}

std::string_view command_example(std::string_view path) {
    for (const auto& example : examples) {
        if (example.first == path) return example.second;
    }
    return {};
}

std::string_view command_note(std::string_view path) {
    for (const auto& note : notes) {
        if (note.first == path) return note.second;
    }
    return {};
}

}
