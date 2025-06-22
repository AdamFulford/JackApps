add_executable(synth440 main.cpp)

target_include_directories(synth440 PRIVATE
    ${JACK_INCLUDE_DIRS}
)

target_link_libraries(synth440
    ${JACK_LIBRARIES}
)