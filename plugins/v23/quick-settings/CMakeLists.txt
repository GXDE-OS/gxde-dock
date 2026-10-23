set(PLUGIN_NAME "quick-settings-v23")

project(${PLUGIN_NAME})

# The icons in icons/ are those of Deepin V23: the bloom icon theme
# (deepin-icon-theme 2024.07.31) where the plugins look them up first, else
# the resources of dde-tray-loader 1.0.9, dde-shell 1.0.10 and
# deepin-screen-recorder 6.0.9; the DCI ones were converted to PNG.
file(GLOB SRCS "*.h" "*.cpp" "../../../widgets/*.h" "../../../widgets/*.cpp")

find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg DBus)
pkg_check_modules(dtk2widget REQUIRED dtk2widget)
find_package(DFrameworkdbusQt6 REQUIRED)
pkg_check_modules(NETUTILS REQUIRED gxde-network-utils-qt6)

add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")
add_library(${PLUGIN_NAME} SHARED ${SRCS} quick-settings.qrc)

set_target_properties(${PLUGIN_NAME} PROPERTIES
  OUTPUT_NAME quick-settings
  LIBRARY_OUTPUT_DIRECTORY ../)
target_include_directories(${PLUGIN_NAME} PUBLIC ${dtk2widget_INCLUDE_DIRS}
                                                 ${NETUTILS_INCLUDE_DIRS}
                                                 ../../../interfaces)
target_link_libraries(${PLUGIN_NAME} PRIVATE
  Qt6::DBus
  ${dtk2widget_LIBRARIES}
  ${NETUTILS_LIBRARIES}
  DFrameworkdbusQt6::DFrameworkdbusQt6
  Qt6::Widgets
  Qt6::Svg
)

install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/gxde-dock/plugins/v23)
