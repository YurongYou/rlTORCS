-- luacheck: globals log __threadid logroll
local classic = require 'classic'
local threads = require 'threads'
local tds = require 'tds'
-- local signal = require 'posix.signal'
local Model = require 'Model'
-- local AttentionAgent = require 'async/AttentionAgent'
-- local ValidationAgent = require 'async/ValidationAgent'
require 'socket'
threads.Threads.serialization('threads.sharedserialize')

local FINISHED = -99999999

local AgentMaster = classic.class('AgentMaster')

local function checkNotNan(t)
	local sum = t:sum()
	local ok = sum == sum
	if not ok then
		log.error('ERROR '.. sum)
	end
	assert(ok)
end

local function torchSetup(opt)
	local tensorType = opt.tensorType
	local seed = opt.seed
	return function()
		log.info('Setting up Torch7')
		require 'nn'
		pcall(require, 'cutorch')
		require 'modules/GradientRescale'
		-- Set number of BLAS threads to 1 (per thread)
		torch.setnumthreads(1)
		-- Set default Tensor type (float is more efficient than double)
		torch.setdefaulttensortype(tensorType)
		-- Set manual seed (different for each thread to have different experiences)
		log.info("set random seed %d", seed * __threadid)
		torch.manualSeed(seed * __threadid)
		require 'modules/GradualMix'
	end
end

local function threadedFormatter(thread)
	local threadName = thread

	return function(level, ...)
	local msg

	-- luacheck: ignore fn pprint
	if #{...} > 1 then
	    msg = string.format(({...})[1], table.unpack(fn.rest({...})))
	else
	    msg = pprint.pretty_string(({...})[1])
	end

	return string.format("[%s: %s - %s] - %s\n", threadName, logroll.levels[level], os.date("%Y_%m_%d_%X"), msg)
	end
end

local function setupLogging(opt, thread)
	local foldername = opt.foldername
	local threadName = thread
	return function()
		unpack = table.unpack -- TODO: Remove global unpack from dependencies
		-- Create log10 for Lua 5.2
		if not math.log10 then
			math.log10 = function(x)
				return math.log(x, 10)
			end
		end

		require 'logroll'
		local thread = threadName or __threadid
		local start = 0
		if type(thread) == 'number' then
			thread = ('%02d'):format(thread)
			if opt.resume then
				start = torch.load(paths.concat(opt.experiments, foldername, thread, 'step.t7'))
			end
		end

		local file = paths.concat(opt.experiments, foldername, thread, 'log.'.. thread .. '@' .. start ..'.txt')
		flog = logroll.file_logger(file)
		local formatterFunc = threadedFormatter(thread)
		local plog = logroll.print_logger({formatter = formatterFunc})
		log = logroll.combine(flog, plog)
	end
end

