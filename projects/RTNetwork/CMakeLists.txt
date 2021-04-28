project(RTNetwork)

if(${KIGS_PLATFORM} STREQUAL "Windows" )
	add_executable(RTNetwork "")
	

	set(CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG "${CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG} /SAFESEH:NO /DEBUG:FULL")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE} /SUBSYSTEM:WINDOWS /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS} /SUBSYSTEM:WINDOWS /DEBUG:FULL /SAFESEH:NO")


	set(RTNetwork_GUID_CMAKE "2D8901E0-7421-44BB-81F8-4064CDBF5887" CACHE INTERNAL "RTNetwork GUID")

	target_link_libraries(RTNetwork PRIVATE DataDrivenApplication HTTPRequest)

	target_include_directories(RTNetwork PRIVATE "Headers")


	file(GLOB_RECURSE all_sources "Sources/*.cpp")
	file(GLOB_RECURSE all_headers "Headers/*.h")

	target_sources(RTNetwork 
		PRIVATE 
			${all_sources}
			"${KIGS_FRAMEWORK}/StandAloneUtilityClasses/Sources/Histogram.cpp"
			${all_headers}
			)
	set_property(TARGET RTNetwork PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Data")

endif()
