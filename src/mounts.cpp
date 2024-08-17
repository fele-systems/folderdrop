#include <filesystem>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string_view>

#include <mounts.h>
#include <string_utils.h>

using namespace std::string_literals;

/// @brief Parses a comma separated string
/// @param css comma separated string
/// @return A vector
std::vector<std::string> parse_css(std::string css)
{
    std::vector<std::string> vec;

    std::string s;
    auto itr = css.begin();
    auto end = css.end();
    while (itr != end)
    {
        if (*itr == ',')
        {
            lr_trim(s);
            if (!s.empty())
            {
                vec.push_back(s);
                s.clear();
            }
            ++itr; // Skip the comma
        }
        s += *itr;
        ++itr;
    }

    // Parse the last one
    lr_trim(s);
    if (!s.empty())
        vec.push_back(s);

    return vec;
}

/// @brief Tests if the given arg corresponds to any of the option's forms
/// @param arg Value from argv
/// @param short_opt The short option format
/// @param long_opt The long option format
/// @return 
inline bool test_option(std::string_view arg, const char* short_opt, const char* long_opt)
{
    // TODO:LOW Also accept the "long option with equals" form (--config-file=<file>)
    return arg == short_opt || arg == long_opt;
}

__attribute__((noreturn)) void fail_no_mount_defined(std::string_view option)
{
    throw std::runtime_error{ "Trying to define "s + option + " but not mount was defined. Define a mount using -m or --mount before any calls to " + option };
}

__attribute__((noreturn)) void fail_missing_value(std::string_view option, std::string_view mount)
{
    throw std::runtime_error{ "Missing value for "s + option + " while building mount " + mount };
}

std::string missing_value(std::string_view option, std::string_view mount)
{
    return "Missing value for "s + option + " while building mount " + mount;
}

Result<std::string, LoadMountErrorCode> consume_option_value(const std::vector<std::string>& args, std::vector<std::string>::iterator& current)
{
    if (current + 1 == args.end())
    {
        return Error{LoadMountErrorCode::missing_value, *current};
    }
    else
    {
        return *(++current);
    }
}

Result<std::string, LoadMountErrorCode> consume_option_value(std::vector<std::string>::iterator& current, std::vector<std::string>::iterator end)
{
    if (current + 1 == end)
    {
        return Error{LoadMountErrorCode::missing_value, *current};
    }
    else
    {
        return *(++current);
    }
}

Result<Mounts, LoadMountErrorCode> load_mount_cmd(std::vector<std::string>::iterator arg_itr, std::vector<std::string>::iterator end)
{
    Mounts mounts;
    auto mount = mounts.end();

    while (arg_itr != end)
    {
        if (const auto& a = *arg_itr; a == "-m" || a == "--mount")
        {
            OUTCOME_TRY(auto mount_name, consume_option_value(arg_itr, end));
            bool inserted = false;
            std::tie(mount, inserted) = mounts.try_emplace(mount_name);
            if (!inserted)
                return Error{LoadMountErrorCode::duplicate_mount, mount_name};
            mount->second.patterns = {".*"};
        }
        else if (a == "-p" || a == "--path")
        {
            if (mount == mounts.end()) Error{LoadMountErrorCode::option_missing_mount, a};
            OUTCOME_TRY(mount->second.path, consume_option_value(arg_itr, end));
        }
        else if (a == "-P" || a == "--patterns")
        {
            if (mount == mounts.end()) Error{LoadMountErrorCode::option_missing_mount, a};
            OUTCOME_TRY(auto arg_value, consume_option_value(arg_itr, end));
            mount->second.patterns = parse_css(arg_value);
        }
        else if (a == "-t" || a == "--tags")
        {
            if (mount == mounts.end()) Error{LoadMountErrorCode::option_missing_mount, a};
            OUTCOME_TRY(auto arg_value, consume_option_value(arg_itr, end));
            mount->second.tags = parse_css(arg_value);
        }
        else if (a == "-c" || a == "--collection")
        {
            if (mount == mounts.end()) Error{LoadMountErrorCode::option_missing_mount, a};
            OUTCOME_TRY(mount->second.collection, consume_option_value(arg_itr, end));
        }
        else if (a == "-l" || a == "--link-prefix")
        {
            if (mount == mounts.end()) Error{LoadMountErrorCode::option_missing_mount, a};
            OUTCOME_TRY(mount->second.link_prefix, consume_option_value(arg_itr, end));
        }
        else
        {
            return Error{LoadMountErrorCode::unknown_option, a};
        }

        ++arg_itr;
    }

    return mounts;
}

