#include <jack/jack.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "../external/DaisySP/Source/daisysp.h"
using namespace daisysp;

jack_port_t* input_ports[2];
jack_port_t* output_ports[2];
jack_client_t* client = nullptr;

Oscillator osc;

// Real-time audio callback
int process(jack_nframes_t nframes, void* arg)
{
    float* inL = (float*)jack_port_get_buffer(input_ports[0], nframes);
    float* inR = (float*)jack_port_get_buffer(input_ports[1], nframes);
    float* outL = (float*)jack_port_get_buffer(output_ports[0], nframes);   
    float* outR = (float*)jack_port_get_buffer(output_ports[1], nframes);

    for (jack_nframes_t i = 0; i < nframes; i++) {

        float gain{( osc.Process() + 1.0f) * 0.5f} ;

        outL[i] = inL[i] * gain;  // passthrough left
        outR[i] = inR[i] * gain;  // passthrough right
    }

    return 0;
}

void jack_shutdown(void*)
{
    std::cerr << "JACK shut down unexpectedly!" << std::endl;
    exit(1);
}

int main()
{

    osc.Init(48000.0f);
    osc.SetFreq(10.0f);
    osc.SetAmp(1.0f);
    osc.SetWaveform(Oscillator::WAVE_TRI);


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

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    jack_client_close(client);
    return 0;
}