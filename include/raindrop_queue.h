#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <raindrop.h>
#include <nlohmann/json.hpp>
#include <functional>

class RaindropQueue
{
public:
    static constexpr int max_queue_size = 100;
    RaindropQueue(const RaindropAccount& account, uint64_t collection_id, std::vector<std::string> tags);
    RaindropQueue(const RaindropQueue&) = delete;
    RaindropQueue& operator=(const RaindropQueue&) = delete;
    ~RaindropQueue();
    std::optional<nlohmann::json> append(RaindropBuilder rd);
    std::optional<nlohmann::json> offload();
private:
    void init_payload();
private:
    const RaindropAccount& account;
    uint64_t collection_id;
    std::vector<std::string> tags;
    nlohmann::json payload;
    std::function<void(nlohmann::json&)> observer;
};