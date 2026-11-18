#pragma once

#include "../cli.hpp"

namespace mccli {
enum class Action { none, help, list, info, formats, requirements, session, screenshot, docs, run };

struct Commands {
    CLI::App app{"Record your screen, camera and audio. Capture screenshots and raw frames.", "MediaCap"};
    bool json = false;
    bool verbose = false;
    Action action = Action::none;
    std::string selected, topic, source_id, request_file, docs_file;
    std::string docs_format;
    std::vector<std::string> path;
    CLI::App* help_node = &app;
    ListOptions list_options;
    std::map<std::string, RecordingOptions> options;
    std::map<std::string, ScreenshotOptions> screenshot_options;

    Commands();
};
}
