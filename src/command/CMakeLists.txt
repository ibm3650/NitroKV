add_library(nitrokv_command STATIC
        parser.cpp
)

target_link_libraries(nitrokv_command
        PUBLIC
        nitrokv_common
)

target_include_directories(nitrokv_command
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

nitrokv_set_warnings(nitrokv_command)
nitrokv_enable_sanitizers(nitrokv_command)
nitrokv_enable_clang_tidy(nitrokv_command)
