#include <jack/jack.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>


#include "../external/DaisySP/Source/daisysp.h"
using namespace daisysp;

jack_port_t* input_ports[2];
jack_port_t* output_ports[2];
jack_client_t* client = nullptr;

std::atomic<bool> running{true};


// Constants
constexpr size_t kDelaySize = 48000; // 1 second @ 48kHz

// === GLOBAL STATIC BUFFERS ===
static float delay_buffer_L[kDelaySize];
static float delay_buffer_R[kDelaySize];

static DelayLine<float, kDelaySize> delay_L;
static DelayLine<float, kDelaySize> delay_R;

// Oscillator osc;

// === JACK AUDIO CALLBACK ===
int process(jack_nframes_t nframes, void*)
{
    float* inL  = (float*)jack_port_get_buffer(input_ports[0], nframes);
    float* inR  = (float*)jack_port_get_buffer(input_ports[1], nframes);
    float* outL = (float*)jack_port_get_buffer(output_ports[0], nframes);
    float* outR = (float*)jack_port_get_buffer(output_ports[1], nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i)
    {
        float dryL = inL[i];
        float dryR = inR[i];

        float wetL = delay_L.Read();
        float wetR = delay_R.Read();

        delay_L.Write(dryL + wetL * 0.8f); // simple feedback
        delay_R.Write(dryR + wetR * 0.8f);

        outL[i] = wetL;
        outR[i] = wetR;
    }

    return 0;
}



void jack_shutdown(void*)
{
    std::cerr << "JACK shut down unexpectedly!" << std::endl;
    exit(1);
}

void signal_handler(int) {
    running = false;
}

int main()
{

        // Init delay line with static buffer
    delay_L.Init();
    delay_R.Init();

    delay_L.SetDelay(10000.0f);
    delay_R.SetDelay(8000.f);

    // osc.Init(48000.0f);
    // osc.SetFreq(10.0f);
    // osc.SetAmp(1.0f);
    // osc.SetWaveform(Oscillator::WAVE_TRI);

    std::signal(SIGINT, signal_handler);


    const char* client_name = "jack_passthrough_stereo";
    jack_options_t options = JackNullOption;
    jack_status_t status;

    client = jack_client_open(client_name, options, &status);
    if (!client) {
        std::cerr << "Failed to open JACK client" << std::endl;
        return 1;
    }

    jack_set_process_callback(client, process, nullptr);
    jack_on_shutdown(client, jack_shutdown, nullptr);

    // Register input and output ports for L/R
    input_ports[0] = jack_port_register(client, "input_L",
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsInput, 0);
    input_ports[1] = jack_port_register(client, "input_R",
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        JackPortIsInput, 0);

    output_ports[0] = jack_port_register(client, "output_L",
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);
    output_ports[1] = jack_port_register(client, "output_R",
                                         JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);

    if (!input_ports[0] || !input_ports[1] || !output_ports[0] || !output_ports[1]) {
        std::cerr << "Failed to register JACK ports" << std::endl;
        return 1;
    }

    if (jack_activate(client)) {
        std::cerr << "Cannot activate JACK client" << std::endl;
        return 1;
    }

    jack_nframes_t sample_rate = jack_get_sample_rate(client);
    jack_nframes_t buffer_size = jack_get_buffer_size(client);

    std::cout << "Stereo passthrough running. Connect ports via QjackCtl or jack_connect." << std::endl;
    std::cout << "Sample rate: " << sample_rate << " Hz" << std::endl;
    std::cout << "Block size: " << buffer_size << " frames" << std::endl;
    std::cout << "Press Ctrl+C to quit." << std::endl;

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    jack_client_close(client);
    return 0;
}