-- Read-only local geometry capture after the original Dino VDF evaluation.
-- Directory must exist. Captured game data is a private, ignored build input.
local options = DINO_MOTION_OPTIONS or {}
local directory = options.directory or 'harness/build/dino_motion'
local first = options.first or 64
local count = options.count or 64
assert(first >= 32 and first % 32 == 0 and count >= 64 and count % 32 == 0)
local memory = PCSX.getMemPtr()
local function u32(address)
    local p = address - 0x80000000
    return tonumber(memory[p]) + 256 * tonumber(memory[p+1])
        + 65536 * tonumber(memory[p+2]) + 16777216 * tonumber(memory[p+3])
end
local stream = assert(io.open(directory .. '/frames.jsonl', 'w'))
local vertices = assert(io.open(directory .. '/vertices.bin', 'wb'))
local previous = first - 1
PS1_DINO_MOTION = PCSX.addBreakpoint(0x80012650, 'Exec', 4,
    'Dino VDF evaluation complete', function()
    local frame = u32(0x80044438)
    if frame >= first and frame < first + count then
        assert(frame == previous + 1, 'Missing or repeated deformation update')
        previous = frame
        assert(u32(0x80013688) == 0x27bdffd0, 'Unsupported VDF evaluator')
        assert(u32(0x80027644) == 0x4b98003d, 'Unsupported GTE deformation operation')
        assert(u32(0x8003ddc8) == 9, 'VDF is not loaded')
        local object_count = u32(0x8003ddb8)
        local objects = u32(0x8003ddc4)
        assert(object_count > 0 and object_count < 32)
        local counts = {}
        for i = 0, object_count - 1 do
            local pointer = u32(objects + 28*i)
            local n = u32(objects + 28*i + 4)
            assert(n > 0 and n < 10000)
            assert(pointer >= 0x80000000 and pointer + n*8 <= 0x80200000)
            vertices:write(ffi.string(memory + pointer - 0x80000000, n*8))
            counts[#counts+1] = tostring(n)
        end
        local weights = {}
        local weight_pointer = u32(0x8003ddd0)
        for i = 0, 8 do weights[#weights+1] = tostring(u32(weight_pointer+4*i)) end
        stream:write(string.format(
            '{"frame":%d,"phase":%d,"counts":[%s],"weights":[%s]}\n',
            frame, frame % 32, table.concat(counts, ','), table.concat(weights, ',')))
        if frame == first then
            local output = assert(io.open(directory .. '/ram.bin', 'wb'))
            output:write(ffi.string(memory, 2097152))
            output:close()
        end
    elseif frame == first + count then
        assert(previous == frame - 1)
        stream:close()
        vertices:close()
        PCSX.pauseEmulator()
        print('PS1_DINO_MOTION_DONE ' .. directory)
    end
    return true
end)
