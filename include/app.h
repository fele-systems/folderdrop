#pragma once

#include <regex>

#include <ada.h>

#include <raindrop.h>
#include <raindrop_cache.h>

#include <mounts.h>

enum class ExecutionCode
{
    malformed_mount_config = 1,
    no_such_collection,
    generic
};

struct RunStats
{
    /** Number of created raindrops */
    int created;
    /** Number of files skipped because already existed in collection */
    int skipped;
    /** Total number of files excluded by pattern */
    int excluded;
};

using AppMount = std::tuple<uint64_t, ada::url, std::filesystem::path, std::vector<std::string>, std::vector<std::regex>>;

class App
{
public:
    App(Mounts mounts);
    Result<void, ExecutionCode> run();
private:
    Result<RunStats, ExecutionCode> execute_mount(const AppMount& appMount);
public:
    static bool VERBOSE;
private:
    Mounts mounts;
    RaindropAccount account;
    RaindropCache cache {account, 100};
};

using Raindrop = std::pair<std::string, uint64_t>;

/// @brief Fetch the current raindrops inside the collection
/// @param raindropio Raindrop account
/// @param collection_id Id of the collection
/// @return Vector of raindrops
std::vector<Raindrop> get_raindrops(
    const RaindropAccount& raindropio,
    uint64_t collection_id);


void log_created_raindrops(const nlohmann::json &r);