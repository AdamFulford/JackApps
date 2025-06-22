add_executable(passthrough main.cpp)

target_include_directories(passthrough PRIVATE
    ${JACK_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/external/DaisySP/Source
)

target_link_libraries(passthrough
    ${JACK_LIBRARIES}
    DaisySP
)