#include <mounts.h>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <string_utils.h>
#include <fstream>
#include <cerrno>
#include <cstring>
#include <iostream>

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

Mounts load_mount_cmd(int argc, char** argv)
{
    std::vector<std::string> args;
    std::transform(&argv[0], &argv[argc], std::back_inserter(args), [](const char* arg)
    {
        return std::string{arg};
    });

    Mounts mounts;
    auto mount = mounts.end();

    auto arg = ++args.begin(); // Skip arg[0], which is executable path
    const auto end = args.end();
    while (arg != end)
    {
        if (const auto& a = *arg; a == "-m" || a == "--mount")
        {
            if (arg + 1 == end)
                throw std::runtime_error{ "Missing mount name for " + a + "!" };
            auto mount_name = *(arg + 1);

            bool inserted = false;
            std::tie(mount, inserted) = mounts.try_emplace(mount_name);
            if (!inserted)
                throw std::runtime_error{ "Duplicated mount: " + mount_name + "!" };
            mount->second.patterns = {"*"};
            ++arg; // Skip the processed value
        }
        else if (a == "-p" || a == "--path")
        {
            if (mount == mounts.end())
                throw std::runtime_error{ "Trying to define " + a + " but not mount was defined. Define a mount using -m or --mount before any calls to " + a };
            if (arg + 1 == end)
                throw std::runtime_error{ "Missing value for " + a + " while building mount " + mount->first };

            mount->second.path = *(arg + 1);
            ++arg; // Skip the processed value
        }
        else if (a == "-P" || a == "--patterns")
        {
            if (mount == mounts.end())
                throw std::runtime_error{ "Trying to define " + a + " but not mount was defined. Define a mount using -m or --mount before any calls to " + a };
            if (arg + 1 == end)
                throw std::runtime_error{ "Missing value for " + a + " while building mount " + mount->first };

            mount->second.patterns = parse_css(*(arg + 1));
            ++arg; // Skip the processed value
        }
        else if (a == "-t" || a == "--tags")
        {
            if (mount == mounts.end())
                throw std::runtime_error{ "Trying to define " + a + " but not mount was defined. Define a mount using -m or --mount before any calls to " + a };
            if (arg + 1 == end)
                throw std::runtime_error{ "Missing value for " + a + " while building mount " + mount->first };

            mount->second.tags = parse_css(*(arg + 1));
            ++arg; // Skip the processed value
        }
        else if (a == "-c" || a == "--collection")
        {
            if (mount == mounts.end())
                throw std::runtime_error{ "Trying to define " + a + " but not mount was defined. Define a mount using -m or --mount before any calls to " + a };
            if (arg + 1 == end)
                throw std::runtime_error{ "Missing value for " + a + " while building mount " + mount->first };

            mount->second.collection = *(arg + 1);
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

Mounts load_mount_config(const std::string_view filename)
{
    Mounts mounts;
    auto mount = mounts.end();
    std::ifstream istream{ filename.data() };
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
