-- minimal-osc.lua
-- Minimal On-Screen Controller: Play/Pause + Seekbar
-- Green theme (#00F46A)

local assdraw = require 'mp.assdraw'

-- ASS uses BGR: #00F46A → R=0x00 G=0xF4 B=0x6A → &H6AF400&
local CLR_GREEN = "6AF400"
local CLR_TRACK = "444444"
local CLR_BG    = "000000"

-- Layout (all scaled 20% up)
local BAR_H   = 53
local PAD      = 14
local BTN_SZ   = 43
local SEEK_H   = 7
local KNOB_SZ  = 17

-- State
local osd      = mp.create_osd_overlay("ass-events")
local logged   = false

-- Cached layout for hit-testing (set during render)
local L = { bar_y = 1e9, sx = 0, sw = 1 }

local function render()

    local d = mp.get_property_native("osd-dimensions")
    if not d or d.w == 0 or d.h == 0 then return end

    local w  = d.w
    local h  = d.h
    local ml = d.ml or 0
    local mr = d.mr or 0
    local mt = d.mt or 0
    local mb = d.mb or 0

    -- Debug: write all positions once to file
    if not logged then
        logged = true
        local f = io.open("/tmp/osc-debug.txt", "w")
        if f then
            local bar_y_dbg = h - BAR_H
            local cy_dbg    = bar_y_dbg + BAR_H / 2
            local bx_dbg    = PAD
            local by_dbg    = cy_dbg - BTN_SZ / 2
            local sx_dbg    = PAD + BTN_SZ + PAD
            local sw_dbg    = w - sx_dbg - PAD
            local sy_dbg    = cy_dbg - SEEK_H / 2
            f:write(string.format("OSD: w=%d h=%d ml=%d mr=%d mt=%d mb=%d\n", w, h, ml, mr, mt, mb))
            f:write(string.format("BG bar: y=%d to h=%d (full width 0-%d)\n", bar_y_dbg, h, w))
            f:write(string.format("Pause btn: x=%d y=%d size=%d\n", bx_dbg, by_dbg, BTN_SZ))
            f:write(string.format("Seekbar track: x=%d y=%d w=%d h=%d\n", sx_dbg, sy_dbg, sw_dbg, SEEK_H))
            f:write(string.format("Seekbar fill: green from x=%d\n", sx_dbg))
            f:close()
        end
    end

    -- Video rectangle inside the window/OSD canvas
    local vx0 = ml
    local vx1 = w - mr
    local vy0 = mt
    local vy1 = h - mb

    -- Place bar at bottom of *video* (not window)
    local bar_y = vy1 - BAR_H
    local cy    = bar_y + BAR_H / 2

    -- Button position (inside video rect)
    local bx = vx0 + PAD
    local by = cy - BTN_SZ / 2

    -- Seekbar position (inside video rect)
    local sx = bx + BTN_SZ + PAD
    local sw = (vx1 - PAD) - sx
    local sy = cy - SEEK_H / 2

    -- Cache for click handler
    L.bar_y = bar_y
    L.sx    = sx
    L.sw    = sw

    local ass = assdraw.ass_new()

    -- Background bar (semi-transparent black, full width)
    ass:new_event()
    ass:pos(0, 0)
    ass:append("{\\1c&H" .. CLR_BG .. "&\\1a&H80&\\bord0\\shad0\\p1}")
    ass:draw_start()
    ass:rect_cw(vx0, bar_y, vx1, vy1)
    ass:draw_stop()

    -- Play/Pause icon (green)
    local paused = mp.get_property_bool("pause", false)
    ass:new_event()
    ass:pos(0, 0)
    ass:append("{\\1c&H" .. CLR_GREEN .. "&\\bord0\\shad0\\p1}")
    ass:draw_start()
    if paused then
        -- Play triangle
        ass:move_to(bx, by)
        ass:line_to(bx + BTN_SZ, by + BTN_SZ / 2)
        ass:line_to(bx, by + BTN_SZ)
        ass:line_to(bx, by)
    else
        -- Pause bars
        local bw  = BTN_SZ * 0.28
        local gap = BTN_SZ * 0.16
        local x1  = bx + (BTN_SZ - 2 * bw - gap) / 2
        ass:rect_cw(x1, by, x1 + bw, by + BTN_SZ)
        ass:rect_cw(x1 + bw + gap, by, x1 + 2 * bw + gap, by + BTN_SZ)
    end
    ass:draw_stop()

    -- Seekbar track (dim grey)
    ass:new_event()
    ass:pos(0, 0)
    ass:append("{\\1c&H" .. CLR_TRACK .. "&\\bord0\\shad0\\p1}")
    ass:draw_start()
    ass:rect_cw(sx, sy, sx + sw, sy + SEEK_H)
    ass:draw_stop()

    -- Seekbar fill (green)
    local pct = mp.get_property_number("percent-pos", 0) / 100
    local fw  = sw * pct
    if fw > 0 then
        ass:new_event()
        ass:pos(0, 0)
        ass:append("{\\1c&H" .. CLR_GREEN .. "&\\bord0\\shad0\\p1}")
        ass:draw_start()
        ass:rect_cw(sx, sy, sx + fw, sy + SEEK_H)
        ass:draw_stop()
    end

    -- Seekbar knob (green square at current position)
    local kx = sx + fw
    local ky = cy
    local half = KNOB_SZ / 2
    ass:new_event()
    ass:pos(0, 0)
    ass:append("{\\1c&H" .. CLR_GREEN .. "&\\bord0\\shad0\\p1}")
    ass:draw_start()
    ass:rect_cw(kx - half, ky - half, kx + half, ky + half)
    ass:draw_stop()

    osd.data = ass.text
    osd:update()
end

-- Click handler: tap video to pause, tap bar to interact with controls
mp.add_forced_key_binding("MBTN_LEFT", "mosc-click", function()
    local m = mp.get_property_native("mouse-pos")
    if not m or not m.hover then return end

    if m.y < L.bar_y then
        -- Tap on video area: toggle pause
        mp.commandv("cycle", "pause")
        return
    end

    if m.x < L.sx then
        -- Play/pause button
        mp.commandv("cycle", "pause")
    else
        -- Seekbar: seek to tapped position
        local frac = math.max(0, math.min(1, (m.x - L.sx) / L.sw))
        mp.commandv("seek", frac * 100, "absolute-percent", "exact")
    end
end)

-- Start rendering when video loads and keep updating
mp.register_event("file-loaded", function()
    render()
    mp.add_periodic_timer(0.25, render)
end)

-- Re-render on pause state change
mp.observe_property("pause", "bool", function()
    render()
end)
