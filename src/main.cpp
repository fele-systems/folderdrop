#include <iostream>
#include <filesystem>

int main()
{
    const std::filesystem::path folder = "../upp";
    for (auto const& dir_entry : std::filesystem::directory_iterator{folder}) 
        std::cout << dir_entry.path() << '\n';


    return 0;
}