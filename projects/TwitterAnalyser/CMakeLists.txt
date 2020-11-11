project(TwitterAnalyser)

if(${KIGS_PLATFORM} STREQUAL "Windows" )
	add_executable(TwitterAnalyser "")
	

	set(CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG "${CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG} /SAFESEH:NO /DEBUG:FULL")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE} /SUBSYSTEM:WINDOWS /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS} /SUBSYSTEM:WINDOWS /DEBUG:FULL /SAFESEH:NO")


	set(TwitterAnalyser_GUID_CMAKE "51E06C82-2CE7-4BD4-8D7F-5F1D06FD503A" CACHE INTERNAL "TwitterAnalyser GUID")

	target_link_libraries(TwitterAnalyser PRIVATE DataDrivenApplication HTTPRequest)

	target_include_directories(TwitterAnalyser PRIVATE "Headers")


	file(GLOB_RECURSE all_sources "Sources/*.cpp")
	file(GLOB_RECURSE all_headers "Headers/*.h")

	target_sources(TwitterAnalyser 
		PRIVATE 
			${all_sources}
			${all_headers}
			)
	set_property(TARGET TwitterAnalyser PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Data")

endif()
