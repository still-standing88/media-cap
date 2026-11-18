#pragma once

#include "../cli.hpp"
#include "json.hpp"

namespace mccli {
using DiscoveryJson = nlohmann::ordered_json;

struct SourceRow {
    std::vector<std::string> cells;
    DiscoveryJson data;
    std::string detail;
};

struct Listing {
    std::string title;
    std::vector<std::string> headers;
    std::vector<SourceRow> rows;
    std::string diagnostic;
    MediaCap::Error warning{};

    Listing(std::string name, std::vector<std::string> columns)
        : title(std::move(name)), headers(std::move(columns)) {}

    DiscoveryJson array() const;
    std::string table(const ListOptions& options) const;
};

Listing inventory(const std::string& category, const ListOptions& options, bool verbose);
std::string id_for_category(const std::string& kind, const std::string& id);
}
