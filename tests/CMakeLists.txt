include(GoogleTest)

add_executable(nitrokv_tests
		test.cpp
		protocol_parser.cpp
		protocol_decoding.cpp
        protocol_commands.cpp
)

target_link_libraries(nitrokv_tests
		PRIVATE
		nitrokv_common
		nitrokv_protocol
		GTest::gtest_main
)

nitrokv_set_warnings(nitrokv_tests)
nitrokv_enable_sanitizers(nitrokv_tests)
nitrokv_enable_clang_tidy(nitrokv_tests)

gtest_discover_tests(nitrokv_tests)
