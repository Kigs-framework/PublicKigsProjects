project(projects)
add_subdirectory(YoutubeAnalyser)
add_subdirectory(Mandelbrot)
add_subdirectory(VoxelLandscape)
add_subdirectory(TwitterAnalyser)
if(${KIGS_PLATFORM} STREQUAL "Windows")
	add_subdirectory(WebScraper)
endif()

