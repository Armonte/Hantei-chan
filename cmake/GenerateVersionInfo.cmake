# Generate version info with build number
# This script generates a version.h file with version and build info

# When run with -P, these variables are passed in from command line
# When included normally, they come from project()
if(NOT DEFINED VERSION_MAJOR)
    set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(VERSION_PATCH ${PROJECT_VERSION_PATCH})
endif()

# Try to get git information
find_package(Git QUIET)
if(GIT_FOUND)
    # Get the current commit hash (short)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Get the commit count (used as build number)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE BUILD_NUMBER
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    
    # Check if repo has uncommitted changes
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_DIRTY
        ERROR_QUIET
    )
    
    if(GIT_DIRTY)
        set(GIT_DIRTY_FLAG "-dirty")
    else()
        set(GIT_DIRTY_FLAG "")
    endif()
else()
    set(GIT_COMMIT_HASH "unknown")
    set(BUILD_NUMBER "0")
    set(GIT_DIRTY_FLAG "")
endif()

# Get build timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S" UTC)

# Generate the version header file
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

