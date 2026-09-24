# Isolated build profiles

A generic library cannot depend on any example's validation mode or generated
model headers. The Makefile uses two independent object caches: the library
is keyed by its actual generic C/C++/assembly options (profiling and fault
injection), while each example is keyed by its compiler options, IP profile,
and library variant. Compiled generated-asset objects also live in the example
variant root.

```text
build/objects/library/<library-key>/libsaturn.a
build/objects/examples/<example>/<variant-key>/{examples,build/generated,...}/*.o
build/variants/<example>/<variant-key>/<example>.{elf,bin,iso,cue}
```

Use `make -s print-build-paths EXAMPLE=skybridge_3d` to inspect the
selected roots without the SH-2 compiler. Compare that with
`make -s print-build-paths EXAMPLE=skybridge_3d SAT_SKYBRIDGE_VALIDATION=1`.
`SAT_SKYBRIDGE_PARALLEL_MODE` changes **only** the example and ROM variant;
the game's validation switch enables the reusable library's generic
`SAT_PROFILE_METRICS`. Both the Skybridge and parallel-runtime validation
profiles can reuse the same instrumented library objects when their generic
flags match.

`make EXAMPLE=skybridge_3d all` compiles or reuses the chosen variant,
then exports the selected ELF/BIN/ISO/CUE to the historical
`build/skybridge_3d.*` names. Those exports are not dependencies and never
determine which object archive is linked; switching back to a cached profile
republishes its correct ROM instead of retaining the previously selected ROM.
The Windows `build-example.ps1` wrapper still reads those exported paths.
Use `make clean` to remove all variants.

**Scope:** conversion source staging under `build/generated/<example>/`
is shared across variants of the same example. A changed model-import
signature regenerates those inputs, and each variant has independent compiled
objects. Do not run concurrent builds of the *same example* with different
model-import settings in one checkout: the asset generator and public exported
names are shared. The final ISO tree, link map, object archives, and ROMs
themselves are profile-specific.
