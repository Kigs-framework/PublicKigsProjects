project(kigs)

IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/projects)
	message(STATUS "CMake enter projects")
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/projects projects)
endif()
