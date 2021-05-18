project(WebScraper)

add_library(WebScraper MODULE "")

target_link_libraries(WebScraper PUBLIC BaseFramework Core)

target_include_directories(WebScraper PUBLIC "Headers")

set_property(TARGET WebScraper PROPERTY  VS_PACKAGE_REFERENCES "Microsoft.Web.WebView2_1.0.705.50;Microsoft.Windows.ImplementationLibrary_1.0.210203.1")

if(UNITY_BUILD)
	GRAB_UNITY_BUILD_SOURCES(WebScraper "")
else()
	GRAB_BASE_SOURCES(WebScraper "")
endif()

