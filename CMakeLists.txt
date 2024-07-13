# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(folderdrop CXX)

find_package(PkgConfig)

#pkg_check_modules(OpenSSL REQUIRED libopenssl)
#pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 871ed52d350214a034f6ef8a3b8f51c5ce1bd400) # The commit hash for 1.9.0. Replace with the latest from: https://github.com/libcpr/cpr/releases
FetchContent_MakeAvailable(cpr)

# Ada
add_library(ada STATIC thirdparty/ada/ada.cpp)


# Main
add_executable("${PROJECT_NAME}"
            src/main.cpp src/raindrop.cpp src/mounts.cpp src/raindrop_queue.cpp src/app.cpp src/raindrop_cache.cpp)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_17)

target_link_libraries("${PROJECT_NAME}" PRIVATE cpr::cpr ada)

target_include_directories("${PROJECT_NAME}" PRIVATE
            ./include
            ./thirdparty/ada
            ./thirdparty/outcome
            ./thirdparty/magic_enum
            "${NLOHMANN_JSON_INCLUDE_DIRS}")
