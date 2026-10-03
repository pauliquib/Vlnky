cmake_minimum_required(VERSION 3.21)
project(vlnky VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick OpenGL ShaderTools Test Concurrent)

find_package(ZLIB REQUIRED)

option(VLNKY_COPY_USER_QML_MODULE "Also copy the QML module to ~/.local/lib64/qml at build time" ON)

# Qt RHI headers (rhi/qrhi.h) are shipped with QtGui but some distributions put them in a
# separate package (Fedora: qt6-qtbase-private-devel). Prefer the system copy; fall back to the
# bundled Qt 6.6 headers in third_party/ (LGPL-3.0 / GPL-2.0 / GPL-3.0, The Qt Company).
include(CheckIncludeFileCXX)
get_target_property(_qtgui_includes Qt6::Gui INTERFACE_INCLUDE_DIRECTORIES)
set(CMAKE_REQUIRED_INCLUDES ${_qtgui_includes})
set(CMAKE_REQUIRED_FLAGS "-std=c++17 -fPIC")
check_include_file_cxx("rhi/qrhi.h" VLNKY_HAVE_SYSTEM_RHI)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_FLAGS)
if(NOT VLNKY_HAVE_SYSTEM_RHI)
    message(WARNING "rhi/qrhi.h not found in the system Qt; using bundled third_party/rhi headers. "
                    "Install the Qt private/RHI headers (Fedora: qt6-qtbase-private-devel) for an exact match.")
endif()

