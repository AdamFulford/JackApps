#include <jack/jack.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>


#include "../external/DaisySP/Source/daisysp.h"
using namespace daisysp;

constexpr int NUM_DELAYS = 128;          // Number of stereo delay lines
constexpr size_t MAX_DELAY_MS = 5000.0f; // Max delay time in milliseconds
constexpr size_t MIN_DELAY_MS = 100.0f;
constexpr float FEEDBACK = 0.4f;       // Shared feedback amount

// JACK audio buffers
jack_port_t *input_l, *input_r;
jack_port_t *output_l, *output_r;
jack_client_t *client;

std::atomic<bool> running{true};

float sampleRate;

// A pair of delays for stereo
struct StereoDelay {
    DelayLine<float, MAX_DELAY_MS * 48 + 1> left;
    DelayLine<float, MAX_DELAY_MS * 48 + 1> right;
    float delayTimeMs;
};

std::vector<StereoDelay> delays;

// Simple helper to randomize float in range
float randomFloat(float min, float max) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

// JACK audio callback
int audioCallback(jack_nframes_t nframes, void *arg) {
    auto *inL = (float *)jack_port_get_buffer(input_l, nframes);
    auto *inR = (float *)jack_port_get_buffer(input_r, nframes);
    auto *outL = (float *)jack_port_get_buffer(output_l, nframes);
    auto *outR = (float *)jack_port_get_buffer(output_r, nframes);

    for (jack_nframes_t i = 0; i < nframes; ++i) {
        float dryL = inL[i];
        float dryR = inR[i];
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (auto &d : delays) {
            float delayedL = d.left.Read();
            float delayedR = d.right.Read();

            sumL += delayedL * 0.5f;
            sumR += delayedR * 0.5f;

            // Write current input + feedback
            d.left.Write(dryL + delayedL * FEEDBACK);
            d.right.Write(dryR + delayedR * FEEDBACK);
        }

        outL[i] = dryL + sumL;
        outR[i] = dryR + sumR;
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


int main() {

    std::signal(SIGINT, signal_handler);
    // Open JACK client
    const char *client_name = "jack_passthrough_stereo";
    jack_options_t options = JackNullOption;
    jack_status_t status;
    client = jack_client_open(client_name, options, &status);

    if (!client) {
        std::cerr << "Failed to connect to JACK." << std::endl;
        return 1;
    }

    // Get sample rate
    sampleRate = jack_get_sample_rate(client);

    // Create and init stereo delays
    delays.resize(NUM_DELAYS);
    for (auto &d : delays) {
        d.delayTimeMs = randomFloat(MIN_DELAY_MS, MAX_DELAY_MS);
        float delaySamples = d.delayTimeMs * (sampleRate / 1000.0f);
        d.left.Init();
        d.right.Init();
        d.left.SetDelay(delaySamples);
        d.right.SetDelay(delaySamples);
    }

    // Register JACK ports
    input_l = jack_port_register(client, "input_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    input_r = jack_port_register(client, "input_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    output_l = jack_port_register(client, "output_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    output_r = jack_port_register(client, "output_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    // Set process callback and activate
    jack_set_process_callback(client, audioCallback, 0);

    if (jack_activate(client)) {
        std::cerr << "Cannot activate JACK client." << std::endl;
        return 1;
    }

    std::cout << "Multi-delay JACK client running with " << NUM_DELAYS << " stereo delay lines.\n";

    // Keep running
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
        

    // Cleanup
    jack_client_close(client);
    return 0;
}