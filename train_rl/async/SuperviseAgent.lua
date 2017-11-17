-- luacheck: globals log __threadid flog
local classic = require 'classic'
local optim = require 'optim'
local Model = require 'Model'
local threads = require 'threads'
local Plot = require 'itorch.Plot'
local math = require 'math'
local image = require 'image'
require 'modules/sharedRmsProp'
local CircularQueue = require 'structures/CircularQueue'
threads.Threads.serialization('threads.sharedserialize')
local SuperviseAgent = classic.class("SuperviseAgent")

local TINY_EPSILON = 1e-20
local function checkNotNan(t)
	local sum = t:sum()
	local ok = sum == sum
	if not ok then
		log.error('ERROR '.. sum)
	end
	assert(ok)
end

local function checkNotInf(t)
	local sum = t:sum()
	local ok = -math.huge < sum and sum < math.huge
	if not ok then
		log.error('ERROR INF!')
	end
	assert(ok)
end

local function clip( c, x )
	if x < -c then return -c
	elseif x > c then return c
	else return x
	end
end

local regularize = require 'Gadgets/regularize'

-- policyNet is just a net used to copy from
function SuperviseAgent:_init(opt, policyNet, global_theta, atomic, sharedG, global_theta_mutex_id)
	-- test
	self.total 		= 0
	self.num 		= 0
	self.timetic 	= 0

	-- Environment Setting ---------------------------------------------
	self.model					= Model(opt)
	self.env 					= self.model:getEnv()
	self.envName 				= opt.env
	if not self.env.cleanUp then
		self.env.cleanUp = function() end
	end

	self.name 					= opt.foldername
	self.experiment_base_folder = opt.experiments
	self.threadfoldfer 			= ('%02d'):format(__threadid)
	self.atomic 				= atomic
	self.mutex 					= threads.Mutex(global_theta_mutex_id)
	-- the terminal signal
	self.isTerminal 			= opt.isTerminal
	self.Tensor 				= opt.Tensor
	self.total_steps 			= math.floor(opt.steps / opt.threads)
	self.now_step				= 0
	self.progFreq 				= opt.progFreq
	self.env:training()
	self.tic					= 0
	self.originImageSpec		= self.env:getStateSpec()
	self.obsSpec				= opt.obsSpec
	self.attentionSpec			= opt.attentionSpec
	self.verbose 				= opt.verbose
	self.gpu 					= opt.gpu
	self.use_attention  		= opt.use_attention
	self.constant_sigma			= opt.constant_sigma
	self.discrete				= opt.discrete
	-- End environment Setting -----------------------------------------

	-- Training Statistics ---------------------------------------------
	self.statistic 							= {}
	self.statistic.statisticFreq			= math.floor(self.progFreq / 10)
	self.statistic.statisticSteps			= {}
	self.statistic.statisticSteps_Norm		= {}
	self.statistic.statisticSteps_TD		= {}

	self.statistic.lastNormReport			= 0
	self.statistic.normStatistics			= {}
	self.statistic.rewardStatistics			= {}
	self.statistic.TDErrStatistics			= {}

	self.statistic.sigmaStatistics			= {}
	self.statistic.sigmaStatistics.steer	= {}
	self.statistic.sigmaStatistics.accel	= {}

	self.statistic.muStatistics				= {}
	self.statistic.muStatistics.steer		= {}
	self.statistic.muStatistics.accel		= {}

	if self.discrete then
		self.statistic.discrete				= {}
		for i = 1, opt.action_num do
			self.statistic.discrete[i] 		= {}
		end
	end
	-- End Training Statistics -----------------------------------------


	-- Resuming --------------------------------------------------------
	if opt.resume then
		self.now_step = torch.load(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'step.t7'))
		log.info("resume training from %d th steps", self.now_step)
		self.statistic = torch.load(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'statistic.t7'))
	end
	-- End Resuming ----------------------------------------------------

	-- Parameters ------------------------------------------------------
	self.optimiser 				= optim[opt.optimiser]
	self.optimParams = {
		learningRate 			= opt.eta,
		momentum 				= opt.momentum,
		rmsEpsilon 				= opt.rmsEpsilon,
		g 						= sharedG
	}
	self.learningRateStart 		= opt.eta

		-- get #actions
	self.action_num				= opt.action_num
		-- for dicounted total reward
	self.gamma 					= opt.gamma
		-- for entropy
	self.entropyBeta 			= opt.entropyBeta
	self.batchSize				= opt.batchSize
	self.gradClip 				= opt.gradClip
	self.rewardClip 			= opt.rewardClip
	-- End Parameters --------------------------------------------------

	-- The Net ---------------------------------------------------------
	self.global_theta = global_theta
		-- create the agent net from given net
	log.info('Initializing SuperviseAgent')
	self.policyNet = policyNet:clone()
	self.theta, self.dTheta = self.policyNet:getParameters()
	self.dTheta:zero()

		-- for recording and updating
	self.batchIdx 				= 0

	self.values 				= self.Tensor(self.batchSize)
	self.action_mus				= self.Tensor(self.batchSize, self.action_num)
	self.action_sigmas			= self.Tensor(self.batchSize, self.action_num)


	self.rewards 				= self.Tensor(self.batchSize)
			-- will be resized later, observation with hislen
	self.observations 			= self.Tensor(0)
			-- will be resized later, attention with hislen
	self.actions 				= self.Tensor(self.batchSize, self.action_num)
	self.reg_actions			= self.discrete and self.Tensor(self.batchSize) or self.Tensor(self.batchSize, self.action_num)
			-- in discrete, an action is just a number
	self.observationBuffer 		= CircularQueue(opt.histLen, opt.Tensor, self.obsSpec[2])
	self.observationBuffer:clear()

	-- End The Net -----------------------------------------------------
	log.info('SuperviseAgent Initialized')
	classic.strict(self)
