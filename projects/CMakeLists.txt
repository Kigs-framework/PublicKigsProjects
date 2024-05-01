project(projects)



if(NOT "${KIGS_PLATFORM}" STREQUAL "Android")
add_subdirectory(Bounce)
add_subdirectory(PacMan)
add_subdirectory(Mandelbrot)
	add_subdirectory(YoutubeAnalyser)
	add_subdirectory(TwitterAnalyser)
endif()