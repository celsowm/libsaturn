/* The VDP1 command banks: the frame builds in the bank the root does not name, every table and CMDGRDA is an absolute
 * VRAM address, and submit() names the bank with one skip-assign root word. Host memory stands in for VRAM.
 *
 * The second half models the VDP1 over time (registers, the SCU draw-end latch, VBlank) and drives submit() through
 * normal completion, a draw that stalls, a stale EDSR.CEF, a late start, repeated timeouts, recovery and variable
 * pacing. The model fails the test when the CPU writes a bank while a list reads it, when a list is started over
 * another, or when a swap is asked for before the list ended. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/scu/scu.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace vd = saturn::hal::vdp1;

namespace {

constexpr uint32_t kRootBytes = 32u;
constexpr uint32_t kBankBytes = 64u * 1024u + 16u * 1024u;
constexpr uint32_t kCommandBytes = 64u * 1024u;

alignas(4) uint8_t g_vram[512u * 1024u];

uint32_t root_word() {
    uint32_t word;
    std::memcpy(&word, g_vram, sizeof word);
    return word;
}

void protocol_tests();
void auto_tests();
void heavy_tests();
void limit_tests();
void interlace_tests();

}  // namespace

/* The Saturn Technical Bulletin (SCU specifications, item 07) forbids writing the SCU interrupt status register
 * (0x25FE00A4). The VDP1 HAL has no reason to touch it: its source must not name the register at all. */
void check_no_status_write() {
    std::FILE* f = std::fopen("src/hal/vdp1/vdp1.cpp", "r");
    assert(f != nullptr);
    char line[512];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        assert(std::strstr(line, "05FE00A4") == nullptr && std::strstr(line, "25FE00A4") == nullptr);
        assert(std::strstr(line, "0x05FE00") == nullptr && std::strstr(line, "0x25FE00") == nullptr);
    }
    std::fclose(f);
}

int main() {
    vd::set_vram_window(g_vram);

    /* Frame 1: the first bank, at VRAM 32; its table is written in place. */
    vd::begin_frame(12u);
    vd::Command* const first = vd::frame_commands();
    assert(reinterpret_cast<uint8_t*>(first) == g_vram + kRootBytes);
    uint16_t capacity = 0, used = 0, reserved = 0;
    bool overlay = false;
    vd::command_stats(used, capacity, reserved, overlay);
    assert(capacity == 12u && used == 2u);

    vd::PolygonRequest polygon{};
    polygon.color = 0x801Fu;
    assert(vd::push_polygon(polygon) == SAT_OK);
    const uint16_t corners[4] = {0x0001u, 0x0002u, 0x0003u, 0x0004u};
    assert(vd::push_polygon_gouraud(polygon, corners) == SAT_OK);
    /* CMDGRDA of the Gouraud command: bank 0's Gouraud area (32 + 64 KiB) over eight, plus its index. */
    assert(first[3].grda == (kRootBytes + kCommandBytes) / 8u + 0u);
    assert(vd::command_batch_pending() == false);

    /* A batch links its commands by absolute CMDLINK: bank 0 at 32 bytes, so 4 + 4 * index. */
    sat_vdp1_command_batch_t batch{};
    assert(vd::command_batch_reserve(2u, &batch) == SAT_OK);
    assert(vd::frame_command_link(2u) == kRootBytes / 8u + 2u * 4u);
    assert(vd::command_batch_cancel(&batch) == SAT_OK);

    std::uint8_t bank0_before[4 * 32];
    std::memcpy(bank0_before, g_vram + kRootBytes, sizeof bank0_before);

    /* submit() names bank 0 with one 32-bit skip-assign root (CMDCTRL 0x5000, CMDLINK = 32 / 8). */
    vd::submit();
    assert(root_word() == ((0x5000u << 16u) | (kRootBytes / 8u)));

    /* Frame 2 builds in bank 1 (VRAM 32 + 80 KiB) and leaves bank 0, which the VDP1 reads, untouched. */
    vd::begin_frame(12u);
    vd::Command* const second = vd::frame_commands();
    assert(reinterpret_cast<uint8_t*>(second) == g_vram + kRootBytes + kBankBytes);
    assert(std::memcmp(bank0_before, g_vram + kRootBytes, sizeof bank0_before) == 0);
    assert(vd::push_polygon(polygon) == SAT_OK);
    assert(vd::push_polygon_gouraud(polygon, corners) == SAT_OK);
    assert(second[3].grda == (kRootBytes + kBankBytes + kCommandBytes) / 8u + 0u);
    assert(vd::frame_command_link(3u) == (kRootBytes + kBankBytes) / 8u + 3u * 4u);

    /* The capacity is the bank's: 0 or anything above 2048 commands gives the whole bank. */
    vd::begin_frame(0u);
    vd::command_stats(used, capacity, reserved, overlay);
    assert(capacity == 2048u);
    vd::begin_frame(5000u);
    vd::command_stats(used, capacity, reserved, overlay);
    assert(capacity == 2048u);

    check_no_status_write();
    protocol_tests();
    auto_tests();
    heavy_tests();
    limit_tests();
    interlace_tests();
    std::puts("VDP1 command banks: OK");
    return 0;
}

