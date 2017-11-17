-- luacheck: globals log __threadid flog
local moses = require 'moses'
local image = require 'image'
local classic = require 'classic'
local Model = require 'Model'
local CircularQueue = require 'structures/CircularQueue'
local Plot = require 'itorch.Plot'
local nn = require 'nn'
require 'classic.torch'
local threads = require 'threads'

threads.Threads.serialization('threads.sharedserialize')
require 'classic.torch'

local ValidationAgent = classic.class('ValidationAgent')

function ValidationAgent:_init( opt, global_theta, atomic, global_theta_mutex_id, sharedG )
	log.info('creating ValidationAgent')
	-- Environment Setting ---------------------------------------------
	self.model					= Model(opt)
	self.env 					= self.model:getEnv()
	if not self.env.cleanUp then
		self.env.cleanUp = function() end
	end
	self.env.end_step			= 1875
	self.chooseFeature 			= opt.chooseFeature and opt.chooseFeature or -1
	self.globalDeconv 			= opt.globalDeconv and opt.globalDeconv or false
	if self.globalDeconv then
		self.model:setSaliency('guided')
	end
	self.experiment_base_folder = opt.experiments
	self.foldername 			= opt.foldername
	self.name 					= 'VA'
	self.atomic 				= atomic
	self.mutex 					= threads.Mutex(global_theta_mutex_id)
		-- the terminal signal
	self.isTerminal 			= opt.isTerminal
	self.Tensor 				= opt.Tensor
	self.progFreq 				= opt.progFreq
	self.env:training()
	self.obsSpec				= opt.obsSpec
	self.originImageSpec		= self.env:getStateSpec()
	self.gpu					= opt.gpu
	self.constant_sigma			= opt.constant_sigma
	self.discrete				= opt.discrete

	-- End environment Setting -----------------------------------------

	-- Parameters ------------------------------------------------------
		-- get #actions
	self.action_num				= opt.action_num
		-- the adding 4 is the addition attention specifier
	self.action_coord_num 		= self.action_num + 4
	-- End Parameters --------------------------------------------------

	-- The Net ---------------------------------------------------------
	self.sharedG = sharedG
	self.global_theta = global_theta
	self.policyNet = self.model:getNet()
	self.CNN = self.model:getCNN()
	self.afterCNN = self.model:getAfterCNN()

	self.theta = self.policyNet:getParameters()

	self.last_coord				= self.Tensor(4):fill(0)
	self.observationBuffer 		= CircularQueue(opt.histLen, opt.Tensor, self.obsSpec[2])
	self.observationBuffer:clear()
	-- End The Net -----------------------------------------------------

	-- validation infomation -------------------------------------------
	self.valSteps = opt.valSteps
	self.bestValScore = -math.huge
	self.valTotalScores = {}
	self.valAverageScores = {}
	self.valAt = {}
	self.reportWeights = opt.reportWeights
	self.weightSaveCount = 0
	if not paths.dirp(paths.concat(self.experiment_base_folder, self.foldername, 'checkpoints')) then
		paths.mkdir(paths.concat(self.experiment_base_folder, self.foldername, 'checkpoints'))
	end
	-- end validation infomation ---------------------------------------

	-- Resuming --------------------------------------------------------
	if opt.resume then
		local valStatistic = torch.load(paths.concat(self.experiment_base_folder, self.foldername, self.name, 'valStatistic.t7'))
		self.valTotalScores = valStatistic.valTotalScores
		self.valAverageScores = valStatistic.valAverageScores
		self.valAt = valStatistic.valAt
	end
	-- End Resuming ----------------------------------------------------

	-- self.action_record = torch.load(paths.concat("event_leader", opt.game_config, "action_record.t7"))

	log.info('ValidationAgent Initialized')
	classic.strict(self)
end

function ValidationAgent:updataBuffer( rawObservation )
	self.observationBuffer:push(self.model:preprocessing(rawObservation))
end

-- take one action
function ValidationAgent:takeAction( action )
	local reward, rawObservation, terminal

	action = self.discrete and action or action:totable()
	reward, rawObservation, terminal = self.env:step(action)

	self:updataBuffer(rawObservation)

	return reward, terminal, self.observationBuffer:readAll()
end

