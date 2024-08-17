#include <iostream>

#include <ada.h>
#include <outcome.hpp>

#include <app.h>

#include <raindrop.h>
#include <raindrop_cache.h>
#include <fele_error.h>

int main(int argc, char* argv[])
{
    auto config_result = Config::load_config(argc, argv);
    if (config_result.has_error())
    {
        std::cerr << config_result.error().to_string() << std::endl;
        return 1;
    }

    if (auto run = App{std::move(config_result.value())}.run(); run.has_error())
    {
        if (run.error().ecode == ExecutionCode::no_such_collection)
        {
            std::cerr << "[ERROR] Could not find collection named '" << run.error().diag << "' in your account!\n"
                << "Currently only top level (root) collection can be used!" << std::endl;
        }
        else
        {
            std::cerr << run.error().to_string() << std::endl;
        }

        return 1;
    }

    return 0;
}
