cmake_minimum_required(VERSION 3.11)

project(xdgicon2dci)

find_package(Qt6 COMPONENTS Core Concurrent REQUIRED)

add_executable(xdgicon2dci main.cpp)
target_link_libraries(xdgicon2dci PRIVATE Qt6::Core Qt6::Concurrent)

# 设置 C++ 标准
set_property(TARGET xdgicon2dci PROPERTY CXX_STANDARD 17)
set_property(TARGET xdgicon2dci PROPERTY CXX_STANDARD_REQUIRED ON)

install(TARGETS xdgicon2dci DESTINATION libexec/deepin-desktop-theme)
