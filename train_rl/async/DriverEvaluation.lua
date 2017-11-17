local Display = require 'Display'
local ValidationAgent = require 'async/ValidationAgent'
local Model = require 'Model'
local classic = require 'classic'
local tds = require 'tds'
local threads = require 'threads'
threads.Threads.serialization('threads.sharedserialize')
local DriverEvaluation = classic.class('DriverEvaluation')


function DriverEvaluation:_init(opt)
	local model	= Model(opt)
	local env = model:getEnv()
	local policyNet = model:getNet()
	local theta = policyNet:getParameters()
	local mutex 			= threads.Mutex()
	local gtheta_mutex_id 	= mutex:id()
	local weightsFile = paths.concat(opt.experiments, opt.foldername, 'checkpoints/' .. tostring(8 * 50) .. '.t7')
	if opt.network ~= '' then
		weightsFile = paths.concat(opt.network)
	end

	local weights = torch.load(weightsFile)
	theta:copy(weights)

	local atomic = tds.AtomicCounter()
	opt.isTerminal = tds.AtomicCounter()
	opt.isTerminal:set(1)
	self.isTerminal = opt.isTerminal
	self.validDriver = ValidationAgent(opt, theta, atomic, gtheta_mutex_id)
	self.hasDisplay = false
	if opt.displaySpec then
		self.hasDisplay = true
		self.display = Display(opt)
	end
	classic.strict(self)
end


function DriverEvaluation:evaluate()
	local display = self.hasDisplay and self.display or nil
	local atomic = self.isTerminal
	local ctrlpool = threads.Threads(1)
	ctrlpool:addjob(
	function ()
		local unistd = require "posix.unistd"

		__threadid = 0
		local signal = require 'posix.signal'
		signal.signal(signal.SIGINT, function(signum)
			print("\nSIGINT received")
			print('Ex(c)iting')
			atomic:set(-1)
		end)
	end)
	ctrlpool:addjob(
		function ()
			local temp = package.cpath
			package.cpath = package.cpath .. ";../?.so"
		    ctrl = require 'TORCSctrl'
		    package.cpath = temp

			while true do
				-- print('waiting')
				ctrl.sleep(1)
				if atomic:get() < 0 then break end
			end
		end
	)
	self.validDriver:evaluate(display)
	self.isTerminal:set(-1)
	ctrlpool:synchronize()
	ctrlpool:terminate()
	os.exit(0)
end

return DriverEvaluation