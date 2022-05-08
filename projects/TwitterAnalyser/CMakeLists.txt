project(TwitterAnalyser)

add_executable(TwitterAnalyser "")
	
if(${KIGS_PLATFORM} STREQUAL "Windows" )
	set(CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG "${CMAKE_SHARED_LINKER_FLAGS_STATICDEBUG} /SAFESEH:NO /DEBUG:FULL")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASE} /SUBSYSTEM:WINDOWS /SAFESEH:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS "${CMAKE_SHARED_LINKER_FLAGS_STATICRELEASETOOLS} /SUBSYSTEM:WINDOWS /DEBUG:FULL /SAFESEH:NO")
endif()

set(TwitterAnalyser_GUID_CMAKE "51E06C82-2CE7-4BD4-8D7F-5F1D06FD503A" CACHE INTERNAL "TwitterAnalyser GUID")

target_link_libraries(TwitterAnalyser PRIVATE DataDrivenApplication CoreFSM HTTPRequest)

target_include_directories(TwitterAnalyser PRIVATE "Headers")


file(GLOB_RECURSE all_sources "Sources/*.cpp")
file(GLOB_RECURSE all_headers "Headers/*.h")

target_sources(TwitterAnalyser 
	PRIVATE 
		${all_sources}
		"${KIGS_FRAMEWORK}/StandAloneUtilityClasses/Sources/Histogram.cpp"
		${all_headers}
		)
set_property(TARGET TwitterAnalyser PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Data")
	
if(${KIGS_PLATFORM} STREQUAL "Windows" )	
add_custom_command(TARGET TwitterAnalyser  PRE_BUILD 
										 COMMAND AssetManager.exe -i ${CMAKE_CURRENT_SOURCE_DIR}/Ressources -t ${CMAKE_CURRENT_SOURCE_DIR}/TmpRessources -o ${CMAKE_CURRENT_SOURCE_DIR}/Android/assets
										 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../kigs/tools/win32_executable/)
elseif(${KIGS_PLATFORM} STREQUAL "Javascript")
	set(CMAKE_EXECUTABLE_SUFFIX ".js")
	set_target_properties(TwitterAnalyser PROPERTIES LINK_FLAGS "--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Android/assets@/ --js-library ${KIGS_PLATFORM_ROOT}/Platform/2DLayers/2DLayers_JavaScript.js --js-library ${KIGS_PLATFORM_ROOT}/Platform/GUI/GUI_JavaScript.js --js-library ${KIGS_PLATFORM_ROOT}/Platform/Input/Input_JavaScript.js --js-library ${KIGS_PLATFORM_ROOT}/Platform/Renderer/Renderer_JavaScript.js --js-library ${KIGS_PLATFORM_ROOT}/Platform/Sound/Audio.js --js-library ${KIGS_PLATFORM_ROOT}/Platform/HTTPRequest/HTTPRequest_JavaScript.js -s TOTAL_MEMORY=67108864 -s EXPORTED_FUNCTIONS='[_main]' --use-preload-plugins -s WASM=1 -s BINARYEN_METHOD='native-wasm' -s ALLOW_MEMORY_GROWTH=1" )
endif()
