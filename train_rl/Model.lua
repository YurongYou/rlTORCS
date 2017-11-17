-- luacheck: ignore __threadid log
local classic = require 'classic'
local hasCudnn, cudnn = pcall(require, 'cudnn')
local Model = classic.class('Model')
local nn = require 'nn'
local image = require "image"
require 'modules/GuidedReLU'

function Model:_init( opt )
	self.tensorType = opt.tensorType
	self.gpu = opt.gpu
	self.cudnn = opt.cudnn
	self.colorSpace = opt.colorSpace
	self.width = opt.width
	self.height = opt.height
	if __threadid then
	    opt.mkey = __threadid + opt.group * 20
	end
	local Env = require(opt.env)
	self.opt = opt
	self.env = Env(opt)
	self.net, self.CNN, self.afterCNN = createNet(opt)
	if opt.width ~= 0 or opt.height ~= 0 then
		self.resize = true
		self.width = opt.width ~= 0 and opt.width or opt.stateSpec[2][3]
		self.height = opt.height ~= 0 and opt.height or opt.stateSpec[2][2]
	end
	-- classic.strict(self)
end

-- note: currently only suitable for CPU model
function createNet(opt)
	opt.obsSpec = opt.stateSpec
	log.info('Setting up ' .. opt.model)
	local Body = require(opt.model)
	local body = Body(opt)
	local net = body:createBody()
	local CNN = body:getCNN()
	local afterCNN = body:getAfterCNN()
	-- GPU conversion
	if opt.gpu > 0 then
		require 'cunn'
		net:cuda()

		if opt.cudnn and hasCudnn then
			cudnn.convert(net, cudnn)
			-- The following is legacy code that can make cuDNN deterministic (with a large drop in performance)
			--[[
			local convs = net:findModules('cudnn.SpatialConvolution')
			for i, v in ipairs(convs) do
			v:setMode('CUDNN_CONVOLUTION_FWD_ALGO_GEMM', 'CUDNN_CONVOLUTION_BWD_DATA_ALGO_1', 'CUDNN_CONVOLUTION_BWD_FILTER_ALGO_1')
			end
			--]]
		end
	end
	return net, CNN, afterCNN
end

function Model:getEnv()
	return self.env
end

function Model:getNet()
	return self.net
end

function Model:getCNN()
	return self.CNN
end

function Model:getAfterCNN()
	return self.afterCNN
end

function Model:preprocessing(rawObservation)
	local frame = rawObservation:type(self.tensorType)
	if self.colorSpace then
		frame = image['rgb2' .. self.colorSpace](frame)
	end
	if self.resize then
		frame = image.scale(frame, self.width, self.height)
	end
	if frame == rawObservation then
		frame = frame:clone()
	end
	return frame
end

function Model:colorSpaceTransform( rawObservation )
	-- Perform colour conversion if needed
	rawObservation = rawObservation:type(self.tensorType)
	if self.colorSpace then
		frame = image['rgb2' .. self.colorSpace](rawObservation)
	end
	-- Clone if needed
	if frame == rawObservation then
		frame = frame:clone()
	end

	return frame
end

function Model:typeTransform(observation)
	local frame = observation:type(self.tensorType) -- Convert from CudaTensor if necessary

	return frame
end


function Model:scaling(observation)
	local frame = observation:type(self.tensorType) -- Convert from CudaTensor if necessary

	-- Resize screen if needed
	if self.resize then
		frame = image.scale(frame, self.width, self.height)
	end

	-- Clone if needed
	if frame == observation then
		frame = frame:clone()
	end

	return frame
end

function Model:getFilters()
	local filters = {}

	-- Find convolutional layers
	local convs = self.net:findModules(self.cudnn and hasCudnn and 'cudnn.SpatialConvolution' or 'nn.SpatialConvolution')
	for i, v in ipairs(convs) do
	-- Add filter to list (with each layer on a separate row)
		filters[#filters + 1] = image.toDisplayTensor(v.weight:view(v.nOutputPlane*v.nInputPlane, v.kH, v.kW), 1, v.nInputPlane, true)
	end

	return filters
end


function Model:setSaliency(saliency)
  -- Set saliency
  self.saliency = saliency

  -- Find ReLUs on existing model
  local relus, relucontainers = self.net:findModules((hasCudnn and self.gpu > 0) and 'cudnn.ReLU' or 'nn.ReLU')
  if #relus == 0 then
    relus, relucontainers = self.net:findModules('nn.GuidedReLU')
  end
  if #relus == 0 then
    relus, relucontainers = self.net:findModules('nn.DeconvnetReLU')
  end

  -- Work out which ReLU to use now
  local layerConstructor = (hasCudnn and self.gpu > 0) and cudnn.ReLU or nn.ReLU
  self.relus = {} --- Clear special ReLU list to iterate over for salient backpropagation
  if saliency == 'guided' then
    layerConstructor = nn.GuidedReLU
  elseif saliency == 'deconvnet' then
    layerConstructor = nn.DeconvnetReLU
  end

  -- Replace ReLUs
  for i = 1, #relus do
    -- Create new special ReLU
    local layer = layerConstructor()

    -- Copy everything over
    for key, val in pairs(relus[i]) do
      layer[key] = val
    end
    -- Find ReLU in containing module and replace
    for j = 1, #(relucontainers[i].modules) do
      if relucontainers[i].modules[j] == relus[i] then
        relucontainers[i].modules[j] = layer
      end
    end
  end
  -- Create special ReLU list to iterate over for salient backpropagation
  self.relus = self.net:findModules(saliency == 'guided' and 'nn.GuidedReLU' or 'nn.DeconvnetReLU')
end

-- Switches the backward computation of special ReLUs for salient backpropagation
function Model:salientBackprop()
  for i, v in ipairs(self.relus) do
    v:salientBackprop()
  end
end

-- Switches the backward computation of special ReLUs for normal backpropagation
function Model:normalBackprop()
  for i, v in ipairs(self.relus) do
    v:normalBackprop()
  end
end


return Model
