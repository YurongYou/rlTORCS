local Setup = require 'Setup'
local AgentMaster = require 'async/AgentMaster'
local DriverEvaluation = require 'async/DriverEvaluation'

-- Parse options and perform setup
local setup = Setup(arg)
local opt = setup.opt
-- Start master experiment runner
if opt.mode == 'train' then
	local master = AgentMaster(opt)
	master:start()
elseif opt.mode == 'eval' then
	local eval = DriverEvaluation(opt)
	eval:evaluate()
end