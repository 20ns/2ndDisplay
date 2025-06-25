cmake_minimum_required(VERSION 3.24)

project(fec VERSION 0.1.0 LANGUAGES CXX)

# Add library target
add_library(fec INTERFACE)

# Add include directory
target_include_directories(fec INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Set required C++ standard
target_compile_features(fec INTERFACE cxx_std_20)