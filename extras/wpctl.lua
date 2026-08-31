-- # USAGE
--
-- Requires WirePlumber `wpctl`.
--
-- 1. Copy this file to ~/.config/hypr/plugins/hyprgrass/extras/wpctl.lua
-- 2. add the following to your ~/.config/hypr/hyprland.lua:
--
--    ```lua
--    -- replace USER with your actual username
--    package.path = package.path .. ";/home/USER/.config/hypr/?.lua;/home/USER/.config/hypr/?/init.lua"
--	  if hl.plugin.hyprgrass then
--	  	local wpctl = require("plugins.hyprgrass.extras.wpctl")
--	  	hg.gesture {
--	  		pattern = {kind = "edge", origin = "right", direction = "vertical"}
--	  		action = wpctl.volume_action({
--	  			direction = "vertical",
--	  			-- optional args and their default values:
--	  			flip = false,
--	  			throttle_ms = 100,
--	  		})
--	  	}
--	  end
--    ```
local M = {}

local function xor(a, b) return a == b end

---@class hyprgrass.extras.wpctl.volume_action.Opt
---@field direction "vertical"|"horizontal"
---@field flip boolean? Flip the direction for increasing/decreasing volume. Default value false means swipe right/down to increase.
---@field throttle_ms number? Throttle duration (default 100)

---@param opt hyprgrass.extras.wpctl.volume_action.Opt
---@return {start: function, update: function, finish: function}
function M.volume_action(opt)
	assert(opt.direction == "vertical" or opt.direction == "horizontal",
		"volume_action: invalid direction, must be 'horizontal' or 'vertical'")
	assert(opt.throttle_ms == nil or type(opt.throttle_ms) == "number",
		"volume_action: invalid throttle_ms, must be nil or number")

	local axis = opt.direction == "vertical" and "y" or "x"
	local flip = opt.flip or false
	local throttle_ms = opt.throttle_ms or 100

	local delta_accumulate = 0
	local last_update_time = 0
	local total_delta = 0

	return {
		start = function()
			delta_accumulate = 0
			last_update_time = 0
			total_delta = 0
		end,
		update = function(ev)
			delta_accumulate = delta_accumulate + ev.delta[axis]
			if ev.time_ms - last_update_time < throttle_ms then
				return
			end

			-- With default gesture scale, delta.x and delta.y are a number between
			-- 0 and hl.get_config('gestures.workspace_swipe_distance').
			-- delta=workspace_swipe_distance would mean swiping from top to bottom
			-- or left to right
			local volume_change = delta_accumulate / hl.get_config('gestures.workspace_swipe_distance')
			local change_abs = math.min(1.0, math.abs(volume_change))
			total_delta = total_delta + volume_change

			local sign = xor(flip, volume_change >= 0) and "-" or "+"
			local cmd = string.format("wpctl set-volume @DEFAULT_SINK@ --limit 0.5 %.2f%%%s",
				change_abs * 100, sign)
			os.execute(cmd)
			last_update_time = ev.time_ms
			delta_accumulate = 0
		end,
		finish = function() end,
	}
end

return M
