#ifndef SATURN_CORE_MEMORY_SECTION_HPP
#define SATURN_CORE_MEMORY_SECTION_HPP

/* Only the SH-2 linker script owns .wram_l. Native tests use ordinary zeroed
 * storage; in particular Mach-O requires a different section syntax. */
#if defined(__sh__)
#define SATURN_WRAM_L __attribute__((section(".wram_l")))
#else
#define SATURN_WRAM_L
#endif

#endif
