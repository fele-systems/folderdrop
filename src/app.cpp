#include "app.h"

#include <iostream>

#include <fmt/color.h>

#include <raindrop_queue.h>

using namespace std::string_literals;
namespace fs = std::filesystem;

bool is_verbose()
{
    auto verbose = std::getenv("RD_VERBOSE");
    return verbose != nullptr && strlen(verbose) > 0;
}

bool App::VERBOSE = is_verbose();

App::App(Config config)
    : config(std::move(config))
{
    auto rd_token = std::getenv("RD_TOKEN");
    if (rd_token != nullptr)
        account = RaindropAccount{ rd_token };
}

Result<void, ExecutionCode> App::run()
{
    OUTCOME_TRY(std::vector<AppMount> appMounts, check_config());

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

Result<std::vector<AppMount>, ExecutionCode> App::check_config() const
{
    std::vector<AppMount> appMounts;
    bool error = false;

    if (!config.dry_run && account.token.empty())
    {
        fmt::print(fg(fmt::color::red), "Missing account token. Define it via the RD_TOKEN environment variable\n");
        error = true;
    }
    else if (config.dry_run && account.token.empty())
    {
        fmt::println("[{}] Skipping collection validation for dry-run because no token was provied",
            fmt::styled("WARNING", fg(fmt::color::yellow)));
    }

    for (const auto& [mount_name, m] : config.mounts)
    {
        fmt::println("{}\n\t{}", mount_name, std::to_string(m));
    }

    for (const auto& [mount_name, m] : config.mounts)
    {
        fmt::println("Checking mount {}", fmt::styled(mount_name, fg(fmt::color::alice_blue) | fmt::emphasis::bold));
        auto result = check_mount(m);
        // Only build the AppMount when 
        if (!result.has_error())
        {
            appMounts.push_back(result.assume_value());
            fmt::println("Mount {} is {}!",
                fmt::styled(mount_name, fg(fmt::color::alice_blue) | fmt::emphasis::bold),
                fmt::styled("OK", fg(fmt::color::green)));
        }
        else
        {
            error = true;
        }
    }

    if (error)
        return Error{ExecutionCode::malformed_mount_config, std::string{}};
    
    return appMounts;
}

Result<AppMount, ExecutionCode> App::check_mount(const Mount &mount) const
{
    AppMount appMount;
    bool error = false;

    if (!mount.collection)
    {
        fmt::println("[{}] Missing collection", fmt::styled("ERROR", fg(fmt::color::red)));
        error = true;
    }
    else if (!account.token.empty())
    {
        const auto collection_name = *mount.collection;
        auto optCol = find_collection(account, collection_name);
        if (!optCol)
        {
            fmt::println("[{}] No such collection: {}",
                         fmt::styled("ERROR", fg(fmt::color::red)),
                         collection_name);
            error = true;
        }
        else
        {
            const auto collection = *optCol;
            appMount.collection_id = collection["_id"].get<uint64_t>();
        }
    }

    if (mount.link_prefix)
    {
        auto link_prefix = ada::parse<ada::url>(*mount.link_prefix);
        if (!link_prefix) {
            fmt::println("[{}] The link prefix '{}' is not a valid URL",
                         fmt::styled("ERROR", fg(fmt::color::red)),
                         *mount.link_prefix);
            error = true;
            // return Error{ExecutionCode::malformed_mount_config, "The link prefix for mount " + mount_name + " is not a valid URL"};
        }
        else
        {
            appMount.link_prefix = *link_prefix;
        }
    }
    else
    {
        fmt::println("[{}] Missing link prefix", fmt::styled("ERROR", fg(fmt::color::red)));
    }

    if (!mount.path)
    {
        fmt::println("[{}] Missing path", fmt::styled("ERROR", fg(fmt::color::red)));
        error = true;
        // return Error{ExecutionCode::malformed_mount_config, "Missing collection for mount " + mount_name};
    }
    else
    {
        appMount.path = *mount.path;
    }

    if (mount.tags) appMount.tags = *mount.tags;

    // Compile regexes
    for (const auto& pattern : mount.patterns.value_or(std::vector<std::string>{}))
    {
        appMount.patterns.emplace_back(pattern, std::regex::ECMAScript);
    }

     if (error)
        return Error{ExecutionCode::malformed_mount_config, std::string{}};
    
    return appMount;
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
