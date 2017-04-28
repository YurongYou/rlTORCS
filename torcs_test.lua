local threads = require 'threads'
threads.Threads.serialization('threads.sharedserialize')
local tds = require 'tds'

local function threadedFormatter(thread)
	local threadName = thread

	return function(level, ...)
		local msg = nil

		if #{...} > 1 then
			msg = string.format(({...})[1], table.unpack(fn.rest({...})))
		else
			msg = pprint.pretty_string(({...})[1])
		end

		return string.format("[%s: %s - %s] - %s\n", threadName, logroll.levels[level], os.date("%Y_%m_%d_%X"), msg)
	end
end

local function setupLogging(thread)
	local thread = thread or __threadid
	require 'logroll'
	if type(thread) == 'number' then
		thread = ('%02d'):format(thread)
	end
	local file = paths.concat('log.'.. thread ..'.txt')
	local flog = logroll.file_logger(file)
	local formatterFunc = threadedFormatter(thread)
	local plog = logroll.print_logger({formatter = formatterFunc})
	log = logroll.combine(flog, plog)
end

local ctrlpool = threads.Threads(1)
local atomic = tds.AtomicCounter()
atomic:set(1)
num = 1

ctrlpool:addjob(setupLogging)
ctrlpool:addjob(
	function ()
		local unistd = require "posix.unistd"

		__threadid = 0
		local signal = require 'posix.signal'
		signal.signal(signal.SIGINT, function(signum)
			log.info("SIGINT received")
			log.info('Ex(c)iting')
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
			ctrl.sleep(1)
			if atomic:get() < 0 then break end
		end
	end
)

local function test()
	log.info(env:getStateSpec())
	log.info(env:getActionSpec())
	totalstep = 200
	nowstep = 0
	log.info("env starting!")
	local reward, terminal, state = 0, false, env:start()
	log.info("is terminal: " .. tostring(terminal))
	counting = 1
	repeat
		repeat
			reward, observation, terminal = env:step({0.1, -0.1})
			nowstep = nowstep + 1
			if term:get() < 0 then
				env:cleanUp()
				log.info("finish")
				return
			end
		until terminal

		if terminal then
			reward, terminal, state = 0, false, env:start()
		end
	until nowstep >= totalstep
	log.info("finish")
	env:cleanUp()
end

game = threads.Threads(num,
	function (id)
		temp = package.path
		package.path = package.path .. ";../?.lua"
		TORCS = require 'TORCS.TorcsContinuous'
		package.path = temp
		opt = {}
		opt.mkey = id
		opt.server = false
		opt.game_config = 'quickrace_continuous_single_ushite-city.xml'
		term = atomic
		local image = require 'image'
	end,
	function ()
		env = TORCS(opt)
	end,
	setupLogging,
	function ( id )
	    log.info('Setting up Torch7')
	    -- Use enhanced garbage collector
	    torch.setheaptracking(true)
	    -- Set number of BLAS threads to 1 (per thread)
	    torch.setnumthreads(1)
	    -- Set default Tensor type (float is more efficient than double)
	    torch.setdefaulttensortype("torch.FloatTensor")
	    -- Set manual seed (different for each thread to have different experiences)
	    torch.manualSeed(id)
	end
)

for _ = 1, num do
	game:addjob(test)
end

game:synchronize()
game:terminate()
atomic:set(-1)
ctrlpool:synchronize()
ctrlpool:terminate()