/* ---- the VDP1 model ------------------------------------------------------------------------------------------ */

namespace saturn::hal::scu {
uint16_t ticks_per_frame() { return 0u; }
uint64_t elapsed_ticks() { return 0u; }
uint32_t frame_counter() { return 0u; }
}  // namespace saturn::hal::scu


namespace {

bool g_display_vblank = false;
bool g_display_odd = false;

alignas(4) volatile uint8_t g_regs[0x20];
volatile uint16_t& reg(uint32_t offset) { return *reinterpret_cast<volatile uint16_t*>(g_regs + offset); }

constexpr uint32_t bank_base(int bank) { return kRootBytes + static_cast<uint32_t>(bank) * kBankBytes; }
int bank_of(const void* p) { return static_cast<int>((static_cast<const uint8_t*>(p) - g_vram - kRootBytes) / kBankBytes); }

/* The END command's table address / 8 of the list in `bank`. */
uint16_t end_address(int bank) {
    for (uint32_t i = 0u; i < 2048u; ++i) {
        uint16_t ctrl;
        std::memcpy(&ctrl, g_vram + bank_base(bank) + i * 32u, 2u);
        if ((ctrl & 0x8000u) != 0u) return static_cast<uint16_t>((bank_base(bank) + i * 32u) / 8u);
    }
    return 0u;
}

/* One model step per register poll, i.e. per iteration of any wait in the library. The VDP1 starts a list some polls
 * after the PTMR write (EDSR.CEF keeps the previous list's end until then), reads the bank the root names at the
 * start, runs for `duration` polls (or while `hold`), then sets CEF and the SCU draw-end latch. */
struct Model {
    enum State { Idle, Latency, Drawing } state;
    int latency;
    int duration;
    bool hold;
    int remaining;
    int reading;                 /* bank the running list reads, or -1 */
    uint8_t snapshot[kBankBytes];
    uint32_t tick;
    bool in_irq;
    int violations;
    int started, ended;
    int forced_copr;

