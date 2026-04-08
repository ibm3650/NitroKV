add_library(nitrokv_protocol STATIC
#		resp_serializer.cpp
#		command_parser.cpp
		parser.cpp
		encoding.cpp
		decoding.cpp
)

target_link_libraries(nitrokv_protocol
		PUBLIC
		nitrokv_common
)

target_include_directories(nitrokv_protocol
		PUBLIC
		${PROJECT_SOURCE_DIR}/include
)

nitrokv_set_warnings(nitrokv_protocol)
nitrokv_enable_sanitizers(nitrokv_protocol)
nitrokv_enable_clang_tidy(nitrokv_protocol)
