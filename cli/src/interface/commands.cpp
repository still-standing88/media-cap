#include "commands.hpp"
#include "../../help/help.hpp"

namespace mccli {
Commands::Commands() {
    app.formatter(std::make_shared<HelpFormatter>());
    app.set_version_flag("--version", "MediaCap 1.0.0");
    app.set_help_all_flag("--help-all", "Show the complete reference for this command");
    app.require_subcommand(0, 1);
    app.add_flag("--json", json, "Machine-readable output");
    app.add_flag("-v,--verbose", verbose, "Include diagnostic details");
    app.add_flag("--_camera-worker", list_options.worker)->group("");
    app.add_option("--_exact-id", list_options.id)->group("");
    auto filters = [&](CLI::App* node, bool processes = false) {
        node->add_option("--name", list_options.name, "Filter names (case-insensitive)");
        node->add_flag("--details", list_options.details, "Show device IDs and capabilities");
        if (processes) node->add_flag("--active", list_options.active, "Only processes playing audio");
    };
    auto* list = app.add_subcommand("list", "List capture sources");
    list->require_subcommand(0, 1); filters(list);
    list->callback([this] { if (selected.empty()) { action = Action::list; selected = "all"; } });
    auto add_list = [&](CLI::App* parent, const std::string& name, const std::string& description, const std::string& target) {
        auto* c = parent->add_subcommand(name, description);
        filters(c, target == "processes");
        c->callback([this, target] { action = Action::list; selected = target; });
        return c;
    };
    add_list(list, "monitors", "Connected displays", "monitors")->alias("screens");
    add_list(list, "windows", "Window handles and titles", "windows");
    add_list(list, "cameras", "Connected cameras", "cameras");
    auto* audio = list->add_subcommand("audio", "Input, output and process audio sources");
    audio->require_subcommand(0, 1); filters(audio);
    audio->callback([this] { if (selected.empty()) { action = Action::list; selected = "audio"; } });
    add_list(audio, "input", "Microphones and other recording inputs", "audio-input")->alias("inputs");
    add_list(audio, "output", "Speakers and other playback outputs", "audio-output")->alias("outputs");
    add_list(audio, "processes", "Applications with audio sessions", "processes")->alias("process");
    add_list(audio, "devices", "All input and output devices", "audio");
    add_list(audio, "compat", "Alternate capture-device list", "audio-classic");
    add_list(list, "all", "All source categories", "all");
    // Keep old scripts working without exposing duplicate commands in help.
    add_list(list, "audio-classic", "", "audio-classic")->group("");
    add_list(list, "processes", "", "processes")->group("");
    auto* info = app.add_subcommand("info", "Show details for one source");
    info->require_subcommand(0, 1);
    info->callback([this, info] { if (selected.empty()) { action = Action::help; help_node = info; } });
    for (const auto& entry : std::vector<std::pair<std::string, std::string>>{
             {"monitor", "Monitor ID"}, {"window", "Window handle"}, {"camera", "Camera index"},
             {"audio-input", "Input index or device ID"}, {"audio-output", "Output index or device ID"},
             {"audio-process", "Process ID"}, {"compat-audio", "Compatibility audio index"}}) {
        auto* child = info->add_subcommand(entry.first, "Inspect one " + entry.first + " source");
        child->add_option("id", source_id, entry.second)->required();
        child->callback([this, kind = entry.first] { action = Action::info; selected = kind; });
    }
    auto* caps = app.add_subcommand("capabilities", "Check supported capture modes");
    caps->require_subcommand(0, 1);
    caps->add_flag("--details", list_options.details, "Show routing limits");
    caps->callback([this] { if (action == Action::none) { action = Action::list; selected = "capabilities"; } });
    auto* audio_caps = caps->add_subcommand("audio", "Audio modes and process routing");
    audio_caps->add_flag("--details", list_options.details, "Show routing limits");
    caps->add_subcommand("formats", "List available containers and codecs")->callback([this] { action = Action::formats; });
    caps->add_subcommand("requirements", "Show platform prerequisites for capture features")
        ->callback([this] { action = Action::requirements; });
    for (const std::string mode : {"record", "capture"}) {
        const bool raw = mode == "capture";
        auto* session = app.add_subcommand(mode, raw ? "Capture raw frames and audio samples" : "Record sources to a media file");
        session->require_subcommand(0, 1);
        session->callback([this, session] { if (selected.empty()) { action = Action::help; help_node = session; } });
        for (const auto& entry : std::vector<std::pair<std::string, std::string>>{
                 {"screen", "a monitor"}, {"window", "a window"}, {"camera", "a camera"},
                 {"audio", "input, output, process or mixed audio"}}) {
            const auto key = mode + ' ' + entry.first;
            auto& o = options[key];
            o.kind = entry.first;
            o.capture_only = raw;
            auto* child = session->add_subcommand(entry.first, std::string(raw ? "Capture " : "Record ") + entry.second);
            add_record_options(*child, o);
            child->callback([this, key] { action = Action::session; selected = key; });
        }
    }
    auto* screenshots = app.add_subcommand("screenshot", "Save a monitor or window image");
    screenshots->require_subcommand(0, 1);
    screenshots->callback([this, screenshots] { if (selected.empty()) { action = Action::help; help_node = screenshots; } });
    for (const std::string kind : {"screen", "window"}) {
        auto& o = screenshot_options[kind];
        o.kind = kind;
        auto* child = screenshots->add_subcommand(kind, kind == "screen" ? "Capture one monitor image" : "Capture one window image");
        if (kind == "screen") {
            child->alias("monitor");
            child->add_option("--monitor", o.monitor, "Monitor ID (default: first monitor)");
        } else {
            auto* handle = child->add_option("--window", o.window, "Decimal or hexadecimal window handle");
            auto* title = child->add_option("--title", o.window_title, "Exact, unique window title");
            handle->excludes(title);
        }
        child->add_option("-o,--output", o.output, "BMP output file")->type_name("FILE")->required();
        child->add_option("--region", o.region, "Crop in source pixels, measured from the top-left corner")->type_name("X,Y,W,H");
        child->add_option("--size", o.size, "Resize after cropping")->type_name("WIDTHxHEIGHT");
        child->add_option("--timeout", o.timeout, "Time to wait for the first frame. Default 10000 ms")->type_name("MS")->check(CLI::NonNegativeNumber);
        child->add_flag("--overwrite", o.overwrite, "Replace an existing output file");
        child->callback([this, kind] { action = Action::screenshot; selected = kind; });
    }
    auto* docs = app.add_subcommand("docs", "Print or export the command reference");
    docs->add_option("-o,--output", docs_file, "Write to a file. A .rst filename selects reStructuredText")->type_name("FILE");
    docs->add_option("--format", docs_format, "Choose text or rst. Standard output defaults to text")
        ->type_name("FORMAT")->check(CLI::IsMember({"text", "rst"}));
    docs->callback([this] { action = Action::docs; });
    auto* run = app.add_subcommand("run", "Run a command from a JSON request");
    run->add_option("file", request_file, "JSON request file (use - for stdin)")->required();
    run->callback([this] { action = Action::run; });
    auto* help = app.add_subcommand("help", "Examples and command help");
    help->add_option("topic", topic, "A command or a guide name");
    help->add_option("subcommand", path, "Remaining command path");
    help->callback([this] { action = Action::help; });
    std::function<void(CLI::App*)> globals = [&](CLI::App* node) {
        node->fallthrough();
        for (auto* c : node->get_subcommands([](CLI::App*) { return true; })) globals(c);
    };
    globals(&app);
}
}
