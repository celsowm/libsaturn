# Water Pool

Water Pool is a tiled pool with a floating ball and a live wave simulation,
after Evan Wallace's WebGL Water. It runs on a stock Sega Saturn at 320×224
through the SDK scene painter. The simulation is `saturn/water_sim.h`; the
ball is cut at the waterline with `sat_water_split_quad_shaded`.

| Saturn pad | Action |
| --- | --- |
| D-pad | Move the ball |
| X + D-pad | Move the ripple cursor |
| A | Drop a ripple at the cursor (hold for a trail) |
| B | Toggle gravity |
| Z + Up / Down | Raise or push the ball down |
| L / R | Orbit the camera |
| C | Cycle the 9×9, 13×13 and 17×17 grids |
| Y | Pause |
| Z + Y | Replay one ripple from a fresh pool, then pause at a fixed tick |
| Start | Reset |

Reflection, refraction and caustics are per-vertex Gouraud approximations,
not per-pixel effects: the surface shading comes from the slope of the water
and the floor and walls from its curvature, so waves show as dips and crests
and light patches move across the tiles. The tile and water colors are tuned
for contrast, not for realism. With the 9×9 grid, Ymir measured
18 ms of scene preparation and about 38 presented frames per second. The 17×17
grid takes about 42 ms and runs near 20 frames per second. These are
emulator figures only; physical-hardware acceptance is pending.

```sh
./build-example.sh water_pool
./run-example.sh water_pool
./harness/run-water-pool.sh
python3 harness/tests/test_water_pool.py
```

The procedural tile and ball are original and covered by the repository's MIT
license. The default local disc embeds a boot payload whose redistribution
rights are pending in the SDK packaging documentation.

[Water rendering](../../docs/WATER_RENDERING.md) · [Water Lab](../water_lab/README.md) · [License](../../LICENSE)
