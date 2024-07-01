#include <raindrop.h>

const std::string RaindropAccount::base_url = "https://api.raindrop.io";

RaindropBuilder::RaindropBuilder(const std::string &url)
    : body({
        { "link", url },
        { "pleaseParse", {} }
    })
{
}

RaindropBuilder &RaindropBuilder::set_tags(std::vector<std::string> tags)
{
    body["tags"] = std::move(tags);

    return *this;
}

RaindropBuilder &RaindropBuilder::set_title(std::string title)
{
    body["title"] = std::move(title);

    return *this;
}

RaindropBuilder &RaindropBuilder::set_collection(uint64_t id)
{
    body["collection"] = {
        { "$ref", "collections" },
        { "$id", id }
    };

    return *this;
}

nlohmann::json RaindropBuilder::get_json() const
{
    return body;
}

nlohmann::json create_raindrop(const RaindropAccount &raindropio, nlohmann::json request)
{
    auto r = cpr::Post(cpr::Url{ RaindropAccount::base_url + "/rest/v1/raindrop/" },
        cpr::Bearer{ raindropio.token },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Header{{ "Accept", "application/json" }},
        cpr::Body{request.dump()});

    if (r.status_code != 200)
    {
        throw std::runtime_error{ "Error creating a raindrop: " + r.text };
    }

    return nlohmann::json::parse(r.text);
}

std::optional<nlohmann::json> find_collection(const RaindropAccount& raindrop, const std::string& collection_name)
{
    auto r = cpr::Get(cpr::Url{RaindropAccount::base_url + "/rest/v1/collections"},
        cpr::Bearer{ raindrop.token },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Header{{ "Accept", "application/json" }});

    if (r.status_code != 200)
    {
        throw std::runtime_error{ "Error creating a raindrop: " + r.text };
    }

    auto response = nlohmann::json::parse(r.text);

    return find_collection_in_response(response, collection_name);
}

std::optional<nlohmann::json> find_collection_in_response(nlohmann::json &response, const std::string &collection_name)
{
    auto& items = response["items"];
    auto itr = std::find_if(items.begin(), items.end(), [&](auto&& item)
    {
        return collection_name == item["title"].template get<std::string>();
    });

    if (itr == items.end())
        return {};
    else
        return *itr;
}

bool get_raindrops(const RaindropAccount &radindropio, uint64_t id, std::vector<nlohmann::json>& result, int page, int perpage)
{
    auto r = cpr::Get(cpr::Url{RaindropAccount::base_url + "/rest/v1/raindrops/" + std::to_string(id)},
        cpr::Bearer{ radindropio.token },
        cpr::Header{{ "Content-Type", "application/json" }},
        cpr::Header{{ "Accept", "application/json" }});

    if (r.status_code != 200)
    {
        throw std::runtime_error{ "Error creating a raindrop: " + r.text };
    }

    auto j = nlohmann::json::parse(r.text);
    auto items = j["items"];
    
    auto fetched_records = perpage * page + items.size();
    auto total_records = j["count"].get<int>();

    result = items;

    return !items.empty() && fetched_records < total_records;
}
