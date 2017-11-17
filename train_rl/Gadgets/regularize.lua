-- local function dxdf( alpha, beta, gamma )
-- 	-- df / dx = (f(x) + beta) * ( gamma - fx - beta ) / (gamma * alpha)
-- 	-- dx / df = (gamma * alpha) / ((f(x) + beta) * ( gamma - fx - beta ))
-- 	return function (fx)
--         return (gamma * alpha) / ((fx + beta) * ( gamma - fx - beta ))
--     end
-- end

local function regularize(action, coord, originImageSpec, gpu)
	local reg = nn.Sigmoid()
	if gpu > 0 then
		reg:cuda()
	end
	local reg_action = action:clone():div(100)


	reg_action = reg:forward(reg_action):clone()
	reg_action = reg_action * 2 - 1
	-- keep the type of tensor consistent
	-- local action_reg_target = reg_action:clone():apply(dxdf(100, 1, 2))
	local reg_coord = nil
	if coord then

		reg_coord = coord:clone():div(100)
		-- local coord_reg_target = reg_coord:clone():zero()

		reg_coord = reg:forward(reg_coord):clone()
		-- xc
		reg_coord[1] = reg_coord[1] * originImageSpec[2][3] - originImageSpec[2][3] / 2
		-- coord_reg_target[1] = dxdf(100, originImageSpec[2][3] / 2, originImageSpec[2][3])(reg_coord[1])
		-- yc
		reg_coord[2] = reg_coord[2] * originImageSpec[2][2] - originImageSpec[2][2] / 2
		-- coord_reg_target[2] = dxdf(100, originImageSpec[2][2] / 2, originImageSpec[2][2])(reg_coord[2])
		-- w
		reg_coord[3] = reg_coord[3] * originImageSpec[2][3]
		-- coord_reg_target[3] = dxdf(100, 0, originImageSpec[2][3])(reg_coord[3])
		-- h
		reg_coord[4] = reg_coord[4] * originImageSpec[2][2]
		-- coord_reg_target[4] = dxdf(100, 0, originImageSpec[2][2])(reg_coord[4])
	end
	return reg_action, reg_coord
end

return regularize
