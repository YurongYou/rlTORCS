local GradualMix, parent = torch.class('nn.GradualMix', 'nn.Module')
local threads = require 'threads'
local tds = require 'tds'
threads.Threads.serialization('threads.sharedserialize')

function GradualMix:__init(atomic, total_steps)
  parent.__init(self)
  self.total_steps = total_steps
  self.atomic = atomic
  self.currentAlpha = 1 - (self.total_steps - self.atomic:get()) / self.total_steps
end

function GradualMix:updateOutput(input)
  self.currentAlpha = 1 - (self.total_steps - self.atomic:get()) / self.total_steps
  self.output:resizeAs(input)
  self.output:copy(input)
  self.output:mul(self.currentAlpha):add(1 - self.currentAlpha)
  return self.output
end

function GradualMix:updateGradInput(input, gradOutput)
  if self.gradInput then
    self.gradInput:resizeAs(gradOutput)
    self.gradInput:copy(gradOutput)
    self.gradInput:mul(self.currentAlpha)
    return self.gradInput
  end
end