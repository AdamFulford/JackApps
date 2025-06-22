add_executable(passthrough main.cpp)

target_include_directories(passthrough PRIVATE
    ${JACK_INCLUDE_DIRS}
)

target_link_libraries(passthrough
    ${JACK_LIBRARIES}
)