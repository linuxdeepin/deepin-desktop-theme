cmake_minimum_required(VERSION 3.11)

project(xdgicon2dci)

find_package(Qt6 COMPONENTS Core Gui REQUIRED)
find_package(Dtk6 COMPONENTS Core REQUIRED)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    find_package(Qt6 COMPONENTS GuiPrivate REQUIRED)
endif()

add_executable(xdgicon2dci main.cpp)
target_link_libraries(xdgicon2dci PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Dtk6::Core
)

set_property(TARGET xdgicon2dci PROPERTY CXX_STANDARD 17)
set_property(TARGET xdgicon2dci PROPERTY CXX_STANDARD_REQUIRED ON)

install(TARGETS xdgicon2dci DESTINATION libexec/deepin-desktop-theme)
