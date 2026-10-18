#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

#include "RtMidi.h"

extern "C" {
#include "bog.h"
#include "builtins.h"
#include "scheduler.h"
}

namespace {

struct MidiEvent {
    double time;
    std::array<unsigned char, 3> message;
};

struct MidiContext {
    std::unique_ptr<RtMidiOut> midi;
    std::vector<MidiEvent> queue;
    std::mutex mutex;
    std::condition_variable cv;
    std::thread worker;
    bool worker_running = false;
    bool quit_requested = false;
    std::chrono::steady_clock::time_point start_time;
    double drum_duration = 0.05;
    double synth_duration = 0.3;
    int drum_channel = 9;
    int synth_channel = 0;
};

std::atomic<bool> g_running(true);

void handle_signal(int signal)
{
    if (signal == SIGINT) {
        g_running.store(false);
    }
}

double midi_now(const MidiContext* ctx)
{
    using namespace std::chrono;
    if (!ctx)
        return 0.0;
    const auto now = steady_clock::now();
    duration<double> elapsed = now - ctx->start_time;
    return elapsed.count();
}

void midi_worker(MidiContext* ctx)
{
    if (!ctx)
        return;
    std::unique_lock<std::mutex> lock(ctx->mutex);
    while (true) {
        ctx->cv.wait(lock, [&] {
            return ctx->quit_requested || !ctx->queue.empty();
        });
        if (ctx->quit_requested && ctx->queue.empty())
            break;
        if (ctx->queue.empty())
            continue;
        double now = midi_now(ctx);
        double wait_time = ctx->queue.front().time - now;
        if (wait_time > 0.0) {
            ctx->cv.wait_for(
                lock, std::chrono::duration<double>(wait_time));
            continue;
        }
        MidiEvent ev = ctx->queue.front();
        ctx->queue.erase(ctx->queue.begin());
        lock.unlock();
        if (ctx->midi) {
            ctx->midi->sendMessage(ev.message.data(),
                                   ev.message.size());
        }
        lock.lock();
    }
}

void start_worker(MidiContext* ctx)
{
    if (!ctx || ctx->worker_running)
        return;
    ctx->quit_requested = false;
    ctx->worker_running = true;
    ctx->worker = std::thread(midi_worker, ctx);
}

void stop_worker(MidiContext* ctx)
{
    if (!ctx || !ctx->worker_running)
        return;
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->quit_requested = true;
    }
    ctx->cv.notify_all();
    if (ctx->worker.joinable())
        ctx->worker.join();
    ctx->worker_running = false;
    ctx->queue.clear();
}

unsigned char clamp_u8(int value, int min_value, int max_value)
{
    if (value < min_value)
        value = min_value;
    if (value > max_value)
        value = max_value;
    return static_cast<unsigned char>(value);
}

void enqueue_event(MidiContext* ctx, double time_value,
                   unsigned char status, unsigned char data1,
                   unsigned char data2)
{
    if (!ctx)
        return;
    MidiEvent ev;
    ev.time = time_value;
    ev.message = { status, data1, data2 };
    {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        auto it = std::upper_bound(
            ctx->queue.begin(), ctx->queue.end(), ev.time,
            [](double t, const MidiEvent& rhs) {
                return t < rhs.time;
            });
        ctx->queue.insert(it, ev);
    }
    ctx->cv.notify_one();
}

void schedule_note(MidiContext* ctx, int channel, int note,
                   double velocity, double when, double duration)
{
    if (!ctx || !ctx->midi)
        return;
    if (duration <= 0.0)
        duration = 0.05;
    const int midi_note = clamp_u8(note, 0, 127);
    int vel = static_cast<int>(std::lround(velocity * 127.0));
    vel = std::max(0, std::min(127, vel));
    unsigned char status_on = static_cast<unsigned char>(
        0x90 | (channel & 0x0F));
    enqueue_event(ctx, when, status_on,
                  static_cast<unsigned char>(midi_note),
                  static_cast<unsigned char>(vel));
    unsigned char status_off = static_cast<unsigned char>(
        0x80 | (channel & 0x0F));
    enqueue_event(ctx, when + duration, status_off,
                  static_cast<unsigned char>(midi_note), 0);
}

double midi_time(void* userdata)
{
    MidiContext* ctx = static_cast<MidiContext*>(userdata);
    return midi_now(ctx);
}

void midi_init(void* userdata)
{
    MidiContext* ctx = static_cast<MidiContext*>(userdata);
    if (!ctx)
        return;
    ctx->start_time = std::chrono::steady_clock::now();
    start_worker(ctx);
}

// GM drum notes for the drum voices; any other voice plays its pitch on the
// synth channel
int drum_note(const char* voice)
{
    static const struct {
        const char* name;
        int note;
    } drums[] = {
        { "kick", 36 }, { "snare", 38 }, { "hat", 42 },
        { "clap", 39 }, { "noise", 46 },
    };
    for (const auto& drum : drums) {
        if (std::strcmp(voice, drum.name) == 0)
            return drum.note;
    }
    return -1;
}

