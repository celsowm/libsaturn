# Dual SH-2 infrastructure example

This example starts the Slave SH-2 through the public low-level API, waits for
its explicit initialization acknowledgment, and then exchanges one-slot
request/response messages while both CPUs continue executing their own loops.
The Slave doubles the Master-provided input (`100 -> 200`) and publishes the
result through the directional mailbox.

Build through the normal project workflow:

```powershell
.\build-example.ps1 dual_sh2
```

The generated artifacts are copied to `build/examples/dual_sh2.*`. To run with
an installed emulator, use the repository launcher:

```powershell
.\run-example.ps1 dual_sh2 -Emulator mednafen -BuildFirst
```

The status is only `SLAVE READY` after the Slave entry has written the shared
state and sent its reverse FRT signal. The default path uses polling for FRT
capture so it does not take ownership of the Master's frame-clock interrupt.
Press START to leave the demo; shutdown is requested before `SSHOFF` is sent.
