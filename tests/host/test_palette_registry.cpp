#include <cstdio>
#include <cstdlib>

#include "src/graphics/2d/palette/registry.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static void make_palette(uint16_t* palette, uint16_t seed) {
    for (uint16_t i = 0; i < saturn::core::kCramBankEntries; ++i) {
        palette[i] = static_cast<uint16_t>(seed + i);
    }
}

int main() {
    using namespace saturn::core;

    PaletteRegistry state{};
    palette_registry_reset(state);

    uint16_t a[256]{};
    uint16_t b[256]{};
    make_palette(a, 0x100u);
    make_palette(b, 0x200u);

    OK(palette_claim_external(state, 0u, 2u) == SAT_OK);
    OK((state.external_mask & 0x01u) != 0u);

    uint16_t bank_a = 0u;
    bool upload = false;
    OK(palette_acquire_logical(state, a, &bank_a, &upload) == SAT_OK);
    OK(bank_a == 1u);
    OK(upload);
    OK(state.logical_refs[bank_a] == 1u);

    uint16_t bank_a2 = 0u;
    upload = true;
    OK(palette_acquire_logical(state, a, &bank_a2, &upload) == SAT_OK);
    OK(bank_a2 == bank_a);
    OK(!upload);
    OK(state.logical_refs[bank_a] == 2u);

    OK(palette_claim_external(state, 256u, 1u) == SAT_ERR_CAPACITY);

    uint16_t rebound = 0u;
    upload = true;
    OK(palette_rebind_logical(state, bank_a, b, &rebound, &upload) == SAT_OK);
    OK(rebound != bank_a);
    OK(upload);
    OK(state.logical_refs[bank_a] == 1u);
    OK(state.logical_refs[rebound] == 1u);

    OK(palette_release_logical(state, bank_a) == SAT_OK);
    OK((state.logical_mask & static_cast<uint8_t>(1u << bank_a)) == 0u);

    uint16_t rebound_same = 0u;
    upload = false;
    uint16_t c[256]{};
    make_palette(c, 0x300u);
    OK(palette_rebind_logical(state, rebound, c, &rebound_same, &upload) == SAT_OK);
    OK(rebound_same == rebound);
    OK(upload);
    OK(palette_equal(state.logical_palettes[rebound], c));

    PaletteRegistry full{};
    palette_registry_reset(full);
    OK(palette_claim_external(full, 0u, kCramWordCount) == SAT_OK);
    uint16_t no_bank = 0u;
    upload = false;
    OK(palette_acquire_logical(full, a, &no_bank, &upload) == SAT_ERR_CAPACITY);

    std::puts("palette registry: OK");
    return 0;
}
