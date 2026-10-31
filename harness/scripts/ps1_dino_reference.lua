-- Read-only cadence observer for Developer's Demo CD / DINO.EXE.
-- Start before emulation with PCSX-Redux -dofile; see docs/DINO_PS1_REFERENCE.md.
local options = DINO_REFERENCE_OPTIONS or {}
local first = options.first or 3600
local last = options.last or 4080
local output = options.output or 'harness/build/ps1_dino_reference.jsonl'
local memory = PCSX.getMemPtr()
local function u32(address)
    local p = address - 0x80000000
    return tonumber(memory[p]) + 256 * tonumber(memory[p+1])
        + 65536 * tonumber(memory[p+2]) + 16777216 * tonumber(memory[p+3])
end
assert(first >= 1 and last > first)
local stream = assert(io.open(output, 'w'))
local field = 0
PS1_DINO_REFERENCE = PCSX.Events.createEventListener('GPU::Vsync', function()
    field = field + 1
    if field == first then
        -- Pin the observed counter read/increment and phase calculation.
        assert(u32(0x80011270) == 0x97820978, 'Unsupported DINO.EXE counter read')
        assert(u32(0x80011274) == 0x8f8303d0, 'Unsupported DINO.EXE speed read')
        assert(u32(0x80011284) == 0x00021983, 'Unsupported DINO.EXE phase calculation')
        assert(u32(0x8001149c) == 0x24420001, 'Unsupported DINO.EXE counter increment')
        assert(u32(0x8003ddc8) == 9, 'Dino VDF is not loaded')
    end
    if field >= first and field < last then
        local frame = u32(0x80044438)
        local weights = u32(0x8003ddd0)
        assert(weights >= 0x80000000 and weights + 36 <= 0x80200000)
        local values = {}
        for i = 0, 8 do values[#values+1] = tostring(u32(weights + 4*i)) end
        stream:write(string.format(
            '{"field":%d,"guest_frame":%d,"phase":%d,"speed":%d,"weights":[%s]}\n',
            field, frame, frame % 32, u32(0x80043e90), table.concat(values, ',')))
    end
    if field == last then
        stream:close()
        PCSX.pauseEmulator()
        print('PS1_DINO_REFERENCE_DONE ' .. output)
    end
end)
