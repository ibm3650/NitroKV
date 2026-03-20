function(nitrokv_set_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE
				/W4
				/permissive-
		)
	else()
		target_compile_options(${target} PRIVATE
				-Wall
				-Wextra
				-Wpedantic
				-Wshadow
				-Wconversion
				-Wsign-conversion
				-Wnull-dereference
				-Wdouble-promotion
				-Wformat=2
		)

		if(NITROKV_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE -Werror)
		endif()
	endif()
endfunction()
