
set(PLUGIN_NAME "power")

project(${PLUGIN_NAME})

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp" "../../widgets/*.h" "../../widgets/*.cpp")

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg DBus)
pkg_check_modules(dtk2widget REQUIRED dtk2widget)
find_package(DFrameworkdbusQt6 REQUIRED)


add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS} power.qrc)
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../system-trays)
target_include_directories(${PLUGIN_NAME} PUBLIC ${dtk2widget_INCLUDE_DIRS}
                                                 
                                                 ../../interfaces)
target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${dtk2widget_LIBRARIES}
    Qt6::Widgets
    Qt6::Svg
    Qt6::DBus
    DFrameworkdbusQt6::DFrameworkdbusQt6
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/gxde-dock/plugins/system-trays)
