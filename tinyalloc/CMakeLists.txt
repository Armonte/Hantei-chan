add_library(tinyalloc STATIC
	tinyalloc.c
)

target_include_directories(tinyalloc PUBLIC ".")