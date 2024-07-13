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

Result<Mounts, LoadMountErrorCode> load_mounts(int argc, char **argv)
{
    OUTCOME_TRY(const auto mounts_cmd, load_mount_cmd(argc, argv));

    Mounts mounts;
    
    if (std::filesystem::exists("config.bs"))
    {
        OUTCOME_TRY(mounts, load_mount_config("config.bs"));
    }

    for (const auto& [key, mount] : mounts_cmd)
    {
        auto prev = mounts.find(key);

        if (prev == mounts.end())
        {
            mounts.insert_or_assign(key, mount);
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

    std::cout << "[DEBUG] Checking configs...\n";
    for (const auto& [k,m] : mounts) {
        std::cout << k << ": \n\t" << std::to_string(m) << std::endl;

        if (!m.path)
            throw std::runtime_error{ "Missing required option for " + k + ": path" };
        else if (!m.collection)
            throw std::runtime_error{ "Missing required option for " + k + ": collection" };
        else if (!m.link_prefix)
            throw std::runtime_error{ "Missing required option for " + k + ": link_prefix" };
    }

    return mounts;
}

Result<std::string, LoadMountErrorCode> get_option_value(const std::vector<std::string>& args, std::vector<std::string>::iterator current)
{
    if (auto next = current + 1; next == args.end())
    {
        return Error{LoadMountErrorCode::missing_value, *current};
    }
    else
    {
        return *next;
    }
}

Result<Mounts, LoadMountErrorCode> load_mount_cmd(int argc, char** argv)
{
    std::vector<std::string> args;
    // Skip arg[0], which is executable path
    std::transform(&argv[1], &argv[argc], std::back_inserter(args), [](const char* arg)
    {
        return std::string{arg};
    });

    Mounts mounts;
    auto mount = mounts.end();

    auto arg = args.begin(); 
    const auto end = args.end();

    while (arg != end)
    {
        if (const auto& a = *arg; a == "-m" || a == "--mount")
        {

            OUTCOME_TRY(auto mount_name, get_option_value(args, arg));

            bool inserted = false;
            std::tie(mount, inserted) = mounts.try_emplace(mount_name);

            if (!inserted)
                return Error{LoadMountErrorCode::duplicate_mount, mount_name};
                
            mount->second.patterns = {"*"};
            ++arg; // Skip the processed value
        }
        else if (a == "-p" || a == "--path")
        {
            if (mount == mounts.end()) Error{LoadMountErrorCode::option_missing_mount, a};
            
            OUTCOME_TRY(mount->second.path, get_option_value(args, arg));

            ++arg; // Skip the processed value
        }
        else if (a == "-P" || a == "--patterns")
        {
            if (mount == mounts.end()) fail_no_mount_defined(a);
            if (arg + 1 == end) fail_missing_value(a, mount->first);

            mount->second.patterns = parse_css(*(arg + 1));
            ++arg; // Skip the processed value
        }
        else if (a == "-t" || a == "--tags")
        {
            if (mount == mounts.end()) fail_no_mount_defined(a);
            if (arg + 1 == end) fail_missing_value(a, mount->first);

            mount->second.tags = parse_css(*(arg + 1));
            ++arg; // Skip the processed value
        }
        else if (a == "-c" || a == "--collection")
        {
            if (mount == mounts.end()) fail_no_mount_defined(a);
            if (arg + 1 == end) fail_missing_value(a, mount->first);

            mount->second.collection = *(arg + 1);
            ++arg; // Skip the processed value
        }
        else if (a == "-l" || a == "--link-prefix")
        {
            if (mount == mounts.end()) fail_no_mount_defined(a);
            if (arg + 1 == end) fail_missing_value(a, mount->first);

            mount->second.link_prefix = *(arg + 1);
            ++arg; // Skip the processed value
        }
        else
        {
            throw std::runtime_error{ "Unknown option: " + a };
        }

        ++arg;
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
