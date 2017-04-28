local classic = require 'classic'

local Env = classic.class('Env')

-- Denote interfaces
Env:mustHave('start')
Env:mustHave('step')
Env:mustHave('getStateSpec')
Env:mustHave('getActionSpec')
Env:mustHave('getRewardSpec')

return Env
