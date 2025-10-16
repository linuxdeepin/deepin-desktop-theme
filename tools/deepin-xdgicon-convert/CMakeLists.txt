cmake_minimum_required(VERSION 3.11)
set(TOOL_NAME deepin-xdgicon-convert)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Dtk6 REQUIRED COMPONENTS Gui Widget)

set(TRANSLATION_LANGUAGES zh_CN)

set(TRANSLATION_FILES "")
foreach(LANG ${TRANSLATION_LANGUAGES})
    list(APPEND TRANSLATION_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/deepin-xdgicon-convert_${LANG}.ts")
endforeach()

# can generate ts and qm file
qt_add_translations(${TOOL_NAME}
    TS_FILES ${TRANSLATION_FILES}
    SOURCES ${TRANS_FILE_SOURCE}
    LUPDATE_OPTIONS -no-obsolete -no-ui-lines -locations none
    QM_FILES_OUTPUT_VARIABLE TR_QM_FILES
)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/filechooserwidget.cpp
    src/convertworker.cpp
    src/res/main.qrc
)

add_executable(${TOOL_NAME} ${SOURCES})

# 链接库
target_link_libraries(${TOOL_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Dtk6::Gui
    Dtk6::Widget
)

install(TARGETS ${TOOL_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${TR_QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/${TOOL_NAME}/translations)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/misc/deepin-xdgicon-convert.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/misc/deepin-xdgicon-convert.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps/)
