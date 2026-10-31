-- Controlled rendering study: hold the same recovered original Walk pose
-- used by Saturn. The source coordinates come from verified integer VDF replay.
-- Independent jaw/roar channels are excluded on both sides. Rendering is native.
local directory = 'harness/build/dino_fair_comparison'
local memory = PCSX.getMemPtr()
local function u32(a)
    local p=a-0x80000000
    return tonumber(memory[p])+256*tonumber(memory[p+1])
        +65536*tonumber(memory[p+2])+16777216*tonumber(memory[p+3])
end
local function w16(a,v)
    local p=a-0x80000000; v=v%65536
    memory[p]=v%256; memory[p+1]=math.floor(v/256)
end
local function w32(a,v)
    v=v%4294967296; w16(a,v%65536); w16(a+2,math.floor(v/65536))
end
local field, slot, phase = 0, 0, 0
local last_phase, evaluations = -1, 0
local out=assert(io.open(directory..'/ps1.jsonl','w'))
local poses_file=assert(io.open(directory..'/walk-fixed.bin','rb'))
local poses=poses_file:read('*a'); poses_file:close()
assert(#poses==32*1376*8)
DINO_FAIR_POSE=PCSX.addBreakpoint(0x80012650,'Exec',4,'Freeze walk for visual study',function()
    if field>=600 then
        local objects=u32(0x8003ddc4)
        assert(u32(objects+4)==1376)
        local pointer=u32(objects)
        local pose=poses:sub(phase*1376*8+1,(phase+1)*1376*8)
        ffi.copy(memory+pointer-0x80000000,pose,#pose)
        last_phase=phase
        evaluations=evaluations+1
    end
    return true
end)
DINO_FAIR_CAPTURE=PCSX.Events.createEventListener('GPU::Vsync',function()
    field=field+1
    if field>=600 and field<1240 then
        slot=math.floor((field-600)/80)
        phase=(slot%4)*8
        w16(0x80043ed8,0); w16(0x80043eda,slot<4 and 3072 or 0)
        w16(0x80043edc,0)
        w32(0x8004855c,slot<4 and -6000 or 0)
        w32(0x80048560,0); w32(0x80048564,-50000)
        if (field-600)%80==60 then
            assert(last_phase==phase,'Frozen original walk phase mismatch')
            local stem=string.format('%s/ps1_%d_%02d',directory,slot<4 and 270 or 0,phase)
            local f=assert(io.open(stem..'.vram','wb'))
            f:write(tostring(PCSX.GPU.getVRAM())); f:close()
            local weights={}
            local wp=u32(0x8003ddd0)
            for i=0,8 do weights[#weights+1]=tostring(u32(wp+4*i)) end
            out:write(string.format('{"field":%d,"yaw":%d,"phase":%d,"evaluations":%d,"weights":[%s]}\n',
                field,slot<4 and 270 or 0,last_phase,evaluations,table.concat(weights,',')))
            out:flush()
        end
    elseif field==1240 then
        out:close(); PCSX.pauseEmulator(); print('DINO_FAIR_DONE')
    end
end)

local projection=assert(io.open(directory..'/ps1-projection.jsonl','w'))
local seen={}
DINO_FAIR_GTE={}
for _,address in ipairs({0x80024290,0x800243b4,0x800244dc,0x80024624,0x80024770,0x80024890,0x800249b8,0x80024b4c,0x80024ce0,0x80024d58,0x80024e30,0x80024ea8,0x80024f84,0x80024ff4,0x800250fc,0x8002516c,0x80025270,0x800252e8,0x800253bc,0x80025430,0x80025514,0x800255ac,0x800256f4,0x8002578c,0x8002675c,0x800267c0,0x80026834,0x80026860,0x800268ac,0x80026904,0x80026930,0x80026970,0x800269ac,0x80026a30,0x80026a88,0x80026ae0,0x80026b68,0x80026c08,0x80026c5c,0x80026ca8,0x80026d04,0x80026d9c,0x80026e64,0x80026f1c,0x800284d4,0x800285e4,0x800286f8,0x80028818,0x800292ac,0x80029318,0x80029420,0x80029498,0x80029ea0,0x8002a21c,0x8002a2e8,0x8002a698,0x8002ab2c,0x8002abf0,0x8002ae64,0x8002af88,0x8002b0b0,0x8002b1e4,0x8002b314,0x8002b434,0x8002b55c,0x8002b6dc,0x8002b854,0x8002b8cc,0x8002b9a4,0x8002ba1c,0x8002baf8,0x8002bb6c,0x8002bc5c,0x8002bcd0,0x8002bdc4,0x8002be3c,0x8002bf10,0x8002bf84,0x8002c068,0x8002c0e8,0x8002c234,0x8002c2b4,0x8002c3a8,0x8002c3e4,0x8002ca70,0x8002cefc,0x8002cf74,0x8002d3bc,0x8002d8bc,0x8002d980}) do
    DINO_FAIR_GTE[#DINO_FAIR_GTE+1]=PCSX.addBreakpoint(address,'Exec',4,'Observe original projection',function()
        if field>=600 and field<1240 and (field-600)%80==50 then
            local regs=PCSX.getRegisters()
            local c,v={},{}
            for i=0,31 do c[#c+1]=tostring(tonumber(regs.CP2C.r[i])) end
            for i=0,5 do v[#v+1]=tostring(tonumber(regs.CP2D.r[i])) end
            local key=tostring(slot)..':'..table.concat(c,',')
            if not seen[key] then
                seen[key]=true
                projection:write(string.format('{"slot":%d,"pc":%d,"control":[%s],"vertices":[%s]}\n',slot,address,table.concat(c,','),table.concat(v,',')))
                projection:flush()
            end
        end
        return true
    end)
end
