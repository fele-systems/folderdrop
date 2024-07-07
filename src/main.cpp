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

using Raindrop = std::pair<std::string, uint64_t>;

/// @brief Fetch the current raindrops inside the collection
/// @param raindropio Raindrop account
/// @param collection_id Id of the collection
/// @return Vector of raindrops
std::vector<Raindrop> get_raindrops(
    const RaindropAccount& raindropio,
    uint64_t collection_id);

Mounts load_mounts(int argc, char** argv);

int execute_mount(const Mount& mount, const RaindropAccount& account);

int main(int argc, char* argv[])
{
    using namespace std::string_literals;

    const auto mounts = load_mounts(argc, argv);
    const RaindropAccount account { std::getenv("RD_TOKEN") };

    const std::string collection_name = "test";
    const std::vector<std::string> tags = { "#is/video" };
    const std::string link_prefix = std::getenv("RD_LINKPREFIX");
    const std::filesystem::path folder = argc > 1
        ? argv[1]
        : "./";

    

    std::cout << "Token: " << account.token << std::endl;

    auto optCol = find_collection(account, collection_name);

    if (!optCol.has_value())
    {
        std::cerr << "Could not find collection named '" << collection_name << "' in your account!\n"
            << "Currently only top level (root) collection can be used!" << std::endl;
        return 1;
    }
    
    auto collection = *optCol;
    const auto col_id = collection["_id"].get<uint64_t>();
    std::vector<Raindrop> raindrops_cache = get_raindrops(account, col_id);

    for (auto const& dir_entry : std::filesystem::directory_iterator{folder})
    {
        if (!dir_entry.is_regular_file())
        {
            std::cout << "Skipping non-regular file: " << dir_entry.path() << std::endl;
            continue;
        }

        const auto file = dir_entry.path().filename().string();
        const auto link = link_prefix + file; // TODO: Better handling of URLs

        auto existing_rd = std::find_if(raindrops_cache.begin(), raindrops_cache.end(), [&link](auto& rd)
        {
            return rd.first == link;
        });

        if (existing_rd == raindrops_cache.end())
        {
            const auto rd = create_raindrop(account, RaindropBuilder{ link }
                .set_tags(tags)
                .set_collection(col_id)
                .set_title(file)
                .get_json());    
            const auto rd_id = rd["item"]["_id"].get<uint64_t>();
            std::cout << "Created raindrop: " << rd_id << std::endl;
        }
        else
        {
            std::cout << "Skipping " << file << " because it already exists with id: " << existing_rd->second << std::endl;
        }
    }


    return 0;
}

std::vector<Raindrop> get_raindrops(const RaindropAccount &raindropio, uint64_t collection_id)
{
    std::vector<Raindrop> vec;
    int current_page = 0;
    std::vector<nlohmann::json> out_raindrops;
    auto mapper = [](const nlohmann::json& rd) -> Raindrop
    {
        const auto link = rd["link"].get<std::string>();
        const auto id = rd["_id"].get<uint64_t>();
        
        return {link, id};
    };
    auto has_more = get_raindrops(raindropio, collection_id, out_raindrops, current_page, 5);
    ++current_page;

    std::transform(out_raindrops.begin(), out_raindrops.end(), std::back_inserter(vec), mapper);

    while (has_more)
    {
        has_more = get_raindrops(raindropio, collection_id, out_raindrops, current_page, 5);
        ++current_page;
    }

    std::transform(out_raindrops.begin(), out_raindrops.end(), std::back_inserter(vec), mapper);

    return vec;
}

Mounts load_mounts(int argc, char **argv)
{
    const auto mounts_cmd = load_mount_cmd(argc, argv);
    auto mounts = load_mount_config("config.bs");

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

    std::cout << "Checking configs...\n";
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
    const auto link_prefix = *mount.link_prefix;
    const auto path = *mount.path;
    const auto tags = mount.tags.value_or(std::vector<std::string>{});
    const auto patterns = *mount.patterns;
    
    auto optCol = find_collection(account, collection_name);

    if (!optCol)
    {
        std::cerr << "Could not find collection named '" << collection_name << "' in your account!\n"
            << "Currently only top level (root) collection can be used!" << std::endl;
        return 1;
    }

    const auto collection = *optCol;
    const auto collection_id = collection["_id"].get<uint64_t>();
    const std::vector<Raindrop> raindrops_cache = get_raindrops(account, collection_id);

    for (auto const& dir_entry : std::filesystem::directory_iterator{path})
    {
        if (!dir_entry.is_regular_file())
        {
            std::cout << "Skipping non-regular file: " << dir_entry.path() << std::endl;
            continue;
        }

        const auto file = dir_entry.path().filename().string();
        const auto link = link_prefix + file; // TODO: Better handling of URLs

        auto existing_rd = std::find_if(raindrops_cache.begin(), raindrops_cache.end(), [&link](auto& rd)
        {
            return rd.first == link;
        });

        if (existing_rd == raindrops_cache.end())
        {
            const auto rd = create_raindrop(account, RaindropBuilder{ link }
                .set_tags(tags)
                .set_collection(collection_id)
                .set_title(file)
                .get_json());    
            const auto rd_id = rd["item"]["_id"].get<uint64_t>();
            std::cout << "Created raindrop: " << rd_id << std::endl;
        }
        else
        {
            std::cout << "Skipping " << file << " because it already exists with id: " << existing_rd->second << std::endl;
        }
    }



}
