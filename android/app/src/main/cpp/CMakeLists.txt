cmake_minimum_required(VERSION 3.22)
project(hhmg_android LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# the same dependency-free engine the VST3 plugin uses (repo root /core)
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core)
file(GLOB CORE_SRC ${CORE}/src/*.cpp)

add_library(hhmg_android SHARED native_bridge.cpp ${CORE_SRC})
target_include_directories(hhmg_android PRIVATE ${CORE}/include)
target_link_libraries(hhmg_android log)
