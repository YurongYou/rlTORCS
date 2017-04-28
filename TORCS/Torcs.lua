-- luacheck: ignore __threadid log action_description super
-- luacheck: globals Env
local classic = require 'classic'
local threads = require 'threads'
local image = require 'image'
threads.Threads.serialization('threads.sharedserialize')
local Env = require 'TORCS.Env'
local math = require 'math'
local Torcs, super = classic.class('Torcs', Env)

-- WARNING: we should never use posix package here!!! (i.e. never call `posix = require 'posix'` etc.)
-- otherwise the signal handler outside the thread will be blocked.

Torcs:mustHave("decodeAction")

-- Constructor
function Torcs:_init(opts)
	opts = opts or {}
	if opts.mkey then
		self.mkey = opts.mkey
	else
		self.mkey = 817 -- in honor of the elder, +1s!
	end
	self.nowStep = 0

	local temp = package.cpath
	package.cpath = package.cpath .. ";?.so"
	self.ctrl = require 'TORCSctrl'
	package.cpath = temp

	self.damage = 0
	self.distance = -9999
	self.distance_gap = 0
	self.frontNum = -1
	self.observation_gray = torch.FloatTensor(1, 480, 640)
	self.observation_RGB = torch.FloatTensor(3, 480, 640)
	self.server = opts.server
	self.isStarted = false
	self.game_config = opts.game_config and opts.game_config or 'quickrace_discrete_single.xml'
	self.auto_back = opts.auto_back and opts.auto_back or false
	self.use_RGB = opts.use_RGB and opts.use_RGB or false

	self.wrapperPid = -1
	self.nan_count = 0
	self.stuck_count = 0

	self.end_step = 10000

	classic.strict(self)
end

-- 1 state returned, 1 channel with height 480, width 640 (gray scale)
function Torcs:getStateSpec()
	if self.use_RGB then
		return {'real', {3, 480, 640}, {0, 1}}
	else
		return {'real', {1, 480, 640}, {0, 1}}
	end
end

-- RGB screen of height 480, width 640
function Torcs.getDisplaySpec()
	return {'real', {3, 480, 640}, {0, 1}}
end

-- Min and max reward (unknown)
function Torcs.getRewardSpec()
	return nil, nil
end

function Torcs:start()
	-- log.info("environment start")
	if not self.isStarted then
		self.ctrl.setUp(self.mkey)
		self.isStarted = true
	end

	self.nowStep = 0
	self.damage = 0
	self.stuck_count = 0
	self.distance = -99999
	self.frontNum = -1

	config_path = paths.concat('game_config', self.game_config)
	self.ctrl.initializeMem()
	self.wrapperPid = self.ctrl.newGame(self.auto_back and 1 or 0, self.mkey, self.server and 1 or 0, __threadid and __threadid or -1, config_path)
	self:connect()
	self.distance = self.ctrl.getDist()
	self.distance_gap = 0
	if self.use_RGB then
		-- return image.toDisplayTensor(torch.Tensor(self.ctrl.getImg()))
		self.ctrl.getRGBImage(self.observation_RGB, 0)
		return self.observation_RGB
	else
		self.ctrl.getGreyScale(self.observation_gray)
		return self.observation_gray
	end
end

function Torcs:connect( action )
	local accel, brake, steer
	if action then
		accel, brake, steer = self.decodeAction(action)
	else
		accel, brake, steer = 0, 0, 0
	end
	self.ctrl.setAccelCmd(accel)
	self.ctrl.setBrakeCmd(brake)
	self.ctrl.setSteerCmd(steer)
	self.ctrl.setWritten(0)
	local count = 0
	while self.ctrl.getWritten() ~= 1 do
		self.ctrl.sleep(1)
		count = count + 1
		if count > 20000 then
			log.error("failed to connect to torcs")
			self:terminate(self.wrapperPid)
			self:terminate(self.ctrl.getPid())
			return false
		end
	end
	self.nowStep = self.nowStep + 1
	self.distance_gap = self.ctrl.getDist() - self.distance
	self.distance = self.ctrl.getDist()
	self.frontNum = self.ctrl.getFrontCarNum()
	return true
end

function Torcs:reward()
	local rate = 0.005
	local speedReward = (self.ctrl.getSpeed() * math.cos(self.ctrl.getAngle()))
	local collisionPunish = ((self.ctrl.getDamage() - self.damage) * 0.01)
    local pos = self.ctrl.getPos()*2
	return (speedReward - collisionPunish - pos) * rate
end

function Torcs:isStuck()
	return math.abs(self.ctrl.getAngle()) > math.pi * 50 / 180 and self.ctrl.getSpeed() < 1
end

function Torcs:getSegType()
	local seg_type = self.ctrl.getSegType()
	if seg_type == 3 then
		return "straight"
	else
		return "bend"
	end
end

function Torcs:getFrontCarNum()
	return self.ctrl.getFrontCarNum()
end

function Torcs:getFrontCarDist()
	return self.ctrl.getFrontCarDist()
end

function Torcs:isTerminal()
	return self.nowStep > self.end_step or self.damage > 90000
end

function Torcs:step( action )
	if not self:connect(action) then
		if self.use_RGB then
			return 0, self.observation_RGB:zero(), true
		else
			return 0, self.observation_gray:zero(), true
		end
	end
	-- self.ctrl.sleep(100)
	local newDamage = self.ctrl.getDamage()
	local reward = self:reward()
	self.damage = newDamage
	local terminal = self.ctrl.getIsEnd() == 1
	if self:isTerminal() then
		terminal = true
	end
	if self:isStuck() then
		-- log.info("stuck_count: %d", self.stuck_count)
		self.stuck_count = self.stuck_count + 1
		if self.stuck_count > 500 then
			terminal = true
		end
	end
    if reward ~= reward then
		log.error("nan reward!")
		if not paths.dirp(paths.concat("test_img")) then
			paths.mkdir("test_img")
		end
		self.nan_count = self.nan_count + 1
		log.error("saving image to " .. "test_img/nan_thread_" .. tostring(__threadid) .. "_" .. tostring(self.nan_count) .. ".jpg")
		image.save("test_img/nan_thread_" .. tostring(__threadid) .. "_" .. tostring(self.nan_count) .. ".jpg", self:getDisplay())
		log.error("image saved")
		reward = 0
		terminal = true
	end
	if terminal then
		self:kill()
	end
	if self.use_RGB then
		self.ctrl.getRGBImage(self.observation_RGB, 0)
		return reward, self.observation_RGB, terminal
	else
		self.ctrl.getGreyScale(self.observation_gray)
		return reward, self.observation_gray, terminal
	end
end

function Torcs:getDisplay(choose)
	self.ctrl.getRGBImage(self.observation_RGB, choose)
	return self.observation_RGB
	-- return image.toDisplayTensor(torch.Tensor(self.ctrl.getImg(choose)))
	-- return self.observation_gray
end

function Torcs:terminate(pid)
    self.ctrl.ctrl_kill(pid)
	self.ctrl.ctrl_wait(pid)
end

function Torcs:kill()
	self:terminate(self.wrapperPid)
		self:terminate(self.ctrl.getPid())
		self.wrapperPid = -1
end

function Torcs:cleanUp()
    self.isStarted = false
	if self.wrapperPid > 0 then
		self:terminate(self.wrapperPid)
    end
	self:terminate(self.ctrl.getPid())
	self.ctrl.cleanUp()
end

-- to specify the different render mode?
function Torcs:training()
	self.end_step = 10000
end

function Torcs:evaluate()
	self.end_step = 1875
end

function Torcs:getMkey()
	return self.mkey
end

return Torcs