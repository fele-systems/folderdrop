#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include <fele_error.h>

struct Mount
{
    std::optional<std::string> path;
    std::optional<std::vector<std::string>> tags;
    std::optional<std::vector<std::string>> patterns;
    std::optional<std::string> collection;
    std::optional<std::string> link_prefix;
};

using Mounts = std::unordered_map<std::string, Mount>;

constexpr auto help_message = R"(Folderdrop: bookmark files in your computer to raindrop.io

usage: folderdrop [-hvCd] -m mount1 [-pPtcl] ...
    options:
        -h, --help          show this message
        -v, --verbose       enables verbose logging
        -C, --config-file   load mount definitions from file
        -d, --dry-run       do not execute modifying actions
        -m, --mount         defines a new mount
    mount definition:
        -p, --path          set mount path
        -P, --pattern       set a comma separated list of patterns to filter files
        -t, --tags          set a comma separated list of tags to put on created raindrops
        -c, --collection    set collection name
        -l, --link-prefix   set the prefix used for all links in raindrops
    Environment variables:
        RD_TOKEN (required) token for your Raindrop.io account
        RD_VERBOSE          same as -v, --verbose
)";

namespace std
{
    string to_string(const Mount& mount);
}

/**
 * Error code for config loading operations
 */
enum class LoadMountErrorCode
{
    /** When a given option that requires a value has no value. Diag = option. */
    missing_value = 1,
    /** Multiple options defined with the same name. Diag = option */
    duplicate_mount,
    /** A mount specific option is defined while no option was defined previously. Diag = option */
    option_missing_mount,
    /** Diag = option */
    unknown_option
};

class Config
{
public:
    static Result<Config, LoadMountErrorCode> load_config(int argc, char** argv);
public:
    Mounts mounts;
    bool is_verbose = false;
    bool show_help = false;
    bool dry_run = false;
};

/// @brief Load mounts from the command line
/// @param arg_itr Begin iterator of args to read
/// @param end End iterator of args to read
/// @return Map with the mounts
Result<Mounts, LoadMountErrorCode> load_mount_cmd(std::vector<std::string>::iterator arg_itr, std::vector<std::string>::iterator end);

/// @brief Loads mounts from the configuration file
/// @param filename The filename
/// @return Map with the mounts
Result<Mounts, LoadMountErrorCode>  load_mount_config(const std::string_view filename);