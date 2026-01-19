Mednafen setup

- Run install_mednafen.bat to download and extract Mednafen into tools/emulator/mednafen.
- examples/hello_world/run.bat will auto-use it if SATURN_EMU is not set.
- Mednafen requires Saturn BIOS files (sega_101.bin and/or mpr-17933.bin) in tools/emulator/mednafen/firmware.
- Run install_bios_mpr17933.bat to download mpr-17933.bin after agreement (saves to firmware).
- examples/hello_world/run.bat will generate IP.BIN via tools/ip/make_ip.bat when needed.
- To use another emulator, set SATURN_EMU to its executable path.
