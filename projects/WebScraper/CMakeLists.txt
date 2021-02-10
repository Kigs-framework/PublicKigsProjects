project(WebScraper)

add_library(WebScraper MODULE "")

target_link_libraries(WebScraper PUBLIC BaseFramework Core)

target_include_directories(WebScraper PUBLIC "Headers")

if(UNITY_BUILD)
	GRAB_UNITY_BUILD_SOURCES(WebScraper "")
else()
	GRAB_BASE_SOURCES(WebScraper "")
endif()

