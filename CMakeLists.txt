
cmake_minimum_required(VERSION 3.13)

project(NAS VERSION 0.2)

configure_file(NAS.h.in ${PROJECT_SOURCE_DIR}/NAS.h)

add_executable(NAS.out NAS.c)


add_library(NAS_handler NAS_handler.c)
set_target_properties(NAS_handler PROPERTIES PUBLIC_HEADER NAS)

add_library(NAS_argument_handler NAS_argument_handler.c)
set_target_properties(NAS_argument_handler PROPERTIES PUBLIC_HEADER NAS)

target_include_directories(NAS.out PUBLIC ${PROJECT_BINARY_DIR})

target_link_libraries(NAS.out PUBLIC NAS_argument_handler NAS_handler)
