#include "help.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace mccli {
namespace {

using Commands = std::vector<const CLI::App*>;
using Options = std::vector<const CLI::Option*>;

Commands children(const CLI::App& command) {
    return command.get_subcommands([](const CLI::App* child) {
        return !child->get_name().empty() && !child->get_group().empty() && !child->get_disabled();
    });
}

std::string path_of(const CLI::App& command) {
    if (!command.get_parent()) return {};
    auto prefix = path_of(*command.get_parent());
    return prefix.empty() ? command.get_name() : prefix + ' ' + command.get_name();
}

std::string command_line(const CLI::App& command) {
    const auto path = path_of(command);
    return path.empty() ? "MediaCap" : "MediaCap " + path;
}

Options visible_options(const CLI::App& command) {
    return command.get_options([](const CLI::Option* option) { return !option->get_group().empty(); });
}

bool global_option(const CLI::Option& option) {
    const auto& names = option.get_lnames();
    for (const std::string_view name : {"help", "help-all", "version", "json", "verbose"}) {
        if (std::find(names.begin(), names.end(), name) != names.end()) return true;
    }
    return false;
}

std::string argument_name(const CLI::Option& option) {
    if (option.get_positional()) {
        auto name = option.get_name(true);
        std::transform(name.begin(), name.end(), name.begin(), [](char character) {
            return character >= 'a' && character <= 'z' ? static_cast<char>(character - 'a' + 'A') : character;
        });
        return name;
    }
    auto type = option.get_type_name();
    type = type.substr(0, type.find(':'));
    if (type == "FLOAT") return "NUMBER";
    if (type == "UINT" || type == "INT") return "N";
    return type.empty() || type == "TEXT" ? "VALUE" : type;
}

std::string option_label(const CLI::Option& option, bool usage = false) {
    if (option.get_positional()) return argument_name(option);
    if (!option.get_option_text().empty() && !usage) {
        return option.get_name(false, true, true) + ' ' + option.get_option_text();
    }
    std::string label;
    if (!usage) {
        for (const auto& name : option.get_snames()) label += '-' + name + ", ";
        for (const auto& name : option.get_lnames()) label += "--" + name + ", ";
        if (!label.empty()) label.resize(label.size() - 2);
    } else {
        label = option.get_name();
    }
    if (option.get_expected_max() != 0) label += ' ' + argument_name(option);
    return label;
}

std::string usage(const CLI::App& command) {
    auto line = command_line(command);
    if (!children(command).empty()) {
        const bool optional = command.get_require_subcommand_min() == 0 && command.get_parent();
        line += optional ? " [COMMAND]" : " COMMAND";
    }
    for (const auto* option : visible_options(command)) {
        const bool positional = option->get_positional();
        if (positional || option->get_required()) {
            auto label = option_label(*option, true);
            if (positional && option->get_expected_max() > 1) label += "...";
            line += option->get_required() ? ' ' + label : " [" + label + ']';
        }
    }
    return line + " [options]";
}

std::string option_description(const CLI::Option& option) {
    auto description = option.get_description();
    if (!description.empty() && description.back() != '.') description += '.';
    if (option.get_required() && description.find("required") == std::string::npos) description += " Required.";
    if (!option.get_default_str().empty()) description += " Default " + option.get_default_str() + '.';
    const auto append = [&](const std::set<CLI::Option*>& options, const std::string& prefix) {
        if (options.empty()) return;
        std::set<std::string> names;
        for (const auto* related : options) names.insert(related->get_name());
        description += prefix;
        auto separator = "";
        for (const auto& name : names) {
            description += separator + name;
            separator = ", ";
        }
        description += '.';
    };
    append(option.get_needs(), " Requires ");
    append(option.get_excludes(), " Incompatible with ");
    return description;
}

std::string example(const CLI::App& command) {
    const auto text = command_example(path_of(command));
    return text.empty() ? command_line(command) + " --help" : "MediaCap " + std::string(text);
}

std::string aliases(const CLI::App& command) {
    std::string result;
    for (const auto& alias : command.get_aliases()) {
        result += (result.empty() ? "" : ", ") + alias;
    }
    return result;
}

class TerminalHelp {
    std::ostringstream text_;

public:
    void line(std::string_view value = {}) { text_ << value << '\n'; }
    void section(std::string_view title) { text_ << '\n' << title << '\n'; }

