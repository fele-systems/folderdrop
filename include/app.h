#pragma once

#include <raindrop.h>
#include <mounts.h>

class App
{
public:
    App(int argc, char** argv);
    int run();
private:
    static Mounts load_mounts(int argc, char** argv);
    void execute_mount(const Mount& mount);
private:
    Mounts mounts;
    RaindropAccount account;
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

void append_to_url(std::string& url, std::string_view append);