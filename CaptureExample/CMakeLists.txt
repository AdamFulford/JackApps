add_executable(capture main.cpp)

target_include_directories(capture PRIVATE
    ${JACK_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external/DaisySP/Source
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(capture
    ${JACK_LIBRARIES}
    DaisySP
    ${SNDFILE_LIBRARIES}
)