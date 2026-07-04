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

set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        v1.3.0
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(nfd)

if(BOX3D_BUILD_SHADERS AND (NOT SOKOL_SHDC_EXECUTABLE OR NOT EXISTS "${SOKOL_SHDC_EXECUTABLE}"))
    FetchContent_Declare(sokol_tools_bin
        GIT_REPOSITORY https://github.com/floooh/sokol-tools-bin.git
        GIT_TAG        1a9a4e54090fec42c5d13169b638f09f25474953
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(sokol_tools_bin)
endif()

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
set_target_properties(imgui PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)

add_library(implot STATIC
    "${implot_SOURCE_DIR}/implot.cpp"
    "${implot_SOURCE_DIR}/implot_items.cpp"
    "${implot_SOURCE_DIR}/implot_demo.cpp"
    "${implot_SOURCE_DIR}/implot.h"
    "${implot_SOURCE_DIR}/implot_internal.h"
)
target_link_libraries(implot PUBLIC imgui)
target_include_directories(implot SYSTEM PUBLIC "${implot_SOURCE_DIR}")
set_target_properties(implot PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)

set(sokol_SOURCE_DIR "${CMAKE_SOURCE_DIR}/extern/sokol")
add_library(sokol INTERFACE)
target_include_directories(sokol SYSTEM INTERFACE "${sokol_SOURCE_DIR}")
if(WIN32)
    target_compile_definitions(sokol INTERFACE SOKOL_D3D11)
    target_link_libraries(sokol INTERFACE d3d11 dxgi dxguid user32 gdi32 shell32 ole32)
elseif(APPLE)
    target_compile_definitions(sokol INTERFACE SOKOL_METAL)
    target_link_libraries(sokol INTERFACE "-framework Foundation" "-framework Cocoa" "-framework QuartzCore" "-framework Metal" "-framework MetalKit")
elseif(UNIX)
    target_compile_definitions(sokol INTERFACE SOKOL_GLCORE)
    find_package(OpenGL REQUIRED)
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    if(NOT X11_Xi_FOUND OR NOT X11_Xcursor_FOUND)
        message(FATAL_ERROR "sokol_app needs Xi and Xcursor; install libxi-dev and libxcursor-dev.")
    endif()
    target_link_libraries(sokol INTERFACE OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS} m X11::X11 X11::Xi X11::Xcursor)
else()
    message(FATAL_ERROR "Unsupported platform for sokol backend selection.")
endif()

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
        set(SOKOL_SHDC_EXECUTABLE "${sokol_tools_bin_SOURCE_DIR}/bin/${_shdc_subdir}/${_shdc_exe}" CACHE FILEPATH "Path to sokol-shdc binary" FORCE)
    endif()
    unset(_shdc_subdir)
    unset(_shdc_exe)
endif()

add_subdirectory(shaders)
add_subdirectory(gfx)

set(SAMPLE_FILES
    main.cpp
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
    jsmn.h
    tiny_obj_loader.h
    earcut.h
    jozz_vehicle_asset_contract.cpp
    jozz_vehicle_asset_contract.h
    jozz_vehicle_asset_dimensions.cpp
    jozz_vehicle_asset_dimensions.h
    jozz_vehicle_corner_rig.cpp
    jozz_vehicle_corner_rig.h
    jozz_vehicle_asset_metadata.cpp
    jozz_vehicle_asset_metadata.h
    jozz_vehicle_debug_preview.cpp
    jozz_vehicle_debug_preview.h
    jozz_vehicle_image_decode.cpp
    jozz_vehicle_image_decode.h
    jozz_vehicle_primitive_corner_lab.cpp
    jozz_vehicle_primitive_corner_lab.h
    jozz_vehicle_visual_asset.cpp
    jozz_vehicle_visual_asset.h
    jozz_vehicle_visual_mesh.cpp
    jozz_vehicle_visual_mesh.h
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

add_executable(jozz_vehicle_validation
    jozz_vehicle_validation.cpp
    jozz_vehicle_asset_contract.cpp
    jozz_vehicle_asset_contract.h
    jozz_vehicle_asset_dimensions.cpp
    jozz_vehicle_asset_dimensions.h
    jozz_vehicle_asset_metadata.cpp
    jozz_vehicle_asset_metadata.h
    jsmn.h
)

if(APPLE)
    set_source_files_properties(host/sokol_app_impl.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(host/sokol_app_impl.c PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

target_include_directories(samples PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../data")
target_link_libraries(samples PRIVATE gfx shared imgui implot sokol nfd)

target_include_directories(jozz_vehicle_validation PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set_target_properties(jozz_vehicle_validation PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

if (WIN32)
    target_link_libraries(samples PRIVATE windowscodecs)
endif()

if (MINGW)
    target_link_libraries(samples PRIVATE winmm)
endif()

set_target_properties(samples PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SAMPLE_FILES})
