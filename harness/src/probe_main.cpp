// probe_main.cpp — GPL-3.0 (see harness/LICENSE / harness/README.md).
//
// Headless probe: boots a libsaturn-built .iso inside Ymir's emulator core,
// runs it for a fixed number of frames, and dumps VDP1/VDP2 register and
// VRAM state to JSON for assertion by harness/tests/*.py.
//
// Deliberately does NOT capture a composited video frame — Ymir's public
// vdp_callbacks.hpp only exposes a "frame finished" notification, not an
// output buffer, so this pass sticks to register/VRAM/framebuffer state,
// which is enough to assert the two PR1 bugs (Yst==Py in the rotation
// parameter table; EWDR left opaque) without depending on Ymir's own
// rendering being correct.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <ymir/media/loader/loader.hpp>
#include <ymir/sys/saturn.hpp>

namespace {

struct VramRange {
    uint32_t base_word;
    uint32_t word_count;
};

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
    bool print_sh2_state = false;    // diagnostic: master SH2 PC/registers to stderr
};

void print_usage() {
    std::fprintf(stderr,
        "usage: probe --iso <path> --bios <path> --bin <path> [--out <path>] [--frames N]\n"
        "             [--boot-frames N] [--dump-vram BASE_WORD:WORD_COUNT ...] [--fb-sample N]\n"
        "             [--dump-wram-high <path>] [--print-sh2-state]\n");
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
        } else if (arg == "--print-sh2-state") {
            out->print_sh2_state = true;
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
    if (std::getenv("PROBE_CONNECT_PAD") != nullptr) {
        auto& port = saturn->SMPC.GetPeripheralPort1();
        if (std::getenv("PROBE_HOLD_START") != nullptr) {
            using ReportCb = void (*)(ymir::peripheral::PeripheralReport&, void*);
            port.SetPeripheralReportCallback(ReportCb([](ymir::peripheral::PeripheralReport& report, void*) {
                report.report.controlPad.buttons = ymir::peripheral::Button::All & ~ymir::peripheral::Button::Start;
            }));
        }
        port.ConnectControlPad();
    }

    bool trace_frames = std::getenv("PROBE_TRACE_FRAMES") != nullptr;
    auto& sh2p = saturn->masterSH2.GetProbe();
    auto run_frames = [&](uint32_t count, uint32_t frame_offset) {
        for (uint32_t i = 0; i < count; i++) {
            saturn->RunFrame();
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

    // -- Phase 2: inject the built program directly into work RAM and jump
    // to it -- what the BIOS would have done had its CD read completed.
    // src/core/crt0.s's _start is self-sufficient (masks IRQs, sets its own
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
    sh2p.R(15) = 0x060FFFFCu; // stack top -- matches .Lstack_top in src/core/crt0.s
    sh2p.PR() = load_addr;    // if _main ever returns, land in crt0's hang loop
    sh2p.PC() = load_addr;    // _start
    sh2p.RefillPipeline();    // PC was changed behind the interpreter's back

    // -- Phase 3: run the injected program for --frames frames.
    run_frames(args.frames, args.boot_frames);

    const uint32_t pc_after_run = sh2p.PC();

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

    j.key("vdp1");
    j.begin_object();
    // EWDR: the erase write data register. 0x0000 = transparent erase
    // (VDP2 backdrop shows through); (rgb555 | 0x8000) = opaque erase.
    // See src/hal/vdp1.cpp and docs/sega_saturn_hardware/hard/vdp1/hon/p04_14.md.
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
