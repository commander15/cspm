cmake_minimum_required(VERSION 3.30)

project(cspm VERSION 1.0.0)

find_package(Qt6 REQUIRED COMPONENTS Core Network)

add_subdirectory(src)