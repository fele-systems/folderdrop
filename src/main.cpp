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

using Raindrop = std::pair<std::string, uint64_t>;

/// @brief Fetch the current raindrops inside the collection
/// @param raindropio Raindrop account
/// @param collection_id Id of the collection
/// @return Vector of raindrops
std::vector<Raindrop> get_raindrops(
    const RaindropAccount& raindropio,
    uint64_t collection_id);

int main(int argc, char* argv[])
{
    const std::string collection_name = "test";
    const std::vector<std::string> tags = { "#is/video" };
    const std::string link_prefix = std::getenv("RD_LINKPREFIX");
    const std::filesystem::path folder = argc > 1
        ? argv[1]
        : "./";


    const RaindropAccount account { std::getenv("RD_TOKEN") };

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
