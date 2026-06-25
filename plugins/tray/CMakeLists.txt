
set(PLUGIN_NAME "tray")

project(${PLUGIN_NAME})

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp" "../../widgets/*.h" "../../widgets/*.cpp"
    "../../frame/util/themeappicon.h" "../../frame/util/themeappicon.cpp"
    "../../frame/util/dockpopupwindow.h" "../../frame/util/dockpopupwindow.cpp"
    "../../frame/util/abstractpluginscontroller.h" "../../frame/util/abstractpluginscontroller.cpp"
    "../../frame/util/pluginloader.h" "../../frame/util/pluginloader.cpp"
    "../../frame/dbus/sni/*.h" "../../frame/dbus/sni/*.cpp"
    "../../frame/dbus/dbusmenu.h" "../../frame/dbus/dbusmenu.cpp"
    "../../frame/dbus/dbusmenumanager.h" "../../frame/dbus/dbusmenumanager.cpp")

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg DBus Gui)
pkg_check_modules(dtk2widget REQUIRED dtk2widget)
find_package(DFrameworkdbusQt6 REQUIRED)

pkg_check_modules(XCB_LIBS REQUIRED xcb-ewmh xcb xcb-image xcb-composite xtst xcb-icccm)
pkg_check_modules(DDE-Network-Utils REQUIRED gxde-network-utils)
pkg_check_modules(QGSettings REQUIRED gsettings-qt6)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS} tray.qrc)
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../)
target_include_directories(${PLUGIN_NAME} PUBLIC ${dtk2widget_INCLUDE_DIRS}
                                                 ${XCB_LIBS_INCLUDE_DIRS}
                                                 ${DDE-Network-Utils_INCLUDE_DIRS}
                                                 
                                                 ${QGSettings_INCLUDE_DIRS}
                                                 ../../interfaces
                                                 ../../frame)
# Vendored dbusmenu-lxqt headers
target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/3rdparty/libdbusmenu/src"
    "${CMAKE_BINARY_DIR}/cmake/libdbusmenu/dbusmenu-build/src")
target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${dtk2widget_LIBRARIES}
    Qt6::Widgets
    Qt6::DBus
    Qt6::Svg
    Qt6::Gui
    Qt6::GuiPrivate
    dbusmenu-lxqt
    ${XCB_LIBS_LIBRARIES}
    ${DDE-Network-Utils_LIBRARIES}
    DFrameworkdbusQt6::DFrameworkdbusQt6
    ${QGSettings_LIBRARIES}
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/gxde-dock/plugins)
