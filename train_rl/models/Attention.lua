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
	self.attentionSpec = opts.attentionSpec
	self.hiddenSize = opts.hiddenSize
	self.action_num = opts.action_num
end

function Body:createBody()
	local histLen = self.histLen

	local net = nn.Sequential()

	local ANN = nn.ParallelTable()

	obsInputDim = clone(self.obsSpec[2])
	obsInputDim[1] = obsInputDim[1] * histLen
	local NormalNet = nn.Sequential()
	NormalNet:add(nn.View(obsInputDim[1], obsInputDim[2], obsInputDim[3]))
	NormalNet:add(nn.SpatialConvolution(obsInputDim[1], 16, 8, 8, 4, 4, 1, 1))
	NormalNet:add(nn.ReLU(true))
	NormalNet:add(nn.SpatialConvolution(16, 32, 4, 4, 2, 2))
	NormalNet:add(nn.ReLU(true))
	NormalNet:add(nn.View(torch.prod(torch.Tensor(getOutputSize(NormalNet, obsInputDim)))))

	attentionInputDim = clone(self.attentionSpec[2])
	attentionInputDim[1] = attentionInputDim[1] * histLen
	local AttentionNet = nn.Sequential()
	AttentionNet:add(nn.View(attentionInputDim[1], attentionInputDim[2], attentionInputDim[3]))
	AttentionNet:add(nn.SpatialConvolution(attentionInputDim[1], 16, 8, 8, 4, 4, 1, 1))
	AttentionNet:add(nn.ReLU(true))
	AttentionNet:add(nn.SpatialConvolution(16, 32, 4, 4, 2, 2))
	AttentionNet:add(nn.ReLU(true))
	AttentionNet:add(nn.View(torch.prod(torch.Tensor(getOutputSize(AttentionNet, attentionInputDim)))))

	ANN:add(NormalNet)
	ANN:add(AttentionNet)

	net:add(ANN)
	net:add(nn.JoinTable(1))

	example_input = {torch.Tensor(torch.LongStorage(obsInputDim)), torch.Tensor(torch.LongStorage(attentionInputDim))}
	bodyOutputSize = table.unpack(net:forward(example_input):size():totable())
	net:add(nn.Linear(bodyOutputSize, self.hiddenSize))
	net:add(nn.ReLU(true))

	local value_mu_sigma = nn.ConcatTable()

	local valueFunction = nn.Linear(self.hiddenSize, 1)

	local action_mu = nn.Sequential()
	action_mu:add(nn.Linear(self.hiddenSize, self.action_num))

	local action_sigma = nn.Sequential()
	action_sigma:add(nn.Linear(self.hiddenSize, self.action_num))
	action_sigma:add(nn.SoftPlus())
	-- action_sigma:add(nn.AddConstant(1))

	local coord_mu = nn.Sequential()
	coord_mu:add(nn.Linear(self.hiddenSize, 4))

	local coord_sigma = nn.Sequential()
	coord_sigma:add(nn.Linear(self.hiddenSize, 4))
	coord_sigma:add(nn.SoftPlus())
	-- coord_sigma:add(nn.AddConstant(1))

	value_mu_sigma:add(valueFunction)
    value_mu_sigma:add(action_mu)
    value_mu_sigma:add(action_sigma)
    value_mu_sigma:add(coord_mu)
    value_mu_sigma:add(coord_sigma)

    net:add(value_mu_sigma)
	return net
end

return Body
