
set(PLUGIN_NAME "disk-mount")

project(${PLUGIN_NAME})

# Sources files
file(GLOB SRCS "*.h" "*.cpp")

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
pkg_check_modules(dtk2widget REQUIRED dtk2widget)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS} resources.qrc)
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ../)
target_include_directories(${PLUGIN_NAME} PUBLIC ${dtk2widget_INCLUDE_DIRS} ../../interfaces)
target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${dtk2widget_LIBRARIES}
    Qt6::Widgets
    Qt6::Svg
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/gxde-dock/plugins)
