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

    local net_trained = nn.Sequential()

    obsInputDim = clone(self.obsSpec[2])
    obsInputDim[1] = obsInputDim[1] * histLen
    local NormalNet = nn.Sequential()
    NormalNet:add(nn.View(obsInputDim[1], obsInputDim[2], obsInputDim[3]))
    NormalNet:add(nn.SpatialConvolution(obsInputDim[1], 32, 8, 8, 4, 4))
    NormalNet:add(nn.ReLU(true))
    NormalNet:add(nn.SpatialConvolution(32, 32, 4, 4, 2, 2))
    NormalNet:add(nn.ReLU(true))    
    NormalNet:add(nn.SpatialConvolution(32, 32, 3, 3, 1, 1, 1, 1))
    NormalNet:add(nn.ReLU(true))
    NormalNet:add(nn.View(torch.prod(torch.Tensor(getOutputSize(NormalNet, obsInputDim)))))

    net_trained:add(NormalNet)
    --why not use the previous calculated bodyOutputSize?????
    example_input = torch.Tensor(torch.LongStorage(obsInputDim))
    bodyOutputSize = table.unpack(net_trained:forward(example_input):size():totable())

    net_trained:add(nn.Linear(bodyOutputSize, self.hiddenSize))

    net_trained:add(nn.ReLU(true))
    -- net:add(nn.Linear(self.hiddenSize, self.hiddenSize / 2))
    -- net:add(nn.ReLU(true))

    local valueAndPolicy = nn.ConcatTable()

    local valueFunction = nn.Linear(self.hiddenSize, 1)

    local policy = nn.Sequential()
    policy:add(nn.Linear(self.hiddenSize, self.action_num))
    policy:add(nn.SoftMax())

    valueAndPolicy:add(valueFunction)
    valueAndPolicy:add(policy)

    net_trained:add(valueAndPolicy)

    local theta, dtheta = net_trained:getParameters();
    local weights = torch.load("best.weights.t7")
    theta:copy(weights)





	local net = nn.Sequential()
    local frontEnd = nn.ParallelTable()
    frontEnd:add(nn.Identity())

--    local outputdim = getOutputSize(NormalNet, obsInputDim)
  --  print(outputdim)
    NormalNet:add(nn.View(32, 81))
    frontEnd:add(NormalNet)

    net:add(frontEnd)
    net:add(nn.MM())
    local flat_size = 32*81
    net:add(nn.View(flat_size))
    local linear = nn.Linear(flat_size, self.hiddenSize)
    --print(net_trained.modules[2])
    --print(type(net_trained))
    -- linear.weight:copy(net_trained.modules[2].weight)
    -- net:add(linear)
    net:add(net_trained.modules[2])
    net:add(nn.ReLU(true))

	local value_policy_attention = nn.ConcatTable()

    local attention = nn.Sequential()
    attention:add(nn.Linear(self.hiddenSize, self.semantic_attention_dim))
    attention:add(nn.SoftMax())
    --attention:add(nn.L1Penalty(0.001))
    attention:add(nn.MulConstant(self.semantic_attention_dim, true))
    attention:add(nn.MulConstant(self.attention_alpha, true))
    attention:add(nn.AddConstant(1-self.attention_alpha,true))
        
	value_policy_attention:add(valueFunction)
	value_policy_attention:add(policy)
    value_policy_attention:add(attention)

	net:add(value_policy_attention)
	return net
end

return Body
