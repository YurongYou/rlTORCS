-- coordinate: xc, yc, w, h
local function imageCropper(input, coordinate, originImageSpec, attentionSpec)
	local xc = coordinate[1]
	xc = xc + originImageSpec[2][3] / 2
	local yc = coordinate[2]
	yc = yc + originImageSpec[2][2] / 2
	local w = coordinate[3]
	local h = coordinate[4]

	w = math.ceil(w)
	h = math.ceil(h)

	-- log.info("xc is " .. xc .. ", yc is " .. yc)
	local x1 = xc - w / 2
	local y1 = yc - h / 2
	local ww = w
	local hh = h

	x1 = math.ceil(x1)
	y1 = math.ceil(y1)

	if x1 < 1 then
		ww = ww - 1 + x1
		x1 = 1
	end
	if x1 + ww - 1 > originImageSpec[2][3] then
		ww = originImageSpec[2][3] + 1 - x1
	end

	if y1 < 1 then
		hh = hh - 1 + y1
		y1 = 1
	end
	if y1 + hh - 1 > originImageSpec[2][2] then
		hh = originImageSpec[2][2] + 1 - y1
	end

	-- log.info("x1: %d, ww: %d, y1: %d, hh: %d", x1, ww, y1, hh)
	if ww >= 1 and hh >= 1 then
		return image.scale(input:narrow(3, x1, ww):narrow(2, y1, hh), attentionSpec[2][3], attentionSpec[2][2])
	else
		return torch.zeros(table.unpack(attentionSpec[2]))
	end
end

return imageCropper