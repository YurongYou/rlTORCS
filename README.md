# TORCS for Reinforcement Learning
[TORCS (The Open Racing Car Simulator)](http://torcs.sourceforge.net/) is a famous open-source racing car simulator, which provides a realistic physical racing environment and a set of highly customizable API. But it is not so convenient to train a RL model on this environment, for it does not provide visual API and typically needs to go through a GUI MANU to start the game. 

This is a modified version of TORCS in order to suit the needs for deep reinforcement learning training **with visual observation**. Through this environment, researchers can easily train deep reinforcement learning models on TORCS via a lua [interface](https://github.com/Kaixhin/rlenvs#api) (Python interface might also be supported soon).

The original torcs environment is modified in order to

1. Be able to start a racing game with visual output **without going through a GUI MANU**
2. Obtain the **first-person visual observation** in games
3. Provide fast and user-friendly environment-agent communication interface for training
4. Train visual RL models on TORCS on a server (machines without display output)
5. Support multi-thread RL training on TORCS environment
6. Support semantic segmentation on the visual in inputs via some hacks

This environment interface is originally written for my RL research project, and I think open-sourcing this can make it much more easier for current studies on both autonomous cars and reinforcement learning.

## Environment Specification
This repository contains

* `torcs-1.3.6` the source code of a modified torcs-1.3.6
* `TORCSctrl.cpp` a dynamic linking library for communications between RL models (in lua) and TORCS
* `TORCS` the lua interfaces
* `makefile` for compiling `TORCSctrl.so`
* `install.sh` for installing torcs

## System Requirements
* ubuntu 14.04+ (16.04 is preferable, see below)
* [torch](http://torch.ch/)

## IMPORTANT!!
* If you want to run this environment on a server (without display device) with NVIDIA graphic cards, **please make sure your nvidia drivers are installed with flag `--no-opengl-files`**! (otherwise there will be problems when using xvfb, see [this](https://davidsanwald.github.io/2016/11/13/building-tensorflow-with-gpu-support.html))
* In order to correctly install the dependency packages and if you are using a desktop version of Ubuntu, please go to *System Settings -> Software & Updates* and tick the *source code* checkbox before installation, or you should modify your `/etc/apt/sources.list`.
* If your server is Ubuntu 14.04, please do 
	
		cd torcs-1.3.6
		./configure --disable-xrandr
	
	before installation, otherwise there will also be some problems when using xvfb.

## Installation
Just run `install.sh` in the root folder with sudo:

	sudo ./install.sh

After installation, in command line, type `torcs` (`xvfb-run -s "-screen 0 640x480x24" torcs` if on a server). If there jumps off a torcs window (or there is no errors popping up on the command line), the installation has succeeded. Press `ctrl + \` to terminate it (`ctrl + c` has been masked.).

## Interface Specification
The overall interface of this environment follows https://github.com/Kaixhin/rlenvs#api

**[NOTICE]** You need to call `cleanUp()` if the whole training is ended.

### Folder structure
Make sure the folder structure of the environment interface is like this

```
\TORCS
- Env.lua
- Torcs.lua
- TorcsContinuous.lua
- TORCSctrl.so
- TorcsDiscrete.lua
- TorcsDiscreteConstDamagePos.lua
\game_config
- aaa.xml
- bbb.xml
...
```

### Basic Usage (on a desktop ubuntu)
```lua
local TorcsDiscrete = require 'TORCS.TorcsDiscrete'

-- configure the environment
local opt = {}
opt.server = false
opt.game_config = 'quickrace_discrete_single_ushite-city.xml'
opts.use_RGB = false
opts.mkey = 817
opts.auto_back = false

local env = TorcsDiscrete(opt)
local totalstep = 817
print("begin")
local reward, terminal, state = 0, false, env:start()
repeat
	repeat
		reward, observation, terminal = env:step(doSomeActions(state))
		nowstep = nowstep + 1
	until terminal
		
	if terminal then
		reward, terminal, state = 0, false, env:start()
	end
until nowstep >= totalstep
print("finish")
env:cleanUp()
```

### Basic Usage (on a server ubuntu)
Before running the environment, run this on a terminal window:

```
./xvfb_init.sh 99
```
Then a xvfb is set up on display port 99, allowing at most 14 torcs running on this xvfb

Example script: `test.lua`

```lua
-- test.lua
local TorcsDiscrete = require 'TORCS.TorcsDiscrete'

-- configure the environment
local opt = {}
opt.server = true
opt.game_config = 'quickrace_discrete_single_ushite-city.xml'
opts.use_RGB = false
opts.mkey = 817
opts.auto_back = false

local env = TorcsDiscrete(opt)
local totalstep = 817
print("begin")
local reward, terminal, state = 0, false, env:start()
repeat
	repeat
		reward, observation, terminal = env:step(doSomeActions(state))
		nowstep = nowstep + 1
	until terminal
		
	if terminal then
		reward, terminal, state = 0, false, env:start()
	end
until nowstep >= totalstep
print("finish")
env:cleanUp()
```

Run the script while the xvfb is running:

```bash
export DISPLAY=99:
th test.lua
```

### Options specification
You can pass a table to the environment to configure the racing car environment:

* `server`: bool value, set `true` to run on a server
* `game_config`: string, the name of game configuration file, should be stored in the `game_config` folder
* `use_RGB`: bool value, set `true` to use RGB visual observation, otherwise gray scale visual observation
* `mkey`: integer, the key to set up memory sharing. Different running environment should have different mkey
* `auto_back`: bool value,  set `false` to disable the car's ability to reverse its gear (for going backward)

### Multithread example
see `torcs_test.lua`

### Action Space
We provide environments with two different action spaces. Note that different action space should choose different type of game configurations respectively (see Further Customization section below for explanations on game configuration).

In original torcs, the driver can have controls on four different actions:

* **throttle**: real value ranging from `[0, 1]`. `0` for zero acceleration, `1` for full acceleration
* **brake**: a real value ranging from `[0, 1]`. `0` for zero brake, `1` for full brake
* **steer**: a real value ranging from `[-1, 1]`. `-1` for full left-turn, `1` for full right-turn
* **gear**:	a integer ranging from `[-1, 6]`. `-1` for going backward.

The following is my customization, you can customize the action space by yourself (see Further Customization section).

#### Discrete Action Space (`TORCS/TorcsDiscrete.lua`)
This environment has a discrete action space:

```
1 : brake = 0, throttle = 1, steer =  1
2 : brake = 0, throttle = 1, steer =  0
3 : brake = 0, throttle = 1, steer = -1
4 : brake = 0, throttle = 0, steer =  1
5 : brake = 0, throttle = 0, steer =  0
6 : brake = 0, throttle = 0, steer = -1
7 : brake = 1, throttle = 0, steer =  1
8 : brake = 1, throttle = 0, steer =  0
9 : brake = 1, throttle = 0, steer = -1
```

The corresponding driver controlled by this environment is `torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp`. Note that the action will be performed in a gradually changing way, e.g., if the current throttle command of the driver is `0` while the action is `2`, then the actual action will be some value between `0` and `1`. Such customization mimics the way a human controls a car using a keyboard. See `torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp:drive` for more details.


#### Continuous Action Space (`TORCS/TorcsContinuous.lua`)
There are two dimensions of continuous actions in this environment:

* **brake/throttle**: a real value ranging from `[-1, 1]`. `-1` for full brake, `1` for full throttle, i.e.
	
	```
	if action[1] > 0 then
		throttle = action[1]
		brake = 0
	else
		brake = -action[1]
		throttle = 0
	end
	```
* **steer**: a real value ranging from `[-1, 1]`. `-1` for full left-turn, `1` for full right-turn


**In above two customized environment, note that**

* There are hard-coded ABS and ASR strategies on corresponding drivers
* gear changes automatically




## [Optional] System Explanation



## [Optional] For Further Customization

## Reference 
* [DeepDriving from Princeton](http://deepdriving.cs.princeton.edu/): the memory sharing scheme of this training environment is the same with this project.
* [Custom RL environment](https://github.com/Kaixhin/rlenvs#api)
* [Implementations of Several Deep Reinforcement Learning Algorithm](https://github.com/Kaixhin/Atari)