add_library(xmbwave STATIC
    libxmbwave/rco_error.cpp
    libxmbwave/rco/rco_reader.cpp
    libxmbwave/xmb_crc32.cpp
    libxmbwave/xmbwave.cpp
    libxmbwave/wave_extract.cpp
    libxmbwave/xmb_gmo_parser.cpp
    libxmbwave/xmb_fcurve.cpp
    libxmbwave/xmb_texture.cpp
    libxmbwave/xmb_channel_map.cpp
    libxmbwave/xmb_mesh_builder.cpp
    libxmbwave/xmb_cache.cpp
    libxmbwave/xmb_diagnostics.cpp
    libxmbwave/psp_wave.cpp
)
set_target_properties(xmbwave PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(xmbwave PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libxmbwave)
target_link_libraries(xmbwave PUBLIC Qt6::Core ZLIB::ZLIB)

add_library(vlnkywave SHARED
    src/xmb_wave_renderer.cpp
    src/wave_scanner.cpp
    src/plugin.cpp
    src/xmb_logging.cpp
)
target_include_directories(vlnkywave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(NOT VLNKY_HAVE_SYSTEM_RHI)
    target_include_directories(vlnkywave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
endif()
target_link_libraries(vlnkywave PRIVATE xmbwave Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::Concurrent)
set_target_properties(vlnkywave PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

qt6_add_shaders(vlnkywave "wave_shaders" PREFIX "/" FILES
    shaders/wave.vert
    shaders/wave.frag
    shaders/post.vert
    shaders/downsample.frag
    shaders/blur.frag
    shaders/composite.frag
)

add_executable(vlnky-preview tools/xmb_preview_app.cpp)
target_include_directories(vlnky-preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vlnky-preview PRIVATE vlnkywave xmbwave Qt6::Gui Qt6::Qml Qt6::Quick)

add_executable(vlnky-extract tools/xmb_extract_cli.cpp)
target_link_libraries(vlnky-extract PRIVATE xmbwave Qt6::Gui)

enable_testing()

add_executable(tst_rco_bounds tests/tst_rco_bounds.cpp)
target_link_libraries(tst_rco_bounds PRIVATE xmbwave Qt6::Test Qt6::Core)
add_test(NAME tst_rco_bounds COMMAND tst_rco_bounds)

add_executable(tst_fcurve_hermite tests/tst_fcurve_hermite.cpp)
target_link_libraries(tst_fcurve_hermite PRIVATE xmbwave Qt6::Test Qt6::Core)
add_test(NAME tst_fcurve_hermite COMMAND tst_fcurve_hermite)

add_executable(tst_no_fake_anim tests/tst_no_fake_anim.cpp)
target_link_libraries(tst_no_fake_anim PRIVATE xmbwave Qt6::Test Qt6::Core)
add_test(NAME tst_no_fake_anim COMMAND tst_no_fake_anim)

if(DEFINED ENV{VLNKY_WAVE_COLLECTION})
    add_executable(tst_golden_rco tests/tst_golden_rco.cpp)
    target_compile_definitions(tst_golden_rco PRIVATE VLNKY_WAVE_COLLECTION="$ENV{VLNKY_WAVE_COLLECTION}")
    target_link_libraries(tst_golden_rco PRIVATE xmbwave Qt6::Test Qt6::Core)
    add_test(NAME tst_golden_rco COMMAND tst_golden_rco)

    add_executable(tst_rco_reader tests/tst_rco_reader.cpp)
    target_compile_definitions(tst_rco_reader PRIVATE VLNKY_WAVE_COLLECTION="$ENV{VLNKY_WAVE_COLLECTION}")
    target_link_libraries(tst_rco_reader PRIVATE xmbwave Qt6::Test Qt6::Core)
    add_test(NAME tst_rco_reader COMMAND tst_rco_reader)

    add_executable(tst_psp_wave tests/tst_psp_wave.cpp)
    target_compile_definitions(tst_psp_wave PRIVATE VLNKY_WAVE_COLLECTION="$ENV{VLNKY_WAVE_COLLECTION}")
    target_link_libraries(tst_psp_wave PRIVATE xmbwave Qt6::Test Qt6::Core)
    add_test(NAME tst_psp_wave COMMAND tst_psp_wave)

    add_executable(tst_gmo_orientation tests/tst_gmo_orientation.cpp)
    target_compile_definitions(tst_gmo_orientation PRIVATE VLNKY_WAVE_COLLECTION="$ENV{VLNKY_WAVE_COLLECTION}")
    target_link_libraries(tst_gmo_orientation PRIVATE xmbwave Qt6::Test Qt6::Core)
    add_test(NAME tst_gmo_orientation COMMAND tst_gmo_orientation)
endif()

set(PKG_DIR ${CMAKE_BINARY_DIR}/plasma-wallpaper/package)
set(QML_MODULE_DIR ${PKG_DIR}/contents/imports/org/psvec/vlnky)
set(QML_MODULE_UI_DIR ${PKG_DIR}/contents/ui/org/psvec/vlnky)
set(QML_MODULE_GLOBAL_DIR "$ENV{HOME}/.local/lib64/qml/org/psvec/vlnky")

set(QMLDIR_CONTENT "module org.psvec.vlnky\nplugin vlnkywave\nclassname VlnkyPlugin\n")
file(WRITE ${CMAKE_BINARY_DIR}/qmldir.vlnkywave "${QMLDIR_CONTENT}")

set(VLNKY_GLOBAL_QML_COMMANDS)
if(VLNKY_COPY_USER_QML_MODULE)
    set(VLNKY_GLOBAL_QML_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E make_directory ${QML_MODULE_GLOBAL_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vlnkywave> ${QML_MODULE_GLOBAL_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/qmldir.vlnkywave ${QML_MODULE_GLOBAL_DIR}/qmldir)
endif()

add_custom_target(plasma-wallpaper-package ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/plasma-wallpaper ${PKG_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QML_MODULE_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QML_MODULE_UI_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vlnkywave> ${QML_MODULE_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vlnkywave> ${QML_MODULE_UI_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/qmldir.vlnkywave ${QML_MODULE_DIR}/qmldir
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/qmldir.vlnkywave ${QML_MODULE_UI_DIR}/qmldir
    ${VLNKY_GLOBAL_QML_COMMANDS}
    DEPENDS vlnkywave
    COMMENT "Assembling Plasma/Wallpaper package + GPU QML module"
)

message(STATUS "After build run:")
message(STATUS "  bash scripts/install.sh")
message(STATUS "  systemctl --user restart plasma-plasmashell.service")
