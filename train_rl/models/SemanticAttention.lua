local nn = require 'nn'
require 'classic.torch' -- Enables serialisation

Body = classic.class('Body')
-- Calculates network output size
local function getOutputSize(net, inputDims)
	return net:forward(torch.Tensor(torch.LongStorage(inputDims))):size():totable()
end

-- dirty table clone
local function clone(org)
	return {table.unpack(org)}
end

-- Constructor
function Body:_init(opts)
	opts = opts or {}
	self.histLen = opts.histLen
	self.obsSpec = opts.obsSpec
	self.hiddenSize = opts.hiddenSize
	self.action_num = opts.action_num
    self.semantic_attention_dim = opts.semantic_attention_dim
    self.attention_alpha = opts.attention_alpha
end

function Body:createBody()
	local histLen = self.histLen--number of consecutive states

	local net = nn.Sequential()
    local frontEnd = nn.ParallelTable()
    frontEnd:add(nn.Identity())

    obsInputDim = clone(self.obsSpec[2])
    obsInputDim[1] = obsInputDim[1] * histLen

    local CNN = nn.Sequential()
	CNN:add(nn.View(obsInputDim[1], obsInputDim[2], obsInputDim[3]))
	CNN:add(nn.SpatialConvolution(obsInputDim[1], 32, 8, 8, 4, 4))
	CNN:add(nn.ReLU(true))
	CNN:add(nn.SpatialConvolution(32, 32, 4, 4, 2, 2))
	CNN:add(nn.ReLU(true))
	CNN:add(nn.SpatialConvolution(32, 32, 3, 3, 1, 1, 1, 1))
    CNN:add(nn.ReLU(true))
    local outputdim = getOutputSize(CNN, obsInputDim)

    CNN:add(nn.View(outputdim[1], outputdim[2] * outputdim[3]))
    frontEnd:add(CNN)
    self.CNN = CNN

    net:add(frontEnd)
    net:add(nn.MM())
    local flat_size = outputdim[1] * outputdim[2] * outputdim[3]
    net:add(nn.View(flat_size))

    net:add(nn.Linear(flat_size, self.hiddenSize))
    net:add(nn.ReLU(true))

	local value_policy_attention = nn.ConcatTable()

	local valueFunction = nn.Linear(self.hiddenSize, 1)

	local policy = nn.Sequential()
	policy:add(nn.Linear(self.hiddenSize, self.action_num))
	policy:add(nn.SoftMax())

    local attention = nn.Sequential()
    attention:add(nn.Linear(self.hiddenSize, self.semantic_attention_dim))
    attention:add(nn.SoftMax())
    attention:add(nn.MulConstant(self.semantic_attention_dim, true))
    attention:add(nn.MulConstant(self.attention_alpha, true))
    attention:add(nn.AddConstant(1-self.attention_alpha,true))

	value_policy_attention:add(valueFunction)
	value_policy_attention:add(policy)
    value_policy_attention:add(attention)

	net:add(value_policy_attention)
    -- local t,dt = net:getParameters()
    -- print(#t)
    -- local params, gp = net:parameters()
    -- print(#gp)
    -- for i = 1,12 do
    --     print(#gp[i])
    -- end
    --print(gp[6][3])


	return net
end

function Body:getCNN()
    return self.CNN.modules
end

return Body
