# Default Saturn boot template

`ip_yaul_template.bin` is the default template used by the Make/PowerShell and
installed CMake disc builders. Its CD command hook waits for `HIRQ.CMOK` before
reading the result registers. A counter expiration cannot substitute for command
completion. Alternative explicitly selected templates retain their own boot code.

The official CMOK change replaces exactly four bytes at offset `0x0EDE`:

| Instructions | Bytes |
| --- | --- |
| Previous: `dt r7; bf poll` | `47 10 8B FA` |
| Default: `bra poll; nop` | `AF FB 00 09` |

The complete poll at offset `0x0ED8` is:

```asm
poll:
    mov.w @r5, r0       ! HIRQ
    tst #1, r0         ! CMOK
    bf done            ! completion reported
    bra poll
    nop
done:
    ! Existing result-register reads follow unchanged.
```

Relative branches still work when the BIOS hook copies the routine. The security
block, region text, header and all other template bytes are preserved.
`tools/check_ip_bin.py` checks the CMOK instructions for this default template
in addition to its existing header, security and area-code comparisons.

An unresponsive CD block can keep this early boot wait pending indefinitely;
the code does not proceed with an uncompleted command. No frame clock or
application panic renderer is available here. Physical Satiator validation is
tracked in [the Dino investigation](../../docs/DINO_SATIATOR_BOOT_INVESTIGATION.md).