Result<Mounts, LoadMountErrorCode> load_mount_config(const std::string_view filename)
{
    Mounts mounts;
    auto mount = mounts.end();
    std::ifstream istream{ std::string{filename} };
    int line_no = 0;

    if (!istream.is_open())
        throw std::runtime_error{ "Could not open file: "s + std::strerror(errno) };
    
    do
    {
        std::string line;
        std::getline(istream, line, '\n');
        line = fele::trim(line);
        ++line_no;

        if (line.empty() || line.front() == '#')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            const auto mount_name = lr_trim_cp(line.substr(1, line.size() - 2));
            if (mount_name.empty())
                throw std::runtime_error{ "Missing mount name at line "s + std::to_string(line_no) };

            bool inserted = false;
            std::tie(mount, inserted) = mounts.try_emplace(mount_name);
            if (!inserted)
                throw std::runtime_error{ "Duplicated mount: " + mount_name + " defined at line " + std::to_string(line_no) + "!" };
            mount->second.patterns = {"*"};
        }
        else
        {
            const auto equals_pos = line.find('=');
            if (equals_pos == std::string::npos)
                throw std::runtime_error{ "Invalid option. Missing equals to separate option name from value in: '" + line + "' defined at line " + std::to_string(line_no) + "." };

            const auto option = line.substr(0, equals_pos);
            const auto value = line.substr(equals_pos + 1);

            if (mount == mounts.end())
                throw std::runtime_error{ "Trying to set option " + option + " but no block was defined. Define a module with [module name]. Line: " + std::to_string(line_no) + "." };

            if (option == "path")
                mount->second.path = value;
            else if (option == "patterns")
                mount->second.patterns = parse_css(value);
            else if (option == "tags")
                mount->second.tags = parse_css(value);
            else if (option == "collection")
                mount->second.collection = value;
            else if (option == "link_prefix")
                mount->second.link_prefix = value;
            else
                throw std::runtime_error{ "Unknown option: '" + line + "' defined at line " + std::to_string(line_no) + "." };
        }
    } while (istream);
    return mounts;
}

template<typename T>
std::string opt_to_string(const std::optional<T>& opt)
{
    if constexpr (std::is_same_v<std::string, std::decay_t<T>> || std::is_convertible_v<T, std::string>)
        return opt.has_value() ? *opt : "empty";
    else
        return opt.has_value() ? std::to_string(*opt) : "empty";
}

std::string std::to_string(const Mount &mount)
{
    return "Mount{path=" + opt_to_string(mount.path)
        + "; tags=" + (mount.tags ? join(*mount.tags) : "empty")
        + "; patterns=" + (mount.patterns ? join(*mount.patterns) : "empty")
        + "; collection=" + opt_to_string(mount.collection) + "}";
}



Result<Config, LoadMountErrorCode> Config::load_config(int argc, char **argv)
{
    std::vector<std::string> args;
    // Skip arg[0], which is executable path
    std::transform(&argv[1], &argv[argc], std::back_inserter(args), [](const char* arg) { return std::string{arg}; });

    Config config;

    auto arg_itr = args.begin();
    const auto end = args.end();
    std::string config_filename;

    // First, parse common options
    while (arg_itr != end)
    {
        if (const auto& a = *arg_itr; test_option(a, "-v", "--verbose"))
        {
            config.is_verbose = true;
        }
        else if (test_option(a, "-h", "--help"))
        {
            config.show_help = true;
            // If help is called, stop processing any further
            return config;
        }
        else if (test_option(a, "-C", "--config-file"))
        {
            OUTCOME_TRY(config_filename, consume_option_value(args, arg_itr));
        }
        else if (test_option(a, "-d", "--dry-run"))
        {
            config.dry_run = true;
        }
        else if (test_option(a, "-m", "--mount"))
        {
            // When we hit the first -m, stop immediately to
            // avoid returning the error bellow!
            break;
        }
        else
        {
            return Error{LoadMountErrorCode::unknown_option, a};
        }

        ++arg_itr;
    }

    // Second, parse the mound definitions from command line and then from config file (is specified)
    OUTCOME_TRY(const auto mounts_cmd, load_mount_cmd(arg_itr, end));

    if (!config_filename.empty())
    {
        OUTCOME_TRY(config.mounts, load_mount_config(config_filename));
    }

    // Third, merge the mounts from cmd and config
    for (const auto& [key, mount] : mounts_cmd)
    {
        auto prev = config.mounts.find(key);

        if (prev == config.mounts.end())
        {
            config.mounts.insert_or_assign(key, mount);
        }
        else
        {
            if (mount.path) prev->second.path = *mount.path;
            if (mount.collection) prev->second.collection = *mount.collection;
            if (mount.patterns) prev->second.patterns = *mount.patterns;
            if (mount.tags) prev->second.tags = *mount.tags;
            if (mount.link_prefix) prev->second.link_prefix = *mount.link_prefix;
        }
    }

    return config;
}