void midi_note(void* userdata, const char* voice, double time_value,
               double midi_note, double velocity)
{
    MidiContext* ctx = static_cast<MidiContext*>(userdata);
    const int drum = drum_note(voice);
    if (drum >= 0) {
        schedule_note(ctx, ctx->drum_channel, drum, velocity, time_value,
                      ctx->drum_duration);
        return;
    }
    schedule_note(ctx, ctx->synth_channel,
                  static_cast<int>(std::lround(midi_note)), velocity,
                  time_value, ctx->synth_duration);
}

bool read_file(const char* path, std::string& out)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    out = ss.str();
    return true;
}

void print_usage(const char* exe)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s --list                 # list MIDI output ports\n"
            "  %s <program> [bpm] [port] # run program at bpm on port\n",
            exe, exe);
}

void list_ports()
{
    try {
        RtMidiOut midi;
        unsigned int count = midi.getPortCount();
        if (count == 0) {
            printf("No MIDI output ports detected. A virtual port will be "
                   "created when running.\n");
            return;
        }
        printf("Available MIDI output ports:\n");
        for (unsigned int i = 0; i < count; ++i) {
            std::string name = midi.getPortName(i);
            printf("  [%u] %s\n", i, name.c_str());
        }
    } catch (const RtMidiError& err) {
        fprintf(stderr, "Failed to enumerate MIDI ports: %s\n",
                err.getMessage().c_str());
    }
}

bool open_midi_port(MidiContext* ctx, unsigned int requested_port)
{
    if (!ctx)
        return false;
    ctx->midi = std::make_unique<RtMidiOut>();
    unsigned int port_count = ctx->midi->getPortCount();
    if (port_count == 0) {
        fprintf(stderr,
                "No MIDI outputs found. Opening virtual port 'bog'.\n");
        ctx->midi->openVirtualPort("bog");
        return true;
    }
    if (requested_port >= port_count) {
        fprintf(stderr,
                "Requested MIDI port %u unavailable. Using port 0.\n",
                requested_port);
        requested_port = 0;
    }
    std::string name = ctx->midi->getPortName(requested_port);
    fprintf(stderr, "Opening MIDI port %u: %s\n", requested_port,
            name.c_str());
    ctx->midi->openPort(requested_port, "bog");
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "--list") == 0) {
        list_ports();
        return 0;
    }
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* program_path = argv[1];
    double bpm = 120.0;
    if (argc >= 3) {
        bpm = strtod(argv[2], NULL);
        if (!(bpm > 0.0))
            bpm = 120.0;
    }
    unsigned int port_index = 0;
    if (argc >= 4)
        port_index = (unsigned int)strtoul(argv[3], NULL, 10);

    std::string program_src;
    if (!read_file(program_path, program_src)) {
        fprintf(stderr, "Failed to read %s\n", program_path);
        return 1;
    }

    MidiContext midi_ctx;
    try {
        if (!open_midi_port(&midi_ctx, port_index))
            return 1;
    } catch (const RtMidiError& err) {
        fprintf(stderr, "MIDI error: %s\n", err.getMessage().c_str());
        return 1;
    }

    BogArena* arena = bog_arena_create();
    if (!arena) {
        fprintf(stderr, "Failed to allocate arena\n");
        return 1;
    }
    char* parse_error = NULL;
    BogProgram* program = bog_parse_program(program_src.c_str(), arena,
                                                    &parse_error);
    if (!program) {
        fprintf(stderr, "Parse error: %s\n",
                parse_error ? parse_error : "unknown");
        free(parse_error);
        bog_arena_destroy(arena);
        return 1;
    }
    free(parse_error);

    BogBuiltins* builtins = bog_create_builtins(arena);
    BogStateManager* state_mgr = bog_state_manager_create();
    if (!state_mgr) {
        fprintf(stderr, "Failed to create state manager\n");
        bog_arena_destroy(arena);
        return 1;
    }

    BogAudioCallbacks callbacks = {};
    callbacks.userdata = &midi_ctx;
    callbacks.init = midi_init;
    callbacks.time = midi_time;
    callbacks.note = midi_note;

    BogScheduler* scheduler = bog_scheduler_create(
        &callbacks, builtins, state_mgr);
    if (!scheduler) {
        fprintf(stderr, "Failed to create scheduler\n");
        stop_worker(&midi_ctx);
        bog_state_manager_destroy(state_mgr);
        bog_arena_destroy(arena);
        return 1;
    }

    bog_scheduler_set_program(scheduler, program);
    bog_scheduler_configure(scheduler, bpm, 0.0, 80.0, 0.25);
    bog_scheduler_start(scheduler);

    std::signal(SIGINT, handle_signal);
    printf("Running %s at %.2f BPM. Press Ctrl+C to stop.\n",
           program_path, bpm);

    using namespace std::chrono_literals;
    while (g_running.load()) {
        bog_scheduler_tick(scheduler);
        std::this_thread::sleep_for(5ms);
    }

    bog_scheduler_stop(scheduler);
    stop_worker(&midi_ctx);
    bog_scheduler_destroy(scheduler);
    bog_state_manager_destroy(state_mgr);
    bog_arena_destroy(arena);
    midi_ctx.midi.reset();
    printf("Stopped.\n");
    return 0;
}
