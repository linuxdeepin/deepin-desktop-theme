cmake_minimum_required(VERSION 3.11)

message(${CMAKE_CURRENT_SOURCE_DIR})

string(REGEX REPLACE ".*/(.*)" "\\1" PROJ ${CMAKE_CURRENT_SOURCE_DIR})
message(${PROJ})
project(${PROJ})

add_subdirectory("v23-icon")
add_subdirectory("hazy-color")
add_subdirectory("organic-glass")
