cmake_minimum_required(VERSION 3.11)

message(${CMAKE_CURRENT_SOURCE_DIR})

project(deepin-desktop-theme)

if(NOT DEFINED VERSION)
    set(VERSION 1.0.0)
endif()
add_definitions(-DVERSION="${VERSION}")

add_subdirectory("bloom")
add_subdirectory("vintage")
add_subdirectory("flow")
add_subdirectory("hazy-color")
add_subdirectory("organic-glass")
add_subdirectory("macaron")
add_subdirectory("square")
add_subdirectory("nirvana")
add_subdirectory("origin")

add_subdirectory("tools")

file(GLOB SOURCEDIRS "${CMAKE_CURRENT_SOURCE_DIR}/icons/*.dci")
foreach ( val  ${SOURCEDIRS} )
    install(FILES ${val} DESTINATION share/dsg/icons)
endforeach ()
