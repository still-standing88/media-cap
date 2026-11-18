#include "commands.hpp"
#include "../../help/help.hpp"
#include "json_request.hpp"
#include "../../../src/utils/Utf8.h"
#include <algorithm>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
#ifdef _WIN32
struct QueryApartment {
    bool initialized = false;
    void initialize() {
        const auto status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(status)) throw std::runtime_error("Cannot initialize camera query.");
        initialized = true;
    }
    ~QueryApartment() { if (initialized) CoUninitialize(); }
};
#endif
CLI::App* active_command(CLI::App& root) {
    CLI::App* node = &root;
    while (!node->get_subcommands().empty()) node = node->get_subcommands().back();
    return node;
}
std::string command_path(CLI::App* node) {
    std::string path = node->get_name();
    while ((node = node->get_parent())) path = node->get_name() + ' ' + path;
    return path;
}
}
int main(int argc, char** argv) {
    using namespace mccli;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif
    Commands commands;
    auto& app = commands.app;
    auto& json = commands.json;
    auto& verbose = commands.verbose;
    std::unique_ptr<Output> output;
    auto report = [&](const std::string& message, const std::string& fields = "") noexcept {
        try {
            if (!output) output = std::make_unique<Output>(json, verbose);
            output->error(message, fields);
        } catch (...) { std::fprintf(stderr, "Error: %s\n", single_line(message).c_str()); }
    };
    const auto present = [&](const std::string& type, const std::string& content) {
        if (json) std::cout << "{\"event\":" << quote(type) << ",\"text\":" << quote(content) << "}\n";
        else std::cout << content;
        std::cout.flush();
        if (!std::cout) throw std::runtime_error("Cannot write CLI output.");
    };
    try {
        argv = app.ensure_utf8(argv);
        for (int i = 1; i < argc; ++i)
            if (!MediaCap::Utf8::valid(argv[i])) {
                json = std::any_of(argv + 1, argv + argc, [](const char* arg) { return std::string_view(arg) == "--json"; });
                throw UsageError("Command arguments must be valid UTF-8.");
            }
        app.parse(argc, argv);
        if (commands.action == Action::run) {
            const bool outer_verbose = verbose;
            const bool force_json = json;
            json = true;
            auto request = read_json_request(commands.request_file, app, force_json);
            json = request.json;
            commands.action = Action::none;
            app.clear();
            if (outer_verbose && std::find(request.arguments.begin(), request.arguments.end(), "--verbose") == request.arguments.end())
                request.arguments.insert(request.arguments.begin(), "--verbose");
            std::reverse(request.arguments.begin(), request.arguments.end());
            app.parse(std::move(request.arguments));
        }
#ifdef _WIN32
        // Camera format queries use the same COM apartment as capture workers.
        QueryApartment query_apartment;
        if (commands.list_options.worker) query_apartment.initialize();
#endif
        if (commands.action == Action::none) { present("help", app.help()); return 0; }
        if (commands.action == Action::help) {
            const auto guide = commands.path.empty() ? guide_help(commands.topic) : std::string_view{};
            if (!guide.empty()) { present("help", std::string(guide)); return 0; }
            if (!commands.topic.empty()) {
                commands.path.insert(commands.path.begin(), commands.topic);
                for (const auto& name : commands.path) {
                    const auto children = commands.help_node->get_subcommands([](CLI::App*) { return true; });
                    const auto found = std::find_if(children.begin(), children.end(), [&](const CLI::App* child) {
                        const auto& aliases = child->get_aliases();
                        return child->get_name() == name || std::find(aliases.begin(), aliases.end(), name) != aliases.end();
                    });
                    if (found == children.end()) throw UsageError("Unknown help topic: " + name + ". Use 'MediaCap --help'.");
                    commands.help_node = *found;
                }
            }
            present("help", commands.help_node->help());
            return 0;
        }
        if (commands.action == Action::docs) {
            const bool rst = commands.docs_format.empty()
                ? std::filesystem::u8path(commands.docs_file).extension() == ".rst"
                : commands.docs_format == "rst";
            const auto reference = rst ? command_reference(app) : command_help(&app, true);
            if (commands.docs_file.empty()) present("documentation", reference);
            else {
                std::ofstream file(std::filesystem::u8path(commands.docs_file), std::ios::binary | std::ios::trunc);
                if (!file || !file.write(reference.data(), static_cast<std::streamsize>(reference.size())) || !file.flush())
                    throw std::runtime_error("Cannot write the CLI reference to " + commands.docs_file);
                present("documentation", "Saved " + commands.docs_file + "\n");
            }
            return 0;
        }
        output = std::make_unique<Output>(json, verbose);
        if (commands.action == Action::list) discover(commands.selected, *output, commands.list_options);
        else if (commands.action == Action::info) source_info(commands.selected, commands.source_id, *output);
        else if (commands.action == Action::formats) formats(*output);
        else if (commands.action == Action::requirements) requirements(*output);
        else if (commands.action == Action::screenshot) screenshot(commands.screenshot_options.at(commands.selected), *output);
        else if (commands.action == Action::session) {
            auto& o = commands.options.at(commands.selected);
            validate(o);
            if (o.dry_run) {
                std::string summary = "Ready: " + o.kind;
                if (!o.output.empty()) summary += " -> " + o.output;
                if (o.duration > 0) summary += " (" + std::to_string(o.duration) + " seconds)";
                summary += "\nAudio: ";
                if (o.audio.empty()) summary += "none";
                for (size_t i = 0; i < o.audio.size(); ++i) { if (i) summary += ", "; summary += o.audio[i]; }
                summary += "\nDry run; sources have not been opened.";
                output->event("plan", plan_json(o), summary);
            } else record(o, *output);
        }
        return output->failed ? 1 : 0;
    } catch (const CLI::ParseError& e) {
        json = json || app.get_option("--json")->count() != 0;
        verbose = verbose || app.get_option("--verbose")->count() != 0;
        if (e.get_exit_code() == 0) {
            try {
                if (dynamic_cast<const CLI::CallForVersion*>(&e)) present("version", std::string(e.what()) + '\n');
                else present("help", command_help(active_command(app), dynamic_cast<const CLI::CallForAllHelp*>(&e) != nullptr));
                return 0;
            } catch (const std::exception& failure) {
                report(failure.what(), "\"exit_code\":1");
                return 1;
            }
        }
        report(single_line(e.what(), 110) + ". See '" + command_path(active_command(app)) + " --help'.", "\"exit_code\":2");
        return 2;
    } catch (const LibraryError& e) {
        report(friendly_error(e.detail), "\"exit_code\":1," + error_json(e.detail));
        if (output) output->diagnostic(e.what());
        return 1;
    } catch (const std::filesystem::filesystem_error& e) {
        report("Cannot access the output path. Check the folder and permissions.", "\"exit_code\":1");
        if (output) output->diagnostic(e.what());
        return 1;
    } catch (const std::exception& e) {
        const int status = dynamic_cast<const UsageError*>(&e) ? 2 : 1;
        report(single_line(e.what()), "\"exit_code\":" + std::to_string(status));
        return status;
    } catch (...) {
        report("Unexpected CLI failure.", "\"exit_code\":1");
        return 1;
    }
}
