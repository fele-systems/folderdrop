#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

struct Mount
{
    std::optional<std::string> path;
    std::optional<std::vector<std::string>> tags;
    std::optional<std::vector<std::string>> patterns;
    std::optional<std::string> collection;
};

using Mounts = std::unordered_map<std::string, Mount>;

namespace std
{
    string to_string(const Mount& mount);
}

/// @brief Load mounts from the command line
/// @param argc Argc
/// @param argv Argv
/// @return Map with the mounts
Mounts load_mount_cmd(int argc, char** argv);

/// @brief Loads mounts from the configuration file
/// @param filename The filename
/// @return Map with the mounts
Mounts load_mount_config(const std::string_view filename);