#include <jack/jack.h>
#include <cmath>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

jack_client_t* client;
jack_port_t* output_port_l;
jack_port_t* output_port_r;
float phase = 0.0f;
bool running = true;

// Ctrl+C handler
void signal_handler(int) {
    running = false;
}

int process(jack_nframes_t nframes, void* arg) {
    auto* buffer_l = static_cast<float*>(jack_port_get_buffer(output_port_l, nframes));
    auto* buffer_r = static_cast<float*>(jack_port_get_buffer(output_port_r, nframes));

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        float sample = 0.2f * sinf(phase);
        phase += 2.0f * M_PI * 440.0f / jack_get_sample_rate(client);  // 440 Hz tone
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;

        buffer_l[i] = sample;
        buffer_r[i] = sample;
    }

    return 0;
}

int main() {
    std::signal(SIGINT, signal_handler);

    client = jack_client_open("jack_dsp_app", JackNullOption, nullptr);
    if (!client) {
        std::cerr << "Failed to connect to JACK\n";
        return 1;
    }

    jack_set_process_callback(client, process, nullptr);

    output_port_l = jack_port_register(client, "out_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    output_port_r = jack_port_register(client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate(client)) {
        std::cerr << "Failed to activate JACK client\n";
        return 1;
    }

    // Auto-connect to Pisound stereo outputs
    jack_connect(client, "jack_dsp_app:out_l", "system:playback_1");
    jack_connect(client, "jack_dsp_app:out_r", "system:playback_2");

    std::cout << "Running... press Ctrl+C to quit.\n";
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    jack_client_close(client);
    std::cout << "Exited cleanly.\n";
    return 0;
}