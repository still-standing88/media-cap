#pragma once

#include <string>
#include <vector>

namespace CLI { class App; }

namespace mccli {
struct JsonRequest {
    std::vector<std::string> arguments;
    bool json = true;
};

JsonRequest read_json_request(const std::string& filename, const CLI::App& app, bool force_json);
}
