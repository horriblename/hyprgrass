```lua
-- moving your finger this much the volume from zero to 100 or -100
--
-- e.g. 0.5 means moving my finger by half of the screen increases the
-- volume from 0% to 100%
local VOLUME_MONITOR_RATIO = 0.5

-- "horizontal" or "vertical", should match your pattern.direction
local VOLUME_GESTURE_DIRECTION = "vertical"

---@type file*?
local stream
---@type {x: integer, y: integer}?
local last_pos
local update_already_failed = false
---@param direction "horizontal"|"vertical"
---@param opt {pos: {x: number, y: number}, monitor: {width: number, height: number}}
---@return string? err
local function maybe_adjust_volume(direction, opt)
    if not stream or not last_pos then
        -- the `start` action likely failed, ignore
        return nil
    end

    local delta_px, mon_size
    if direction == "vertical" then
        -- flipped because I want up = increase
        delta_px = last_pos.y - opt.pos.y
        mon_size = opt.monitor.height
    elseif direction == "horizontal" then
        delta_px = opt.pos.x - last_pos.x
        mon_size = opt.monitor.width
    else
        return "unknown direction '" .. direction .. "', must be 'vertical' or 'horizontal'"
    end
    local delta_mon_ratio = delta_px / mon_size

    local volume_change = delta_mon_ratio / VOLUME_MONITOR_RATIO
    local change_abs = math.min(1.0, math.abs(volume_change))

    -- arbitrary threshold of 0.05
    if change_abs > 0.05 then
        local sign = volume_change >= 0 and 1 or -1
        local _, err = stream:write(sign * change_abs * 100, "\n")
        if err then
            return err
        end
        stream:flush()

        last_pos = opt.pos
    end
end

hl.plugin.hyprgrass.gesture {
    pattern = { kind = "edge", origin = "left", direction = "vertical" },
    action = {
        start = function(opt)
            local err
            stream, err = io.popen("/home/py/repo/hyprgrass/build/extras/pa_set_volume_from_stdin", "w")
            if err then
                print("failed to execute pa_set_volume_from_stdin:", err)
                return
            end

            last_pos = opt.pos
        end,
        update = function(opt)
            local err = maybe_adjust_volume(VOLUME_GESTURE_DIRECTION, opt)
            if err and not update_already_failed then
                -- prevents log spam
                update_already_failed = true
                print("could not adjust volume:", err)
            end
        end,
        finish = function()
            last_pos = nil
            update_already_failed = false
            if stream then
                stream:close()
                stream = nil
            end
        end,
    }
}
```
