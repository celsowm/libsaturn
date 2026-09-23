// probe_main.cpp — GPL-3.0 (see harness/LICENSE / harness/README.md).
//
// Headless probe: boots a libsaturn-built .iso inside Ymir's emulator core,
// runs it for a fixed number of frames, and dumps VDP1/VDP2 register and
// VRAM state to JSON for assertion by harness/tests/*.py.
//
// Captures the real composited video frame (VDP1 sprites over VDP2 layers)
// via VDP::SetSoftwareRenderCallback, which hands the software renderer's
// output buffer straight to us -- the same picture Ymir's own GUI displays.
// An earlier pass believed only a "frame finished" notification was exposed
// and worked from register/VRAM state alone; that is still dumped, because
// register assertions are precise in a way pixels are not, but screenshots
// no longer require the caller to reconstruct a picture from VDP1 VRAM.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "png_writer.hpp"

#include <ymir/hw/cart/cart_impl_bup.hpp>
#include <ymir/hw/cart/cart_impl_dram.hpp>
#include <span>
#include <ymir/hw/smpc/peripheral/peripheral_state_common.hpp>
#include <ymir/media/loader/loader.hpp>
#include <ymir/sys/saturn.hpp>

namespace {

struct VramRange {
    uint32_t base_word;
    uint32_t word_count;
};

struct ScspSlotSnapshot {
    uint8_t index;
    bool active;
    bool key_on;
    bool pcm8;
    uint32_t start_address;
    uint16_t loop_start;
    uint16_t loop_end;
    uint32_t curr_sample;
    uint8_t loop_control;
    uint8_t octave;
    uint16_t fns;
    uint8_t total_level;
    uint8_t direct_send_level;
    uint8_t direct_pan;
    bool ram_half_hash_valid;
    std::array<uint64_t, 2> ram_half_hashes;
};

struct ScspFrameSnapshot {
    uint32_t frame;
    std::array<ScspSlotSnapshot, 4> stream_slots;
};

struct InstructionCounter final : ymir::debug::ISH2Tracer {
    uint64_t count = 0;
    void ExecuteInstruction(uint32_t, uint16_t, bool) override { ++count; }
};

uint64_t fnv1a64(const uint8_t* data, size_t size) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

ScspFrameSnapshot capture_scsp_stream_snapshot(ymir::Saturn& saturn, uint32_t frame) {
    constexpr uint8_t kFirstStreamSlot = 28u;
    constexpr uint32_t kStreamHalfSamples = 4096u;
    constexpr uint32_t kStreamLoopSamples = 2u * kStreamHalfSamples;

    ScspFrameSnapshot snapshot{};
    snapshot.frame = frame;

    std::ostringstream sound_ram_out;
    saturn.SCSP.DumpWRAM(sound_ram_out);
    const std::string sound_ram = sound_ram_out.str();
    const auto& slots = saturn.SCSP.GetProbe().GetSlots();

    for (uint8_t i = 0; i < snapshot.stream_slots.size(); ++i) {
        const uint8_t slot_index = static_cast<uint8_t>(kFirstStreamSlot + i);
        const auto& slot = slots[slot_index];
        ScspSlotSnapshot& out = snapshot.stream_slots[i];

        out.index = slot_index;
        out.active = slot.active;
        out.key_on = slot.keyOnBit;
        out.pcm8 = slot.pcm8Bit;
        out.start_address = slot.startAddress;
        out.loop_start = slot.loopStartAddress;
        out.loop_end = slot.loopEndAddress;
        out.curr_sample = slot.currSample;
        out.loop_control = static_cast<uint8_t>(slot.loopControl);
        out.octave = slot.octave;
        // Ymir stores FNS internally with bit 10 toggled; expose the
        // register-visible value so harness assertions match the SCSP manual.
        out.fns = static_cast<uint16_t>(slot.freqNumSwitch ^ 0x400u);
        out.total_level = slot.totalLevel;
        out.direct_send_level = slot.directSendLevel;
        out.direct_pan = slot.directPan;

        if (!slot.pcm8Bit &&
            static_cast<uint32_t>(slot.loopEndAddress) + 1u == kStreamLoopSamples) {
            constexpr uint32_t kBytesPerSample = 2u;
            constexpr uint32_t kHalfBytes = kStreamHalfSamples * kBytesPerSample;
            const uint32_t first = slot.startAddress;
            const uint32_t second = first + kHalfBytes;
            if (second + kHalfBytes <= sound_ram.size()) {
                const auto* bytes = reinterpret_cast<const uint8_t*>(sound_ram.data());
                out.ram_half_hash_valid = true;
                out.ram_half_hashes[0] = fnv1a64(bytes + first, kHalfBytes);
                out.ram_half_hashes[1] = fnv1a64(bytes + second, kHalfBytes);
            }
        }
    }
    return snapshot;
}

// Scheduled single-button press for --pad-button: held (bit set, matching
// Ymir's PeripheralReport.buttons convention — see the pre-existing
// PROBE_HOLD_START callback below) for frame indices in
// [pad_press_at, pad_release_at) of phase 3 (the injected program's run),
// released every other frame. File-scope state because Ymir's report
// callback is a plain C function pointer, not a capturing std::function.
uint32_t g_pad_frame_counter = 0;
uint32_t g_pad_held_observed = 0; // times the report callback actually reported the button held
ymir::peripheral::Button g_pad_button = ymir::peripheral::Button::None;
uint32_t g_pad_press_at = 0;
uint32_t g_pad_release_at = 0;
// The BIOS polls peripherals on its own during the --boot-frames warm-up
// (phase 1, before our program has even been injected), which would
// otherwise burn through the press/release frame window before phase 3
// starts. Counting only starts once this is set, right before phase 3.
bool g_pad_active = false;

// Frame-indexed input timeline for --pad-script. Each entry says "from this
// program frame onward, hold exactly these buttons", so a whole play session
// is a short text file rather than a single press window. File scope because
// Ymir's report callback is a plain function pointer.
struct PadEvent {
    uint32_t frame;
    ymir::peripheral::Button buttons;
};
std::vector<PadEvent> g_pad_script;
// The emulated frame index within phase 3, set by the run loop rather than
// counted inside the report callback: the SMPC polls at a rate the program
// controls, so counting reports would make the timeline depend on how often
// the game happens to read the pad.
uint32_t g_pad_frame_index = 0;
bool g_pad_use_script = false;
bool g_pad_script_game_frame = false;

ymir::peripheral::Button button_from_name(const std::string& name) {
    using ymir::peripheral::Button;
    if (name == "UP") return Button::Up;
    if (name == "DOWN") return Button::Down;
    if (name == "LEFT") return Button::Left;
    if (name == "RIGHT") return Button::Right;
    if (name == "START") return Button::Start;
    if (name == "A") return Button::A;
    if (name == "B") return Button::B;
    if (name == "C") return Button::C;
    if (name == "X") return Button::X;
    if (name == "Y") return Button::Y;
    if (name == "Z") return Button::Z;
    if (name == "L") return Button::L;
    if (name == "R") return Button::R;
    return Button::None;
}

// "UP+A", "START", "NONE" -> a button mask.
ymir::peripheral::Button buttons_from_spec(const std::string& spec) {
    using ymir::peripheral::Button;
    if (spec.empty() || spec == "NONE" || spec == "-") {
        return Button::None;
    }
    Button mask = Button::None;
    size_t start = 0;
    while (start <= spec.size()) {
        const size_t plus = spec.find('+', start);
        const std::string name = spec.substr(start, plus == std::string::npos ? std::string::npos : plus - start);
        if (!name.empty()) {
            const Button b = button_from_name(name);
            if (b == Button::None) {
                std::fprintf(stderr, "unknown button in spec: %s\n", name.c_str());
                return Button::None;
            }
            mask = static_cast<Button>(static_cast<uint16_t>(mask) | static_cast<uint16_t>(b));
        }
        if (plus == std::string::npos) {
            break;
        }
        start = plus + 1;
    }
    return mask;
}

// Converts a "these buttons are down" mask into what the SMPC actually
// reports.
//
// Ymir's peripheral_report.hpp is explicit about this: "Button states
// (1=released, 0=pressed)", and the enum's Default is All. So Button::None --
// which reads like "nothing pressed" -- is every button held at once, and a
// single-button mask is every button EXCEPT that one. Getting it backwards
// still moves the game, which is why it survived: a script that meant LEFT
// was really sending everything-but-LEFT, and the guest dutifully did
// something different for each script.
ymir::peripheral::Button pad_report(ymir::peripheral::Button down) {
    using ymir::peripheral::Button;
    return static_cast<Button>(
        static_cast<uint16_t>(Button::All) & ~static_cast<uint16_t>(down));
}

// Buttons held at a given program frame: the most recent script entry at or
// before it.
ymir::peripheral::Button buttons_at_frame(uint32_t frame) {
    ymir::peripheral::Button held = ymir::peripheral::Button::None;
    for (const PadEvent& e : g_pad_script) {
        if (e.frame > frame) {
            break;
        }
        held = e.buttons;
    }
    return held;
}

struct Args {
    std::string iso_path;
    std::string bios_path;
    std::string bin_path;
    std::string out_path = "probe.json";
    uint32_t frames = 60;
    uint32_t boot_frames = 90; // BIOS-only frames run before injecting --bin
    std::vector<VramRange> vdp2_vram_ranges;
    uint32_t fb_sample_count = 16; // first N bytes of the VDP1 display framebuffer
    std::string dump_wram_high_path; // diagnostic: raw 1MiB High Work RAM dump
    std::string dump_fb_path;        // raw VDP1 display framebuffer, for visual checks
    std::string profile_pc_path;     // one master-SH2 PC sample per frame, for profiling
    std::string profile_cycles_path; // master-SH2 cycles consumed by each program frame
    std::string profile_instructions_path; // master/slave SH-2 instructions per frame
    std::string profile_transfers_path; // VDP1 VRAM, VDP2 VRAM and CRAM words per frame
    std::string pad_script_path;     // frame-indexed input timeline
    bool pad_script_game_frame = false;
    std::string backup_ram_path;     // persistent 32 KiB internal Backup RAM image
    std::string backup_cart_path;    // existing external image; mapped copy-on-write
    std::string ram_cart = "none";  // none, 1m, 4m: volatile expansion
    std::vector<std::pair<uint32_t, std::string>> screenshots;  // frame -> PNG path
    bool print_sh2_state = false;    // diagnostic: master SH2 PC/registers to stderr
    std::string pad_button;          // e.g. "A", "UP" — scheduled single-button press
    uint32_t pad_press_at = 0;       // frame index (within --frames) buttons becomes held
    uint32_t pad_release_at = 0;     // frame index buttons becomes released again
    bool scsp_trace = false;          // sample SCSP stream slots/Sound RAM every program frame
    uint32_t slave_reset_entry = 0;  // Ymir direct-injection compatibility handoff
    uint32_t skybridge_telemetry_address = 0;
    std::string skybridge_telemetry_csv_path;
};

constexpr uint32_t kSkybridgeTelemetryMagic = 0x5342544Du;
constexpr uint32_t kSkybridgeTelemetryVersion = 5u;
constexpr uint32_t kSkybridgeTelemetryWords = 66u;
constexpr uint32_t kSkybridgeTelemetryHeaderBytes = 20u;

struct SkybridgeTelemetrySample {
    uint32_t emulated_frame = 0u;
    std::array<uint32_t, kSkybridgeTelemetryWords> words{};
};

constexpr std::array<const char*, kSkybridgeTelemetryWords> kSkybridgeTelemetryNames = {
    "serial", "frame", "game_ticks", "game_hash", "animation_hash", "merge_order_hash",
    "frame_cpu_ms", "frame_ms", "wait_ms", "visible_gems", "master_items",
    "slave_items", "master_faces", "slave_faces", "merge_batches", "merged_faces",
    "animation_submitted", "animation_completed", "animation_failed",
    "animation_master", "animation_slave", "geometry_submitted", "geometry_completed",
    "geometry_failed", "geometry_master", "geometry_slave", "timeouts", "failures",
    "recovery_blocked", "gem_pending", "gem_task_state", "fault_mask",
    "sync_fallback_items", "game_course", "game_pickups", "game_paused",
    "game_finished", "game_support", "game_x", "game_y", "game_z",
    "game_vx", "game_vy", "game_vz", "animation_clip", "animation_frame",
    "animation_time", "pad_held", "pad_pressed",
    "frame_cpu_frt_ticks", "frame_frt_ticks", "geometry_prepare_frt_ticks",
    "geometry_merge_frt_ticks", "task_input_publish_frt_ticks",
    "task_submit_frt_ticks", "task_wait_frt_ticks", "task_completion_frt_ticks",
    "task_release_frt_ticks", "task_master_frt_ticks", "task_slave_frt_ticks",
    "scene_painter_frt_ticks", "scene_emit_frt_ticks", "scene_command_hash",
    "scene_command_count", "scene_command_capacity",
    "timer_read_overhead_frt_ticks"
};

void print_usage() {
    std::fprintf(stderr,
        "usage: probe --iso <path> --bios <path> --bin <path> [--out <path>] [--frames N]\n"
        "             [--boot-frames N] [--dump-vram BASE_WORD:WORD_COUNT ...] [--fb-sample N]\n"
        "             [--dump-wram-high <path>] [--dump-fb <path>]\n"
        "             [--profile-pc <path>] [--profile-cycles <path>] [--profile-instructions <path>] [--profile-transfers <path>] [--print-sh2-state]\n"
        "             [--pad-script <path>] [--screenshot FRAME:PATH ...]\n"
        "             [--pad-button NAME] [--pad-press-at N] [--pad-release-at N]\n"
        "             [--backup-ram <path>] [--backup-cart <existing-path>] [--ram-cart none|1m|4m] [--scsp-trace]\n"
        "             [--slave-reset-entry <address>]\n");
}

bool parse_vram_range(const std::string& spec, VramRange* out) {
    size_t colon = spec.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    out->base_word = static_cast<uint32_t>(std::strtoul(spec.substr(0, colon).c_str(), nullptr, 0));
    out->word_count = static_cast<uint32_t>(std::strtoul(spec.substr(colon + 1).c_str(), nullptr, 0));
    return out->word_count > 0;
}

bool parse_args(int argc, char** argv, Args* out) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", flag);
                return nullptr;
            }
            return argv[++i];
        };
        if (arg == "--iso") {
            const char* v = next("--iso");
            if (!v) return false;
            out->iso_path = v;
        } else if (arg == "--bios") {
            const char* v = next("--bios");
            if (!v) return false;
            out->bios_path = v;
        } else if (arg == "--bin") {
            const char* v = next("--bin");
            if (!v) return false;
            out->bin_path = v;
        } else if (arg == "--out") {
            const char* v = next("--out");
            if (!v) return false;
            out->out_path = v;
        } else if (arg == "--frames") {
            const char* v = next("--frames");
            if (!v) return false;
            out->frames = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--boot-frames") {
            const char* v = next("--boot-frames");
            if (!v) return false;
            out->boot_frames = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--fb-sample") {
            const char* v = next("--fb-sample");
            if (!v) return false;
            out->fb_sample_count = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--dump-wram-high") {
            const char* v = next("--dump-wram-high");
            if (!v) return false;
            out->dump_wram_high_path = v;
        } else if (arg == "--screenshot") {
            const char* v = next("--screenshot");
            if (!v) return false;
            const std::string spec(v);
            // The FIRST colon separates the frame number from the path: a
            // frame number never contains one, but a Windows path does
            // ("C:/..."), so searching from the right splits in the wrong
            // place and silently drops the drive letter.
            const size_t colon = spec.find(':');
            if (colon == std::string::npos || colon == 0) {
                std::fprintf(stderr, "invalid --screenshot spec: %s (want FRAME:PATH)\n", v);
                return false;
            }
            out->screenshots.emplace_back(
                static_cast<uint32_t>(std::strtoul(spec.substr(0, colon).c_str(), nullptr, 0)),
                spec.substr(colon + 1));
        } else if (arg == "--pad-script") {
            const char* v = next("--pad-script");
            if (!v) return false;
            out->pad_script_path = v;
        } else if (arg == "--pad-script-game-frame") {
            out->pad_script_game_frame = true;
        } else if (arg == "--profile-pc") {
            const char* v = next("--profile-pc");
            if (!v) return false;
            out->profile_pc_path = v;
        } else if (arg == "--profile-cycles") {
            const char* v = next("--profile-cycles");
            if (!v) return false;
            out->profile_cycles_path = v;
        } else if (arg == "--profile-instructions") {
            const char* v = next("--profile-instructions");
            if (!v) return false;
            out->profile_instructions_path = v;
        } else if (arg == "--profile-transfers") {
            const char* v = next("--profile-transfers");
            if (!v) return false;
            out->profile_transfers_path = v;
        } else if (arg == "--dump-fb") {
            const char* v = next("--dump-fb");
            if (!v) return false;
            out->dump_fb_path = v;
        } else if (arg == "--print-sh2-state") {
            out->print_sh2_state = true;
        } else if (arg == "--pad-button") {
            const char* v = next("--pad-button");
            if (!v) return false;
            out->pad_button = v;
        } else if (arg == "--pad-press-at") {
            const char* v = next("--pad-press-at");
            if (!v) return false;
            out->pad_press_at = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--pad-release-at") {
            const char* v = next("--pad-release-at");
            if (!v) return false;
            out->pad_release_at = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--backup-ram") {
            const char* v = next("--backup-ram");
            if (!v) return false;
            out->backup_ram_path = v;
        } else if (arg == "--backup-cart") {
            const char* v = next("--backup-cart");
            if (!v) return false;
            out->backup_cart_path = v;
        } else if (arg == "--ram-cart") {
            const char* v = next("--ram-cart");
            if (!v) return false;
            out->ram_cart = v;
            if (out->ram_cart != "none" && out->ram_cart != "1m" && out->ram_cart != "4m") {
                std::fprintf(stderr, "invalid --ram-cart: %s (use none, 1m, or 4m)\n", v);
                return false;
            }
        } else if (arg == "--scsp-trace") {
            out->scsp_trace = true;
        } else if (arg == "--slave-reset-entry") {
            const char* v = next("--slave-reset-entry");
            if (!v) return false;
            out->slave_reset_entry = static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--skybridge-telemetry-address") {
            const char* v = next("--skybridge-telemetry-address");
            if (!v) return false;
            out->skybridge_telemetry_address =
                static_cast<uint32_t>(std::strtoul(v, nullptr, 0));
        } else if (arg == "--skybridge-telemetry-csv") {
            const char* v = next("--skybridge-telemetry-csv");
            if (!v) return false;
            out->skybridge_telemetry_csv_path = v;
        } else if (arg == "--dump-vram") {
            const char* v = next("--dump-vram");
            if (!v) return false;
            VramRange range;
            if (!parse_vram_range(v, &range)) {
                std::fprintf(stderr, "invalid --dump-vram spec: %s (want BASE_WORD:WORD_COUNT)\n", v);
                return false;
            }
            out->vdp2_vram_ranges.push_back(range);
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            return false;
        }
    }
    if (out->iso_path.empty() || out->bios_path.empty() || out->bin_path.empty()) {
        std::fprintf(stderr, "--iso, --bios and --bin are required\n");
        return false;
    }
    if (!out->pad_button.empty() && button_from_name(out->pad_button) == ymir::peripheral::Button::None) {
        std::fprintf(stderr, "unknown --pad-button: %s\n", out->pad_button.c_str());
        return false;
    }
    if ((out->skybridge_telemetry_address == 0u) !=
        out->skybridge_telemetry_csv_path.empty()) {
        std::fprintf(stderr,
            "Skybridge telemetry requires both address and CSV output path\n");
        return false;
    }
    return true;
}

