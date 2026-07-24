# spike_kernel_v0 — samodzielny projekt CMake (poza buildem sampli).
# Linkuje prebudowana statyczna biblioteke box3d z build/ repo.
cmake_minimum_required(VERSION 3.20)
project(spike_kernel_v0 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# box3d w tym repo jest budowany ze statycznym CRT (/MTd) — dopasowanie.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(spike_kernel_v0 kernel_v0.cpp)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
target_include_directories(spike_kernel_v0 PRIVATE "${REPO_ROOT}/include")
target_link_libraries(spike_kernel_v0 PRIVATE
	"$<$<CONFIG:Debug>:${REPO_ROOT}/build/src/Debug/box3dd.lib>"
	"$<$<CONFIG:Release>:${REPO_ROOT}/build/src/Release/box3d.lib>"
)
