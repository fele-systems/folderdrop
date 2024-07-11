#include <iostream>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <errno.h>
#include <vector>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <optional>
#include <raindrop.h>
#include <algorithm>
#include <string_utils.h>
#include <cstdlib>
#include <mounts.h>
#include <regex>
#include <raindrop_queue.h>

using Raindrop = std::pair<std::string, uint64_t>;

/// @brief Fetch the current raindrops inside the collection
/// @param raindropio Raindrop account
/// @param collection_id Id of the collection
/// @return Vector of raindrops
std::vector<Raindrop> get_raindrops(
    const RaindropAccount& raindropio,
    uint64_t collection_id);

Mounts load_mounts(int argc, char** argv);

int execute_mount(const Mount &mount, const RaindropAccount &account);

void log_created_raindrops(const nlohmann::json &r);

void append_to_url(std::string& url, std::string_view append);

int main(int argc, char* argv[])
{
    using namespace std::string_literals;

    const auto mounts = load_mounts(argc, argv);
    if (std::getenv("RD_TOKEN") == nullptr)
    {
        std::cerr << "Missing RD_TOKEN with the Raindrop API token." << std::endl;
        return 1;
    }
    const RaindropAccount account { std::getenv("RD_TOKEN") };

    for (const auto& [k, m] : mounts)
    {
        try
        {
            std::cout << "[INFO] Executing mount " << k << std::endl;
            execute_mount(m, account);
        }
        catch (std::exception& e)
        {
            std::cerr << "[ERROR] While processing " << k << ": \n" << e.what() << std::endl;
            return 1;
        }
    }

    return 0;
}

std::vector<Raindrop> get_raindrops(const RaindropAccount &raindropio, uint64_t collection_id)
{
    const auto perpage = 100;
    std::vector<Raindrop> vec;
    int current_page = 0;
    std::vector<nlohmann::json> out_raindrops;
    auto mapper = [](const nlohmann::json& rd) -> Raindrop
    {
        const auto link = rd["link"].get<std::string>();
        const auto id = rd["_id"].get<uint64_t>();
        
        return {link, id};
    };
    auto has_more = get_raindrops(raindropio, collection_id, out_raindrops, current_page, perpage);
    ++current_page;

    std::transform(out_raindrops.begin(), out_raindrops.end(), std::back_inserter(vec), mapper);

    while (has_more)
    {
        has_more = get_raindrops(raindropio, collection_id, out_raindrops, current_page, perpage);
        std::transform(out_raindrops.begin(), out_raindrops.end(), std::back_inserter(vec), mapper);
        ++current_page;
    }

    return vec;
}

Mounts load_mounts(int argc, char **argv)
{
    const auto mounts_cmd = load_mount_cmd(argc, argv);

    Mounts mounts;
    
    if (std::filesystem::exists("config.bs"))
    {
        mounts = load_mount_config("config.bs");
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

int execute_mount(const Mount &mount, const RaindropAccount& account)
{
    const auto collection_name = *mount.collection;
    auto optCol = find_collection(account, collection_name);
    if (!optCol)
    {
        std::cerr << "[ERROR] Could not find collection named '" << collection_name << "' in your account!\n"
            << "Currently only top level (root) collection can be used!" << std::endl;
        return 1;
    }

    const auto collection = *optCol;
    const auto collection_id = collection["_id"].get<uint64_t>();
    const auto link_prefix = *mount.link_prefix;
    const auto path = *mount.path;
    const auto tags = mount.tags.value_or(std::vector<std::string>{});

    RaindropQueue queue { account, collection_id, tags };

    std::vector<std::regex> patterns;
    // Compile regexes
    for (const auto& pattern : *mount.patterns)
    {
        patterns.emplace_back(pattern, std::regex::ECMAScript);
    }

    // Get currently existing raindrops
    const std::vector<Raindrop> raindrops_cache = get_raindrops(account, collection_id);

    std::cout << "[DEBUG] existing raindrops: \n";
    for (const auto& [url, id] : raindrops_cache)
        std::cout << '\t' << url << ':' << std::to_string(id) << std::endl;

    for (auto const& dir_entry : std::filesystem::directory_iterator{path})
    {
        if (!dir_entry.is_regular_file())
        {
            std::cout << "Skipping non-regular file: " << dir_entry.path() << std::endl;
            continue;
        }

        const auto file = dir_entry.path().filename().string();
        auto match = std::find_if(patterns.begin(), patterns.end(), [&file](const std::regex& r)
        {
            return std::regex_match(file, r, std::regex_constants::match_not_bol | std::regex_constants::match_not_eol);
        });

        if (match == patterns.end())
        {
            std::cout << "[INFO] " << file << " not matched against patterns" << std::endl;
            continue;
        }

        auto link = link_prefix;
        append_to_url(link, file);

        std::cout << "[DEBUG] expected url: " << link << std::endl;

        auto existing_rd = std::find_if(raindrops_cache.begin(), raindrops_cache.end(), [&link](auto& rd)
        {
            return rd.first == link;
        });

        if (existing_rd == raindrops_cache.end())
        {
            if (auto r = queue.append(RaindropBuilder{ link }.set_title(file)); r.has_value())
                log_created_raindrops(*r);
        }
        else
        {
            std::cout << "[INFO] Skipping " << file << " because it already exists with id: " << existing_rd->second << std::endl;
        }
    }

    if (auto r = queue.offload(); r.has_value())
        log_created_raindrops(*r);


    return 0;
}

void log_created_raindrops(const nlohmann::json &r)
{
    std::cout << "[INFO] Created raindrops:\n";
    for (const auto &item : r["items"])
    {
        const auto id = item["_id"].get<uint64_t>();
        std::cout << "\t- " << std::to_string(id) << '\n';
    }
    std::cout.flush();
}

void append_to_url(std::string &url, std::string_view append)
{
    if (append.empty() || url.empty()) return;

    if (append[0] == '/')
        append = append.substr(1);

    if (url.back() != '/')
        url.push_back('/');

    for (const char ch : append)
    {
        if (ch >= ' ' && ch <= ',')
        {
            url.push_back('%');
            std::array<char, 64> buffer;            
            size_t written = snprintf(buffer.data(), buffer.size(), "%x", static_cast<uint32_t>(ch));
            url.append(buffer.data(), written);
        }
        else
        {
            url.push_back(ch);
        }
    }
}
