add_library(nitrokv_dispatcher STATIC
        dispatcher.cpp
)

target_link_libraries(nitrokv_dispatcher
        PUBLIC
        nitrokv_common
)

target_include_directories(nitrokv_dispatcher
        PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

nitrokv_set_warnings(nitrokv_dispatcher)
nitrokv_enable_sanitizers(nitrokv_dispatcher)
nitrokv_enable_clang_tidy(nitrokv_dispatcher)