end

function SuperviseAgent:start()
	-- update the buffer
	-- use lock to avoid TORCS environment problem
	-- self.mutex:lock()
	self.observationBuffer:push(self.model:preprocessing(self.env:start()))
	-- self.mutex:unlock()

	return 0, false, self.observationBuffer:readAll()
end

function SuperviseAgent:step()
	local reward, rawObservation, terminal = self.env:step()

	-- update the buffer
	self.observationBuffer:push(self.model:preprocessing(rawObservation))

	return reward, terminal, self.observationBuffer:readAll()
end

-- local function dlogpdmu( mus, sigmas, xs )
-- 	-- log.info("in dlopdmu, mus:\n%s, sigmas:\n%s, xs:\n%s", mus, sigmas, xs)

-- 	-- calculate the partial derivative of mu on normal distribution
-- 	-- (x - mu) / sigma^2

-- 	-- You should not compute it like this
-- 	-- return torch.csub(xs, mus):cdiv(sigmas):cdiv(sigmas)
-- 	-- otherwise might get nan!
-- 	-- You should do it like this

-- 	return torch.cdiv(xs, sigmas):cdiv(sigmas):csub(torch.cdiv(mus, sigmas):cdiv(sigmas))
-- end

-- local function dlogpdsigma( mus, sigmas, xs )
-- 	-- log.info("in dlogpdsigma, mus:\n%s, sigmas:\n%s, xs:\n%s", mus, sigmas, xs)

-- 	-- calculate the partial derivative of presigma on normal distribution
-- 	-- (x^2 - 2*x*mu + mu^2 - sigma^2)/sigma^3
-- 	-- = ((x - mu)^2 - sigma^2)/sigma^3
-- 	-- = (x - mu + sigma)(x - mu - sigma)/sigma^3

-- 	return torch.csub(xs, mus):add(sigmas):cmul(torch.csub(xs, mus):csub(sigmas)):cdiv(sigmas):cdiv(sigmas):cdiv(sigmas)
-- end

