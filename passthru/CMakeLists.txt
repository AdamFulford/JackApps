add_executable(passthrough main.cpp)

target_include_directories(passthrough PRIVATE
    ${JACK_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external/DaisySP/Source
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(passthrough
    ${JACK_LIBRARIES}
    DaisySP
    ${SNDFILE_LIBRARIES}
)