    void row(std::string_view label, std::string_view description) {
        constexpr std::size_t column = 29, width = 80;
        text_ << "  " << label;
        std::size_t position = label.size() + 2;
        if (description.empty()) {
            line();
            return;
        }
        if (position >= column) {
            line();
            position = 0;
        }
        text_ << std::string(column - position, ' ');
        position = column;
        std::istringstream words{std::string(description)};
        std::string word;
        while (words >> word) {
            if (position > column && position + 1 + word.size() > width) {
                text_ << '\n' << std::string(column, ' ');
                position = column;
            }
            if (position > column) {
                text_ << ' ';
                ++position;
            }
            text_ << word;
            position += word.size();
        }
        line();
    }

    std::string str() const { return text_.str(); }
};

void add_descendants(const CLI::App& command, Commands& commands) {
    for (const auto* child : children(command)) {
        commands.push_back(child);
        add_descendants(*child, commands);
    }
}

void reference_commands(TerminalHelp& help, const CLI::App& command) {
    const auto nested = children(command);
    if (nested.empty()) return;
    for (const auto* child : nested) {
        auto label = usage(*child);
        label.erase(0, std::string_view("MediaCap ").size());
        help.row(label, child->get_description());
        reference_commands(help, *child);
    }
}

void focused_options(TerminalHelp& help, const CLI::App& command) {
    auto options = visible_options(command);
    options.erase(std::remove_if(options.begin(), options.end(), [](const CLI::Option* option) {
        return global_option(*option);
    }), options.end());
    std::vector<std::string> groups;
    for (const auto* option : options) {
        const auto group = option->get_positional() ? "Arguments" : option->get_group();
        if (std::find(groups.begin(), groups.end(), group) == groups.end()) groups.push_back(group);
    }
    for (const auto& group : groups) {
        help.section(group == "OPTIONS" ? "Options" : group);
        for (const auto* option : options) {
            if ((option->get_positional() ? "Arguments" : option->get_group()) == group) {
                help.row(option_label(*option), option_description(*option));
            }
        }
    }
}

struct Variant {
    std::string description;
    std::vector<std::string> paths;
};

std::map<std::string, std::vector<Variant>> collect_options(const Commands& commands) {
    std::map<std::string, std::vector<Variant>> options;
    for (const auto* command : commands) {
        for (const auto* option : visible_options(*command)) {
            if (global_option(*option)) continue;
            auto& variants = options[option_label(*option)];
            auto description = option_description(*option);
            const auto match = std::find_if(variants.begin(), variants.end(), [&](const Variant& variant) {
                return variant.description == description;
            });
            auto path = path_of(*command);
            if (path.empty()) path = "MediaCap";
            if (match == variants.end()) variants.push_back({std::move(description), {std::move(path)}});
            else match->paths.push_back(std::move(path));
        }
    }

    return options;
}

std::string describe_variant(const Variant& variant, size_t command_count) {
    auto description = variant.description;
    if (variant.paths.size() < command_count) {
        description += " Applies to ";
        auto separator = "";
        for (const auto& path : variant.paths) {
            description += separator + path;
            separator = ", ";
        }
        description += '.';
    }
    return description;
}

void reference_options(TerminalHelp& help, const Commands& commands) {
    const auto options = collect_options(commands);
    if (options.empty()) return;
    help.section("Arguments and options");
    for (const auto& [name, variants] : options) {
        for (size_t i = 0; i < variants.size(); ++i)
            help.row(i == 0 ? name : "", describe_variant(variants[i], commands.size()));
    }
}

std::string rst_text(std::string_view value) {
    std::string escaped;
    for (const auto character : value) {
        if (character == '\\' || character == '`' || character == '*' || character == '_') escaped += '\\';
        escaped += character;
    }
    return escaped;
}

void reference_section(std::ostream& document, const CLI::App& command, char underline) {
    auto title = command_line(command);
    document << title << '\n' << std::string(title.size(), underline) << "\n\n";
    document << rst_text(command.get_description()) << "\n\n";
    document << ".. code-block:: console\n\n   " << usage(command) << "\n\n";
    const auto alternatives = aliases(command);
    if (!alternatives.empty()) document << "Aliases are " << rst_text(alternatives) << ".\n\n";
    const auto note = command_note(path_of(command));
    if (!note.empty()) document << rst_text(note) << "\n\n";
    if (command.get_parent()) {
        document << "**Example**\n\n.. code-block:: console\n\n   " << example(command) << "\n\n";
    }
}

void reference_children(std::ostream& document, const CLI::App& command, char underline) {
    for (const auto* child : children(command)) {
        reference_section(document, *child, underline);
        reference_children(document, *child, '"');
    }
}

}