    void reset(int latency_polls, int duration_polls) {
        std::memset(const_cast<uint8_t*>(g_regs), 0, sizeof g_regs);
        reg(0x10u) = 0x0002u;    /* CEF: nothing is drawing */
        reg(0x14u) = 0x7777u;    /* COPR: where an earlier list stopped */
        g_display_vblank = false;
        g_display_odd = false;
        state = Idle;
        latency = latency_polls;
        duration = duration_polls;
        hold = false;
        remaining = 0;
        reading = -1;
        tick = 0u;
        in_irq = false;
        violations = 0;
        started = ended = 0;
        forced_copr = -1;
    }
    void violate(const char* what) {
        ++violations;
        std::printf("VIOLATION at tick %u: %s\n", tick, what);
    }
    /* The bank the VDP1 reads must hold what it held when the list started. */
    void check_unmodified() {
        if (reading < 0) return;
        if (std::memcmp(snapshot, g_vram + bank_base(reading), kBankBytes) != 0) {
            violate("CPU wrote the bank the VDP1 is reading");
            std::memcpy(snapshot, g_vram + bank_base(reading), kBankBytes);
        }
    }
    void end_draw() {
        check_unmodified();
        const int bank = reading;
        reading = -1;
        state = Idle;
        reg(0x10u) = 0x0002u;
        /* COPR keeps the END command's table address / 8 until the next list starts (4.8) */
        reg(0x14u) = end_address(bank);
        ++ended;
    }
    void step();
} g_model;

void Model::step() {
    if (in_irq) return;
    ++tick;
    /* Display: a VBlank of 60 polls every 400; its start is the VBlank-IN interrupt. */
    const bool vblank = (tick % 400u) < 60u;
    if (vblank && !g_display_vblank) {
        g_display_vblank = true;
        g_display_odd = !g_display_odd;
        in_irq = true;
        vd::on_vblank_in();
        in_irq = false;
        /* PTMR = 2 (plot at swap, 1-cycle): every field restarts the list, which reads the root when it starts. */
        if (reg(0x04u) == 2u) {
            if (state == Drawing) {
                reading = -1;
                state = Idle;
            }
            state = Latency;
            remaining = 0;
            ++started;
        }
    }
    g_display_vblank = vblank;
    /* A manual framebuffer change asked for while a list runs would show it half drawn. */
    if ((reg(0x02u) & 0x0003u) == 0x0003u) {
        reg(0x02u) = 0u;
        reg(0x10u) = static_cast<uint16_t>((reg(0x10u) & 0x0002u) >> 1u);   /* CEF moves to BEF and clears (4.6) */
        if (state != Idle) violate("swap requested before the list ended");
    }
    /* PTMR = 1 plots now; written while a list runs it restarts that list (VDP1 manual 4.6). */
    if (reg(0x04u) == 1u) {
        reg(0x04u) = 0u;
        if (state != Idle) violate("list started while another one has not ended");
        state = Latency;
        remaining = latency;
        ++started;
    }
    if (state == Latency && remaining-- <= 0) {
        const uint32_t root = *reinterpret_cast<volatile uint32_t*>(g_vram);
        reading = static_cast<int>(((root & 0xFFFFu) * 8u - kRootBytes) / kBankBytes);
        std::memcpy(snapshot, g_vram + bank_base(reading), kBankBytes);
        reg(0x10u) = 0u;         /* CEF is cleared when the list starts */
        reg(0x14u) = static_cast<uint16_t>(bank_base(reading) / 8u);   /* COPR follows the commands (4.8) */
        remaining = duration;
        state = Drawing;
    }
    if (state == Drawing && !hold && remaining-- <= 0) end_draw();
    if (forced_copr >= 0) reg(0x14u) = static_cast<uint16_t>(forced_copr);
}

void poll() { g_model.step(); }

}  // namespace

namespace saturn::hal::vdp2 {
/* Reading TVSTAT takes time like any other register read. */
uint16_t read_tvstat() {
    poll();
    return static_cast<uint16_t>((g_display_vblank ? 0x0008u : 0u) | (g_display_odd ? 0x0002u : 0u));
}
}  // namespace saturn::hal::vdp2

