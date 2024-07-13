#include "app.h"

#include <iostream>

#include <raindrop_queue.h>

using namespace std::string_literals;
namespace fs = std::filesystem;

bool is_verbose()
{
    auto verbose = std::getenv("RD_VERBOSE");
    return verbose != nullptr && strlen(verbose) > 0;
}

bool App::VERBOSE = is_verbose();

App::App(Mounts mounts)
    : mounts(std::move(mounts))
{
    if (std::getenv("RD_TOKEN") == nullptr)
        throw std::runtime_error{ "Missing RD_TOKEN with the Raindrop API token." };
        
    account = RaindropAccount{ std::getenv("RD_TOKEN") };
}

Result<void, ExecutionCode> App::run()
{
    std::vector<AppMount> appMounts;

    for (const auto& [k, m] : mounts)
    {
        std::cout << "Checking mount " << k << std::endl;

        if (!m.collection)
            return Error{ExecutionCode::malformed_mount_config, "Missing collection for mount " + k};

        const auto collection_name = *m.collection;

        auto optCol = find_collection(account, collection_name);
        if (!optCol)
        {
            return Error{ExecutionCode::no_such_collection, collection_name};
        }

        const auto collection = *optCol;
        const auto collection_id = collection["_id"].get<uint64_t>();

        auto link_prefix = ada::parse<ada::url>(*m.link_prefix);
        if (!link_prefix) {
            return Error{ExecutionCode::malformed_mount_config, "The link prefix for mount " + k + " is not a valid URL"};
        }

        if (!m.path)
            return Error{ExecutionCode::malformed_mount_config, "Missing collection for mount " + k};

        auto path = std::filesystem::path{ *m.path };

        auto tags = m.tags.value_or(std::vector<std::string>{});

        std::vector<std::regex> patterns;
        // Compile regexes
        for (const auto& pattern : *m.patterns)
        {
            patterns.emplace_back(pattern, std::regex::ECMAScript);
        }

        appMounts.emplace_back(collection_id, std::move(*link_prefix), std::move(path), std::move(tags), std::move(patterns));
    }

    RunStats stats{ 0, 0, 0 };
    for (const auto& appMount : appMounts)
    {
        OUTCOME_TRY(auto stat, execute_mount(appMount));
        stats.created += stat.created;
        stats.excluded += stat.excluded;
        stats.skipped += stat.skipped;

        std::cout << "Mount Stats: created " << stat.created << " / excluded " << stat.excluded << " / skipped " << stat.skipped << std::endl;
    }

    std::cout << "Run Stats: created " << stats.created << " / excluded " << stats.excluded << " / skipped " << stats.skipped << std::endl;

    std::ofstream cache_txt{"cache.txt"};

    for (const auto& [cl, cc] : cache.get_collection_caches())
    {
        cache_txt << "Cache for collection " << cl << " (stopped at page " << cc.next_page_to_fetch << ")" << std::endl;

        if (cc.entries.empty())
        {
            cache_txt << "EMPTY" << std::endl;
        }
        else
        {
            for (const auto& [url, id] : cc.entries)
            {
                cache_txt << "- " << std::to_string(id) << " | " << url << std::endl;
            }
        }
        
    }

    return outcome::success();
}

Result<RunStats, ExecutionCode> App::execute_mount(const AppMount& appMount)
{
    const auto& [col, link_prefix, path, tags, patterns] = appMount;
    RaindropQueue queue { account, col, tags };
    RunStats stats{ 0, 0, 0 };

    for (auto const& dir_entry :fs::directory_iterator{path})
    {
        if (!dir_entry.is_regular_file())
        {
            if (VERBOSE)
                std::cout << "Skipping non-regular file: " << dir_entry.path() << std::endl;

            stats.excluded++;
            continue;
        }

        const auto file = dir_entry.path().filename().string();
        auto match = std::find_if(patterns.begin(), patterns.end(), [&file](const std::regex& r)
        {
            return std::regex_match(file, r, std::regex_constants::match_not_bol | std::regex_constants::match_not_eol);
        });

        if (match == patterns.end())
        {
            if (VERBOSE)
                std::cout << "[INFO] " << file << " not matched against patterns" << std::endl;

            stats.excluded++;
            continue;
        }

        auto link = link_prefix;

        link.set_pathname( (fs::path{ link.get_pathname() } / dir_entry.path().filename()).string() );

        const auto compiled_link = link.get_href();

        auto find_result = cache.find_by_link(col, compiled_link);
        
        if (is_error(find_result, FindErrorCode::no_such_raindrop) )
        {
            if (VERBOSE)
                std::cout << "Will create " << compiled_link << std::endl;
            if (auto r = queue.append(RaindropBuilder{ compiled_link }.set_title(file)); r.has_value())
            {
                stats.created += (*r)["items"].size();
                log_created_raindrops(*r);
            }
        }
        else if (find_result.has_error())
        {
            return Error{ExecutionCode::generic, find_result.error().to_string()};
        }
        else
        {
            if (VERBOSE)
                std::cout << "[INFO] Skipping " << file << " because it already exists with id: " << find_result.value().second << std::endl;

            stats.skipped++;
        }
    }

    if (auto r = queue.offload(); r.has_value())
    {
        stats.created += (*r)["items"].size();
        log_created_raindrops(*r);
    }


    return stats;
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
