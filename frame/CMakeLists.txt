cmake_minimum_required(VERSION 3.7)

set(BIN_NAME gxde-dock)
set(TRANS_DIR "${CMAKE_CURRENT_DIR}/translate")
configure_file(environments.h.in environments.h @ONLY)

# Sources files
file(GLOB_RECURSE SRCS "*.h" "*.cpp" "*.c" "../widgets/*.h" "../widgets/*.cpp")

# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent DBus Gui)
find_package(LayerShellQt REQUIRED)

pkg_check_modules(dtk2widget REQUIRED dtk2widget)
pkg_check_modules(dtk6core REQUIRED dtk6core)
pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11)
pkg_check_modules(QGSettings REQUIRED gsettings-qt6)

# Qt6 VER. of dframeworkdbus
find_package(DFrameworkdbusQt6 REQUIRED)

# driver-manager
add_executable(${BIN_NAME} ${SRCS} ${INTERFACES} item/item.qrc frame.qrc)
target_include_directories(${BIN_NAME} PUBLIC ${dtk2widget_INCLUDE_DIRS}
                                              ${dtk6core_INCLUDE_DIRS}
                                              ${XCB_EWMH_INCLUDE_DIRS}
                                              /usr/include/libdframeworkdbus-2.0
                                              ${PROJECT_BINARY_DIR}
                                              ${QGSettings_INCLUDE_DIRS}
                                              ../interfaces)
target_link_libraries(${BIN_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Gui
    Qt6::GuiPrivate
    ${dtk2widget_LIBRARIES}
    ${dtk6core_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${QGSettings_LIBRARIES}
    DFrameworkdbusQt6::DFrameworkdbusQt6
    LayerShellQt::Interface
    wayland-client
)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "sw_64")
    target_compile_definitions(${BIN_NAME} PUBLIC DISABLE_SHOW_ANIMATION)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "mips64")
    target_compile_definitions(${BIN_NAME} PUBLIC DISABLE_SHOW_ANIMATION)
endif()

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    target_compile_definitions(${BIN_NAME} PUBLIC DISABLE_SHOW_ANIMATION)
endif()

# bin
install(TARGETS ${BIN_NAME} DESTINATION bin)
