# Box3D samples

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.17
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(implot)

# nativefiledialog-extended for the Replay open dialog. Native OS file picker, target nfd.
set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        v1.3.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(nfd)

# The shader compiler is only needed when recompiling shaders. A fresh clone
# with BOX3D_BUILD_SHADERS=OFF builds from the committed headers and skips this.
# Jozz: skip the download when a usable SOKOL_SHDC_EXECUTABLE is already provided.
if(BOX3D_BUILD_SHADERS AND (NOT SOKOL_SHDC_EXECUTABLE OR NOT EXISTS "${SOKOL_SHDC_EXECUTABLE}"))
    FetchContent_Declare(sokol_tools_bin
        GIT_REPOSITORY https://github.com/floooh/sokol-tools-bin.git
        GIT_TAG        1a9a4e54090fec42c5d13169b638f09f25474953
        # unfortunately cannot be shallow when using a SHA
        # GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(sokol_tools_bin)
endif()

# imgui static lib: the five core sources. sokol_imgui is the backend (no
# glfw/opengl3 backend sources), so it is not listed here.
add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
set_target_properties(imgui PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

add_library(implot STATIC
    "${implot_SOURCE_DIR}/implot.cpp"
    "${implot_SOURCE_DIR}/implot_items.cpp"
    "${implot_SOURCE_DIR}/implot_demo.cpp"
    "${implot_SOURCE_DIR}/implot.h"
    "${implot_SOURCE_DIR}/implot_internal.h"
)
target_link_libraries(implot PUBLIC imgui)
target_include_directories(implot SYSTEM PUBLIC "${implot_SOURCE_DIR}")
set_target_properties(implot PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# --- sokol: vendored headers INTERFACE target + per-platform backend. --------
set(sokol_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/sokol")
add_library(sokol INTERFACE)
target_include_directories(sokol SYSTEM INTERFACE "${sokol_SOURCE_DIR}")
if(WIN32)
    target_compile_definitions(sokol INTERFACE SOKOL_D3D11)
    target_link_libraries(sokol INTERFACE
        d3d11 dxgi dxguid
        user32 gdi32 shell32 ole32)
elseif(APPLE)
    target_compile_definitions(sokol INTERFACE SOKOL_METAL)
    target_link_libraries(sokol INTERFACE
        "-framework Foundation"
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        "-framework MetalKit")
elseif(UNIX)
    target_compile_definitions(sokol INTERFACE SOKOL_GLCORE)
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    if(NOT X11_Xi_FOUND OR NOT X11_Xcursor_FOUND)
        message(FATAL_ERROR
            "sokol_app needs Xi and Xcursor; install libxi-dev and libxcursor-dev.")
    endif()
    target_link_libraries(sokol INTERFACE
        OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS} m
        X11::X11 X11::Xi X11::Xcursor)
else()
    message(FATAL_ERROR "Unsupported platform for sokol backend selection.")
endif()

# sokol-shdc executable path, consumed by shaders/CMakeLists.txt.
# Jozz: only derive the path when the cache does not already point at a real binary.
if(BOX3D_BUILD_SHADERS)
    if(WIN32)
        set(_shdc_subdir win32)
        set(_shdc_exe    sokol-shdc.exe)
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_shdc_subdir osx_arm64)
        else()
            set(_shdc_subdir osx)
        endif()
        set(_shdc_exe sokol-shdc)
    else()
        set(_shdc_subdir linux)
        set(_shdc_exe    sokol-shdc)
    endif()
    if(NOT SOKOL_SHDC_EXECUTABLE OR NOT EXISTS "${SOKOL_SHDC_EXECUTABLE}")
        set(SOKOL_SHDC_EXECUTABLE
            "${sokol_tools_bin_SOURCE_DIR}/bin/${_shdc_subdir}/${_shdc_exe}"
            CACHE FILEPATH "Path to sokol-shdc binary" FORCE)
    endif()
    unset(_shdc_subdir)
    unset(_shdc_exe)
endif()

# --- Renderer + shaders
add_subdirectory(shaders)
add_subdirectory(gfx)

# Jozz Vehicle headless core: the physics/contract/config-io TUs with no GUI
# dependency. SINGLE SOURCE OF TRUTH - compiled into BOTH `samples` (below)
# and `jozz_vehicle_validation` (further down). Add a new headless file HERE
# once and both targets get it; previously the list was duplicated and a file
# added to one target could silently drift from the other. A future rig-editor
# target reuses the same variable.
set(JOZZ_VEHICLE_CORE_FILES
    jozz_vehicle_asset_contract.cpp
    jozz_vehicle_asset_contract.h
    jozz_vehicle_asset_dimensions.cpp
    jozz_vehicle_asset_dimensions.h
    jozz_vehicle_asset_metadata.cpp
    jozz_vehicle_asset_metadata.h
    jozz_vehicle_asset_paths.cpp
    jozz_vehicle_asset_paths.h
    jozz_vehicle_json.cpp
    jozz_vehicle_json.h
    jozz_vehicle_m5_vehicle.cpp
    jozz_vehicle_m5_vehicle.h
    jozz_vehicle_m6_config_io.cpp
    jozz_vehicle_m6_config_io.h
    jozz_vehicle_m6_suspension_rig.cpp
    jozz_vehicle_m6_suspension_rig.h
    jozz_vehicle_m7_suspension_import.cpp
    jozz_vehicle_m7_suspension_import.h
    jozz_vehicle_steering_suspension_contract.cpp
    jozz_vehicle_steering_suspension_contract.h
    jsmn.h
)

set(SAMPLE_FILES
    main.cpp

    # Host
    host/camera.cpp
    host/camera.h
    host/gui.cpp
    host/gui.h
    host/imgui_impl.cpp
    host/sokol_app_impl.c

    sample.cpp
    sample.h
    mesh_loader.cpp
    mesh_loader.h
    tiny_obj_loader.h
    earcut.h

    # Jozz Vehicle layer: shared headless core (see JOZZ_VEHICLE_CORE_FILES)
    ${JOZZ_VEHICLE_CORE_FILES}

    # Jozz Vehicle layer: GUI-only (labs, benches, renderer, image decode) -
    # samples target only, not the headless validator.
    jozz_vehicle_corner_rig.cpp
    jozz_vehicle_corner_rig.h
    jozz_vehicle_debug_preview.cpp
    jozz_vehicle_debug_preview.h
    jozz_vehicle_image_decode.cpp
    jozz_vehicle_image_decode.h
    jozz_vehicle_m5_drivable_lab.cpp
    jozz_vehicle_m5_drivable_lab.h
    jozz_vehicle_m5_test_course.cpp
    jozz_vehicle_m5_test_course.h
    jozz_vehicle_m6_rig_lab.cpp
    jozz_vehicle_m6_rig_lab.h
    jozz_vehicle_m6_rig_lab_internal.h
    jozz_vehicle_m6_rig_lab_mount_visual.cpp
    jozz_vehicle_m6_rig_lab_persistence.cpp
    jozz_vehicle_m6_rig_lab_ui_tabs.cpp
    jozz_vehicle_m8_rig_bench.cpp
    jozz_vehicle_m8_rig_bench.h
    jozz_vehicle_m9_steering_rig_bench.cpp
    jozz_vehicle_m9_steering_rig_bench.h
    jozz_vehicle_primitive_corner_lab.cpp
    jozz_vehicle_primitive_corner_lab.h
    jozz_vehicle_visual_asset.cpp
    jozz_vehicle_visual_asset.h
    jozz_vehicle_visual_mesh.cpp
    jozz_vehicle_visual_mesh.h

    # Samples
    sample_benchmark.cpp
    sample_bodies.cpp
    sample_character.cpp
    sample_collision.cpp
    sample_compound.cpp
    sample_continuous.cpp
    sample_determinism.cpp
    sample_events.cpp
    sample_geometry.cpp
    sample_issues.cpp
    sample_joint.cpp
    sample_jozz_vehicle_lab.cpp
    host/screenshot.cpp
    host/screenshot.h
    sample_manifold.cpp
    sample_mesh.cpp
    sample_ragdoll.cpp
    sample_replay.cpp
    sample_robustness.cpp
    sample_shapes.cpp
    sample_stacking.cpp
    sample_tree.cpp
    sample_world.cpp
)

add_executable(samples ${SAMPLE_FILES})

# The Jozz Vehicle labs' UI text is Polish, so source files carry non-ASCII
# UTF-8 bytes (e.g. "prędkość"). Without this flag MSVC falls back to
# interpreting narrow string literals via the current Windows code page
# instead of UTF-8, corrupting those bytes at compile time - a different, more
# fundamental problem than the font glyph range (host/gui.cpp) also fixed for
# the same reason. /utf-8 is a no-op for the plain-ASCII box3d demo samples.
target_compile_options(samples PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Jozz: headless metadata/contract/vehicle guard, no GUI. Runs in CI-style validation.
# R1 split (2026-07-10): the probe bodies live in validation/, one file per
# responsibility group, sharing validation/jozz_validation_helpers.{h,cpp}.
set(JOZZ_VALIDATION_FILES
    validation/jozz_validation_helpers.h
    validation/jozz_validation_helpers.cpp
    validation/jozz_probes_m5_m6.cpp
    validation/jozz_probes_m7.cpp
    validation/jozz_probes_steering.cpp
    validation/jozz_probes_config.cpp
)
add_executable(jozz_vehicle_validation
    jozz_vehicle_validation.cpp
    ${JOZZ_VEHICLE_CORE_FILES}
    ${JOZZ_VALIDATION_FILES}
)

if(APPLE)
    # sokol_app's Cocoa backend is Objective-C. Compile its impl TU as OBJC with ARC.
    set_source_files_properties(host/sokol_app_impl.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(host/sokol_app_impl.c PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

# Include root is the samples dir so "gfx/..." and "host/..." prefixes resolve.
# The data dir is on the path too so samples can #include recorded dumps
# (sample_issues pulls dumps/.../box3d_dump.inl at compile time).
target_include_directories(samples PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../data")

# gfx: the renderer + Draw*/adapter. shared: human, utils (Random*), the
# benchmark/determinism helpers the samples lean on.
target_link_libraries(samples PRIVATE gfx shared imgui implot sokol nfd)

target_include_directories(jozz_vehicle_validation PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")
# The M5 drive smoke steps a real Box3D world headlessly.
target_link_libraries(jozz_vehicle_validation PRIVATE box3d)
set_target_properties(jozz_vehicle_validation PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Jozz: the glTF visual proof decodes PNG data URIs through WIC on Windows.
if (WIN32)
    target_link_libraries(samples PRIVATE windowscodecs)
endif()

# MSVC auto-links winmm via a #pragma comment in main.cpp. MinGW ignores that
# pragma, so link the timeBeginPeriod/timeEndPeriod library explicitly.
if (MINGW)
    target_link_libraries(samples PRIVATE winmm)
endif()

set_target_properties(samples PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SAMPLE_FILES})
