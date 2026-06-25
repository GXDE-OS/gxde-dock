
set(PLUGIN_NAME "keyboard-layout")

project(${PLUGIN_NAME})

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp")

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg DBus)
pkg_check_modules(dtk2widget REQUIRED dtk2widget)
find_package(DFrameworkdbusQt6 REQUIRED)
find_package(DFrameworkdbus REQUIRED)

pkg_check_modules(QGSettings REQUIRED gsettings-qt6)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS})
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../)
target_include_directories(${PLUGIN_NAME} PUBLIC ${dtk2widget_INCLUDE_DIRS}
                                                 
                                                 ${QGSettings_INCLUDE_DIRS}
                                                 ../../interfaces)
target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${dtk2widget_LIBRARIES}
    DFrameworkdbusQt6::DFrameworkdbusQt6
    ${QGSettings_LIBRARIES}
    Qt6::Widgets
    Qt6::Svg
    Qt6::DBus
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/gxde-dock/plugins)
install(FILES ./keybord_layout.json DESTINATION /etc/gxde-dock/indicator)
