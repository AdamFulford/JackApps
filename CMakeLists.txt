cmake_minimum_required(VERSION 3.5)
project(my_audio_apps)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JACK
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)

# External libraries
#add_subdirectory(external/DaisySP)

# Optional: common shared code
#add_subdirectory(common)

# Add individual app folders
add_subdirectory(passthru)
add_subdirectory(synth440)