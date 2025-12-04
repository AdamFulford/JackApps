#include <jack/jack.h>
#include <jack/midiport.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

#include "../external/DaisySP/Source/daisysp.h"
using namespace daisysp;

constexpr int NUM_DELAYS = 4;          // Number of stereo delay lines
constexpr size_t MAX_DELAY_MS = 1000.0f; // Max delay time in milliseconds
constexpr size_t MIN_DELAY_MS = 50.0f;


// constexpr float FEEDBACK = 0.4f;       // Shared feedback amount



// A pair of delays for stereo
struct StereoDelay {
    DelayLine<float, MAX_DELAY_MS * 48 + 1> left;
    DelayLine<float, MAX_DELAY_MS * 48 + 1> right;
    float delayTimeMs;
};

struct MidiCC {
    uint8_t cc;
    uint8_t value;
};

//simple SPSC ring buffer
template <typename T, size_t N>
class RingBuf {
public:
    bool push(const T& v) {
        size_t next = (head + 1) % N;
        if (next == tail) return false; // full
        buffer[head] = v;
        head = next;
        return true;
    }
    bool pop(T& out) {
        if (tail == head) return false; // empty
        out = buffer[tail];
        tail = (tail + 1) % N;
        return true;
    }
private:
    T buffer[N];
    size_t head = 0, tail = 0;
};

//globals
// JACK audio buffers
jack_port_t *input_l, *input_r;
jack_port_t *output_l, *output_r;
jack_port_t *midi_in;
jack_client_t *client;

std::atomic<bool> running{true};
std::atomic<float> FEEDBACK;
float sampleRate;

RingBuf<MidiCC, 256> midiQueue;


std::vector<StereoDelay> delays;
std::vector<float> delayTimes;
Svf filters[2];

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

    void* midiBuf = jack_port_get_buffer(midi_in, nframes);
    jack_nframes_t eventCount = jack_midi_get_event_count(midiBuf);
    jack_midi_event_t event;

    for (jack_nframes_t i = 0; i < eventCount; ++i) {
        if (jack_midi_event_get(&event, midiBuf, i) == 0) {
            const uint8_t* data = event.buffer;

            if ((data[0] & 0xF0) == 0xB0) { // CC message on any channel
                MidiCC ccMsg{ data[1], data[2] };
                midiQueue.push(ccMsg);     // very fast lock-free enqueue
            }
        }
    }

    for (jack_nframes_t i = 0; i < nframes; ++i) {

        static float delayTimeFiltered[NUM_DELAYS]{};

        for(size_t d=0; d<NUM_DELAYS; d++)
        {
            //fitler then set delay time 
            float delaySamples = delayTimes[d] * (sampleRate / 1000.0f);
            fonepole(delayTimeFiltered[d],delaySamples,0.001f);
            delays[d].left.SetDelay(delayTimeFiltered[d]);
            delays[d].right.SetDelay(delayTimeFiltered[d]);
        }

        float dryL = inL[i];
        float dryR = inR[i];
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (auto &d : delays) {
            float delayedL = d.left.Read();
            float delayedR = d.right.Read();

            sumL += delayedL * 0.1f;
            sumR += delayedR * 0.1f;

            // Write current input + feedback
            d.left.Write(dryL + delayedL * FEEDBACK);
            d.right.Write(dryR + delayedR * FEEDBACK);
        }

        // filters[0].Process(sumL);
        // filters[1].Process(sumR);

        // float filtered_L{filters[0].Low()};
        // float filtered_R{filters[1].Low()};

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

    std::atomic<bool> midiRunning{true};
    std::thread midiThread([&]() {
    while (midiRunning) {
        MidiCC msg;
        while (midiQueue.pop(msg)) {
            // Handle CC from any MIDI controller
            std::cout << "CC " << int(msg.cc)
                      << " = " << int(msg.value) << std::endl;

            if (msg.cc == 1)
            {
                
                FEEDBACK = (1.2f) * (msg.value / 127.0f);
                
                // float TimeFactor = 0.1f + (1.0f - 0.1f) * (msg.value / 127.0f);
                // for(size_t d=0; d<NUM_DELAYS; d++)
                // {
                //     //set delay time 
                //     float delaySamples = delays[d].delayTimeMs * (sampleRate / 1000.0f) * TimeFactor;
                //     delays[d].left.SetDelay(delaySamples);
                //     delays[d].right.SetDelay(delaySamples);
                // }
            }

            if (msg.cc == 71 || msg.cc == 72  || msg.cc == 73 || msg.cc == 74)
            {
                delayTimes[msg.cc - 71] = 100.0f + (1900.0f) * (msg.value / 127.0f);

                // float TimeFactor = 0.1f + (1.0f - 0.1f) * (msg.value / 127.0f);
                // for(size_t d=0; d<NUM_DELAYS; d++)
                // {
                //     //set delay time 
                //     float delaySamples = delays[d].delayTimeMs * (sampleRate / 1000.0f) * TimeFactor;
                //     delays[d].left.SetDelay(delaySamples);
                //     delays[d].right.SetDelay(delaySamples);
                // }
            }
            
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
});

    std::signal(SIGINT, signal_handler);
    // Open JACK client
    const char *client_name = "jack_multi_delay";
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
    delayTimes.resize(NUM_DELAYS);
    // for (auto &d : delays) {
    for(size_t d=0; d<NUM_DELAYS; d++)
    {
        delays[d].left.Init();
        delays[d].right.Init();

        delayTimes[d] = 100;
        delays[d].delayTimeMs = delayTimes[d];
        float delaySamples = delays[d].delayTimeMs * (sampleRate / 1000.0f);
        delays[d].left.SetDelay(delaySamples);
        delays[d].right.SetDelay(delaySamples);
    }

    //init filters

    filters[0].Init(sampleRate);
    filters[1].Init(sampleRate);

    filters[0].SetFreq(800.0f);
    filters[1].SetFreq(800.0f);

    // Register JACK ports
    input_l = jack_port_register(client, "input_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    input_r = jack_port_register(client, "input_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    output_l = jack_port_register(client, "output_L", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    output_r = jack_port_register(client, "output_R", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    midi_in = jack_port_register(client, "midi_in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

    // Set process callback and activate
    jack_set_process_callback(client, audioCallback, 0);

    if (jack_activate(client)) {
        std::cerr << "Cannot activate JACK client." << std::endl;
        return 1;
    }

    //connect ports

    jack_connect(client, "system:capture_1", jack_port_name(input_l));
    jack_connect(client, "system:capture_2", jack_port_name(input_r));

    jack_connect(client, jack_port_name(output_l), "system:playback_5");
    jack_connect(client, jack_port_name(output_r), "system:playback_6");

    jack_connect(client, "system:midi_capture_3", jack_port_name(midi_in));

    std::cout << "Multi-delay JACK client running with " << NUM_DELAYS << " stereo delay lines.\n";

    // Keep running
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
        
    // Cleanup
    // ---------- SHUTDOWN SEQUENCE ----------
    midiRunning = false;  // tell MIDI thread to exit
    midiThread.join();    // wait for it to finish (important!)
    jack_client_close(client);
    return 0;
}