namespace {

/* The CPU side: build one frame (checking where), and submit it. */
vd::Command* build_frame(uint16_t id) {
    vd::begin_frame(0u);
    vd::Command* const cmds = vd::frame_commands();
    if (g_model.reading >= 0 && bank_of(cmds) == g_model.reading) g_model.violate("frame built in the bank being read");
    vd::PolygonRequest polygon{};
    polygon.color = static_cast<uint16_t>(0x8000u | id);
    assert(vd::push_polygon(polygon) == SAT_OK);
    const uint16_t gouraud[4] = {id, id, id, id};
    assert(vd::push_polygon_gouraud(polygon, gouraud) == SAT_OK);
    g_model.check_unmodified();
    return cmds;
}

void begin_scenario(int latency, int duration) {
    g_model.reset(latency, duration);
    vd::set_register_window(g_regs, poll);
    vd::init(320u, 224u, 0u);
    vd::set_frame_mode(true);
}

uint32_t root_link() { return root_word() & 0xFFFFu; }

/* Lets the CPU work for `polls` model steps. */
void cpu_work(uint32_t polls) {
    for (uint32_t i = 0u; i < polls; ++i) g_model.step();
}

void protocol_tests() {
    /* 1. Normal completion: the banks alternate, nothing is dropped, every list starts after the last one ended. */
    begin_scenario(3, 50);
    int previous = -1;
    int before_previous = -1;
    for (uint16_t f = 1u; f <= 8u; ++f) {
        const int bank = bank_of(build_frame(f));
        assert(bank != previous);
        /* Two submissions later the same bank is built again: its END is the same only if the list is the same, and
         * COPR then still names the END of the list drawn in between, never this one. */
        if (f >= 3u) assert(bank == before_previous);
        uint16_t used = 0, capacity_now = 0, reserved = 0;
        bool overlay_now = false;
        vd::command_stats(used, capacity_now, reserved, overlay_now);
        const uint16_t new_end = static_cast<uint16_t>((bank_base(bank) + used * 32u) / 8u);
        before_previous = previous;
        previous = bank;
        vd::submit();
        assert(root_link() == bank_base(bank) / 8u);
        assert(reg(0x14u) != new_end);
        assert(g_model.ended >= f - 1);   /* the list before has ended when this one is published */
        g_model.check_unmodified();
    }
    cpu_work(2000u);
    assert(vd::frames_dropped() == 0u && g_model.violations == 0 && g_model.started == 8 && g_model.ended == 8);

    /* 3. and 4. A stale EDSR.CEF and a late start: the previous list's CEF stays set for `latency` polls after the
     * PTMR write, across VBlanks. The next list is published only after the draw-end of the one before. */
    for (int latency : {200, 900}) {
        begin_scenario(latency, 30);
        for (uint16_t f = 1u; f <= 6u; ++f) {
            const int bank = bank_of(build_frame(f));
            uint16_t used = 0, capacity_now = 0, reserved = 0;
            bool overlay_now = false;
            vd::command_stats(used, capacity_now, reserved, overlay_now);
            const uint16_t new_end = static_cast<uint16_t>((bank_base(bank) + used * 32u) / 8u);
            vd::submit();
            assert(reg(0x14u) != new_end);   /* COPR still names the previous list (or the start value) */
            assert(g_model.ended >= f - 1);
        }
        cpu_work(3000u);
        assert(vd::frames_dropped() == 0u && g_model.violations == 0 && g_model.started == 6 && g_model.ended == 6);
    }

    /* 2., 5., 6. and 8. A list that does not end. The submit times out and publishes nothing: the root, the
     * published bank and the list serial stay, the dropped frame is counted, and every retry builds in the same free
     * bank, never in the one the VDP1 reads. Two timeouts in a row, then the draw ends and the next frame goes out. */
    begin_scenario(3, 20);
    vd::Command* const listed = build_frame(1u);
    const int vdp1_bank = bank_of(listed);
    vd::submit();
    g_model.hold = true;
    const uint32_t timeouts0 = vd::draw_timeouts();
    const uint32_t serial0 = vd::list_serial();
    int cpu_bank = -1;
    for (uint16_t attempt = 0u; attempt < 2u; ++attempt) {
        const int bank = bank_of(build_frame(static_cast<uint16_t>(2u + attempt)));
        assert(bank != vdp1_bank && (cpu_bank < 0 || bank == cpu_bank));
        cpu_bank = bank;
        vd::submit();
        assert(root_link() == bank_base(vdp1_bank) / 8u);
        assert(vd::list_serial() == serial0);
        assert(vd::frames_dropped() == attempt + 1u);
        assert(g_model.started == 1 && g_model.reading == vdp1_bank);
        g_model.check_unmodified();
    }
    assert(vd::draw_timeouts() >= timeouts0 + 2u);
    g_model.hold = false;
    cpu_work(100u);
    assert(g_model.ended == 1);
    const int retry_bank = bank_of(build_frame(4u));
    assert(retry_bank == cpu_bank);
    vd::submit();
    assert(root_link() == bank_base(retry_bank) / 8u);
    cpu_work(200u);
    assert(vd::list_serial() == serial0 + 1u && g_model.started == 2 && g_model.ended == 2);
    assert(vd::frames_dropped() == 2u && g_model.violations == 0);

    /* 7. Variable pacing: lists of random length (several VBlanks), a random start latency, and random CPU time
     * between frames. Nothing is dropped, nothing is overwritten, and the published banks alternate. */
    begin_scenario(0, 10);
    const uint32_t dropped_before = vd::frames_dropped();
    uint32_t seed = 12345u;
    auto next = [&seed](uint32_t modulus) {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8u) % modulus;
    };
    int last_published = -1;
    for (uint16_t f = 1u; f <= 300u; ++f) {
        g_model.latency = static_cast<int>(next(60u));
        g_model.duration = 5 + static_cast<int>(next(1500u));
        const int bank = bank_of(build_frame(f));
        cpu_work(next(800u));
        vd::submit();
        assert(bank != last_published);
        last_published = bank;
        assert(root_link() == bank_base(bank) / 8u);
    }
    cpu_work(3000u);
    assert(vd::frames_dropped() == dropped_before && g_model.violations == 0 && g_model.started == 300 && g_model.ended == 300);