// Minimal JSON writer — the data here is entirely numeric/string, so no
// general-purpose JSON library dependency is needed.
//
// Object members always go through key() (which emits its own comma+key);
// array elements go through array_prefix(), called from value() and from
// begin_object()/begin_array() when used as bare array elements. Which one
// applies is determined by the CURRENT (parent) frame at the time of the
// call, so begin_object()/begin_array() must call array_prefix() BEFORE
// pushing their own frame.
class JsonWriter {
public:
    explicit JsonWriter(std::ostream& out) : m_out(out) {}

    void begin_object() { array_prefix(); m_out << "{"; m_stack.push_back({false, true}); }
    void end_object() { m_stack.pop_back(); m_out << "}"; }

    void begin_array() { array_prefix(); m_out << "["; m_stack.push_back({true, true}); }
    void end_array() { m_stack.pop_back(); m_out << "]"; }

    void key(const char* name) {
        Frame& f = m_stack.back();
        if (!f.first) m_out << ",";
        f.first = false;
        m_out << "\"" << name << "\":";
    }

    void value(uint64_t v) { array_prefix(); m_out << v; }
    void value(int64_t v) { array_prefix(); m_out << v; }
    void value(bool v) { array_prefix(); m_out << (v ? "true" : "false"); }
    void value(const std::string& s) {
        array_prefix();
        write_escaped_string(s);
    }

private:
    struct Frame { bool is_array; bool first; };

