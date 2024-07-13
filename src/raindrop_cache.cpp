#include <algorithm>
#include <iostream>

#include <ada.h>

#include <raindrop_cache.h>
#include <app.h>

RaindropCache::RaindropCache(const RaindropAccount &account)
    : RaindropCache(account, 50)
{
}

RaindropCache::RaindropCache(const RaindropAccount &account, int results_per_page)
    : per_page(results_per_page), account(account)
{
}

Result<RaindropCache::Entry, FindErrorCode> RaindropCache::find_by_link(const uint64_t col, const std::string &link)
{
    if (App::VERBOSE)
        std::cout << "Searching for " << link << " in collection " << col << std::endl;

    auto itr = collections.find(col);
    if (itr == collections.end())
    {
        if (App::VERBOSE)
            std::cout << "Creating a collection cache for collection " << col << std::endl;

        itr = collections.try_emplace(col).first;
    }

    auto& collection = itr->second;

    // entry was already cached, great!
    if (auto entry = std::find_if(collection.entries.begin(), collection.entries.end(), [&link](const Entry& e) { return e.first == link; });
        entry != collection.entries.end())
    {
        if (App::VERBOSE)
            std::cout << "Cache hit!" << std::endl;
        return *entry;
    }

    // we need to fetch more entries

    while (collection.next_page_to_fetch >= 0)
    {
        if (App::VERBOSE)
            std::cout << "Fetching entries from raindrop (continuing at page " << collection.next_page_to_fetch << ")" << std::endl;

        OUTCOME_TRY(auto more_entries, fetch_next_entries(col, collection.next_page_to_fetch));

        // update the cache with fresh entries
        std::copy(more_entries.begin(), more_entries.end(), std::back_inserter(collection.entries));

        // search the entry only in the current page
        if (auto entry = std::find_if(more_entries.begin(), more_entries.end(), [&link](const Entry& e) { return e.first == link; });
            entry != more_entries.end())
        {
            return *entry;
        }
    }

    return Error{FindErrorCode::no_such_raindrop};
}

Result<std::vector<RaindropCache::Entry>, FindErrorCode> RaindropCache::fetch_next_entries(const uint64_t col, int& next_page) const
{
    auto url = ada::parse<ada::url>(RaindropAccount::base_url);
    
    if (!url)
    {
        return Error{FindErrorCode::invalid_url, "Could not parse " + RaindropAccount::base_url};
    }
    
    if (auto pathname = std::filesystem::path{ url->get_pathname() }.append("/rest/v1/raindrops/").append(std::to_string(col)).string(); !url->set_pathname(pathname))
    {
        return Error{FindErrorCode::invalid_url, "Could not parse path '" + pathname + "'"};
    }

    auto res = cpr::Get(cpr::Url{url->get_href()},
        cpr::Parameters{{"perpage", std::to_string(per_page)}, {"page", std::to_string(next_page)}},
        cpr::Bearer{ account.token },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Header{{ "Accept", "application/json" }}
    );

    if (res.status_code != 200)
        return Error{FindErrorCode::fetch_error, "Error while searching for a raindrop: " + res.text };

    auto body = nlohmann::json::parse(res.text);
    auto items = body["items"];

    std::vector<RaindropCache::Entry> entries;

    std::transform(items.begin(), items.end(), std::back_inserter(entries), [](const nlohmann::json& rd)
    {
        const auto link = rd["link"].get<std::string>();
        const auto id = rd["_id"].get<uint64_t>();
        
        return std::make_pair(link, id);
    });

    auto fetched_records = per_page * next_page + items.size();
    auto total_records = body["count"].get<int>();

    if(!entries.empty() && fetched_records < total_records)
    {
        next_page = next_page + 1;
        return entries;
    }
    else
    {
        next_page = -1;
        return entries;
    }
}
