-- luacheck: ignore __threadid log action_description super
-- luacheck: globals Env
local classic = require 'classic'
local threads = require 'threads'
threads.Threads.serialization('threads.sharedserialize')
local Torcs = require 'TORCS.Torcs'
local TorcsContinuous, super = classic.class('TorcsContinuous', Torcs)

-- WARNING: we should never use posix package here!!! (i.e. never call `posix = require 'posix'` etc.)
-- otherwise the signal handler outside the thread will be blocked.

-- Constructor
function TorcsContinuous:_init(opts)
	super._init(self, opts)
end

-- first the accel / brake, second the steer
function TorcsContinuous.getActionSpec()
	return {'real', {2}, {{-1, 1}, {-1, 1}}}
end

function TorcsContinuous.decodeAction( action )
	if action[1] > 0 then
		return action[1], 0, action[2]
	else
		return 0, -action[1], action[2]
	end
end

return TorcsContinuous