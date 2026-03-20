function(nitrokv_enable_clang_tidy target)
	if(NOT NITROKV_ENABLE_CLANG_TIDY)
		return()
	endif()

	find_program(CLANG_TIDY_BIN NAMES clang-tidy)
	if(CLANG_TIDY_BIN)
		set_target_properties(${target} PROPERTIES
				CXX_CLANG_TIDY "${CLANG_TIDY_BIN}"
		)
	endif()
endfunction()
