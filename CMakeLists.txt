
cmake_minimum_required(VERSION 3.13)

project(NAS VERSION 0.5)

configure_file(NAS.h.in ${PROJECT_SOURCE_DIR}/NAS.h)

add_executable(NAS.out NAS.c)

#dodawanie opcji


add_library(NAS_handler NAS_handler.c)
set_target_properties(NAS_handler PROPERTIES PUBLIC_HEADER NAS)

add_library(NAS_functions NAS_functions.c)
set_target_properties(NAS_functions PROPERTIES PUBLIC_HEADER NAS)

add_library(NAS_argument_handler NAS_argument_handler.c)
set_target_properties(NAS_argument_handler PROPERTIES PUBLIC_HEADER NAS)

add_library(db_interface db_interface.c)
set_target_properties(db_interface PROPERTIES PUBLIC_HEADER NAS)

add_library(helper_functions helper_functions.c)
set_target_properties(helper_functions PROPERTIES PUBLIC_HEADER NAS)

add_library(PUT_handle PUT_handle.c)
set_target_properties(PUT_handle PROPERTIES PUBLIC_HEADER NAS)

add_library(sqlite STATIC sqlite3.c)
set_target_properties(sqlite PROPERTIES PUBLIC_HEADER sqlite3)
set_target_properties(sqlite PROPERTIES LINKER_LANGUAGE C)

target_include_directories(NAS.out PUBLIC ${PROJECT_BINARY_DIR})

target_link_libraries(NAS.out PUBLIC NAS_argument_handler NAS_handler NAS_functions db_interface sqlite helper_functions PUT_handle)

option(ENABLE_DEBUG "Enable DEBUG mode" OFF)

#korzystanie z opcji żeby dodać definicję typu #define DEBUG
if(ENABLE_DEBUG)
    target_compile_definitions(PUT_handle PRIVATE DEBUG)
    target_compile_definitions(db_interface PRIVATE DEBUG)
endif()