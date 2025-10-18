add_executable(
    vtsh
    main.c
)

add_executable(
    cpu-mat-mul
    cpu-mat-mul.c
)

target_include_directories(
    vtsh
    PUBLIC
    .
)

target_include_directories(
    cpu-mat-mul
    PUBLIC
    .
)

target_link_libraries(
    cpu-mat-mul
    PRIVATE
    libvtsh
)

target_link_libraries(
    vtsh
    PRIVATE
    libvtsh
)

add_custom_command(TARGET vtsh POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:vtsh>           
        ${CMAKE_SOURCE_DIR}/test/shell 
)
