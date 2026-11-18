#include "../cli.hpp"
#include "formats.hpp"
#include "json.hpp"

#include <sstream>

namespace mccli {
void formats(Output& out) {
    const nlohmann::ordered_json options{
        {"containers", recording_containers},
        {"video_codecs", recording_video_codecs},
        {"audio_codecs", recording_audio_codecs},
        {"camera_pixel_formats", camera_pixel_formats},
        {"compatibility_sample_formats", compatibility_sample_formats},
        {"additional_extensions", {"ogg"}}};
    if (out.json) {
        out.line(options.dump());
        return;
    }
    std::ostringstream text;
    auto print = [&](const char* label, const char* key) {
        text << label << ':';
        for (const auto& value : options[key]) text << ' ' << value.get<std::string>();
        text << '\n';
    };
    print("Recording containers", "containers");
    print("Video codecs", "video_codecs");
    print("Audio codecs", "audio_codecs");
    print("Camera pixel formats", "camera_pixel_formats");
    print("Compatibility audio sample formats", "compatibility_sample_formats");
    text << "Auto container selection uses the filename extension, including .ogg.\n"
         << "Use 'MediaCap help codecs' for common codec and container pairs.";
    out.line(text.str());
}
}
