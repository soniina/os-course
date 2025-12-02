add_executable(
    io-load-cached
    io-load-cached.c
)

target_link_libraries(
    io-load-cached 
    PRIVATE 
    vtpc
)