-- local function dlogpdmu_scaled( mus, sigmas, xs )
-- 	-- log.info("in dlopdmu, mus:\n%s, sigmas:\n%s, xs:\n%s", mus, sigmas, xs)

-- 	-- calculate the partial derivative of mu on normal distribution
-- 	-- (x - mu)

-- 	-- You should not compute it like this
-- 	-- return torch.csub(xs, mus):cdiv(sigmas):cdiv(sigmas)
-- 	-- otherwise might get nan!
-- 	-- You should do it like this

-- 	return torch.csub(xs, mus)
-- end

-- local function dlogpdsigma_scaled( mus, sigmas, xs )
-- 	-- log.info("in dlogpdsigma, mus:\n%s, sigmas:\n%s, xs:\n%s", mus, sigmas, xs)

-- 	-- calculate the partial derivative of presigma on normal distribution
-- 	-- = (x - mu + sigma)(x - mu - sigma)/sigma

-- 	return torch.csub(xs, mus):add(sigmas):cmul(torch.csub(xs, mus):csub(sigmas)):cdiv(sigmas)
-- end


function SuperviseAgent:AccumulateGradients( terminal, observation )
	if self.verbose then log.info("Begin accumulating the gradient") end
	local R = 0

	local vTarget 					= self.Tensor(1)
	local action_muTarget 			= self.Tensor(self.action_num):fill(0)
	local action_sigmaTarget 		= self.Tensor(self.action_num)

	local last_norm = 0
	local this_norm

	local targets
	if not terminal then
		R = self.policyNet:forward(observation)[1][1]
	end
	for i = self.batchIdx, 1, -1 do
		action_muTarget:fill(0)
		action_sigmaTarget:fill(0)
		_, prob = table.unpack(self.policyNet:forward(self.observations[i]))
		prob:add(TINY_EPSILON)
		R = self.rewards[i] + self.gamma * R
		local tdErr = R - self.values[i]
		if (self.now_step - i) % self.statistic.statisticFreq == 0 then
			self.statistic.statisticSteps_TD[#self.statistic.statisticSteps_TD + 1] = self.now_step - i
			self.statistic.TDErrStatistics[#self.statistic.statisticSteps_TD] = tdErr
		end
		if self.verbose then
			log.info("Discounted total reward is %s", R)
			log.info("Estimated value is %s", self.values[i])
			log.info("TDerr is %s", tdErr)
		end
		vTarget[1] = - 0.5 * tdErr

		if self.verbose then
			log.info("action_mus:\n%s, action_sigmas:\n%s, actions:\n%s", self.action_mus[i], self.action_sigmas[i], self.actions[i])
		end
		if not self.discrete then
			if not self.constant_sigma then
				action_sigmaTarget	= dlogpdsigma(self.action_mus[i], self.action_sigmas[i], self.actions[i])
				action_sigmaTarget:mul(-tdErr)
				action_sigmaTarget:add(- self.entropyBeta, self.Tensor(self.action_num):fill(1):cdiv(self.action_sigmas[i]))
			end

			-- calculate the corresponding partial derivative
			action_muTarget		= dlogpdmu(self.action_mus[i], self.action_sigmas[i], self.actions[i])

			-- Negative target for gradient descent
			-- indeed here we want gradient ascent
			action_muTarget:mul(-tdErr)

		else
			-- to prenvent premature convergence, add entropy part into the objective function (make entropy larger)
			-- i.e. gradient ascent for entropy
			-- here calculate the negitative gradient of emtropy
			-- for normal distribution, entropy = ln(sigma * sqrt(2 Pi e))
			-- dEntropy/dSigma = 1 / sigma
			-- here the negative is for gradient ascent (since our update method is gradient descent)

			action_muTarget[self.reg_actions[i]] = -tdErr/prob[self.reg_actions[i]]
			-- Calculate (negative of) gradient of entropy of policy (for gradient descent): -(-logp(s) - 1)
		    local gradEntropy = torch.log(prob) + 1
		    -- Add to target to improve exploration (prevent convergence to suboptimal deterministic policy)
		    action_muTarget:add(self.entropyBeta, gradEntropy)
			-- log.info(action_muTarget)
		end

		-- flog.info(action_muTarget)
		if not self.constant_sigma and not self.discrete then
			targets = { vTarget, action_muTarget, action_sigmaTarget }
		else
			targets = { vTarget, action_muTarget }
		end
		if self.verbose then
			log.info("the entropy's norm is\n%s", self.Tensor(self.action_num):fill(1):cdiv(self.action_sigmas[i]):norm() * self.entropyBeta)
			log.info("vTarget is %s", vTarget)
			log.info("action_muTarget is\n%s", action_muTarget)

			if not self.constant_sigma then log.info("action_sigmaTarget is\n%s", action_sigmaTarget) end
		end

		self.policyNet:backward(self.observations[i], targets)

		if self.verbose then
			this_norm = self.dTheta:norm()
			log.info("This turn dTheta's norm is %s", this_norm - last_norm)
			last_norm = this_norm
		end

	end
	if self.verbose then log.info("Finish accumulating the gradient") end
end

function SuperviseAgent:ApplyGradients()
	if self.verbose then log.info("Begin applying the gradient to global") end
	local dTheta = self.dTheta
    -- checkNotNan(dTheta)
    -- checkNotInf(dTheta)
	if self.verbose then log.info("before clip, dTheta's norm is %s", dTheta:norm()) end
	if self.gradClip > 0 then
	    self.policyNet:gradParamClip(self.gradClip)
	end

	if self.verbose then log.info("dTheta's norm is %s", dTheta:norm()) end
	if self.now_step - self.statistic.lastNormReport > self.statistic.statisticFreq then
		self.statistic.statisticSteps_Norm[#self.statistic.statisticSteps_Norm + 1] = self.now_step
		self.statistic.normStatistics[#self.statistic.statisticSteps_Norm] = dTheta:norm()
		self.statistic.lastNormReport = self.now_step
	end
	local feval = function()
	    -- loss needed for validation stats only which is not computed for async yet, so just 0
	    local loss = 0 -- 0.5 * tdErr ^2
	    return loss, dTheta
	end
	self.optimParams.learningRate = self.learningRateStart * (self.total_steps - self.now_step) / self.total_steps

	self.optimiser(feval, self.global_theta, self.optimParams)

	self.dTheta:zero()
	if self.verbose then log.info("Finish applying the gradient to global") end
end

function SuperviseAgent:learn()
	local total_steps_to_go = self.total_steps - self.now_step
	log.info('SuperviseAgent starting | remaining steps = %d', total_steps_to_go)

	-- luacheck: ignore reward terminal

	local reward, terminal, observation = self:start()


	self.observations:resize(self.batchSize, table.unpack(observation:size():totable()))

	-- Begin training --------------------------------------------------
	self.tic = torch.tic()
	repeat
		self.theta:copy(self.global_theta)
        -- checkNotInf(self.theta)
		self.batchIdx = 0

		repeat
			self.batchIdx = self.batchIdx + 1
			self.observations[self.batchIdx]:copy(observation)
			-- if not self.constant_sigma and not self.discrete then
			-- 	self.values[self.batchIdx], self.action_mus[self.batchIdx], self.action_sigmas[self.batchIdx] = table.unpack(self.policyNet:forward(observation))
			-- else
				self.values[self.batchIdx], self.action_mus[self.batchIdx] = table.unpack(self.policyNet:forward(observation))
			-- end
            -- flog.info(self.action_mus[self.batchIdx])
			-- add tiny eps in case of sigma/mu is zero
			-- self.action_mus[self.batchIdx]:add(TINY_EPSILON)
			-- self.action_sigmas[self.batchIdx]:add(TINY_EPSILON)

			-- if self.constant_sigma then
			-- 	self.action_sigmas[self.batchIdx]:fill(10)
			-- end
			-- if not self.discrete then
			-- 	for i = 1, self.action_num do
			-- 		self.actions[self.batchIdx][i] = torch.normal(self.action_mus[self.batchIdx][i],
			-- 				self.action_sigmas[self.batchIdx][i])
			-- 	end
			-- 	self.reg_actions[self.batchIdx] = regularize(self.actions[self.batchIdx], nil, self.originImageSpec, self.gpu)
			-- else
				-- use supervise action
				self.reg_actions[self.batchIdx] = self.env:getAction()
			-- end

			if self.verbose then
				log.info("before regularized actions are:\n%s", self.actions[self.batchIdx])
				log.info("actions are:\n%s", self.reg_actions[self.batchIdx])
			end

			reward, terminal, observation = self:step()
			if self.rewardClip > 0 then reward = clip(self.rewardClip, reward) end

			self.rewards[self.batchIdx] = reward
			if self.now_step % self.statistic.statisticFreq == 0 then
				local statistic_num 								= #self.statistic.statisticSteps + 1
				self.statistic.statisticSteps[statistic_num]		= self.now_step
				self.statistic.rewardStatistics[statistic_num]		= self.rewards[self.batchIdx]
				-- if self.discrete then
					for i = 1, self.action_num do
						self.statistic.discrete[i][statistic_num] 	= self.action_mus[self.batchIdx][i]
					end
				-- else
				-- 	self.statistic.sigmaStatistics.steer[statistic_num]	= self.action_sigmas[self.batchIdx][1]
				-- 	self.statistic.sigmaStatistics.accel[statistic_num]	= self.action_sigmas[self.batchIdx][2]

				-- 	self.statistic.muStatistics.steer[statistic_num]	= self.action_mus[self.batchIdx][1]
				-- 	self.statistic.muStatistics.accel[statistic_num]	= self.action_mus[self.batchIdx][2]
				-- end
			end

			self:progress(total_steps_to_go)
		until terminal or self.batchIdx == self.batchSize

		self:AccumulateGradients(terminal, observation)

		self:ApplyGradients()

		if self.isTerminal:get() < 0 then
			self:saveState()
			self.env:cleanUp()
			log.info('SuperviseAgent suspends learning at step = %d, remaining = %d', self.now_step, self.total_steps - self.now_step)
			return
		end

		if terminal then
			reward, terminal, observation = self:start()
		end

	until self.now_step >= self.total_steps
	log.info('SuperviseAgent ended learning steps = %d', self.total_steps)
	-- End training ----------------------------------------------------
	self.env:cleanUp()
end

-- supervising the updating norm
function SuperviseAgent:reportNorm()
	local idx = self.statistic.statisticSteps_Norm

	local norms = self.statistic.normStatistics
	plot = Plot():line(idx, norms, 'blue', 'norm'):draw()
	plot:title('dTheta\' Norm Statistics'):redraw()
	plot:xaxis('step'):yaxis('dTheta\'s norm'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'norm.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'norm.t7'), norms)
end

function SuperviseAgent:reportReward()
	local idx = self.statistic.statisticSteps

	local reward = self.statistic.rewardStatistics
	-- print(reward)
	plot = Plot():line(idx, reward, 'blue', 'norm'):draw()
	plot:title('Reward Statistics'):redraw()
	plot:xaxis('step'):yaxis('reward'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'reward.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'reward.t7'), reward)
end

function SuperviseAgent:reportTDErr()
	local idx = self.statistic.statisticSteps_TD

	local TDErr = self.statistic.TDErrStatistics
	plot = Plot():line(idx, TDErr, 'blue', 'TDErr'):draw()
	plot:title('TDErr Statistics'):redraw()
	plot:xaxis('step'):yaxis('TDErr'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'TDErr.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'TDErr.t7'), TDErr)
end

function SuperviseAgent:reportSigma()
	local idx = self.statistic.statisticSteps

	local steer = self.statistic.sigmaStatistics.steer
	local accel = self.statistic.sigmaStatistics.accel
	-- local brake = torch.Tensor(self.sigmaStatistics.brake)

	plot = Plot():line(idx, steer, 'red', 'steer'):line(idx, accel, 'blue', 'accel'):draw()
	plot:title('Sigma Statistics'):redraw()
	plot:xaxis('step'):yaxis('sigma'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'sigma.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'steer_sigma.t7'), steer)
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'accel_sigma.t7'), accel)
end

function SuperviseAgent:reportMu()
	local idx = self.statistic.statisticSteps

	local steer = self.statistic.muStatistics.steer
	local accel = self.statistic.muStatistics.accel
	-- local brake = torch.Tensor(self.muStatistics.brake)

	plot = Plot():line(idx, steer, 'red', 'steer'):line(idx, accel, 'blue', 'accel'):draw()
	plot:title('Mu Statistics'):redraw()
	plot:xaxis('step'):yaxis('mu'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'mu.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'steer_mu.t7'), steer)
	-- torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'accel_mu.t7'), accel)
end

local colors = {'red', 'blue', 'green', 'yellow', 'black', 'brown', 'orange',
      'pink', 'magenta', 'purple'}
local TORCS_action_description = {}
TORCS_action_description[1] = "bk = 0, ac = 1, st =  1"
TORCS_action_description[2] = "bk = 0, ac = 1, st =  0"
TORCS_action_description[3] = "bk = 0, ac = 1, st = -1"
TORCS_action_description[4] = "bk = 0, ac = 0, st =  1"
TORCS_action_description[5] = "bk = 0, ac = 0, st =  0"
TORCS_action_description[6] = "bk = 0, ac = 0, st = -1"
TORCS_action_description[7] = "bk = 1, ac = 0, st =  1"
TORCS_action_description[8] = "bk = 1, ac = 0, st =  0"
TORCS_action_description[9] = "bk = 1, ac = 0, st = -1"

function SuperviseAgent:reportActionProb()
	local idx = self.statistic.statisticSteps
	local plot = Plot()
	for i = 1, self.action_num do
		local action = self.statistic.discrete[i]
		if string.match(self.envName, 'TORCS') then
			plot:line(idx, action, colors[i], TORCS_action_description[i]):redraw()
		else
			plot:line(idx, action, colors[i], tostring(i)):redraw()
		end
	end
	plot:title('Action Statistics'):redraw()
	plot:xaxis('step'):yaxis('prob'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'action.html'))
end

function SuperviseAgent:progress(total_steps_to_go)
	self.atomic:inc()
	self.now_step = self.now_step + 1
	if self.now_step % self.progFreq == 0 then
		local progressPercent = 100 * self.now_step / total_steps_to_go
		local speed = (self.progFreq) / torch.toc(self.tic)
		self.tic = torch.tic()
		log.info('SuperviseAgent | step = %d | %.02f%% | speed = %d / sec | η = %.8f',
		  self.now_step, progressPercent, speed, self.optimParams.learningRate)

		self:reportNorm()
		if self.discrete then
			self:reportActionProb()
		else
			if not self.constant_sigma then
				self:reportSigma()
			end
			self:reportMu()
		end
		self:reportReward()
		self:reportTDErr()

		-- save the state from time to time
		if self.now_step % (self.progFreq * 5) == 0 then self:saveState() end
	end
end


function SuperviseAgent:saveState()
	log.info('save states @ %d th step', self.now_step)
	torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'statistic.t7'), self.statistic)
	torch.save(paths.concat(self.experiment_base_folder, self.name, self.threadfoldfer, 'step.t7'), self.now_step)
end
return SuperviseAgent