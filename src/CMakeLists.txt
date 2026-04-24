add_library(nitrokv_common INTERFACE
        ../include/nitrokv/command/meta.hpp)
target_include_directories(nitrokv_common
		INTERFACE
		${PROJECT_SOURCE_DIR}/include
)
add_subdirectory(protocol)
add_subdirectory(core)
add_subdirectory(command)
add_subdirectory(dispatcher)

add_executable(nitrokv_server
		main.cpp
)

target_link_libraries(nitrokv_server
		PRIVATE
		nitrokv_common
		nitrokv_protocol
		nitrokv_command
		nitrokv_core
        nitrokv_dispatcher
		fmt::fmt
)

nitrokv_set_warnings(nitrokv_server)
nitrokv_enable_sanitizers(nitrokv_server)
nitrokv_enable_clang_tidy(nitrokv_server)
