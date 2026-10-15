# AGENTS

## Execution Policy (Mandatory)

- The agent must **always execute** build, run and emulator commands itself.
- Use the project scripts (`build-example.ps1`, `run-example.ps1`, and launchers in `emulators/`) by default.
- Do not ask the user to run commands manually unless there is a real blocker outside the agent's control (for example, an inaccessible OS window, external permissions, or an emulator crash).
- When a real blocker occurs:
  - describe the blocker objectively;
  - try an automatic alternative;
  - only then ask the user for the smallest necessary action.

## Practical Expectation

- Always report clear status: command executed, result and next step.
- Use Mednafen as the default emulator (`run-example.ps1`); use the Ymir harness for automatic RAM, register and screenshot inspection.
- Write all Markdown documentation in English.

## Hardware / Audio Context

- Before changing SCSP, music streaming or related tests, read `docs/SCSP_AUDIO_STREAMING_GUIDE.md`.
- Treat the manuals in `docs/sega_saturn_hardware/` as the primary source for register semantics.
- The downloaded manuals only overview the CD Block: read `docs/sega_saturn_hardware/hard/cdblock/index.md` before changing CD reads, `sat_disc_stream` or anything that times a disc access, and do not present emulator CD timings as hardware facts.
- For audio regressions, prefer observable invariants (SCSP registers, sample position, Sound RAM and underrun/refill counters) over empirical volume/pitch adjustments.

