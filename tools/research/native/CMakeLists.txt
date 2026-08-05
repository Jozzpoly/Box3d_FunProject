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