    /* 9. Leaving 1-cycle mode while the auto-plot list has not ended: the switch is not taken as done. The frame
     * stays 1-cycle (no list is started by PTMR), and the switch completes once the list ends. */
    begin_scenario(3, 20);
    vd::set_frame_mode(false);
    const int auto_bank = bank_of(build_frame(1u));
    vd::submit();
    reg(0x10u) = 0u;                  /* the list the swaps keep restarting is still drawing */
    reg(0x14u) = 0x7777u;
    vd::set_frame_mode(true);
    (void)build_frame(2u);
    vd::submit();
    assert(g_model.started == 0 && g_model.violations == 0);
    reg(0x10u) = 0x0002u;             /* it ends: COPR names its END */
    reg(0x14u) = end_address(auto_bank);
    (void)build_frame(3u);
    vd::submit();
    cpu_work(300u);
    assert(g_model.started == 1 && g_model.ended == 1 && g_model.violations == 0);
    vd::set_register_window(nullptr, nullptr);
}

/* 10. 1-cycle (PTMR = 2): the VDP1 restarts the published list at every field and reads the root when it starts.
 * Each bank is written only after the root moved away from it and the VDP1 left it (COPR names the old END). Random
 * pacing over 300 frames, then a list that never ends: the reclaim times out, the frame is dropped with nothing
 * written, and the build resumes once the VDP1 leaves the bank. */
