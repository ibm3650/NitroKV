add_library(nitrokv_core STATIC
        core.cpp
)

target_link_libraries(nitrokv_core
        PUBLIC
        nitrokv_common
)

target_include_directories(nitrokv_core
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

nitrokv_set_warnings(nitrokv_core)
nitrokv_enable_sanitizers(nitrokv_core)
nitrokv_enable_clang_tidy(nitrokv_core)
