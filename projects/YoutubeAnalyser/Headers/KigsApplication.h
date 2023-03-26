#pragma once

//! include the real application class file
#include "YoutubeAnalyser.h"

//! yes I want Timer and FileManager to be auto initialized
#define INIT_DEFAULT_MODULES

#ifdef INIT_DEFAULT_MODULES
//! And then I want the base data path to be :
#define BASE_DATA_PATH "assets"
#endif //INIT_DEFAULT_MODULES

//! then define it as the application class 
#define KIGS_APPLICATION_CLASS YoutubeAnalyser

#define APP_KIGS_PACKAGE_NAME "YoutubeAnalyser_assets.kpkg"
