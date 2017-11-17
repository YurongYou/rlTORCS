-- luacheck: ignore math logroll cutorch
require 'logroll'
local _ = require 'moses'
local classic = require 'classic'
local cjson = require 'cjson'

local Setup = classic.class('Setup')

-- Logs and aborts on error
local function abortIf(err, msg)
	if err then
		error(msg)
	end
end

-- Parses command-line options
function Setup:parseOptions(arg)
	-- Detect and use GPU 1 by default

	local cmd = torch.CmdLine()
	-- Base Torch7 options
	cmd:option('-seed', 817, 'Random seed')
	cmd:option('-threads', 4, 'Number of BLAS or async threads')
	cmd:option('-tensorType', 'torch.FloatTensor', 'Default tensor type')
	cmd:option('-gpu', 0, 'GPU device ID (0 to disable)')
	cmd:option('-cudnn', 'false', 'Utilise cuDNN (if available)')

	-- Environment options
	cmd:option('-env', 'rlenvs.Catch', 'Environment class (Lua file to be loaded/rlenv)')
	cmd:option('-zoom', 1, 'Display zoom (requires QT)')
	cmd:option('-mode', 'train', 'Train vs. test mode: train|eval')

	-- State preprocessing options (for visual states)
	cmd:option('-height', 0, 'Resized screen height (0 to disable)')
	cmd:option('-width', 0, 'Resize screen width (0 to disable)')
	-- cmd:option('-attention_h', 0, 'specify the attention height (0 to disable)')
	-- cmd:option('-attention_w', 0, 'specify the attention width (0 to disable)')
	-- cmd:option('-semantic_attention_dim', 32, 'specify the attention dim')
	cmd:option('-colorSpace', '', 'Colour space conversion (screen is RGB): <none>|y|lab|yuv|hsl|hsv|nrgb')

	-- Model options
	cmd:option('-model', 'models.attention', 'Path to Torch nn model')
	cmd:option('-hiddenSize', 512, 'Number of units in the hidden fully connected layer')
	cmd:option('-histLen', 4, 'Number of consecutive states processed/used for backpropagation-through-time') -- DQN standard is 4, DRQN is 10
	cmd:option('-gamma', 0.99, 'Discount rate γ')
	cmd:option('-rewardClip', 1, 'Clips reward magnitude at rewardClip (0 to disable)')
	cmd:option('-tdClip', 1, 'Clips TD-error δ magnitude at tdClip (0 to disable)')

	-- Training options
	cmd:option('-optimiser', 'rmspropm', 'Training algorithm') -- RMSProp with momentum as found in "Generating Sequences With Recurrent Neural Networks"
	cmd:option('-eta', 0.0007, 'Learning rate η') -- Learning rate
	cmd:option('-momentum', 0.99, 'Gradient descent momentum')
	cmd:option('-batchSize', 5, 'Minibatch size')
	cmd:option('-steps', 9000000, 'Training iterations (steps)') -- Time step := consecutive frames treated atomically by the agent
	cmd:option('-gradClip', 10, 'Clips L2 norm of gradients at gradClip (0 to disable)')
	-- Evaluation options
	cmd:option('-progFreq', 1000, 'Interval of steps between reporting progress')
	cmd:option('-reportWeights', 'false', 'Report weight and weight gradient statistics')
	cmd:option('-noValidation', 'false', 'Disable asynchronous agent validation thread') -- TODO: Make behaviour consistent across Master/AsyncMaster
	cmd:option('-valFreq', 25000, 'Interval of steps between validating agent') -- valFreq steps is used as an epoch, hence #epochs = steps/valFreq
	cmd:option('-valSteps', 2500, 'Number of steps to use for validation')
	-- Async options
	-- cmd:option('-async', '', 'Async agent: <none>attention|noattention')
	cmd:option('-rmsEpsilon', 0.1, 'Epsilon for sharedRmsProp')
	cmd:option('-entropyBeta', 0.001, 'Policy entropy regularisation β')
	-- cmd:option('-delay', 0.001, 'Attention delay')
	-- Experiment options
	cmd:option('-experiments', 'experiments', 'Base directory to store experiments')
	cmd:option('-foldername', '', 'foldername of experiment (used to store saved results, defaults to game name)')
	cmd:option('-network', '', 'Saved network weights file to load (weights.t7)')
	cmd:option('-resume', 'false', 'resume from the certain training state')
	cmd:option('-verbose', 'false', 'Log info for every episode (only in train mode)')
	cmd:option('-record', 'false', 'Record screen (only in eval mode)')
	cmd:option('-server', 'false', 'if running on a server')
	cmd:option('-attention_delay', 0.01, 'for training stablization')
	cmd:option('-group', 0, 'different traning group')
	-- cmd:option('-use_attention', 'false', 'whether use attention')
	-- cmd:option('-use_semantic_attention', 'false', 'whether use semantic attention')
	cmd:option('-constant_sigma','false','whether let sigma be constant')
	cmd:option('-discrete', 'false', 'whether choose discrete action mode')

	-- ALEWrap options
	cmd:option('-game', '', 'Name of Atari ROM (stored in "roms" directory)')
	cmd:option('-fullActions', 'false', 'Use full set of 18 actions')
	cmd:option('-actRep', 4, 'Times to repeat action') -- Independent of history length
	cmd:option('-randomStarts', 30, 'Max number of no-op actions played before presenting the start of each training episode')
	cmd:option('-poolFrmsType', 'max', 'Type of pooling over previous emulator frames: max|mean')
	cmd:option('-poolFrmsSize', 2, 'Number of emulator frames to pool over')
	cmd:option('-lifeLossTerminal', 'true', 'Use life loss as terminal signal (training only)')

	cmd:option('-game_config', 'quickrace_discrete_single.xml', 'configuration file uesd in torcs')
	cmd:option('-auto_back', 'false', 'whether the car can automatically goes back')

    -- cmd:option('-attention_entropy_beta',0,'the coefficient for semantic attention')
    -- cmd:option('-attention_alpha', 0, 'alpha + beta = 1, alpha for ones of the attention')
    cmd:option('-chooseFeature',-1, 'choose which feature map to visualize in evaluation mode')
    cmd:option('-globalDeconv', 'false', 'show global deconv')

	-- cmd:option('-flickering', 0, 'Probability of screen flickering (Catch only)')
	local opt = cmd:parse(arg)
	local cuda = pcall(require, 'cutorch')
	opt.gpu = cuda and opt.gpu or 0
	-- Process boolean options (Torch fails to accept false on the command line)
	opt.resume = opt.resume == 'true'
	if opt.mode == 'eval' then
		abortIf(not paths.filep(paths.concat(opt.experiments, opt.foldername, 'opts.t7')), 'Insufficient opt File')
		abortIf(not paths.filep(paths.concat(opt.experiments, opt.foldername, 'last.weights.t7')), 'Insufficient stateFile')
		opt.resume = true
	end

	opt.cudnn = opt.cudnn == 'true'
	opt.verbose = opt.verbose == 'true'
	opt.record = opt.record == 'true'
	opt.noValidation = opt.noValidation == 'true'
	opt.reportWeights = opt.reportWeights == 'true'
	opt.server = opt.server == 'true'
	-- opt.use_attention = opt.use_attention == 'true'
	-- opt.use_semantic_attention = opt.use_semantic_attention == 'true'
	opt.constant_sigma = opt.constant_sigma == 'true'
	opt.discrete = opt.discrete == 'true'
	opt.auto_back = opt.auto_back == 'true'
	opt.globalDeconv = opt.globalDeconv == 'true'

	-- ALEWrap options
	opt.fullActions = opt.fullActions == 'true'
	opt.lifeLossTerminal = opt.lifeLossTerminal == 'true'

	-- force no attention when discrete
	if opt.discrete then
		opt.use_attention = false
	end

	-- Process boolean/enum options
	if opt.colorSpace == '' then opt.colorSpace = false end
	if opt.async == '' then opt.async = false end

	-- Set foldername as env (plus game name) if not set
	if opt.foldername == '' then
		opt.foldername = opt.env
	end

	-- Create one environment to extract specifications
	local Env = require(opt.env)
	local env = Env(opt)
	opt.stateSpec = env:getStateSpec()
	opt.actionSpec = env:getActionSpec()
	opt.action_num = table.getn(opt.actionSpec)
	if opt.discrete then
		opt.action_num = opt.actionSpec[3][2] - opt.actionSpec[3][1] + 1 -- Number of discrete actions
	end

	-- Process display if available (can be used for saliency recordings even without QT)
	if env.getDisplay then
		opt.displaySpec = env:getDisplaySpec()
	end
	return opt
