local nn = require 'nn'
require 'classic.torch' -- Enables serialisation

local Body = classic.class('Body')

-- dirty table clone
function clone(org)
	return {table.unpack(org)}
end

-- Calculates network output size
local function getOutputSize(net, inputDims)
	return net:forward(torch.Tensor(torch.LongStorage(inputDims))):size():totable()
end

-- Constructor
function Body:_init(opts)
	opts = opts or {}
	self.histLen = opts.histLen
	self.obsSpec = opts.obsSpec
	self.hiddenSize = opts.hiddenSize
	self.action_num = opts.action_num
	self.constant_sigma = opts.constant_sigma
end

function Body:createBody()
	local histLen = self.histLen--number of consecutive states

	local net = nn.Sequential()

	obsInputDim = clone(self.obsSpec[2])
	obsInputDim[1] = obsInputDim[1] * histLen
	local NormalNet = nn.Sequential()
	NormalNet:add(nn.View(obsInputDim[1], obsInputDim[2], obsInputDim[3]))
	NormalNet:add(nn.SpatialConvolution(obsInputDim[1], 16, 8, 8, 4, 4))
	NormalNet:add(nn.ReLU(true))
	NormalNet:add(nn.SpatialConvolution(16, 16, 8, 8, 4, 4, 4, 4))
	NormalNet:add(nn.ReLU(true))
	NormalNet:add(nn.SpatialConvolution(16, 32, 4, 4, 2, 2))
	NormalNet:add(nn.ReLU(true))
	NormalNet:add(nn.SpatialConvolution(32, 32, 4, 4, 2, 2, 2, 2))
	NormalNet:add(nn.ReLU(true))
	NormalNet:add(nn.View(torch.prod(torch.Tensor(getOutputSize(NormalNet, obsInputDim)))))

	net:add(NormalNet)
	--why not use the previous calculated bodyOutputSize?????
	example_input = torch.Tensor(torch.LongStorage(obsInputDim))
	bodyOutputSize = table.unpack(net:forward(example_input):size():totable())
--print (bodyOutputSize)
	net:add(nn.Linear(bodyOutputSize, self.hiddenSize))

	net:add(nn.ReLU(true))
	-- net:add(nn.Linear(self.hiddenSize, self.hiddenSize / 2))
	-- net:add(nn.ReLU(true))

	local valueAndPolicy = nn.ConcatTable()

	local valueFunction = nn.Linear(self.hiddenSize, 1)

	local policy = nn.Sequential()
	policy:add(nn.Linear(self.hiddenSize, self.action_num))
	policy:add(nn.SoftMax())

	valueAndPolicy:add(valueFunction)
	valueAndPolicy:add(policy)

	net:add(valueAndPolicy)

-- 	theta = net:getParameters()
-- 	local weights = torch.load("best.weights.t7")
-- 	theta:copy(weights)

-- local net2 = nn.Sequential()

-- 	obsInputDim = clone(self.obsSpec[2])
-- 	obsInputDim[1] = obsInputDim[1] * histLen
	
-- 	net2:add(NormalNet)
-- 	--why not use the previous calculated bodyOutputSize?????
-- 	example_input = torch.Tensor(torch.LongStorage(obsInputDim))
-- 	bodyOutputSize = table.unpack(net2:forward(example_input):size():totable())

-- 	net2:add(net.modules[2])

-- 	net2:add(nn.ReLU(true))
-- 	-- net:add(nn.Linear(self.hiddenSize, self.hiddenSize / 2))
-- 	-- net:add(nn.ReLU(true))

-- 	local valueAndPolicy = nn.ConcatTable()

	
-- 	valueAndPolicy:add(valueFunction)
-- 	valueAndPolicy:add(policy)

-- 	net2:add(valueAndPolicy)
-- 	-- theta = net2:getParameters()
-- 	-- local weights = torch.load("best.weights.t7")
-- 	-- theta:copy(weights)


	return net
end

return Body
