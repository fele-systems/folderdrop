#include <raindrop_queue.h>

#include <iostream>

RaindropQueue::RaindropQueue(const RaindropAccount& account, uint64_t collection_id, std::vector<std::string> tags)
    : account(account),
    collection_id(collection_id),
    tags(std::move(tags))
{
}

RaindropQueue::~RaindropQueue()
{
    offload();
}

std::optional<nlohmann::json> RaindropQueue::append(RaindropBuilder rd)
{
    if (!rd.get_json().contains("collection"))
    {
        rd.set_collection(collection_id);
    }

    if (!rd.get_json().contains("tags"))
    {
        rd.set_tags(tags);
    }
    else
    {
        for (const auto& tag : tags)
            rd.append_tag(tag);
    }
    auto& items = payload["items"];
    
    items.push_back(std::move(rd.get_json()));

    if (items.size() >= max_queue_size)
    {
        return offload();
    }

    return {};
}

std::optional<nlohmann::json> RaindropQueue::offload()
{
    if (payload.contains("items") && !payload["items"].empty())
    {
        auto response = create_raindrops(account, payload);

        if (response["items"].size() != payload["items"].size())
        {
            std::cerr << "[WARNING] Expected " << payload["items"].size() << " raindrops to be created, but only " << response["items"].size() << " came.\n"
                << "Response: <<EOF\n" << response.dump() << "\nEOF" << std::endl;
        }

        init_payload();
        return response;
    }
    else
    {
        return {};
    }
}

void RaindropQueue::init_payload()
{
    payload["items"] = nlohmann::json::array();
}
