LibSaturn Optical Emulators Tester (native boot build)
=======================================================

A tiny Sega Saturn test disc for Satiator and SAROO. It shows how far the boot
gets and tests CD reading with timeouts, so a failure shows up on screen
instead of hanging. It has no game, music or 3D. It is for testing only.

This build uses a new LibSaturn-written boot program (no Jo Engine / YAUL /
SBL boot code), bounded CD waits, and a corrected early hardware init.
Whether it fixes any device is exactly what we are trying to find out.

Two versions:
  minimal  does NOT touch the CD drive after starting. Try this first.
  full     adds CD tests you start by hand (menu below).

Each version comes in two formats. Use ONE at a time, in its own folder:
  bin_cue/  matched .bin + .cue pair
  iso/      single .iso

How to test
-----------
1. Copy a folder (e.g. minimal/bin_cue) into its own game folder:
     Satiator: a folder on the SD card
     SAROO:    its own folder under /SAROO/ISO/
2. Cold boot the Saturn and start the disc from the ODE menu.
3. Note what you see:
     - Stuck on the licensed "SEGA SATURN" screen
     - Black screen
     - The tester screen (white text on black, "HEARTBEAT" counting up)
4. If the tester screen appears, take a photo. Please also note the
   BOOT STAGE number. Then try the full version.

Controls (full version)
-----------------------
  D-pad up/down  choose a test        A  run / retry
  C  show CD register details         B  go back / cancel the long test
Run the tests in order. The 30-second read test is optional.

What to report
--------------
Fill in results.csv (or just send a message) with:
  - Saturn model/region, NTSC or PAL, and ODE (Satiator or SAROO) with
    firmware and menu version
  - which folder/format you used, and cold boot or reset
  - which screen you reached, the BOOT STAGE number, and a photo
  - for full: the result and any TIMEOUTS count of each test

Staying on the licensed screen with no tester screen is a useful result.
Please report it; do not retry many times without noting each attempt.

Check files with SHA256SUMS.txt if something looks corrupted.

Not included: no BIOS, no firmware. The disc contains a Saturn security/area
block whose redistribution rights are not cleared. Please keep this to private
testing and do not post it publicly.
