add_library(
    libvtsh
    STATIC
    vtsh.c
    background.c
    execute.c
)

target_include_directories(
    libvtsh
    PUBLIC
    .
)