void auto_tests() {
    g_model.reset(0, 40);
    vd::set_register_window(g_regs, poll);
    vd::init(320u, 224u, 0u);
    const uint32_t dropped_at_start = vd::frames_dropped();
    uint32_t seed = 777u;
    auto next = [&seed](uint32_t modulus) {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8u) % modulus;
    };
    int last_published = -1;
    for (uint16_t f = 1u; f <= 300u; ++f) {
        const int bank = bank_of(build_frame(f));
        assert(bank != last_published);
        cpu_work(next(800u));
        vd::submit();
        last_published = bank;
        assert(root_link() == bank_base(bank) / 8u);
        cpu_work(next(300u));
    }
    assert(vd::frames_dropped() == dropped_at_start && g_model.violations == 0 && g_model.started > 300);

    /* A draw that started before the last publish is still held in the bank the next frame would build in. PTMR 0
     * stops the field restarts, and COPR stays inside that bank. The build times out and writes nothing. */
    const int stuck = last_published ^ 1;
    reg(0x04u) = 0u;
    reg(0x10u) = 0u;
    reg(0x14u) = static_cast<uint16_t>(bank_base(stuck) / 8u + 1u);
    g_model.state = Model::Drawing;
    g_model.reading = stuck;
    g_model.remaining = 0;
    std::memcpy(g_model.snapshot, g_vram + bank_base(stuck), kBankBytes);
    g_model.hold = true;
    const uint32_t dropped = vd::frames_dropped();
    for (int attempt = 0; attempt < 2; ++attempt) {
        vd::begin_frame(0u);
        assert(vd::frame_commands() == nullptr);
        assert(vd::frames_dropped() == dropped + static_cast<uint32_t>(attempt) + 1u);
        assert(g_model.violations == 0);
    }
    /* The draw ends and COPR names its END: the bank is free, and the next frame builds in it again. */
    g_model.hold = false;
    cpu_work(50u);
    const int resumed = bank_of(build_frame(1000u));
    assert(resumed == stuck);
    reg(0x04u) = 2u;
    vd::submit();
    cpu_work(200u);
    assert(g_model.violations == 0);
    vd::set_register_window(nullptr, nullptr);
}

/* 11. Near capacity: 2046 commands a list, every other one a Gouraud polygon, random pacing, 40 frames in ON_SUBMIT.
 * Nothing is dropped, nothing is written to a bank the VDP1 reads, and every list ends before the next is published. */
void heavy_tests() {
    begin_scenario(2, 60);
    const uint32_t dropped = vd::frames_dropped();
    uint32_t seed = 4242u;
    auto next = [&seed](uint32_t modulus) {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8u) % modulus;
    };
    vd::PolygonRequest polygon{};
    polygon.color = 0x8003u;
    const uint16_t gouraud[4] = {1u, 2u, 3u, 4u};
    for (uint16_t f = 1u; f <= 40u; ++f) {
        vd::begin_frame(2048u);
        vd::Command* const cmds = vd::frame_commands();
        assert(cmds != nullptr);
        if (g_model.reading >= 0 && bank_of(cmds) == g_model.reading) g_model.violate("frame built in the bank being read");
        uint16_t used = 0, capacity_now = 0, reserved = 0;
        bool overlay_now = false;
        vd::command_stats(used, capacity_now, reserved, overlay_now);
        while (used + 2u < capacity_now) {
            const bool with_gouraud = (used & 1u) != 0u;
            assert((with_gouraud ? vd::push_polygon_gouraud(polygon, gouraud) : vd::push_polygon(polygon)) == SAT_OK);
            vd::command_stats(used, capacity_now, reserved, overlay_now);
        }
        assert(capacity_now == 2048u && used + 2u == capacity_now);
        g_model.check_unmodified();
        cpu_work(next(400u));
        vd::submit();
        g_model.check_unmodified();
    }
    cpu_work(4000u);
    assert(vd::frames_dropped() == dropped && g_model.violations == 0 && g_model.started == 40 && g_model.ended == 40);
}

/* 12. The exact limit. A bank holds 2048 entries: at most 2047 draws, and the END submit writes after them is the
 * last. The next draw is refused with SAT_ERR_CAPACITY and writes nothing, and the root (VRAM 0..31) is outside the
 * bank, so it does not count. */