std::string command_help(const CLI::App* command, bool all) {
    TerminalHelp help;
    help.line(command->get_description());
    help.line("Usage: " + usage(*command));
    const auto alternatives = aliases(*command);
    if (!alternatives.empty()) help.row("Aliases", alternatives);

    if (all) {
        const auto nested = children(*command);
        Commands commands{command};
        add_descendants(*command, commands);
        if (!nested.empty()) {
            help.section("Commands");
            reference_commands(help, *command);
        }
        reference_options(help, commands);
        help.section("Examples");
        std::set<std::string> examples;
        for (const auto* child : commands) {
            const auto line = example(*child);
            if (examples.insert(line).second) help.line("  " + line);
        }
    } else {
        const auto nested = children(*command);
        if (!nested.empty()) {
            help.section(path_of(*command) == "record" ? "Recording commands" : "Commands");
            for (const auto* child : nested) help.row(child->get_name(), child->get_description());
        }
        focused_options(help, *command);
        help.section("Example");
        help.line("  " + example(*command));
    }
    if (path_of(*command) == "help") {
        help.section("Guides");
        help.line("  audio, audio-options, codecs, controls, examples, json");
    }
    const auto note = command_note(path_of(*command));
    if (!note.empty()) {
        help.section("Notes");
        help.line(note);
    }
    if (!command->get_parent()) {
        help.section("Global options");
        for (const auto* option : visible_options(*command)) {
            if (global_option(*option)) help.row(option_label(*option), option_description(*option));
        }
        help.line();
        help.line("Use 'MediaCap COMMAND --help' for options and a command example.");
        if (all) help.line("Exit status is 0 for success, 1 for runtime failure, or 2 for invalid input.");
    } else if (!all) {
        help.line();
        help.line("Global options include --json and --verbose. See 'MediaCap --help'.");
    }
    return help.str();
}

std::string HelpFormatter::make_help(const CLI::App* app, std::string, CLI::AppFormatMode mode) const {
    return command_help(app, mode == CLI::AppFormatMode::All);
}

std::string command_reference(const CLI::App& root) {
    std::ostringstream document;
    document << "Command reference\n-----------------\n\n";
    document << rst_text(root.get_description()) << "\n\n"
             << "Global options\n~~~~~~~~~~~~~~\n\n";
    for (const auto* option : visible_options(root)) {
        if (global_option(*option))
            document << "``" << option_label(*option) << "``\n   " << rst_text(option_description(*option)) << "\n\n";
    }
    for (const auto* command : children(root)) {
        reference_section(document, *command, '~');
        Commands commands{command};
        add_descendants(*command, commands);
        const auto options = collect_options(commands);
        if (!options.empty())
            document << "**Arguments and options for " << path_of(*command) << "**\n\n";
        for (const auto& [label, variants] : options) {
            document << "``" << label << "``\n";
            for (const auto& variant : variants)
                document << "   " << rst_text(describe_variant(variant, commands.size())) << "\n\n";
        }
        reference_children(document, *command, '^');
    }
    return document.str();
}

}
