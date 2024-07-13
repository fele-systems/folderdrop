#pragma once

#include <raindrop.h>
#include <utility>
#include <string>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include <fele_error.h>

template<typename T>
struct FetchResult
{
    int next_page;
    int perpage;
    std::vector<T> results;
};

enum class FindErrorCode
{
    fetch_error = 1,
    no_such_raindrop = 2,
    invalid_url = 3
};

class RaindropCache
{
public:
    explicit RaindropCache(const RaindropAccount& account);
    RaindropCache(const RaindropAccount& account, int results_per_page);
    using Entry = std::pair<std::string, uint64_t>;
    struct Collection
    {
        int next_page_to_fetch = 0;
        std::vector<Entry> entries;
    };
    Result<Entry, FindErrorCode> find_by_link(const uint64_t col, const std::string& link);
    const std::map<uint64_t, Collection>& get_collection_caches() const { return collections; }
private:
    Result<std::vector<Entry>, FindErrorCode> fetch_next_entries(const uint64_t col, int& next_page) const;
private:
    const int per_page;
    const RaindropAccount& account;
    std::map<uint64_t, Collection> collections;
};