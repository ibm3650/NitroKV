include(GoogleTest)

add_executable(nitrokv_tests
		test.cpp
)

target_link_libraries(nitrokv_tests
		PRIVATE
		nitrokv_common
		GTest::gtest_main
)

nitrokv_set_warnings(nitrokv_tests)
nitrokv_enable_sanitizers(nitrokv_tests)
nitrokv_enable_clang_tidy(nitrokv_tests)

gtest_discover_tests(nitrokv_tests)
