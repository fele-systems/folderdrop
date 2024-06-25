# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(folderdrop CXX)

# Main
add_executable("${PROJECT_NAME}"
            src/main.cpp)

target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_17)
           
target_include_directories("${PROJECT_NAME}" PRIVATE
            ./include)
