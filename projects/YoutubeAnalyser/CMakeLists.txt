project(YoutubeAnalyser)

if(${KIGS_PLATFORM} STREQUAL "Windows" )
	add_executable(YoutubeAnalyser "")


	set(CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG "${CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG} /SAFESEH:NO /DEBUG:FULL")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE} /SUBSYSTEM:WINDOWS /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS} /SUBSYSTEM:WINDOWS /DEBUG:FULL /SAFESEH:NO")


	set(YoutubeAnalyser_GUID_CMAKE "34FDA678-67F1-4BF3-9996-226730D962DD" CACHE INTERNAL "YoutubeAnalyser GUID")

	target_link_libraries(YoutubeAnalyser PRIVATE DataDrivenApplication HTTPRequest)

	target_include_directories(YoutubeAnalyser PRIVATE "Headers")


	file(GLOB_RECURSE all_sources "Sources/*.cpp")
	file(GLOB_RECURSE all_headers "Headers/*.h")

	target_sources(YoutubeAnalyser 
		PRIVATE 
			${all_sources}
			"${KIGS_FRAMEWORK}/StandAloneUtilityClasses/Sources/Histogram.cpp"
			${all_headers}
			)
	set_property(TARGET YoutubeAnalyser PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Data")

endif()