void limit_tests() {
    begin_scenario(2, 20);
    vd::PolygonRequest polygon{};
    polygon.color = 0x8003u;
    vd::begin_frame(0u);
    const int bank = bank_of(vd::frame_commands());
    uint16_t preamble = 0, capacity = 0, reserved = 0;
    bool overlay = false;
    vd::command_stats(preamble, capacity, reserved, overlay);
    assert(capacity == 2048u);
    /* begin_frame writes the frame's preamble commands (local coordinates, system clip) into the bank first. */
    uint32_t drawn = 0u;
    while (vd::push_polygon(polygon) == SAT_OK) ++drawn;
    assert(drawn == 2047u - preamble);
    uint16_t used = 0;
    vd::command_stats(used, capacity, reserved, overlay);
    assert(used == 2047u && capacity == 2048u);

    uint8_t before[kBankBytes];
    std::memcpy(before, g_vram + bank_base(bank), kBankBytes);
    const uint16_t gouraud[4] = {1u, 2u, 3u, 4u};
    assert(vd::push_polygon(polygon) == SAT_ERR_CAPACITY);
    assert(vd::push_polygon_gouraud(polygon, gouraud) == SAT_ERR_CAPACITY);
    assert(std::memcmp(before, g_vram + bank_base(bank), kBankBytes) == 0);

    vd::submit();
    /* The END is entry 2047: the first END bit in the bank, at the 2048th slot. */
    assert(end_address(bank) == static_cast<uint16_t>((bank_base(bank) + 2047u * 32u) / 8u));
    cpu_work(4000u);
    assert(g_model.violations == 0 && g_model.ended == 1);
}

/* 13. Double interlace (height 448): the VBlank handler publishes the bank at a pair boundary, and the END of the
 * published list is the one the draw-end wait looks for. Before the fix the published END stayed 0, so every
 * wait_draw_end (texture uploads) timed out. */
void interlace_tests() {
    g_model.reset(0, 30);
    vd::set_register_window(g_regs, poll);
    vd::init(704u, 448u, 0u);
    vd::PolygonRequest polygon{};
    polygon.color = 0x8003u;
    vd::begin_frame(0u);
    const int bank = bank_of(vd::frame_commands());
    assert(vd::push_polygon(polygon) == SAT_OK);
    vd::submit();
    /* If the field interrupt never publishes the pending root, do not write
     * either bank or replace that pending list. Retry after fields resume. */
    uint8_t before[2u * kBankBytes];
    std::memcpy(before, g_vram + kRootBytes, sizeof before);
    const uint32_t root_before = root_word();
    const uint32_t dropped_before = vd::frames_dropped();
    const uint32_t timeout_before = vd::draw_timeouts();
    vd::set_register_window(g_regs, nullptr);
    vd::begin_frame(0u);
    assert(vd::frame_commands() == nullptr);
    assert(vd::push_polygon(polygon) != SAT_OK);
    vd::submit();
    assert(vd::frames_dropped() == dropped_before + 1u);
    assert(vd::draw_timeouts() == timeout_before + 1u);
    assert(root_word() == root_before);
    assert(std::memcmp(before, g_vram + kRootBytes, sizeof before) == 0);
    vd::set_register_window(g_regs, poll);
    /* The root names the pending bank only after an odd field. */
    cpu_work(2000u);
    assert(root_link() == bank_base(bank) / 8u);
    /* Its END is the published end: waiting for the draw end sees it and does not time out. */
    const uint32_t timeouts = vd::draw_timeouts();
    assert(vd::wait_draw_end());
    assert(vd::draw_timeouts() == timeouts);
    /* A field boundary alone does not release a bank if COPR still points
     * into it: simulate a draw that remains there past publication. */
    g_model.forced_copr = static_cast<int>(bank_base(bank ^ 1) / 8u);
    std::memcpy(before, g_vram + kRootBytes, sizeof before);
    const uint32_t late_dropped = vd::frames_dropped();
    vd::begin_frame(0u);
    assert(vd::frame_commands() == nullptr);
    assert(vd::frames_dropped() == late_dropped + 1u);
    assert(std::memcmp(before, g_vram + kRootBytes, sizeof before) == 0);
    g_model.forced_copr = -1;
    cpu_work(2000u);
    vd::begin_frame(0u);
    assert(vd::frame_commands() != nullptr);
    assert(bank_of(vd::frame_commands()) != bank);
    vd::set_register_window(nullptr, nullptr);
}

}  // namespace
