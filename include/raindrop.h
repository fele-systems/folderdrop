#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>
#include <optional>

/// @brief User account for raindrop
struct RaindropAccount
{
public:
    static const std::string base_url;
    std::string token;
};

/// @brief Builder for creating a payload for raindrop creation.
/// @see create_raindrop
class RaindropBuilder
{
public:
    explicit RaindropBuilder(const std::string& url);
    RaindropBuilder& set_tags(std::vector<std::string> tags);
    RaindropBuilder& append_tag(const std::string& tag);
    RaindropBuilder& set_title(std::string title);
    RaindropBuilder& set_collection(uint64_t id);
    const nlohmann::json& get_json() const;
    nlohmann::json& get_json();
private:
    nlohmann::json body;
};

/// @brief Creates a raindrop
/// @param raindropio The raindrop account
/// @param request Body of the raindrop
/// @return The response JSON
nlohmann::json create_raindrop(const RaindropAccount& raindropio, const nlohmann::json& request);

/// @brief Creates several raindrops
/// @param raindropio The raindrop account
/// @param request Body of the request
/// @return The response JSON
nlohmann::json create_raindrops(const RaindropAccount& raindropio, const nlohmann::json& request);

/// @brief Searches for a collection in the raindrop account
/// @param raindrop The raindrop account
/// @param collection_name Name of the collection
/// @return A pair with the first element being the collection, if it exists, and the second element as the entire api response
std::optional<nlohmann::json> find_collection(const RaindropAccount& raindrop, const std::string& collection_name);

/// @brief Searches for a collection inside the response
/// @param response The response body for a get root or child collections endpoint
/// @param collection_name Name of the collection
/// @return A collection, if it exists
std::optional<nlohmann::json> find_collection_in_response(nlohmann::json& response, const std::string& collection_name);

/// @brief Returns raindrops inside a collection. This is a paginated api
/// @param radindropio The raindrop account
/// @param id Collection id
/// @param result Output vector that the results will be written to
/// @param page The page to fetch results from
/// @return True when there's more pages to be fetched
bool get_raindrops(const RaindropAccount& radindropio, uint64_t id, std::vector<nlohmann::json>& result, int page = 0, int perpage = 10);