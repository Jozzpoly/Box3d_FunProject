cmake_minimum_required(VERSION 3.22)

project(jozz_vehicle_factory_receipt LANGUAGES C CXX)

set(JV_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
set(BOX3D_BUILD_SHADERS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory("${JV_REPOSITORY_ROOT}" box3d-source EXCLUDE_FROM_ALL)

set(JV_SAMPLES_DIR "${JV_REPOSITORY_ROOT}/samples")
set(JV_RECEIPT_CORE_FILES
    "${JV_SAMPLES_DIR}/jozz_vehicle_asset_contract.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_asset_dimensions.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_asset_metadata.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_asset_paths.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_json.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_m5_vehicle.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_m6_config_io.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_m6_geometry.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_m6_suspension_rig.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_m7_suspension_import.cpp"
    "${JV_SAMPLES_DIR}/jozz_vehicle_steering_suspension_contract.cpp"
)

add_executable(jozz_vehicle_factory_receipt
    main.cpp
    ${JV_RECEIPT_CORE_FILES}
)

target_include_directories(jozz_vehicle_factory_receipt PRIVATE
    "${JV_SAMPLES_DIR}"
    "${JV_REPOSITORY_ROOT}/include"
)
target_link_libraries(jozz_vehicle_factory_receipt PRIVATE box3d)
target_compile_options(jozz_vehicle_factory_receipt PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
set_target_properties(jozz_vehicle_factory_receipt PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