function ValidationAgent:start()
	self.observationBuffer:clear()
	self.last_coord:zero()

	local rawObservation = self.env:start()
	self:updataBuffer( rawObservation )

	return 0, false, self.observationBuffer:readAll()
end

function ValidationAgent:validate()
	self.theta:copy(self.global_theta)
	local version = self.atomic:get()

	-- local last_coord = self.Tensor(4):fill(0)
	self.policyNet:evaluate()

	local valStepStrFormat = '%0' .. (math.floor(math.log10(self.valSteps)) + 1) .. 'd'
	local valEpisode = 1
	local valEpisodeScore = 0
	local valTotalScore = 0

	local action_mus
	local action_sigmas = self.Tensor(self.action_num)
	-- local coord_mus		= self.Tensor(4)
	-- local coord_sigmas	= self.Tensor(4)

	local action = self.discrete and 5 or self.Tensor(self.action_num)

	-- here the observation is the stacked observations
	local reward, terminal, observation = self:start()
	-- local last_semantic_attention = self.Tensor(self.semantic_attention_dim):fill(1)

	for valStep = 0, self.valSteps do

		if self.isTerminal:get() < 0 then
			self.env:cleanUp()
			log.info('Validation stopped')
			return
		end

		if terminal then
			-- Start a new episode
			valTotalScore = valTotalScore + valEpisodeScore -- Only add to total score at end of episode

			local avgScore = valTotalScore / valEpisode
			log.info('[VAL] Steps: ' .. string.format(valStepStrFormat, valStep) .. '/' .. self.valSteps .. ' | Episode ' .. valEpisode .. ' | Score: ' .. valEpisodeScore .. ' | TotScore: ' .. valTotalScore .. ' | AvgScore: %.2f', avgScore)

			_, terminal, observation = self:start()
			-- last_semantic_attention = self.Tensor(self.semantic_attention_dim):fill(1)
			valEpisode = valEpisode + 1
			valEpisodeScore = 0 -- Reset episode score
		else
			if not self.discrete then
				if self.constant_sigma then
					_, action_mus = table.unpack(self.policyNet:forward(observation))
				else
					_, action_mus, action_sigmas = table.unpack(self.policyNet:forward(observation))
				end
			else
				_, action_mus = table.unpack(self.policyNet:forward(observation))
			end


			if not self.discrete then
				if self.constant_sigma then
					action_sigmas:fill(10)
				end
				for i = 1, self.action_num do
					action[i] = torch.normal(action_mus[i],
							action_sigmas[i])
				end
			else
				action = torch.multinomial(action_mus, 1):squeeze()
			end

			reward, terminal, observation = self:takeAction( action )
			valEpisodeScore = valEpisodeScore + reward
		end
	end

	if not terminal then self.env:cleanUp() end

	if valEpisode == 1 then
		valTotalScore = valEpisodeScore
	end

	log.info('Validation finish @ '.. self.atomic:get() .. ', Using theta @ ' .. version)
	log.info('Total Score: ' .. valTotalScore)
	local valAvgScore = valTotalScore / math.max(valEpisode - 1, 1) -- Only average score for completed episodes in general
	log.info('Average Score: ' .. valAvgScore)
	if valTotalScore > self.bestValScore then
		log.info('New best total score')
		self.bestValScore = valTotalScore
		self:saveWeights('best')
	end

	self.valAt[#self.valAt + 1] = version
	self.valTotalScores[#self.valTotalScores + 1] = valTotalScore
	self.valAverageScores[#self.valAverageScores + 1] = valTotalScore / math.max(valEpisode - 1, 1)
	self:visualiseFilters()

	self:plotTotalScore()
	self:plotAverageScore()

	if self.reportWeights then
		local reports = self:weightsReport()
		for r = 1, #reports do
			log.info(reports[r])
		end
	end
	self:saveCheckPoint()
end

function ValidationAgent:saveWeights( name )
	log.info('Saving ' .. name .. ' weights')
	torch.save(paths.concat(self.experiment_base_folder, self.foldername, name .. '.weights.t7'), self.theta)
end

function ValidationAgent:saveCheckPoint()
	self.weightSaveCount = self.weightSaveCount + 1
	log.info("Saving weights at checkpoint %d @ %d ", self.weightSaveCount, self.atomic:get())
	torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'checkpoints', self.weightSaveCount .. '.t7'), self.theta)
end

function ValidationAgent:visualiseFilters()
	local filters = self.model:getFilters()

	for i, v in ipairs(filters) do
		image.save(paths.concat(self.experiment_base_folder, self.foldername, 'conv_layer_' .. i .. '.png'), v)
	end
end

local pprintArr = function(memo, v)
	return memo .. ', ' .. v
end

function ValidationAgent:weightsReport()
	-- Collect layer with weights
	local weightLayers = self.policyNet:findModules('nn.SpatialConvolution')
	local fcLayers = self.policyNet:findModules('nn.Linear')
	weightLayers = moses.append(weightLayers, fcLayers)

	-- Array of norms and maxima
	local wNorms = {}
	local wMaxima = {}
	local wGradNorms = {}
	local wGradMaxima = {}

	-- Collect statistics
	for l = 1, #weightLayers do
		local w = weightLayers[l].weight:clone():abs() -- Weights (absolute)
		wNorms[#wNorms + 1] = torch.mean(w) -- Weight norms:
		wMaxima[#wMaxima + 1] = torch.max(w) -- Weight max
		w = weightLayers[l].gradWeight:clone():abs() -- Weight gradients (absolute)
		wGradNorms[#wGradNorms + 1] = torch.mean(w) -- Weight grad norms:
		wGradMaxima[#wGradMaxima + 1] = torch.max(w) -- Weight grad max
	end

	-- Create report string table
	local reports = {
		'Weight norms: ' .. moses.reduce(wNorms, pprintArr),
		'Weight max: ' .. moses.reduce(wMaxima, pprintArr),
		'Weight gradient norms: ' .. moses.reduce(wGradNorms, pprintArr),
		'Weight gradient max: ' .. moses.reduce(wGradMaxima, pprintArr)
	}

	return reports
end

function ValidationAgent:plotTotalScore()
	local idx = self.valAt
	local scores = self.valTotalScores

	plot = Plot():line(idx, scores, 'blue', 'score'):draw()
	plot:title('Learning Progress'):redraw()
	plot:xaxis('Global Step'):yaxis('Total Score'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.foldername, 'scores.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'scores.t7'), scores)
end

function ValidationAgent:plotAverageScore()
	local idx = self.valAt
	local scores = self.valAverageScores

	plot = Plot():line(idx, scores, 'blue', 'score'):draw()
	plot:title('Learning Progress'):redraw()
	plot:xaxis('Global Step'):yaxis('Average Score'):redraw()
	plot:legend(true)
	plot:redraw()
	plot:save(paths.concat(self.experiment_base_folder, self.foldername, 'AverageScores.html'))
	-- torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'scores.t7'), scores)
end


function ValidationAgent:saveScores()
	log.info('Saving validation scores @ %d th step', self.atomic:get())
	torch.save(paths.concat(self.experiment_base_folder, self.foldername, self.name, 'valStatistic.t7'), {valTotalScores = self.valTotalScores, valAverageScores = self.valAverageScores, valAt = self.valAt})
	torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'states/sharedG.t7'), self.sharedG)
end

function ValidationAgent:getSaliencyMap(input, action_mus)
	local idx = self.chooseFeature
    local bptarget = torch.Tensor(self.CNN[#self.CNN].output:size()):zero()

    local maxTarget = action_mus
    print(maxTarget)
    local Target = {self.Tensor(1):fill(0), maxTarget}
    self.policyNet:backward(input, Target)

    bptarget[idx]:copy(self.afterCNN.gradInput[idx])
    currentGradOutput = bptarget
    currentModule = self.CNN[#self.CNN]

    for i = #self.CNN - 1, 1, -1 do
        local previousModule = self.CNN[i]
        if currentModule.__typename =="nn.ReLU" then
            currentGradOutput = currentModule:backward(previousModule.output, currentGradOutput)
            currentGradOutput = nn.ReLU():forward(currentGradOutput)
        else
            currentGradOutput = currentModule:backward(previousModule.output, currentGradOutput)
        end
        currentModule.gradInput = currentGradOutput
        currentModule = previousModule
    end
    currentGradOutput = currentModule:backward(input, currentGradOutput)
    self.policyNet:zeroGradParameters()
    -- currentGradOutput[torch.lt(currentGradOutput, 0.1)] = 0
    return torch.abs(currentGradOutput[currentGradOutput:size()[1]]:float())
end

function ValidationAgent:getGlobalSaliencyMap(input, action)
	self.model:salientBackprop()
	local maxTarget = self.Tensor(self.action_num):zero()
	maxTarget[action] = 10
	Target = {self.Tensor(1):fill(0), maxTarget}
	local inputGrads = self.policyNet:backward(input, Target)
	local saliencyMap = inputGrads[inputGrads:size()[1]]:float()
	-- local threshold = 0.05
	-- saliencyMap[torch.lt(saliencyMap, threshold)] = 0
	-- saliencyMap[torch.gt(saliencyMap, threshold)] = 0.2
	self.model:normalBackprop()
	return saliencyMap
end

function ValidationAgent:takeQuickAction( action )
	local _, rawObservation = self.env:step(action)
	-- log.info("step: %d, %s", self.env.nowStep, event_table[2])
	self:updataBuffer(rawObservation)
end

function ValidationAgent:leadTo( step )
	log.info("taking to step %d", step)
	for i = 1, step do
		-- log.info("step %d, rad: %.6f, dist: %.6f, speed: %.6f", i, self.env.ctrl.getRadius(), self.env.ctrl.getDist(), self.env.ctrl.getSpeed())
		-- if self.env.ctrl.getRadius() > 0 and self.env.ctrl.getRadius() < 60 then
		-- 	log.info("big curve")
		-- else
		-- 	log.info("small curve")
		-- end
		-- self.env.ctrl.sleep(100)
		self:takeQuickAction(self.action_record[i])
		if self.isTerminal:get() < 0 then
			self.env:cleanUp()
			log.info('Evaluation stopped')
			return self.observationBuffer:readAll()
		end
	end
	self.env.nowStep = 0
	log.info("Now at step %d", step)
	return self.observationBuffer:readAll()
end

function ValidationAgent:evaluate( display )
	self.theta:copy(self.global_theta)
	log.info('Evaluation mode begin')
	self.env:evaluate()
	self.policyNet:evaluate()

	local action_mus
	local action_sigmas = self.Tensor(self.action_num)
	local coord_mus		= self.Tensor(4)
	local coord_sigmas	= self.Tensor(4)
	local action 		= self.discrete and 5 or self.Tensor(self.action_num)

	-- here the observation is the stacked observations
	-- if self.use_attention is false, attention will be nil
	local reward, terminal, observation = self:start()
	local EpisodeScore = reward
	-- local last_semantic_attention = self.Tensor(self.semantic_attention_dim):fill(1)

	-- local action_record = {}
	local originImage
	local removeCar
	local removeMiddle
	local removeSide

	local car
	local middle
	local side
	local window
	-- local average = self.policyNet:get(1):get(7).output[self.chooseFeature > 0 and self.chooseFeature or 1]
	-- local count = 1
	-- Play one game (episode)
	self:leadTo(1121)
	local step = 0
	while not terminal do
		if self.isTerminal:get() < 0 then
			self.env:cleanUp()
			log.info('Evaluation stopped.')
			return
		end


		-- self.env.ctrl.sleep(500)
		-- local feature = self.policyNet:get(1):get(7).output[self.chooseFeature > 0 and self.chooseFeature or 1]
		-- average = (1 / count) * feature + (1 - 1 / count) * average
		-- window = image.display({image=feature, zoom=30, win=window})
		-- local featureMap = self.policyNet
		-- torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'record', step .. '.t7'), observation)

		-- originImage = self.env:getDisplay(0)
		-- removeSide = self.env:getDisplay(1)
		-- removeMiddle = self.env:getDisplay(2)
		-- removeCar = self.env:getDisplay(3)
		-- car = torch.gt(image['rgb2y'](torch.abs(originImage - removeCar)), 0.1):squeeze()
		-- middle = torch.gt(image['rgb2y'](torch.abs(originImage - removeMiddle)), 0.1):squeeze()
		-- side = torch.gt(image['rgb2y'](torch.abs(originImage - removeSide)), 0.1):squeeze()
		-- if (car:sum() > 10 and middle:sum() > 10 and side:sum() > 10) then
		-- 	count = count + 1
		-- 	log.info("get @ %d, the %d th img", step, count)
		-- 	image.save(paths.concat(self.experiment_base_folder, self.foldername, 'new_record', count .. '_origin.jpg'), originImage)
		-- 	torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'new_record', count .. '_seg.t7'), car + middle * 2 + side * 3)
		-- 	torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'new_record', count .. '.t7'), observation)
		-- end


			if not self.discrete then
				if self.constant_sigma then
					_, action_mus = table.unpack(self.policyNet:forward(observation))
				else
					_, action_mus, action_sigmas = table.unpack(self.policyNet:forward(observation))
				end
			else
				_, action_mus = table.unpack(self.policyNet:forward(observation))
			end

		if not self.discrete then
			if self.constant_sigma then
				action_sigmas:fill(10)
				-- coord_sigmas:fill(10)
			end

			for i = 1, self.action_num do
				action[i] = torch.normal(action_mus[i],
							action_sigmas[i])
			end

		else
			action = torch.multinomial(action_mus, 1):squeeze()
		end
		-- action_record[#action_record + 1] = action
		if display then
			local screen = self.env:getDisplay(0)
			if self.chooseFeature > 0 then

				if screen:size()[1] == 1 then
					local temp = screen
					local size = temp:size()
					size[1] = 3
					screen = torch.Tensor(size):fill(0)
					screen[1]:copy(temp)
					screen[2]:copy(temp)
					screen[3]:copy(temp)
				end
				-- local check = torch.Tensor():copy(screen)
				local activation = image.scale(self:getSaliencyMap(observation, action_mus), display.displayWidth, display.displayHeight)
				local stdv = activation:std()
				local threshold = activation:mean() + 3 * stdv
				-- print(threshold)
				screen[1][torch.gt(activation, threshold)] = 1
				-- print(torch.sum(check - screen))
			elseif self.globalDeconv then
				if screen:size()[1] == 1 then
					local temp = screen
					local size = temp:size()
					size[1] = 3
					screen = torch.Tensor(size):fill(0)
					screen[1]:copy(temp)
					screen[2]:copy(temp)
					screen[3]:copy(temp)
				end
				-- saliencyMap[torch.lt(saliencyMap, threshold)] = 0
				-- saliencyMap[torch.gt(saliencyMap, threshold)] = 0.2
				local _, max_action = torch.max(action_mus, 1)
				-- print(TORCS_action_description[max_action[1]])
				local activation = image.scale(self:getGlobalSaliencyMap(observation, max_action[1]), display.displayWidth, display.displayHeight)
				local stdv = activation:std()
				local threshold = activation:mean() + 3 * stdv
				-- print(threshold)
				-- local check = screen:clone()
				screen[1][torch.gt(activation, threshold)] = 1
				-- print(torch.sum(screen - check))
			end

			local text = "action: "
			if action >= 1 and action <= 3 then
				text = text .. " up   "
			elseif action >= 4 and action <= 6 then
				text = text .. "      "
			else
				text = text .. " down "
			end
			text = text .. ", "
			if action % 3 == 1 then
				text = text .. " left"
			elseif action % 3 == 2 then
				text = text .. " middle"
			else
				text = text .. " right"
			end
			screen = image.drawText(screen, text, 100, 400, {color = {0, 255, 0}, size = 2})
			display:display(self, screen, step)
	    end

		reward, terminal, observation = self:takeAction( action )


		EpisodeScore = EpisodeScore + reward
		step = step + 1
	end
	log.info('Final Score: ' .. EpisodeScore)
	if display then
		display:createVideo()
	end
	-- local file = torch.DiskFile(paths.concat(self.experiment_base_folder, self.foldername, 'record', 'action_record.json'), 'w')
	-- file:writeString(cjson.encode(action_record))
	-- file:close()
	-- torch.save(paths.concat(self.experiment_base_folder, self.foldername, 'record', 'action_record.t7'), action_record)
	-- log.info("save actions to %s", paths.concat(self.experiment_base_folder, self.foldername, 'record', 'action_record.t7'))
end

return ValidationAgent
