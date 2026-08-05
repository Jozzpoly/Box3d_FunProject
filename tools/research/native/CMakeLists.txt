add_executable(jv_wheel_soft_q2 wheel_soft_q2.c)

set_target_properties(jv_wheel_soft_q2 PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

target_link_libraries(jv_wheel_soft_q2 PRIVATE box3d)

if(MSVC)
    target_compile_options(jv_wheel_soft_q2 PRIVATE /W4 /wd4820 /wd5045 /wd4710 /wd4711)
else()
    target_compile_options(jv_wheel_soft_q2 PRIVATE -Wall -Wextra -pedantic)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "" FILES wheel_soft_q2.c)


# Native M6 steering-release evidence probe. This deliberately lives under
# research/native rather than the GUI samples target, so it can run in a
# headless build while compiling the same authoritative M6 core translation
# units as the product sample.
find_package(Git QUIET)
set(JV_PROBE_SOURCE_COMMIT "unknown")
set(JV_PROBE_SOURCE_TREE "unknown")
set(JV_PROBE_SOURCE_DIRTY "unknown")
if(GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE JV_PROBE_SOURCE_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" rev-parse "HEAD^{tree}"
        OUTPUT_VARIABLE JV_PROBE_SOURCE_TREE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}" status --porcelain --untracked-files=all
        OUTPUT_VARIABLE JV_PROBE_DIRTY_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(JV_PROBE_DIRTY_OUTPUT STREQUAL "")
        set(JV_PROBE_SOURCE_DIRTY "false")
    else()
        set(JV_PROBE_SOURCE_DIRTY "true")
    endif()
endif()

add_executable(jv_steering_release_probe
    steering_release_probe.cpp
    "${PROJECT_SOURCE_DIR}/samples/jozz_vehicle_m5_vehicle.cpp"
    "${PROJECT_SOURCE_DIR}/samples/jozz_vehicle_m6_geometry.cpp"
    "${PROJECT_SOURCE_DIR}/samples/jozz_vehicle_m6_suspension_rig.cpp"
)

set_target_properties(jv_steering_release_probe PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_include_directories(jv_steering_release_probe PRIVATE
    "${PROJECT_SOURCE_DIR}/samples"
    "${PROJECT_SOURCE_DIR}/include"
)

target_compile_definitions(jv_steering_release_probe PRIVATE
    JV_PROBE_SOURCE_COMMIT="${JV_PROBE_SOURCE_COMMIT}"
    JV_PROBE_SOURCE_TREE="${JV_PROBE_SOURCE_TREE}"
    JV_PROBE_SOURCE_DIRTY="${JV_PROBE_SOURCE_DIRTY}"
    JV_PROBE_BUILD_TYPE="$<CONFIG>"
)

target_link_libraries(jv_steering_release_probe PRIVATE box3d)

if(MSVC)
    target_compile_options(jv_steering_release_probe PRIVATE /W4 /utf-8 /wd4820 /wd5045 /wd4710 /wd4711)
else()
    target_compile_options(jv_steering_release_probe PRIVATE -Wall -Wextra -pedantic)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "" FILES steering_release_probe.cpp)