    void write_escaped_string(const std::string& s) {
        m_out << "\"";
        for (char c : s) {
            switch (c) {
            case '"': m_out << "\\\""; break;
            case '\\': m_out << "\\\\"; break;
            case '\n': m_out << "\\n"; break;
            case '\r': m_out << "\\r"; break;
            case '\t': m_out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    m_out << buf;
                } else {
                    m_out << c;
                }
            }
        }
        m_out << "\"";
    }

    void array_prefix() {
        if (m_stack.empty()) return;
        Frame& f = m_stack.back();
        if (f.is_array) {
            if (!f.first) m_out << ",";
            f.first = false;
        }
    }

    std::ostream& m_out;
    std::vector<Frame> m_stack;
};

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// VDP2 VRAM word W at word address `w` occupies bytes [2w, 2w+1] in the raw
// dump, big-endian (matches Saturn/SH2 big-endian memory convention, and how
// libsaturn itself packs VRAM words — see examples/*/main.c bitmap upload).
uint16_t read_be_word(const std::vector<char>& bytes, uint32_t word_addr) {
    size_t byte_addr = static_cast<size_t>(word_addr) * 2u;
    if (byte_addr + 1 >= bytes.size()) {
        return 0;
    }
    uint8_t hi = static_cast<uint8_t>(bytes[byte_addr]);
    uint8_t lo = static_cast<uint8_t>(bytes[byte_addr + 1]);
    return static_cast<uint16_t>((hi << 8) | lo);
}

