-- luacheck: ignore __threadid log action_description super
-- luacheck: globals Env
local classic = require 'classic'
local threads = require 'threads'
threads.Threads.serialization('threads.sharedserialize')
local Torcs = require 'TORCS.Torcs'
local TorcsDiscrete, super = classic.class('TorcsDiscrete', Torcs)

-- WARNING: we should never use posix package here!!! (i.e. never call `posix = require 'posix'` etc.)
-- otherwise the signal handler outside the thread will be blocked.

local actionMap = {}
actionMap[1] = {1, 0,  1}
actionMap[2] = {1, 0,  0}
actionMap[3] = {1, 0, -1}
actionMap[4] = {0, 0,  1}
actionMap[5] = {0, 0,  0}
actionMap[6] = {0, 0, -1}
actionMap[7] = {0, 1,  1}
actionMap[8] = {0, 1,  0}
actionMap[9] = {0, 1, -1}

-- Constructor
function TorcsDiscrete:_init(opts)
	super._init(self, opts)
end

-- three actions: steer * accer/brake = 3 * 3 = 9
function TorcsDiscrete.getActionSpec()
	return {'int', 1, {1, 9}}
end

function TorcsDiscrete.decodeAction( action )
	-- log.info(actionMap[action])
	return table.unpack(actionMap[action])
end

return TorcsDiscrete