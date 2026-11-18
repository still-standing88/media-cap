#include "json_request.hpp"
#include "../cli.hpp"
#include "json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace mccli {
namespace {
using Json = nlohmann::json;

std::string read_input(const std::string& filename) {
    std::ifstream file;
    if (filename != "-") {
        file.open(std::filesystem::u8path(filename), std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open JSON request: " + filename);
    }
    auto& input = filename == "-" ? std::cin : file;
    constexpr size_t limit = 1024 * 1024;
    std::string text;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        text.append(buffer, static_cast<size_t>(input.gcount()));
        if (text.size() > limit) throw UsageError("JSON request exceeds 1 MiB.");
    }
    if (!input.eof() || input.bad()) throw std::runtime_error("Cannot read the JSON request.");
    return text;
}

Json parse_request(const std::string& text) {
    std::vector<std::set<std::string>> keys;
    try {
        return Json::parse(text, [&](int depth, Json::parse_event_t event, Json& value) {
            if (depth > 12) throw UsageError("JSON request is nested too deeply.");
            if (event == Json::parse_event_t::object_start) keys.emplace_back();
            if (event == Json::parse_event_t::object_end) keys.pop_back();
            if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                throw UsageError("Duplicate JSON key: " + value.get<std::string>());
            return true;
        });
    } catch (const Json::parse_error& e) {
        throw UsageError("Invalid JSON request at byte " + std::to_string(e.byte) + ".");
    } catch (const Json::out_of_range&) {
        throw UsageError("A number in the JSON request is out of range.");
    }
}

std::string scalar(const Json& value, const std::string& field) {
    if (value.is_number()) return value.dump();
    if (!value.is_string()) throw UsageError(field + " expects a string or number.");
    auto text = value.get<std::string>();
    if (text.find('\0') != std::string::npos) throw UsageError(field + " contains a null character.");
    return text;
}

const CLI::App* child_command(const CLI::App& parent, const std::string& name) {
    for (const auto* child : parent.get_subcommands([](const CLI::App*) { return true; })) {
        const auto& aliases = child->get_aliases();
        if (!child->get_group().empty() &&
            (child->get_name() == name || std::find(aliases.begin(), aliases.end(), name) != aliases.end())) {
            return child;
        }
    }
    return nullptr;
}

const CLI::Option* request_option(const CLI::App& node, const std::string& key) {
    for (const auto* current = &node; current; current = current->get_parent()) {
        const auto* option = current->get_option_no_throw(key);
        if (option && !option->get_group().empty()) return option;
    }
    return nullptr;
}
}

JsonRequest read_json_request(const std::string& filename, const CLI::App& app, bool force_json) {
    const auto request = parse_request(read_input(filename));
    if (!request.is_object()) throw UsageError("The JSON request must be an object.");
    for (const auto& item : request.items()) {
        if (item.key() != "command" && item.key() != "options" && item.key() != "arguments")
            throw UsageError("Unknown JSON request field: " + item.key());
    }
    if (!request.contains("command") || !request["command"].is_array() || request["command"].empty())
        throw UsageError("JSON 'command' must be a nonempty array of command names.");

    JsonRequest result;
    const auto* node = &app;
    for (const auto& part : request["command"]) {
        if (!part.is_string()) throw UsageError("JSON command names must be strings.");
        const auto name = scalar(part, "command");
        node = child_command(*node, name);
        if (!node || node->get_name() == "run") throw UsageError("Unknown JSON command: " + name);
        result.arguments.push_back(node->get_name());
    }
    if (request.contains("options")) {
        const auto& options = request["options"];
        if (!options.is_object()) throw UsageError("JSON 'options' must be an object.");
        std::set<std::string> seen;
        for (const auto& item : options.items()) {
            const auto key = item.key().rfind("--", 0) == 0 ? item.key() : "--" + item.key();
            if (!seen.insert(key).second) throw UsageError("Duplicate JSON option: " + key);
            const auto* option = request_option(*node, key);
            if (!option) throw UsageError("Unknown option for this JSON command: " + key);
            const auto& value = item.value();
            if (option->get_expected_min() == 0) {
                if (!value.is_boolean()) throw UsageError(key + " expects a boolean.");
                if (key == "--json") result.json = force_json || value.get<bool>();
                else if (value.get<bool>()) {
                    if (key == "--stdin-control" && filename == "-")
                        throw UsageError("Use a JSON request file when --stdin-control needs stdin.");
                    result.arguments.push_back(key);
                }
            } else if (value.is_array()) {
                if (value.empty()) throw UsageError(key + " expects at least one value.");
                for (const auto& entry : value) result.arguments.push_back(key + "=" + scalar(entry, key));
            } else {
                result.arguments.push_back(key + "=" + scalar(value, key));
            }
        }
    }
    if (request.contains("arguments")) {
        const auto& args = request["arguments"];
        if (!args.is_array()) throw UsageError("JSON 'arguments' must be an array.");
        if (!args.empty()) result.arguments.emplace_back("--");
        for (const auto& arg : args) result.arguments.push_back(scalar(arg, "arguments"));
    }
    return result;
}
}
