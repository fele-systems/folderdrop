#include <iostream>

#include <ada.h>
#include <outcome.hpp>

#include <app.h>

#include <raindrop.h>
#include <raindrop_cache.h>
#include <fele_error.h>

int main(int argc, char* argv[])
{
    auto mounts = load_mounts(argc, argv);
    if (mounts.has_error())
    {
        std::cerr << mounts.error().to_string() << std::endl;
        return 1;
    }

    if (auto run = App{mounts.value()}.run(); run.has_error())
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
