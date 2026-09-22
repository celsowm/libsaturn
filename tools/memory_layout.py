"""Authoritative Saturn boot addresses shared by image tooling and tests."""

WORK_RAM_HIGH_BASE = 0x06000000
WORK_RAM_HIGH_END = 0x06100000
MASTER_VECTOR_BASE = 0x06000000
SLAVE_VECTOR_BASE = 0x06000400
SLAVE_STACK_TOP = 0x06001000
MASTER_STACK_TOP = 0x060FFFFC
DUAL_SH2_CONTROL_BASE = 0x06002000
DUAL_SH2_CONTROL_END = 0x06004000
APPLICATION_LOAD_ADDRESS = 0x06004000
SLAVE_ENTRY_VECTOR = 0x94


def validate() -> None:
    assert MASTER_VECTOR_BASE + 0x400 <= SLAVE_VECTOR_BASE
    assert SLAVE_VECTOR_BASE + 0x400 <= SLAVE_STACK_TOP
    assert SLAVE_STACK_TOP < DUAL_SH2_CONTROL_BASE < DUAL_SH2_CONTROL_END
    assert DUAL_SH2_CONTROL_END == APPLICATION_LOAD_ADDRESS


if __name__ == "__main__":
    validate()
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("field", choices=("app_load_hex", "slave_stack_hex"))
    args = parser.parse_args()
    value = APPLICATION_LOAD_ADDRESS if args.field == "app_load_hex" else SLAVE_STACK_TOP
    print(f"{value:08X}")
