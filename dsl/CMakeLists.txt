cmake_minimum_required(VERSION 3.16)
project(dsl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(dsl
    main.cpp
    codegen.cpp
    parser.cpp
)

target_link_libraries(dsl PRIVATE tinyxml2)