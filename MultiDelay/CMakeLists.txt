add_executable(multiDelay main.cpp)

target_include_directories(multiDelay PRIVATE
    ${JACK_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external/DaisySP/Source
    ${SNDFILE_INCLUDE_DIRS}
)

target_link_libraries(multiDelay
    ${JACK_LIBRARIES}
    DaisySP
    ${SNDFILE_LIBRARIES}
)