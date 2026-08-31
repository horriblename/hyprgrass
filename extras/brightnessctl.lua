-- # USAGE
--
-- Requires `brightnessctl`.
--
-- 1. Copy this file to ~/.config/hypr/plugins/hyprgrass/extras/brightnessctl.lua
-- 2. add the following to your ~/.config/hypr/hyprland.lua:
--
--    ```lua
--    -- replace USER with your actual username
--    package.path = package.path .. ";/home/USER/.config/hypr/?.lua;/home/USER/.config/hypr/?/init.lua"
--	  if hl.plugin.hyprgrass then
--	  	local brightnessctl = require("plugins.hyprgrass.extras.brightnessctl")
--	  	hg.gesture {
--	  		pattern = {kind = "edge", origin = "right", direction = "vertical"}
--	  		action = brightnessctl.volume_action({
--	  			direction = "vertical",
--	  			-- optional args and their default values:
--	  			min_value = 1, -- minimum brightness, as percentage value
--	  			flip = false,
--	  			throttle_ms = 100,
--	  		})
--	  	}
--	  end
--    ```

local M = {}

local function xor(a, b) return a == b end

---@class hyprgrass.extras.brightnessctl.brightness_action.Opt
---@field direction "vertical" | "horizontal"
---@field min_value integer Minimum value, as percentage (default 1%)
---@field flip boolean? Flip the direction for increasing/decreasing volume. Default value false means swipe right/down to increase.
---@field throttle_ms number? Throttle duration (default 16)

---@param opt hyprgrass.extras.brightnessctl.brightness_action.Opt
---@return {start: function, update: function, finish: function}
function M.brightness_action(opt)
	assert(opt.direction == "vertical" or opt.direction == "horizontal",
		"brightness_action: invalid direction, must be 'horizontal' or 'vertical'")
	assert(opt.throttle_ms == nil or type(opt.throttle_ms) == "number",
		"brightness_action: invalid throttle_ms, must be nil or number")
	assert(opt.min_value == nil or type(opt.min_value) == "number",
		"brightness_action: invalid min_value, must be nil or number")

	local axis = opt.direction == "vertical" and "y" or "x"
	local flip = opt.flip or false
	local throttle_ms = opt.throttle_ms or 16

	local delta_accumulate = 0
	local last_update_time = 0
	local total_delta = 0
	local min_value = opt.min_value and tostring(opt.min_value) .. '%' or "1%"

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
			local cmd = string.format("brightnessctl set --min-value=%s %.2f%%%s",
				min_value, change_abs * 100, sign)
			os.execute(cmd)
			last_update_time = ev.time_ms
			delta_accumulate = 0
		end,
		finish = function() end,
	}
end

return M
