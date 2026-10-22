cmake_minimum_required(VERSION 3.16)

project(drming LANGUAGES CXX)

add_subdirectory(client)

if(NOT ANDROID)
    add_subdirectory(server)
endif()
