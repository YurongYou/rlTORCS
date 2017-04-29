# TORCS for Reinforcement Learning

<p align="center">
	<img src ="assets/demo.gif" width="800" />
</p>
<!--<div style="text-align:center"></div>-->

<!--<div style="float:left; margin:0 10px 10px 0" markdown="1">
    ![](assets/demo.gif)
</div>-->


## Table of Content

- [Overview](#overview)
- [Environment Specification](#environment-specification)
- [System Requirements](#system-requirements)
- [IMPORTANT!!](#important)
- [Installation](#installation)
- [Interface Specification](#interface-specification)
	- [Folder Structure](#folder-structure)
	- [Basic Usage \(on a desktop ubuntu\)](#basic-usage-on-a-desktop-ubuntu)
	- [Basic Usage \(on a server ubuntu\)](#basic-usage-on-a-server-ubuntu)
	- [Options Specification](#options-specification)
	- [Multithread Example](#multithread-example)
	- [Action Space](#action-space)
	- [Tracks](#tracks)
	- [Reward](#reward)
- [System Explanation in a Nutshell](#system-Explanation-in-a-nutshell)
	- [Agent-Environment Communication](#agent-environment-communication)
- [Training results](#training-results)
- [\[Optional\] Further Customization](#optional-further-customization)
	- [Skip the GUI MANU](#skip-the-gui-manu)
	- [Memory Sharing Details](#memory-sharing-details)
	- [Obtain the First Person View](#obtain-the-first-person-view)
	- [Customize the Race](#customize-the-race)
	- [Visual Semantic Segmentation](#visual-semantic-segmentation)
- [Reference](#reference)


## Overview
[TORCS (The Open Racing Car Simulator)](http://torcs.sourceforge.net/) is a famous open-source racing car simulator, which provides a realistic physical racing environment and a set of highly customizable API. But it is not so convenient to train an RL model in this environment, for it does not provide visual API and typically needs to go through a GUI MANU to start the game.

This is a modified version of TORCS in order to suit the needs for deep reinforcement learning training **with visual observation**. Through this environment, researchers can easily train deep reinforcement learning models on TORCS via a Lua [interface](https://github.com/Kaixhin/rlenvs#api) (Python interface might also be supported soon).

The original TORCS environment is modified in order to

1. Be able to start a racing game with visual output **without going through a GUI MANU**
2. Obtain the **first-person visual observation** in games
3. Provide fast and user-friendly environment-agent communication interface for training
4. Train visual RL models on TORCS on a server (machines without display output)
5. Support multi-thread RL training on TORCS environment
6. Support semantic segmentation on the visual in inputs via some hacks

This environment interface is originally written for my RL research project, and I think open-sourcing this can make it much easier for current studies on both autonomous cars and reinforcement learning.

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
* If you want to run this environment on a server (without display device) with Nvidia graphic cards, **please make sure your Nvidia drivers are installed with flag `--no-opengl-files`**! (otherwise, there will be problems when using xvfb, see [this](https://davidsanwald.github.io/2016/11/13/building-tensorflow-with-gpu-support.html))
* In order to correctly install the dependency packages and if you are using a desktop version of Ubuntu, please go to *System Settings -> Software & Updates* and tick the *source code* checkbox before installation, or you should modify your `/etc/apt/sources.list`.
* If your server is Ubuntu 14.04, please do

		cd torcs-1.3.6
		./configure --disable-xrandr

	before installation, otherwise, there will also be some problems when using xvfb.

## Installation
Just run `install.sh` in the root folder with sudo:

	sudo ./install.sh

After installation, in command line, type `torcs` (`xvfb-run -s "-screen 0 640x480x24" torcs` if on a server). If there jumps off a torcs window (or there are no errors popping up on the command line), the installation has succeeded. Press `ctrl + \` to terminate it (`ctrl + c` has been masked.).

## Interface Specification
The overall interface of this environment follows https://github.com/Kaixhin/rlenvs#api

**[NOTICE]** You need to call `cleanUp()` if the whole training  ends.

### Folder Structure
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
opt.use_RGB = false
opt.mkey = 817
opt.auto_back = false

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
Before running the environment, run this in a terminal window:

```bash
./xvfb_init.sh 99
```
Then a xvfb is set up on display port 99, allowing at most 14 TORCS environment simultaneously running on this xvfb

Example script: `test.lua`

```lua
-- test.lua
local TorcsDiscrete = require 'TORCS.TorcsDiscrete'

-- configure the environment
local opt = {}
opt.server = true
opt.game_config = 'quickrace_discrete_single_ushite-city.xml'
opt.use_RGB = false
opt.mkey = 817
opt.auto_back = false

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

### Options Specification
You can pass a table to the environment to configure the racing car environment:

* `server`: bool value, set `true` to run on a server
* `game_config`: string, the name of game configuration file, should be stored in the `game_config` folder
* `use_RGB`: bool value, set `true` to use RGB visual observation, otherwise grayscale visual observation
* `mkey`: integer, the key to set up memory sharing. Different running environment should have different mkey
* `auto_back`: bool value,  set `false` to disable the car's ability to reverse its gear (for going backward)

### Multithread Example
See [torcs_test.lua](torcs_test.lua).

### Action Space
We provide environments with two different action spaces. Note that different action space should choose different types of game configurations respectively (see [Further Customization](#optional-further-customization) section below for explanations on game configuration).

In original torcs, the driver can have controls on four different actions:

* **throttle**: real value ranging from `[0, 1]`. `0` for zero acceleration, `1` for full acceleration
* **brake**: a real value ranging from `[0, 1]`. `0` for zero brake, `1` for full brake
* **steer**: a real value ranging from `[-1, 1]`. `-1` for full left-turn, `1` for full right-turn
* **gear**:	a integer ranging from `[-1, 6]`. `-1` for going backward.

The following is my customization, you can customize the action space by yourself (see [Further Customization](#optional-further-customization) section).

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

The corresponding driver controlled by this environment is [torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp](torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp). Note that the action will be performed in a gradually changing way, e.g., if the current throttle command of the driver is `0` while the action is `2`, then the actual action will be some value between `0` and `1`. Such customization mimics the way a human controls a car using a keyboard. See [torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp:drive](torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp:drive) for more details.


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

### Tracks
You can customize the racing track by the way stated in [Customize the Race](#customize-the-race) section. Here we provide some game configuration on different tracks, see `*.xml` files, which are named in format `quickrace_<discrete|continuous>_<single|multi>_<trackname>.xml`, in the `game_config` folder.

### Reward
You can modify the original reward function of the environment by override `reward()` function, as in `TorcsDiscreteConstDamagePos`.

## System Explanation in a Nutshell
In this section, we briefly explain how the racing car environment works.

In TORCS environment, when the game is running, it actually maintains a state machine (see [torcs-1.3.6/src/libs/raceengineclient/racestate.cpp](torcs-1.3.6/src/libs/raceengineclient/racestate.cpp)). On each update of the the state (in `500` Hz), the underlying physical states are updated (e.g. update each cars' speed according to its previous speed and acceleration, etc.), and in `50` Hz, each of car drivers is queried for actions. Drivers can have access to a variety of information (e.g. speed, distance, etc.) to decide what action it will take (i.e. decide the value of `throttle`, `brake`, `steer`, `gear`, etc.). You can refer to [this](http://www.berniw.org/tutorials/robot/tutorial.html) on how to customize drivers.

### Agent-Environment Communication
To make it possible to run RL model in torch or python, we use memory sharing method to set up IPC between the environment and the model (thanks to [DeepDriving from Princeton](http://deepdriving.cs.princeton.edu/)).

Once the environment is set up, it will request a portion of shared memory using the given `mkey`. If the RL agent running in another process also requests a portion of shared memory of the same size with the same `mkey`, then it can see the information written by the customized driver in TORCS.

The customized driver can regularly write information about the current racing status into that memory, and wait for the agent for its action (also, write in different part of that memory). Those information can be

* **current first-person view from the driver**
* the speed of the car
* the angle between the car and the tangent of the track
* current amount of damage
* the distance between the center of the car and the center of the track
* total distance raced
* the radius of the track segment
* the distance between the car and the nearest car in front of it
...

and it can be customized if more information is needed.

## Training results
Here we show a training result of running a typical A3C model ([Asynchronous Methods for Deep Reinforcement Learning](http://arxiv.org/abs/1602.01783)) on our customized environment.

![](https://raw.githubusercontent.com/YurongYou/rlTORCS/master/assets/training_result.png?token=AM-ptbmmeTQxIXNEj7UaI04oMkW1B_tZks5ZDHwwwA%3D%3D)

Details about the test:

* game_config: `quickrace_discrete_slowmulti_ushite-city.xml`
* environment: `TorcsDiscreteConstDamagePos`
* auto_back: `false`
* threads: `12`
* total steps: `2e7`
* observation: gray scale visual input with resizing (`84x84`)
* learning rate: `7e-5`

## [Optional] Further Customization
In this section, I explain how I modify the environment. If you are not intended to further customize the environment, you can just skip this section.

All modifications on the original TORCS source code are indicated by a `yurong` marker.

### Skip the GUI MANU
Related files: 

* [torcs-1.3.6/src/main.cpp](torcs-1.3.6/src/main.cpp) 
* [torcs-1.3.6/src/libs/raceengineclient/raceinit.cpp](torcs-1.3.6/src/libs/raceengineclient/raceinit.cpp)

Run `torcs _rgs <path to game configuration file>` then the race is directly set up and begin running.

### Memory Sharing Details
Related files:

* [torcs-1.3.6/src/main.cpp](torcs-1.3.6/src/main.cpp) -- set up memory sharing
* [torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp](torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp) -- forward the pointers, write first person view image into the shared memory
* [torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp](torcs-1.3.6/src/drivers/ficos_discrete/ficos_discrete.cpp) -- receive command from the shared memory, write race information
* [torcs-1.3.6/src/drivers/ficos_continuous/ficos_continuous.cpp](torcs-1.3.6/src/drivers/ficos_continuous/ficos_continuous.cpp) -- receive command from the shared memory, write race information
* [TORCS/TORCSctrl.cpp](TORCS/TORCSctrl.cpp) -- manage communication between lua and C++

Memory sharing structure

```c
struct shared_use_st
{
    int written;
    uint8_t data[image_width*image_height*3];
    uint8_t data_remove_side[image_width*image_height*3];
    uint8_t data_remove_middle[image_width*image_height*3];
    uint8_t data_remove_car[image_width*image_height*3];
    int pid;
    int isEnd;
    double dist;

    double steerCmd;
    double accelCmd;
    double brakeCmd;

    // for reward building
    double speed;
    double angle_in_rad;
    int damage;
    double pos;
    int segtype;
    double radius;
    int frontCarNum;
    double frontDist;
};
```

To specify the memory sharing key:

* In TORCS: `torcs _mkey <key>`
* In lua interface: `opt.mkey = <key>`

### Obtain the First Person View
Related file: [torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp](torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp).

See

```c
863: glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)pdata);
```

### Customize the Race
You can generate the corresponding `xxx.xml` game configuration file in the following way:

1. Enter `torcs` in cli to enter the game GUI MANU
2. Configure race in `Race -> QuickRace -> Configure Race`. You can choose the tracks, robots. **Do select robot ficos_discrete / ficos_continuous**.
3. In `Race Distance` choose `10 km`
4. click `accept`s
5. In `~/.torcs/config/raceman` there is a file `quickrace.xml`, which is the corresponding game configuration file.

### Visual Semantic Segmentation
To obtain the corresponding semantic segmentation of visual observation, we use some hacks stated below. **Note that the modification should be done on a track-by-track basis.**

Related files:


* [torcs-1.3.6/src/interfaces/collect_segmentation.h](torcs-1.3.6/src/interfaces/collect_segmentation.h) -- compile flag COLLECTSEG
* [torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp](torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp) -- write first person view image into the shared memory
* [torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp](torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp) -- load different 3D models and control the rendering

In the shared memory, `data_remove_side`, `data_remove_middle` and `data_remove_car` are intended to store the visual observations with road shoulders, middle lane and cars removed respectively for track [torcs-1.3.6/data/tracks/road/Ushite-city](torcs-1.3.6/data/tracks/road/Ushite-city). **In such way, the difference between the original observation and the removed ones is the desired segmentation.**

To obtain visual observations with some parts removed, we use a pre-generated 3D model (`.ac` files) where the corresponding part's texture mapping is changed, for example, if we want to obtain visual observations with the middle of the lane marks removed, we need to generate a new 3D model where the texture mapping of the surface of the road is change into another picture.

Before the game start, load the modified 3D model. And when the game is running, we can first render the original observation using the original 3D model, then switch to the modified 3D model to obtain the observation with some parts removed. [Related file: [torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp](torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp)]

**What (generally) you need to do to obtain the segmentation**:

1. Uncomment the `define` line in [torcs-1.3.6/src/interfaces/collect_segmentation.h](torcs-1.3.6/src/interfaces/collect_segmentation.h).
2. Generate the modified 3D model of the track. Duplicate the `.ac` file in the corresponding track folder ([torcs-1.3.6/data/tracks/*](torcs-1.3.6/data/tracks)), and change the texture mapping of the objects.
3. Modify codes around `line 254` of [torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp](torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp) to load your modifed 3D model.
4. Modify codes around `line 269` `grDrawScene` function of [torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp](torcs-1.3.6/src/modules/graphic/ssggraph/grscene.cpp) to make the environment render different models according to `drawIndicator`.
5. Modify codes around `line 857` in [torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp](torcs-1.3.6/src/libs/raceengineclient/raceengine.cpp) to set `drawIndicator` to get different render result, store them in proper places.
6. Reinstall TORCS with `sudo ./install.sh`
7. Start the environment with corresponding game configuration `.xml` file. In the Lua interface, call `env:getDisplay(choose)` to obtain observations with different parts removed. In current implementation for track [torcs-1.3.6/data/tracks/road/Ushite-city](torcs-1.3.6/data/tracks/road/Ushite-city):
	
	```lua
	0: original RGB observation
	1: RGB observation with road shoulders removed
	2: RGB observation with middle lane-marks removed
	3: RGB observation with cars removed
	```

8. Take the difference between the observation with different parts removed and the original observation, and pixels where the difference is large is where the corresponding parts locate.

## Reference
* [DeepDriving from Princeton](http://deepdriving.cs.princeton.edu/): the memory sharing scheme of this training environment is the same with this project.
* [Custom RL environment](https://github.com/Kaixhin/rlenvs#api)
* [Implementations of Several Deep Reinforcement Learning Algorithm](https://github.com/Kaixhin/Atari)
* [Asynchronous Methods for Deep Reinforcement Learning](http://arxiv.org/abs/1602.01783)