function AgentMaster:_init(opt)
	self.opt = opt
	-- Global Net Setting ---------------------------------------------
	self.atomic 			= tds.AtomicCounter()
	opt.atomic 				= self.atomic
	local model 			= Model(opt)
	local policyNet 		= model:getNet()
	log.info('%s', policyNet)
	self.global_theta 		= policyNet:getParameters()
	local sharedG 			= self.global_theta:clone():zero()
	if opt.resume and paths.filep(paths.concat(self.opt.experiments, self.opt.foldername, 'states/sharedG.t7')) then
		sharedG = torch.load(paths.concat(self.opt.experiments, self.opt.foldername, 'states/sharedG.t7'))
		log.info('SharedG resumed')
	end
	-- mutex to prevent nan
	local mutex 			= threads.Mutex()
	local gtheta_mutex_id 	= mutex:id()
	self.use_attention 		= opt.use_attention
	self.use_semantic_attention = opt.use_semantic_attention
	self.isTerminal 		= tds.AtomicCounter()
	self.isTerminal:set(-FINISHED)
	opt.isTerminal			= self.isTerminal
	if self.opt.network ~= '' then
		log.info('Loading specific network weights @ %s', self.opt.network)
		local weights = torch.load(opt.network)
		self.global_theta:copy(weights)
	end
	-- End Global Net Setting -----------------------------------------

	-- Training resume  -----------------------------------------------
	if opt.resume then
		if self.opt.network == '' then
			log.info('Loading last network weights @ %s', paths.concat(opt.experiments, self.opt.foldername, 'last.weights.t7'))
			local weights = torch.load(paths.concat(opt.experiments, self.opt.foldername, 'last.weights.t7'))
			self.global_theta:copy(weights)
		end

		-- calculating total finished steps
		finished_steps = 0
		for thread_id = 1, self.opt.threads do
			finished_steps = finished_steps + torch.load(paths.concat(opt.experiments, opt.foldername, ('%02d'):format(thread_id), 'step.t7'))
		end
		self.atomic:set(finished_steps)
	end
	-- End Training resume  -------------------------------------------

	-- Training control -----------------------------------------------
	local global_theta 		= self.global_theta
	-- local stateFile 		= self.stateFile
	local atomic 			= self.atomic
	local isTerminal 		= self.isTerminal

		-- control thread
	self.controlPool 		= threads.Threads(1)
	self.controlPool:addjob(setupLogging(opt, 'VA'))
	self.controlPool:addjob(torchSetup(opt))
	self.controlPool:addjob(function()
		-- distinguish from thread 1 in the agent pool
		__threadid = 13 -- this number is for convenience of training on server. do not use more than 12 agents. Threadid is used as memory sharing key, should be guaranteed being different between different agents, including validation agent
		local signal = require 'posix.signal'
		local ValidationAgent = require 'async/ValidationAgent'
		validAgent = ValidationAgent(opt, global_theta, atomic, gtheta_mutex_id, sharedG)
		if not opt.noValidation then
			signal.signal(signal.SIGINT, function(signum)
				log.warn('SIGINT received')
				-- log.info('Saving training states')
				-- local globalSteps = atomic:get()
				-- local state = { globalSteps = globalSteps }
				-- torch.save(stateFile, state)
				log.warn('Ex(c)iting!')
				isTerminal:set(FINISHED)
				-- log.info('Saving weights')
				validAgent:saveWeights('last')
				validAgent:saveScores()
				-- validAgent:cleanUp()
				-- atomic:set(FINISHED)
			end)
		end
	end)

	self.controlPool:synchronize()
	-- End Training control -------------------------------------------
	local agentType = 'async.NoAttentionAgent'
	if self.use_attention then
		agentType = 'async.AttentionAgent'
	elseif self.use_semantic_attention then
		agentType = 'async.SemanticAttentionAgent'
	end
	-- Setting Up Agents ----------------------------------------------
	self.pool = threads.Threads(self.opt.threads, function()
	end,
	setupLogging(opt),
	torchSetup(opt),
	function()
		require 'threads'
		local Agent = require(agentType)
		-- opt, policyNet, global_theta, atomic, sharedG
		agent = Agent(opt, policyNet, global_theta, atomic, sharedG, gtheta_mutex_id)
		end
	)
	-- End Setting Up Agents ------------------------------------------
	classic.strict(self)
end

function AgentMaster:start()
	-- Setting Up Staring Point ---------------------------------------
	-- local stepsToGo = math.floor(self.opt.steps / self.opt.threads)
	-- local startStep = 0
	-- if self.opt.network ~= '' and self.state then
	-- 	stepsToGo = math.floor((self.opt.steps - self.state.globalSteps) / self.opt.threads)
	-- 	startStep = math.floor(self.state.globalSteps / self.opt.threads)
	-- 	self.atomic:set(self.state.globalSteps)
	-- 	log.info('Resuming training from step %d', self.state.globalSteps)
	-- end

	-- End Setting Up Staring Point -----------------------------------

	-- Setting Up Signal Handler --------------------------------------
	local atomic = self.atomic
	local opt = self.opt
	local isTerminal = self.isTerminal

	local validator = function()
		local posix = require 'posix'
		local ctrl = require 'TORCSctrl'
		local lastUpdate = -opt.valFreq-10
		while true do
			local globalStep = atomic:get()
			if isTerminal:get() < 0 then return end

			local countSince = globalStep - lastUpdate
			if  countSince > opt.valFreq then
				log.info('Starting validation after %d steps', countSince)
				lastUpdate = globalStep
				local status, err = xpcall(validAgent.validate, debug.traceback, validAgent)
				if not status then
					log.error('%s', err)
					os.exit(128)
				end
				validAgent:saveWeights('last')
				validAgent:saveScores()
			end

			ctrl.sleep(1)
		end
	end

	if not self.opt.noValidation then
		self.controlPool:addjob(validator)
	end
	-- End Setting Up Signal Handler ----------------------------------

	-- Setting Up Drivers ---------------------------------------------
	for i = 1, self.opt.threads do
		self.pool:addjob(function()
				local status, err = xpcall(agent.learn, debug.traceback, agent, nil)
				if not status then
					log.error('%s', err)
					os.exit(128)
				end
			end)
	end
	-- End setting Up Drivers ---------------------------------------------
	self.pool:synchronize()
	self.pool:terminate()
	log.info("All games terminated")
	isTerminal:set(FINISHED)
	self.controlPool:synchronize()
	self.controlPool:terminate()
	log.info("Ctrl terminated")
end

return AgentMaster