end


-- Performs global setup
function Setup:_init(arg)
	-- Create log10 for Lua 5.2
	if not math.log10 then
		math.log10 = function(x)
			return math.log(x, 10)
		end
	end

	-- Parse command-line options
	self.opt = self:parseOptions(arg)
	-- Create experiment directory
	if not paths.dirp(self.opt.experiments) then
		paths.mkdir(self.opt.experiments)
	end

	local start = 0
	-- Save options for reference
	if self.opt.resume and paths.filep(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.t7')) then
		if self.opt.mode == "eval" then
			local temp_seed = self.opt.seed
			local semantic_attention_dim = self.opt.semantic_attention_dim
			local chooseFeature = self.opt.chooseFeature
			local globalDeconv = self.opt.globalDeconv
			self.opt = torch.load(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.t7'))
			self.opt.mode = "eval"
			self.opt.seed = temp_seed
			self.opt.server = false
			self.opt.semantic_attention_dim = semantic_attention_dim
			self.opt.chooseFeature = chooseFeature
			self.opt.globalDeconv = globalDeconv
		else
			self.opt = torch.load(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.t7'))
		end
		self:validateResume()
		self.opt.resume = true
		for thread_id = 1, self.opt.threads do
			start = start + torch.load(paths.concat(self.opt.experiments, self.opt.foldername, ('%02d'):format(thread_id), 'step.t7'))
		end
	else
		paths.mkdir(paths.concat(self.opt.experiments, self.opt.foldername))
		paths.mkdir(paths.concat(self.opt.experiments, self.opt.foldername, 'states'))
		-- save txt file
		local file = torch.DiskFile(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.json'), 'w')
		file:writeString(cjson.encode(self.opt))
		file:close()
		-- save object file
		torch.save(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.t7'), self.opt)
		self.opt.resume = false
	end

	-- Set up logging
	local flog = logroll.file_logger(paths.concat(self.opt.experiments, self.opt.foldername, 'log@'.. start .. '.txt'))
	local plog = logroll.print_logger()
	log = logroll.combine(flog, plog) -- Global logger

	-- Validate command-line options (logging errors)
	self:validateOptions()

	-- Augment environments to meet spec
	self:augmentEnv()

	-- Torch setup
	log.info('Setting up Torch7')
	-- Set number of BLAS threads
	torch.setnumthreads(self.opt.threads)
	-- Set default Tensor type (float is more efficient than double)
	torch.setdefaulttensortype(self.opt.tensorType)
	-- Set manual seed
	torch.manualSeed(self.opt.seed)

	-- Tensor creation function for removing need to cast to CUDA if GPU is enabled
	-- TODO: Replace with local functions across codebase
	self.opt.Tensor = function(...)
		return torch.Tensor(...)
	end

	-- GPU setup
	if self.opt.gpu > 0 then
		log.info('Setting up GPU')
		cutorch.setDevice(self.opt.gpu)
		-- Set manual seeds using random numbers to reduce correlations
		cutorch.manualSeed(torch.random())
		-- Replace tensor creation function
		self.opt.Tensor = function(...)
				return torch.CudaTensor(...)
			end
	end

	classic.strict(self)
end


function Setup:validateResume()
	-- print(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.t7'))
	abortIf(not paths.filep(paths.concat(self.opt.experiments, self.opt.foldername, 'opts.t7')), 'No resuming files')

	-- check if all needed files are existed
	for thread_id = 1, self.opt.threads do
		abortIf(not paths.filep(paths.concat(self.opt.experiments, self.opt.foldername, ('%02d'):format(thread_id), 'step.t7')), ('Insufficient resuming file on steps.t7 on threads %d'):format(thread_id))
		abortIf(not paths.filep(paths.concat(self.opt.experiments, self.opt.foldername, ('%02d'):format(thread_id), 'statistic.t7')), ('Insufficient resuming file on statistic.t7 on threads %d'):format(thread_id))
	end
end
-- Validates setup options
function Setup:validateOptions()
	-- Check environment state is a single tensor
	abortIf(#self.opt.stateSpec ~= 3 or not _.isArray(self.opt.stateSpec[2]), 'Environment state is not a single tensor')

	-- Change state spec if resizing
	if self.opt.height ~= 0 then
		self.opt.stateSpec[2][2] = self.opt.height
	end
	if self.opt.width ~= 0 then
		self.opt.stateSpec[2][3] = self.opt.width
	end

	-- Check colour conversions
	if self.opt.colorSpace then
		abortIf(not _.contains({'y', 'lab', 'yuv', 'hsl', 'hsv', 'nrgb'}, self.opt.colorSpace), 'Unsupported colour space for conversion')
		abortIf(self.opt.stateSpec[2][1] ~= 3, 'Original colour space must be RGB for conversion')
		-- Change state spec if converting from colour to greyscale
		if self.opt.colorSpace == 'y' then
			self.opt.stateSpec[2][1] = 1
		end
	end

	-- if self.opt.use_attention then
	-- 	self.opt.attentionSpec = {'real', {self.opt.stateSpec[2][1], self.opt.attention_h, self.opt.attention_w}, {0, 1}}
	-- end

	-- abortIf(self.opt.use_attention and string.match(self.opt.model, 'NoAttention'), 'Wrong model')
	abortIf(self.opt.discrete and not (string.match(self.opt.model, 'Discrete') or string.match(self.opt.model, 'SemanticAttention')), 'Wrong model')
	abortIf(self.opt.discrete and not (string.match(self.opt.env, 'TORCS.TORCS_discrete') or string.match(self.opt.env, 'rlenvs.Atari') or string.match(self.opt.env, 'TORCS.TorcsDiscrete')), 'Wrong Environment, discrete mode should use env TORCS.TORCS_discrete')
	abortIf(not self.opt.discrete and string.match(self.opt.env, 'TORCS.TORCS_discrete'), 'Wrong Environment, continuous mode should not use env TORCS.TORCS_discrete')

	-- for ALE
	abortIf(string.match(self.opt.env, 'rlenvs.Atari') and self.opt.game == "", 'You have to specify a game rom for Atari game')
end

-- Augments environments with extra methods if missing
function Setup:augmentEnv()
	local Env = require(self.opt.env)
	local env = Env(self.opt)

	-- Set up fake training mode (if needed)
	if not env.training then
		Env.training = function() end
	end
	-- Set up fake evaluation mode (if needed)
	if not env.evaluate then
		Env.evaluate = function() end
	end
end

return Setup
