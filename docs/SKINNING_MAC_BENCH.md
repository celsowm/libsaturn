# Exact Q16 skinning MAC.L experiment

`examples/skin_mac_bench` compares the current `sat_skin_deform_range()` API
with an experimental SH-2 `MAC.L` transform over the same procedural records.
It contains 220 rigid vertices and 723 blended vertices with one, two and three
influences, including exact half-weight pairs. The joint matrices and positions
are generated deterministically; no imported model or game data is used.

The C API remains the reference path. The candidate keeps the current Q16
position conversion, three-term 64-bit matrix accumulation, `+32768` rounding,
translation addition, per-influence Q16 rounding and Q15 blend rounding.
Four matrix poses compare every output from both paths and an independent
quotient/remainder oracle. The qualification records 7,544 vertex comparisons
and rejects any mismatch, incomplete capture, changed checksum or panic.

## Result

On Ymir with the SH-2 cache enabled, O2 and the SDK's 210,540 Hz guest timer,
the C API median was **88,169 ticks** for 32 full-mesh calls. The candidate
median was **122,199 ticks**, **38.60% slower**. Both produced the same checksum;
all 7,544 comparisons passed, with zero panic and zero misaligned accesses.

The comparison gives the candidate favorable setup: it validates the asset
before timing and saves/restores SR.S once per full range. The C API performs
its normal argument checks on each call. Even with that advantage, this
`MAC.L` form loses. It is therefore retained only as a reproducible control;
the production skinning implementation is unchanged. Ymir's instruction model
does not establish a physical Saturn cycle rate.

The inspected candidate emits three memory-operand `MAC.L` instructions for
each transformed axis. The SH-1/SH-2 programming manual documents the multiply
pipeline and contention between consecutive `MAC.L` instructions in
[section 7.5, PDF pages 208–209](sega_saturn_hardware/manuals/sh1-sh2-programming/07-section-7-pipeline-operation.md).
The existing compiler output already uses `DMULS.L` with scheduled carry
handling, so the mnemonic alone did not make this full path cheaper.

## Rejected Q15 batch specialization

A second candidate moved the `pos_shift == 15` decision out of each influence
and into one branch per deformation range. It kept the same fixed-point
products, per-influence rounding and blend arithmetic. On the procedural O2
control, the C API median fell from the earlier 88,169 to 83,220 ticks for 32
calls (5.61%); all 7,544 output comparisons and the checksum still matched.

That isolated result did not carry into the port's O3 build. In matching
12-field Paul Ymir captures, the skin function grew from 474,126 instructions
and 626,424 modeled cycles to 589,984 instructions and 762,064 cycles. The
candidate duplicated the rigid/blended loops for its two specializations. Its
actual 60-second Mednafen CD run matched the existing `FRAME` checkpoints 42,
185 and 328 at seconds 18, 38 and 58; no release cadence gain was shown. The
specialization was removed despite passing the SDK host suite and the exact
procedural comparisons. This is a useful reminder that an O2 microbenchmark
does not establish a win in the port's O3 build or its full renderer.

Private port evidence is in `material-banks-current-hotspots` and
`material-banks-skin-q15-profile`; the actual CD recording is
`material-banks-skin-q15-live-cd`.

## Reproduce

From the SDK root, with the SH-2 toolchain and Ymir probe built:

```sh
python3 harness/scripts/skin_mac_bench.py \
  --bios /path/to/saturn_101a_us.bin \
  --out build/diagnostics/local-skin-mac-o2 \
  --optimization O2
```

The runner preserves the ELF, map, app and disc images, probe output, telemetry,
screenshot, exact command and tool fingerprints. `analysis.json` contains the
validated timer samples and relative change. Use a fresh output directory for
each run. The decoder's malformed and incomplete-capture checks run with the
SDK tools tests.