// IP.BIN (the Saturn boot header BIOS reads from sector 0) stores the
// program's load address at byte offset 0xF0 and its size at 0xF4, both
// big-endian 32-bit words — see tools/gen_ip_bin.py. Reading it back out of
// the built .iso (rather than hardcoding 0x06004000) keeps the harness in
// sync with the Makefile's APP_LOAD_ADDR_HEX if that ever changes.
uint32_t read_be32(const std::vector<uint8_t>& bytes, size_t byte_addr) {
    if (byte_addr + 4 > bytes.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(bytes[byte_addr]) << 24) | (static_cast<uint32_t>(bytes[byte_addr + 1]) << 16) |
           (static_cast<uint32_t>(bytes[byte_addr + 2]) << 8) | static_cast<uint32_t>(bytes[byte_addr + 3]);
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        print_usage();
        return 2;
    }

    std::vector<uint8_t> bios = read_file(args.bios_path);
    if (bios.size() != ymir::sys::kIPLSize) {
        std::fprintf(stderr, "BIOS file %s is %zu bytes, expected exactly %zu (kIPLSize)\n",
                      args.bios_path.c_str(), bios.size(), ymir::sys::kIPLSize);
        return 1;
    }

    // Saturn embeds VDP1/VDP2 VRAM, work RAM and sound RAM as value members
    // (several MB total) — heap-allocate it rather than risk a stack overflow
    // constructing it as a local.
    auto saturn = std::make_unique<ymir::Saturn>();
    saturn->LoadIPL(std::span<uint8_t, ymir::sys::kIPLSize>(bios.data(), ymir::sys::kIPLSize));

    if (!args.backup_ram_path.empty()) {
        // Ymir memory-maps the image in write-through mode when copyOnWrite is
        // false, so a second probe process can observe saves made by the first.
        // CreateFrom semantics inside Ymir also create/format a missing 32 KiB
        // internal image, which gives persistence tests a deterministic first
        // boot without committing any emulator-owned save image to the repo.
        std::error_code backup_error;
        saturn->mem.LoadInternalBackupMemoryImage(
            args.backup_ram_path, /*copyOnWrite=*/false, backup_error);
        if (backup_error) {
            std::fprintf(stderr, "failed to load internal Backup RAM image %s: %s\n",
                         args.backup_ram_path.c_str(), backup_error.message().c_str());
            return 1;
        }
    }

    if (args.ram_cart != "none" && !args.backup_cart_path.empty()) {
        std::fprintf(stderr, "a Backup Memory cart and DRAM expansion cannot occupy the same slot\n");
        return 1;
    }
    ymir::cart::DRAM8MbitCartridge* ram_cart_1m = nullptr;
    ymir::cart::DRAM32MbitCartridge* ram_cart_4m = nullptr;
    if (args.ram_cart == "1m") {
        ram_cart_1m = saturn->InsertCartridge<ymir::cart::DRAM8MbitCartridge>();
        if (ram_cart_1m == nullptr) return 1;
    } else if (args.ram_cart == "4m") {
        ram_cart_4m = saturn->InsertCartridge<ymir::cart::DRAM32MbitCartridge>();
        if (ram_cart_4m == nullptr) return 1;
    }

    ymir::cart::BackupMemoryCartridge* backup_cart = nullptr;
    uint64_t backup_cart_before_hash = 0u;
    if (!args.backup_cart_path.empty()) {
        // Read-only diagnostic path: never create, resize or format images;
        // never persist guest writes to a user-supplied cartridge image.
        ymir::bup::BackupMemory cart_image;
        std::error_code cart_error;
        const auto result = cart_image.LoadFrom(
            args.backup_cart_path, /*copyOnWrite=*/true, cart_error);
        if (result != ymir::bup::BackupMemoryImageLoadResult::Success) {
            std::fprintf(stderr,
                         "failed to load existing backup cartridge image %s (result %u): %s\n",
                         args.backup_cart_path.c_str(),
                         static_cast<unsigned>(result), cart_error.message().c_str());
            return 1;
        }
        const auto bytes_before = cart_image.ReadAll();
        backup_cart_before_hash = fnv1a64(bytes_before.data(), bytes_before.size());
        backup_cart = saturn->InsertCartridge<ymir::cart::BackupMemoryCartridge>(
            std::move(cart_image));
        if (backup_cart == nullptr) {
            std::fprintf(stderr, "failed to insert backup cartridge\n");
            return 1;
        }
    }

    ymir::media::Disc disc;
    // LoadDisc tries several loaders in sequence (CHD, BIN/CUE, MDF/MDS, CCD,
    // ISO) and unconditionally invokes cbMsg on each rejected attempt (e.g.
    // "not a valid CHD") before the matching one succeeds — an empty
    // std::function here throws std::bad_function_call, so a real (if inert)
    // callback is required, not nullptr.
    auto loader_msg_cb = [](ymir::media::MessageType, std::string) {};
    if (!ymir::media::LoadDisc(args.iso_path, disc, /*preloadToRAM=*/true, loader_msg_cb)) {
        std::fprintf(stderr, "failed to load disc image: %s\n", args.iso_path.c_str());
        return 1;
    }
    saturn->LoadDisc(std::move(disc));
    saturn->UsePreferredRegion();
    if (!args.pad_script_path.empty()) {
        // A whole play session as a text file: "FRAME BUTTONS" per line, the
        // buttons held from that program frame until the next line. Comments
        // start with '#'.
        std::ifstream script(args.pad_script_path);
        if (!script) {
            std::fprintf(stderr, "failed to open --pad-script: %s\n", args.pad_script_path.c_str());
            return 1;
        }
        std::string line;
        uint32_t lineno = 0;
        while (std::getline(script, line)) {
            ++lineno;
            const size_t hash = line.find('#');
            if (hash != std::string::npos) {
                line = line.substr(0, hash);
            }
            std::istringstream ls(line);
            uint32_t frame = 0;
            std::string spec;
            if (!(ls >> frame >> spec)) {
                continue;  // blank or comment-only
            }
            g_pad_script.push_back({frame, buttons_from_spec(spec)});
        }
        if (g_pad_script.empty()) {
            std::fprintf(stderr, "--pad-script %s has no usable lines\n", args.pad_script_path.c_str());
            return 1;
        }
        std::stable_sort(g_pad_script.begin(), g_pad_script.end(),
                         [](const PadEvent& a, const PadEvent& b) { return a.frame < b.frame; });
        g_pad_use_script = true;
        g_pad_script_game_frame = args.pad_script_game_frame;

        auto& port = saturn->SMPC.GetPeripheralPort1();
        using ReportCb = void (*)(ymir::peripheral::PeripheralReport&, void*);
        port.SetPeripheralReportCallback(ReportCb([](ymir::peripheral::PeripheralReport& report, void*) {
            if (!g_pad_active) {
                report.report.controlPad.buttons = pad_report(ymir::peripheral::Button::None);
                return;
            }
            const ymir::peripheral::Button held = buttons_at_frame(g_pad_frame_index);
            if (g_pad_script_game_frame) ++g_pad_frame_index;
            report.report.controlPad.buttons = pad_report(held);
            if (held != ymir::peripheral::Button::None) {
                g_pad_held_observed++;
            }
        }));
        port.ConnectControlPad();
        std::fprintf(stderr, "[probe] loaded %zu pad script entries from %s\n",
                     g_pad_script.size(), args.pad_script_path.c_str());
    } else if (!args.pad_button.empty()) {
        // --pad-button: hold exactly one button for a scheduled frame window,
        // released before and after — lets a test observe a single clean
        // press+release edge rather than every-button-held-forever.
        g_pad_button = button_from_name(args.pad_button);
        g_pad_press_at = args.pad_press_at;
        g_pad_release_at = args.pad_release_at;
        auto& port = saturn->SMPC.GetPeripheralPort1();
        using ReportCb = void (*)(ymir::peripheral::PeripheralReport&, void*);
        port.SetPeripheralReportCallback(ReportCb([](ymir::peripheral::PeripheralReport& report, void*) {
            if (!g_pad_active) {
                report.report.controlPad.buttons = pad_report(ymir::peripheral::Button::None);
                return;
            }
            uint32_t frame = g_pad_frame_counter++;
            bool held = frame >= g_pad_press_at && frame < g_pad_release_at;
            report.report.controlPad.buttons =
                pad_report(held ? g_pad_button : ymir::peripheral::Button::None);
            if (held) {
                g_pad_held_observed++;
            }
            if (std::getenv("PROBE_PAD_DEBUG") != nullptr) {
                std::fprintf(stderr, "pad callback: frame=%u held=%d buttons=%04X\n", frame, held,
                              static_cast<unsigned>(report.report.controlPad.buttons));
            }
        }));
        port.ConnectControlPad();
    } else if (std::getenv("PROBE_CONNECT_PAD") != nullptr) {
        auto& port = saturn->SMPC.GetPeripheralPort1();
        if (std::getenv("PROBE_HOLD_START") != nullptr) {
            using ReportCb = void (*)(ymir::peripheral::PeripheralReport&, void*);
            port.SetPeripheralReportCallback(ReportCb([](ymir::peripheral::PeripheralReport& report, void*) {
                report.report.controlPad.buttons = pad_report(ymir::peripheral::Button::Start);
            }));
        }
        port.ConnectControlPad();
    }

    // Latest composited video frame (VDP1 sprites over VDP2 layers), copied
    // out of Ymir's software renderer. Copied rather than referenced because
    // the renderer reuses its buffer for the next frame as soon as the
    // callback returns.
    //
    // Ymir's callback is C-style: a function pointer plus a context pointer
    // passed as the LAST argument, so a captureless lambda works.
    struct FrameSink {
        std::vector<uint32_t> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    FrameSink frame;
    saturn->VDP.SetSoftwareRenderCallback(
        {&frame, [](uint32_t* fb, uint32_t width, uint32_t height, void* ctx) {
             FrameSink* sink = static_cast<FrameSink*>(ctx);
             sink->width = width;
             sink->height = height;
             sink->pixels.assign(fb, fb + static_cast<size_t>(width) * height);
         }});

    bool trace_frames = std::getenv("PROBE_TRACE_FRAMES") != nullptr;
    auto& sh2p = saturn->masterSH2.GetProbe();
    // One PC sample per emulated frame. Coarse, but a program that needs many
    // emulated frames per program frame is exactly the case worth profiling,
    // and there the samples land squarely in whatever is eating the time.
    std::vector<uint32_t> pc_samples;
    std::vector<uint64_t> cycle_samples;
    std::vector<std::array<uint64_t, 2>> instruction_samples;
    std::vector<std::array<uint64_t, 3>> transfer_samples;
    std::vector<SkybridgeTelemetrySample> skybridge_telemetry_samples;
    uint32_t last_skybridge_telemetry_count = 0u;
    InstructionCounter master_instructions;
    InstructionCounter slave_instructions;
    if (!args.profile_instructions_path.empty()) {
        saturn->EnableDebugTracing(true);
        saturn->masterSH2.UseTracer(&master_instructions);
        saturn->slaveSH2.UseTracer(&slave_instructions);
    }
    saturn->VDP.ResetWriteCounters();
    std::vector<ScspFrameSnapshot> scsp_trace;
    bool sampling_pc = false;
    bool taking_screenshots = false;
    auto run_frames = [&](uint32_t count, uint32_t frame_offset) {
        for (uint32_t i = 0; i < count; i++) {
            // Set before running, so the pad reports the buttons this script
            // line asks for during the frame it names rather than after it.
            if (!args.pad_script_game_frame) g_pad_frame_index = i;
            const uint64_t cycles_before = saturn->GetCurrentCycleCount();
            saturn->RunFrame();
            if (args.skybridge_telemetry_address != 0u) {
                const uint32_t address = args.skybridge_telemetry_address & 0x0FFF'FFFFu;
                if (address < 0x0600'0000u || address >= 0x0610'0000u) {
                    std::fprintf(stderr,
                        "Skybridge telemetry address is outside High WRAM: %08X\n",
                        args.skybridge_telemetry_address);
                    std::exit(EXIT_FAILURE);
                }
                const size_t base = static_cast<size_t>(address - 0x0600'0000u);
                const auto& wram = saturn->mem.WRAMHigh;
                auto read_wram_be32 = [&](size_t offset) -> uint32_t {
                    const uint8_t* p = wram.data() + offset;
                    return (static_cast<uint32_t>(p[0]) << 24u) |
                           (static_cast<uint32_t>(p[1]) << 16u) |
                           (static_cast<uint32_t>(p[2]) << 8u) |
                           static_cast<uint32_t>(p[3]);
                };
                if (base + kSkybridgeTelemetryHeaderBytes <= wram.size() &&
                    read_wram_be32(base) == kSkybridgeTelemetryMagic) {
                    const uint32_t version = read_wram_be32(base + 4u);
                    const uint32_t words = read_wram_be32(base + 8u);
                    const uint32_t capacity = read_wram_be32(base + 12u);
                    const uint32_t count_now = read_wram_be32(base + 16u);
                    if (version != kSkybridgeTelemetryVersion ||
                        words != kSkybridgeTelemetryWords || capacity == 0u ||
                        base + kSkybridgeTelemetryHeaderBytes +
                            static_cast<size_t>(capacity) * words * 4u > wram.size()) {
                        std::fprintf(stderr,
                            "Unsupported Skybridge telemetry header (v%u, %u words, cap %u)\n",
                            version, words, capacity);
                        std::exit(EXIT_FAILURE);
                    }
                    const uint32_t first = count_now > capacity &&
                        count_now - last_skybridge_telemetry_count > capacity
                            ? count_now - capacity
                            : last_skybridge_telemetry_count;
                    for (uint32_t serial = first; serial < count_now; ++serial) {
                        const uint32_t slot = serial % capacity;
                        const size_t record_base = base + kSkybridgeTelemetryHeaderBytes +
                            static_cast<size_t>(slot) * words * 4u;
                        SkybridgeTelemetrySample sample{};
                        sample.emulated_frame = frame_offset + i;
                        for (uint32_t word = 0u; word < words; ++word)
                            sample.words[word] = read_wram_be32(record_base + word * 4u);
                        skybridge_telemetry_samples.push_back(sample);
                    }
                    last_skybridge_telemetry_count = count_now;
                }
            }
            if (!args.profile_cycles_path.empty()) {
                cycle_samples.push_back(saturn->GetCurrentCycleCount() - cycles_before);
            }
            if (!args.profile_instructions_path.empty()) {
                instruction_samples.push_back({master_instructions.count, slave_instructions.count});
            }
            if (!args.profile_transfers_path.empty()) {
                const auto& writes = saturn->VDP.GetWriteCounters();
                transfer_samples.push_back({writes.vdp1VRAMWords, writes.vdp2VRAMWords, writes.vdp2CRAMWords});
                saturn->VDP.ResetWriteCounters();
            }
            if (args.scsp_trace && g_pad_active) {
                scsp_trace.push_back(capture_scsp_stream_snapshot(*saturn, i));
            }
            if (sampling_pc) {
                pc_samples.push_back(sh2p.PC());
            }
            if (taking_screenshots) {
                for (const auto& shot : args.screenshots) {
                    if (shot.first != i) {
                        continue;
                    }
                    if (frame.pixels.empty()) {
                        std::fprintf(stderr,
                                     "[probe] frame %u: no composited frame yet, skipping %s\n",
                                     i, shot.second.c_str());
                        continue;
                    }
                    if (harness::png::write_rgb(shot.second, frame.pixels.data(), frame.width, frame.height)) {
                        std::fprintf(stderr, "[probe] frame %u -> %s (%ux%u)\n", i,
                                     shot.second.c_str(), frame.width, frame.height);
                    } else {
                        std::fprintf(stderr, "[probe] frame %u: failed to write %s\n", i,
                                     shot.second.c_str());
                    }
                }
            }
            if (!trace_frames) continue;
            uint32_t gbr = sh2p.GBR();
            uint32_t masked = (gbr + 576u) & 0x0FFF'FFFFu;
            uint32_t val = 0xDEADBEEFu;
            if (masked >= 0x0600'0000u && masked < 0x0610'0000u) {
                uint32_t off = masked - 0x0600'0000u;
                const uint8_t* p = saturn->mem.WRAMHigh.data() + off;
                val = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
                      (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
            } else if (masked >= 0x0020'0000u && masked < 0x0030'0000u) {
                uint32_t off = masked - 0x0020'0000u;
                const uint8_t* p = saturn->mem.WRAMLow.data() + off;
                val = (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
                      (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
            }
            uint8_t flag = saturn->mem.WRAMHigh[0x004da68u];
            uint8_t evcode = saturn->mem.WRAMHigh[0x00b3064u];
            uint8_t substate = saturn->mem.WRAMHigh[0x00b3065u];
            int16_t timeout_ctr = static_cast<int16_t>((saturn->mem.WRAMHigh[0x004da8cu] << 8) |
                                                         saturn->mem.WRAMHigh[0x004da8du]);
            std::fprintf(stderr,
                         "frame %4u: PC=%08X GBR=%08X [GBR+576]=%08X flag@604da68=%02X ev@60b3064=%02X sub@60b3065=%02X "
                         "ctr@604da8c=%d\n",
                         frame_offset + i, sh2p.PC(), gbr, val, flag, evcode, substate, timeout_ctr);
        }
    };

    // -- Phase 1: let the BIOS run alone for --boot-frames frames, so SMPC/
    // SCU/VDP2 hardware init completes. See harness/README.md: Ymir's CD
    // block does not currently complete a real BIOS disc boot for these
    // images (it stalls in the BIOS's CD driver after disc authentication),
    // so this harness does not depend on the BIOS ever finishing that.
    run_frames(args.boot_frames, 0);

    // The real Saturn BIOS transfers the Slave entry point through its
    // slave-start protocol. Ymir's SSHON model resets the Slave from the
    // generic SH-2 reset vector instead, so the dual-SH2 run supplies that
    // vector in the emulated IPL after BIOS-only execution. This is a
    // harness-only compatibility handoff; it does not change the guest ROM
    // or the hardware path exercised by the application.
    if (args.slave_reset_entry != 0) {
        auto write_ipl_be32 = [&](size_t offset, uint32_t value) {
            saturn->mem.IPL[offset + 0] = static_cast<uint8_t>(value >> 24);
            saturn->mem.IPL[offset + 1] = static_cast<uint8_t>(value >> 16);
            saturn->mem.IPL[offset + 2] = static_cast<uint8_t>(value >> 8);
            saturn->mem.IPL[offset + 3] = static_cast<uint8_t>(value);
        };
        write_ipl_be32(0x00, args.slave_reset_entry);
        write_ipl_be32(0x04, 0x06001000u);
        std::fprintf(stderr, "[probe] Ymir Slave reset-vector handoff: PC=%08X SP=06001000\n",
                     args.slave_reset_entry);
    }

    // -- Phase 2: inject the built program directly into work RAM and jump
    // to it -- what the BIOS would have done had its CD read completed.
    // src/core/startup/crt0.s's _start is self-sufficient (masks IRQs, sets its own
    // stack, zeroes BSS, calls _main) and never touches VBR, so nothing more
    // than "resident at the load address with PC pointing there" is needed.
    std::vector<uint8_t> bin = read_file(args.bin_path);
    if (bin.empty()) {
        std::fprintf(stderr, "failed to read --bin file (or it's empty): %s\n", args.bin_path.c_str());
        return 1;
    }

    // Read the load address out of the built .iso's IP.BIN header (the boot
    // sector mkisofs -G embeds at LBA 0 -- see tools/gen_ip_bin.py) rather
    // than hardcoding 0x06004000, so this stays in sync with the Makefile's
    // APP_LOAD_ADDR_HEX if that ever changes.
    std::vector<uint8_t> iso_header(0x100);
    {
        std::ifstream iso_header_in(args.iso_path, std::ios::binary);
        iso_header_in.read(reinterpret_cast<char*>(iso_header.data()), static_cast<std::streamsize>(iso_header.size()));
    }
    uint32_t load_addr = read_be32(iso_header, 0xF0);
    uint32_t header_size = read_be32(iso_header, 0xF4);
    if (load_addr == 0 || header_size != bin.size()) {
        std::fprintf(stderr,
                     "warning: IP.BIN header size (0x%X) does not match --bin file size (0x%zX) -- "
                     "--iso and --bin may not be from the same build\n",
                     header_size, bin.size());
    }

    // The BIOS would normally have copied 0.BIN here off the CD and jumped
    // to it; do exactly that ourselves. Goes through MemWriteByte (not a raw
    // memcpy into mem.WRAMHigh) so the SH2 instruction cache stays coherent
    // with what we just wrote.
    for (size_t i = 0; i < bin.size(); i++) {
        sh2p.MemWriteByte(static_cast<uint32_t>(load_addr + i), bin[i], /*bypassCache=*/false);
    }
    sh2p.R(15) = 0x060FFFFCu; // stack top -- matches .Lstack_top in src/core/startup/crt0.s
    sh2p.PR() = load_addr;    // if _main ever returns, land in crt0's hang loop
    sh2p.PC() = load_addr;    // _start
    sh2p.RefillPipeline();    // PC was changed behind the interpreter's back

    // -- Phase 3: run the injected program for --frames frames.
    g_pad_active = true;
    sampling_pc = !args.profile_pc_path.empty();
    taking_screenshots = !args.screenshots.empty();
    run_frames(args.frames, args.boot_frames);

    const uint32_t pc_after_run = sh2p.PC();
    const ScspFrameSnapshot scsp_final =
        capture_scsp_stream_snapshot(*saturn, args.frames == 0u ? 0u : args.frames - 1u);

    if (args.print_sh2_state) {
        std::fprintf(stderr, "master SH2: PC=%08X PR=%08X SR=%08X GBR=%08X VBR=%08X\n", sh2p.PC(), sh2p.PR(),
                     static_cast<uint32_t>(sh2p.SR().u32), sh2p.GBR(), sh2p.VBR());
        for (int r = 0; r < 16; r++) {
            std::fprintf(stderr, "  R%-2d = %08X\n", r, sh2p.R(static_cast<uint8_t>(r)));
        }

        // Dump the 32-bit word at [GBR+576] directly from whichever WRAM
        // region GBR points into -- this is the value 0x60402e4's busy-loop
        // is spinning on, waiting for it to change (presumably incremented
        // by an interrupt handler).
        uint32_t gbr_target = sh2p.GBR() + 576u;
        const uint8_t* base = nullptr;
        uint32_t base_addr = 0;
        size_t region_size = 0;
        uint32_t masked = gbr_target & 0x0FFF'FFFFu;
        if (masked >= 0x0020'0000u && masked < 0x0030'0000u) {
            base = saturn->mem.WRAMLow.data();
            base_addr = 0x0020'0000u;
            region_size = saturn->mem.WRAMLow.size();
        } else if (masked >= 0x0600'0000u && masked < 0x0610'0000u) {
            base = saturn->mem.WRAMHigh.data();
            base_addr = 0x0600'0000u;
            region_size = saturn->mem.WRAMHigh.size();
        }
        if (base != nullptr) {
            uint32_t off = masked - base_addr;
            if (off + 4 <= region_size) {
                uint32_t value = (static_cast<uint32_t>(base[off]) << 24) | (static_cast<uint32_t>(base[off + 1]) << 16) |
                                  (static_cast<uint32_t>(base[off + 2]) << 8) | static_cast<uint32_t>(base[off + 3]);
                std::fprintf(stderr, "[GBR+576] = %08X (addr %08X)\n", value, gbr_target);
            }
        } else {
            std::fprintf(stderr, "[GBR+576] addr %08X not in WRAMLow/WRAMHigh\n", gbr_target);
        }

        auto& scup = saturn->SCU.GetProbe();
        auto& imask = scup.GetInterruptMask();
        auto& istat = scup.GetInterruptStatus();
        std::fprintf(stderr, "SCU IMASK=%08X (VBlankIN masked=%d) ISTAT=%08X (VBlankIN pending=%d)\n",
                     static_cast<uint32_t>(imask.u32), imask.VDP2_VBlankIN, static_cast<uint32_t>(istat.u32),
                     istat.VDP2_VBlankIN);
    }

    if (!args.profile_pc_path.empty()) {
        std::ofstream pc_out(args.profile_pc_path);
        if (!pc_out) {
            std::fprintf(stderr, "failed to open --profile-pc output: %s\n", args.profile_pc_path.c_str());
        } else {
            for (uint32_t pc : pc_samples) {
                pc_out << std::hex << pc << "\n";
            }
            std::fprintf(stderr, "[probe] wrote %zu PC samples to %s\n",
                         pc_samples.size(), args.profile_pc_path.c_str());
        }
    }

    if (!args.profile_cycles_path.empty()) {
        std::ofstream cycles_out(args.profile_cycles_path);
        if (!cycles_out) {
            std::fprintf(stderr, "failed to open --profile-cycles output: %s\n", args.profile_cycles_path.c_str());
        } else {
            cycles_out << "frame,master_sh2_cycles\n";
            for (size_t i = 0; i < cycle_samples.size(); ++i) {
                cycles_out << i << ',' << cycle_samples[i] << "\n";
            }
            std::fprintf(stderr, "[probe] wrote %zu cycle samples to %s\n",
                         cycle_samples.size(), args.profile_cycles_path.c_str());
        }
    }

    if (!args.profile_instructions_path.empty()) {
        std::ofstream instructions_out(args.profile_instructions_path);
        if (!instructions_out) {
            std::fprintf(stderr, "failed to open --profile-instructions output: %s\n",
                         args.profile_instructions_path.c_str());
        } else {
            instructions_out << "frame,master_sh2_instructions,slave_sh2_instructions\n";
            uint64_t prev_master = 0;
            uint64_t prev_slave = 0;
            for (size_t i = 0; i < instruction_samples.size(); ++i) {
                const uint64_t master = instruction_samples[i][0];
                const uint64_t slave = instruction_samples[i][1];
                instructions_out << i << ',' << (master - prev_master) << ',' << (slave - prev_slave) << "\n";
                prev_master = master;
                prev_slave = slave;
            }
            std::fprintf(stderr, "[probe] wrote %zu instruction samples to %s\n",
                         instruction_samples.size(), args.profile_instructions_path.c_str());
        }
    }

    if (!args.profile_transfers_path.empty()) {
        std::ofstream transfers_out(args.profile_transfers_path);
        if (!transfers_out) {
            std::fprintf(stderr, "failed to open --profile-transfers output: %s\n",
                         args.profile_transfers_path.c_str());
        } else {
            transfers_out << "frame,vdp1_vram_words,vdp2_vram_words,vdp2_cram_words\n";
            for (size_t i = 0; i < transfer_samples.size(); ++i) {
                transfers_out << i << ',' << transfer_samples[i][0] << ','
                              << transfer_samples[i][1] << ',' << transfer_samples[i][2] << "\n";
            }
            std::fprintf(stderr, "[probe] wrote %zu transfer samples to %s\n",
                         transfer_samples.size(), args.profile_transfers_path.c_str());
        }
    }

    if (!args.skybridge_telemetry_csv_path.empty()) {
        std::ofstream telemetry_out(args.skybridge_telemetry_csv_path);
        if (!telemetry_out) {
            std::fprintf(stderr, "failed to open Skybridge telemetry CSV: %s\n",
                         args.skybridge_telemetry_csv_path.c_str());
            return 1;
        }
        telemetry_out << "emulated_frame";
        for (const char* name : kSkybridgeTelemetryNames)
            telemetry_out << ',' << name;
        telemetry_out << '\n';
        for (const auto& sample : skybridge_telemetry_samples) {
            telemetry_out << sample.emulated_frame;
            for (uint32_t word : sample.words) telemetry_out << ',' << word;
            telemetry_out << '\n';
        }
        std::fprintf(stderr, "[probe] wrote %zu Skybridge telemetry rows to %s\n",
                     skybridge_telemetry_samples.size(),
                     args.skybridge_telemetry_csv_path.c_str());
    }

    // Whole VDP1 display framebuffer, so a run can be checked by looking at
    // the picture rather than only at registers: the --fb-sample window of
    // first-N-bytes cannot tell "drew the maze" from "drew nothing".
    if (!args.dump_fb_path.empty()) {
        auto fb = saturn->VDP.VDP1GetDisplayFramebuffer();
        std::ofstream fb_out(args.dump_fb_path, std::ios::binary);
        if (!fb_out) {
            std::fprintf(stderr, "failed to open --dump-fb output: %s\n", args.dump_fb_path.c_str());
        } else {
            fb_out.write(reinterpret_cast<const char*>(fb.data()),
                         static_cast<std::streamsize>(fb.size()));
            std::fprintf(stderr, "[probe] wrote %zu framebuffer bytes to %s\n",
                         fb.size(), args.dump_fb_path.c_str());
        }
    }

    if (!args.dump_wram_high_path.empty()) {
        std::ofstream wram_out(args.dump_wram_high_path, std::ios::binary);
        if (!wram_out) {
            std::fprintf(stderr, "failed to open --dump-wram-high output: %s\n", args.dump_wram_high_path.c_str());
        } else {
            wram_out.write(reinterpret_cast<const char*>(saturn->mem.WRAMHigh.data()),
                            static_cast<std::streamsize>(saturn->mem.WRAMHigh.size()));
            std::fprintf(stderr, "wrote %zu bytes of High WRAM to %s\n", saturn->mem.WRAMHigh.size(),
                         args.dump_wram_high_path.c_str());
        }
    }

    auto& probe = saturn->VDP.GetProbe();
    const auto& vdp1_regs = probe.GetVDP1Regs();
    const auto& vdp2_regs = probe.GetVDP2Regs();

    std::ostringstream vram2_dump_stream;
    saturn->VDP.DumpVDP2VRAM(vram2_dump_stream);
    std::string vram2_dump = vram2_dump_stream.str();
    std::vector<char> vram2_bytes(vram2_dump.begin(), vram2_dump.end());

    std::ofstream out(args.out_path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "failed to open output file: %s\n", args.out_path.c_str());
        return 1;
    }

    JsonWriter j(out);
    j.begin_object();

    j.key("frames_run"); j.value(static_cast<uint64_t>(args.frames));
    j.key("iso_path"); j.value(args.iso_path);

    j.key("ram_cartridge");
    j.begin_object();
    j.key("type"); j.value(args.ram_cart);
    j.key("capacity_bytes"); j.value(static_cast<uint64_t>(
        ram_cart_4m != nullptr ? 4u * 1024u * 1024u :
        ram_cart_1m != nullptr ? 1024u * 1024u : 0u));
    bool edge_verified = false;
    if (ram_cart_4m != nullptr || ram_cart_1m != nullptr) {
        const size_t bank_size = ram_cart_4m != nullptr ? 2u * 1024u * 1024u : 512u * 1024u;
        std::vector<uint8_t> bytes(bank_size * 2u);
        if (ram_cart_4m != nullptr) {
            ram_cart_4m->DumpRAM(std::span<uint8_t, 4u * 1024u * 1024u>(bytes.data(), bytes.size()));
        } else {
            ram_cart_1m->DumpRAM(std::span<uint8_t, 1024u * 1024u>(bytes.data(), bytes.size()));
        }
        constexpr uint8_t expected[8] = {0x19, 0x28, 0x37, 0x46, 0x55, 0x64, 0x73, 0x82};
        edge_verified = true;
        for (size_t i = 0u; i < 8u; ++i) {
            if (bytes[bank_size - 4u + i] != expected[i]) edge_verified = false;
        }
    }
    j.key("bank_edge_verified"); j.value(edge_verified);
    j.end_object();

    j.key("backup_memory");
    j.begin_object();
    j.key("enabled"); j.value(!args.backup_ram_path.empty());
    j.key("path"); j.value(args.backup_ram_path);
    if (!args.backup_ram_path.empty()) {
        auto& backup = saturn->mem.GetInternalBackupRAM();
        j.key("header_valid"); j.value(backup.IsHeaderValid());
        j.key("size"); j.value(static_cast<uint64_t>(backup.Size()));
        j.key("block_size"); j.value(static_cast<uint64_t>(backup.GetBlockSize()));
        j.key("total_blocks"); j.value(static_cast<uint64_t>(backup.GetTotalBlocks()));
        j.key("used_blocks"); j.value(static_cast<uint64_t>(backup.GetUsedBlocks()));
        j.key("files");
        j.begin_array();
        for (const auto& info : backup.List()) {
            j.begin_object();
            j.key("filename"); j.value(info.header.filename);
            j.key("comment"); j.value(info.header.comment);
            j.key("language"); j.value(static_cast<uint64_t>(info.header.language));
            j.key("date"); j.value(static_cast<uint64_t>(info.header.date));
            j.key("size"); j.value(static_cast<uint64_t>(info.size));
            j.key("raw_blocks"); j.value(static_cast<uint64_t>(info.numRawBlocks));
            j.key("blocks"); j.value(static_cast<uint64_t>(info.numBlocks));

            const auto exported = backup.Export(info.header.filename);
            j.key("data_size");
            j.value(static_cast<uint64_t>(exported ? exported->data.size() : 0u));
            j.key("data_hash");
            j.value(exported
                ? fnv1a64(exported->data.data(), exported->data.size())
                : static_cast<uint64_t>(0u));
            j.key("data_prefix");
            j.begin_array();
            if (exported) {
                const size_t prefix = std::min<size_t>(64u, exported->data.size());
                for (size_t i = 0; i < prefix; ++i) {
                    j.value(static_cast<uint64_t>(exported->data[i]));
                }
            }
            j.end_array();
            j.end_object();
        }
        j.end_array();
    } else {
        j.key("files");
        j.begin_array();
        j.end_array();
    }
    j.end_object();

    // External cartridge image is a separate, copy-on-write medium. Never
    // mistake internal backup_memory state for a cartridge operation.
    j.key("backup_cartridge");
    j.begin_object();
    j.key("enabled"); j.value(backup_cart != nullptr);
    j.key("path"); j.value(args.backup_cart_path);
    if (backup_cart != nullptr) {
        auto& image = backup_cart->GetBackupMemory();
        const auto bytes_after = image.ReadAll();
        const uint64_t after_hash = fnv1a64(bytes_after.data(), bytes_after.size());
        j.key("copy_on_write"); j.value(true);
        j.key("header_valid"); j.value(image.IsHeaderValid());
        j.key("size"); j.value(static_cast<uint64_t>(image.Size()));
        j.key("block_size"); j.value(static_cast<uint64_t>(image.GetBlockSize()));
        j.key("total_blocks"); j.value(static_cast<uint64_t>(image.GetTotalBlocks()));
        j.key("used_blocks"); j.value(static_cast<uint64_t>(image.GetUsedBlocks()));
        j.key("raw_hash_before"); j.value(backup_cart_before_hash);
        j.key("raw_hash_after"); j.value(after_hash);
        j.key("guest_changed_cart_memory"); j.value(backup_cart_before_hash != after_hash);
        j.key("files");
        j.begin_array();
        for (const auto& info : image.List()) {
            j.begin_object();
            j.key("filename"); j.value(info.header.filename);
            j.key("comment"); j.value(info.header.comment);
            j.key("data_size"); j.value(static_cast<uint64_t>(info.size));
            j.end_object();
        }
        j.end_array();
    } else {
        j.key("files"); j.begin_array(); j.end_array();
    }
    j.end_object();

    // "boot" records what direct-injection did (harness/README.md): the BIOS
    // never finishes loading our disc under Ymir's CD block, so instead of
    // reading BIOS defaults and calling it a pass, every other assertion in
    // harness/tests/*.py should be gated on pc_after_run actually landing
    // inside [load_addr, load_addr + bin_bytes) -- proof our code ran at all.
    j.key("boot");
    j.begin_object();
    j.key("injected"); j.value(true);
    j.key("load_addr"); j.value(static_cast<uint64_t>(load_addr));
    j.key("bin_bytes"); j.value(static_cast<uint64_t>(bin.size()));
    j.key("boot_frames"); j.value(static_cast<uint64_t>(args.boot_frames));
    j.key("pc_after_run"); j.value(static_cast<uint64_t>(pc_after_run));
    j.end_object();

    if (args.skybridge_telemetry_address != 0u) {
        j.key("skybridge_telemetry");
        j.begin_object();
        j.key("version"); j.value(static_cast<uint64_t>(kSkybridgeTelemetryVersion));
        j.key("address"); j.value(static_cast<uint64_t>(args.skybridge_telemetry_address));
        j.key("samples"); j.begin_array();
        for (const auto& sample : skybridge_telemetry_samples) {
            j.begin_object();
            j.key("emulated_frame"); j.value(static_cast<uint64_t>(sample.emulated_frame));
            for (uint32_t word = 0u; word < kSkybridgeTelemetryWords; ++word) {
                j.key(kSkybridgeTelemetryNames[word]);
                j.value(static_cast<uint64_t>(sample.words[word]));
            }
            j.end_object();
        }
        j.end_array();
        j.end_object();
    }

    auto write_scsp_slot = [&](const ScspSlotSnapshot& slot) {
        j.begin_object();
        j.key("index"); j.value(static_cast<uint64_t>(slot.index));
        j.key("active"); j.value(slot.active);
        j.key("key_on"); j.value(slot.key_on);
        j.key("pcm8"); j.value(slot.pcm8);
        j.key("start_address"); j.value(static_cast<uint64_t>(slot.start_address));
        j.key("loop_start"); j.value(static_cast<uint64_t>(slot.loop_start));
        j.key("loop_end"); j.value(static_cast<uint64_t>(slot.loop_end));
        j.key("curr_sample"); j.value(static_cast<uint64_t>(slot.curr_sample));
        j.key("loop_control"); j.value(static_cast<uint64_t>(slot.loop_control));
        j.key("octave"); j.value(static_cast<uint64_t>(slot.octave));
        j.key("fns"); j.value(static_cast<uint64_t>(slot.fns));
        j.key("total_level"); j.value(static_cast<uint64_t>(slot.total_level));
        j.key("direct_send_level"); j.value(static_cast<uint64_t>(slot.direct_send_level));
        j.key("direct_pan"); j.value(static_cast<uint64_t>(slot.direct_pan));
        j.key("ram_half_hash_valid"); j.value(slot.ram_half_hash_valid);
        j.key("ram_half_hashes");
        j.begin_array();
        j.value(slot.ram_half_hashes[0]);
        j.value(slot.ram_half_hashes[1]);
        j.end_array();
        j.end_object();
    };

    j.key("scsp");
    j.begin_object();
    j.key("stream_half_samples"); j.value(static_cast<uint64_t>(4096u));
    j.key("stream_slots");
    j.begin_array();
    for (const auto& slot : scsp_final.stream_slots) {
        write_scsp_slot(slot);
    }
    j.end_array();
    j.key("trace");
    j.begin_array();
    for (const auto& frame_snapshot : scsp_trace) {
        j.begin_object();
        j.key("frame"); j.value(static_cast<uint64_t>(frame_snapshot.frame));
        j.key("stream_slots");
        j.begin_array();
        for (const auto& slot : frame_snapshot.stream_slots) {
            write_scsp_slot(slot);
        }
        j.end_array();
        j.end_object();
    }
    j.end_array();
    j.end_object();

    j.key("input");
    j.begin_object();
    j.key("connected"); j.value(!args.pad_button.empty());
    j.key("button"); j.value(args.pad_button);
    j.key("press_at"); j.value(static_cast<uint64_t>(args.pad_press_at));
    j.key("release_at"); j.value(static_cast<uint64_t>(args.pad_release_at));
    // How many times the SMPC peripheral report callback actually reported
    // the button held — proof the --pad-button schedule reached Ymir's SMPC
    // layer, independent of whether the injected program ever polled it.
    j.key("observed_held_reports"); j.value(static_cast<uint64_t>(g_pad_held_observed));
    j.end_object();

    j.key("vdp1");
    j.begin_object();
    // EWDR: the erase write data register. 0x0000 = transparent erase
    // (VDP2 backdrop shows through); (rgb555 | 0x8000) = opaque erase.
    // See src/hal/vdp1/vdp1.cpp and docs/sega_saturn_hardware/hard/vdp1/hon/p04_14.md.
    j.key("ewdr"); j.value(static_cast<uint64_t>(vdp1_regs.eraseWriteValue));
    j.key("ew_x1"); j.value(static_cast<uint64_t>(vdp1_regs.eraseX1));
    j.key("ew_y1"); j.value(static_cast<uint64_t>(vdp1_regs.eraseY1));
    j.key("ew_x3"); j.value(static_cast<uint64_t>(vdp1_regs.eraseX3));
    j.key("ew_y3"); j.value(static_cast<uint64_t>(vdp1_regs.eraseY3));

    j.key("display_framebuffer_sample");
    j.begin_array();
    {
        auto fb = saturn->VDP.VDP1GetDisplayFramebuffer();
        uint32_t n = std::min<uint32_t>(args.fb_sample_count, static_cast<uint32_t>(fb.size()));
        for (uint32_t i = 0; i < n; i++) {
            j.value(static_cast<uint64_t>(fb[i]));
        }
    }
    j.end_array();
    j.end_object();

    j.key("vdp2");
    j.begin_object();
    j.key("ramctl"); j.value(static_cast<uint64_t>(vdp2_regs.ReadRAMCTL()));
    j.key("bgon"); j.value(static_cast<uint64_t>(vdp2_regs.ReadBGON()));
    j.key("chctlb"); j.value(static_cast<uint64_t>(vdp2_regs.ReadCHCTLB()));
    j.key("rpmd"); j.value(static_cast<uint64_t>(vdp2_regs.ReadRPMD()));
    j.key("ktctl"); j.value(static_cast<uint64_t>(vdp2_regs.ReadKTCTL()));
    j.key("ktaof"); j.value(static_cast<uint64_t>(vdp2_regs.ReadKTAOF()));
    j.key("rptau"); j.value(static_cast<uint64_t>(vdp2_regs.ReadRPTAU()));
    j.key("rptal"); j.value(static_cast<uint64_t>(vdp2_regs.ReadRPTAL()));
    j.key("prir"); j.value(static_cast<uint64_t>(vdp2_regs.ReadPRIR()));
    j.key("plsz"); j.value(static_cast<uint64_t>(vdp2_regs.ReadPLSZ()));
    j.key("mpofr"); j.value(static_cast<uint64_t>(vdp2_regs.ReadMPOFR()));

    j.key("vram_ranges");
    j.begin_array();
    for (const auto& range : args.vdp2_vram_ranges) {
        j.begin_object();
        j.key("base_word"); j.value(static_cast<uint64_t>(range.base_word));
        j.key("words");
        j.begin_array();
        for (uint32_t i = 0; i < range.word_count; i++) {
            j.value(static_cast<uint64_t>(read_be_word(vram2_bytes, range.base_word + i)));
        }
        j.end_array();
        j.end_object();
    }
    j.end_array();
    j.end_object();

    j.end_object();
    out << "\n";

    std::fprintf(stderr, "probe: wrote %s after %u frames\n", args.out_path.c_str(), args.frames);
    return 0;
}
