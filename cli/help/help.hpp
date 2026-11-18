#pragma once

#include "CLI11.hpp"
#include <string>
#include <string_view>

namespace mccli {

class HelpFormatter final : public CLI::Formatter {
public:
    std::string make_help(const CLI::App* app, std::string, CLI::AppFormatMode mode) const override;
};

std::string command_help(const CLI::App* command, bool all = false);
std::string command_reference(const CLI::App& root);
std::string_view guide_help(std::string_view topic);
std::string_view command_example(std::string_view path);
std::string_view command_note(std::string_view path);

}
