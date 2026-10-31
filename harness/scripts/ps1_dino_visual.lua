-- Lateral visual reference: controller turn, then exact camera alignment.
-- Only view angles/position are written; animation data and code are untouched.
-- Run using PCSX-Redux -dofile; configure DINO_VISUAL_OPTIONS beforehand.
local options = DINO_VISUAL_OPTIONS or {}
local directory = options.directory or 'harness/build/dino_comparison'
local first = options.first or 800
local last = options.last or 920
local interval = options.interval or 4
local rotate_first = options.rotate_first or 600
local rotate_updates = options.rotate_updates or 32
assert(rotate_first < first and first < last and rotate_updates > 0)
assert(interval >= 1)
local pad = PCSX.SIO0.slots[1].pads[1]
-- PadRead swaps the two controller bytes: physical Square drives PADLleft
-- (0x0080) in this demo's handler; physical D-pad Right translates the model.
local button = PCSX.CONSTS.PAD.BUTTON.SQUARE
local memory = PCSX.getMemPtr()
local function u32(address)
    local p = address - 0x80000000
    return tonumber(memory[p]) + 256 * tonumber(memory[p+1])
        + 65536 * tonumber(memory[p+2]) + 16777216 * tonumber(memory[p+3])
end
local function u16(address)
    local p = address - 0x80000000
    return tonumber(memory[p]) + 256 * tonumber(memory[p+1])
end
local function w16(address, value)
    local p = address - 0x80000000
    memory[p] = value % 256
    memory[p+1] = math.floor(value / 256) % 256
end
local function w32(address, value)
    value = value % 4294967296
    w16(address, value % 65536)
    w16(address+2, math.floor(value / 65536))
end
local stream = assert(io.open(directory .. '/side-fields.jsonl', 'w'))
local field = 0
local rotate_frame = nil
DINO_VISUAL_CAPTURE = PCSX.Events.createEventListener('GPU::Vsync', function()
    field = field + 1
    if field == rotate_first then
        assert(u32(0x80011270) == 0x97820978, 'Unsupported DINO.EXE')
        assert(u32(0x8003ddc8) == 9, 'Dino VDF is not loaded')
        rotate_frame = u32(0x80044438)
        pad.setOverride(button)
    elseif rotate_frame and u32(0x80044438) - rotate_frame >= rotate_updates then
        pad.clearOverride(button)
        rotate_frame = nil
    end
    if field == first - 40 then
        assert(not rotate_frame, 'Controller turn has not finished')
        assert(u16(0x80043ed8) == 0 and u16(0x80043edc) == 0)
        -- The demo's integer smoothing leaves a small residual angular error.
        -- Pin the view to a quarter-turn and frame the whole tail in the viewport.
        w16(0x80043eda, 1024)
        w32(0x8004855c, 6000)
        w32(0x80048564, -50000)
    end
    if field >= first and field < last and (field-first) % interval == 0 then
        assert(u16(0x80043eda) == 1024, 'View moved away from exact profile')
        local filename = string.format('%s/ps1_%04d.vram', directory, field)
        local output = assert(io.open(filename, 'wb'))
        output:write(tostring(PCSX.GPU.getVRAM()))
        output:close()
        local frame = u32(0x80044438)
        stream:write(string.format(
            '{"field":%d,"guest_frame":%d,"phase":%d,"speed":%d,"rotation":[%d,%d,%d],"view_position":[%d,%d,%d]}\n',
            field, frame, frame % 32, u32(0x80043e90),
            u16(0x80043ed8), u16(0x80043eda), u16(0x80043edc),
            u32(0x8004855c), u32(0x80048560), u32(0x80048564)-4294967296))
    end
    if field == last then
        stream:close()
        PCSX.pauseEmulator()
        print('DINO_VISUAL_CAPTURE_DONE ' .. directory)
    end
end)
