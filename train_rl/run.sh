#!/bin/bash

# Switch to script directory
cd `dirname -- "$0"`

TEST=$1
shift

export OMP_NUM_THREADS=1

if [[ "$TEST" == 'discrete' ]]; then
  th main.lua -seed 1 -server false -resume false -discrete true -gpu 0 -game_config quickrace_discrete_slowmulti_ushite-city.xml -env TORCS.TorcsDiscreteConstDamagePos -threads 3 -model models.Discrete_action -hiddenSize 256 -height 84 -width 84 -batchSize 5 -momentum 0.99 -rmsEpsilon 0.1 -steps 20000000 -optimiser sharedRmsProp -eta 0.00007 -gradClip 0 -rewardClip 0 -valSteps 100 -valFreq 1000 -tensorType torch.FloatTensor -entropyBeta 0.01 -progFreq 5000 -verbose false -foldername train_TORCS_slowmulti_ushite_city -group 0 "$@"

elif [[ "$TEST" == 'eval_TORCS' ]]; then
  qlua main.lua -mode eval -foldername train_TORCS_slowmulti_ushite_city "$@"

elif [[ "$TEST" == 'train_TORCS_slowmulti_ushite_city_seed1' ]]; then
  th main.lua -seed 1 -server true -resume true -discrete true -gpu 0 -game_config quickrace_discrete_slowmulti_ushite-city.xml -env TORCS.TorcsDiscreteConstDamagePos -threads 12 -model models.Discrete_action -hiddenSize 256 -height 84 -width 84 -batchSize 5 -momentum 0.99 -rmsEpsilon 0.1 -steps 20000000 -optimiser sharedRmsProp -eta 0.00007 -gradClip 0 -rewardClip 0 -valSteps 18750 -valFreq 250000 -tensorType torch.FloatTensor -entropyBeta 0.01 -progFreq 5000 -verbose false -foldername train_TORCS_slowmulti_ushite_city -group 0 "$@"

elif [[ "$TEST" == 'train_TORCS_supermulti_ushite_city' ]]; then
  th main.lua -server true -resume true -discrete true -gpu 0 -game_config quickrace_discrete_supermulti_ushite-city.xml -use_attention false -env TORCS.TorcsDiscreteConstDamagePos -threads 12 -model models.Discrete_action -hiddenSize 256 -height 84 -width 84 -batchSize 5 -momentum 0.99 -rmsEpsilon 0.1 -steps 20000000 -optimiser sharedRmsProp -eta 0.00007 -gradClip 0 -rewardClip 0 -valSteps 1875 -valFreq 25000 -tensorType torch.FloatTensor -entropyBeta 0.01 -progFreq 5000 -verbose false -foldername train_TORCS_supermulti_ushite_city -group 2 "$@"

elif [[ "$TEST" == 'train_TORCS_back_nodamage_slowmulti_ushite_city' ]]; then
  th main.lua -server true -resume true -discrete true -gpu 0 -game_config quickrace_discrete_slowmulti_ushite-city.xml -auto_back true -use_attention false -env TORCS.TorcsDiscreteConstNoDamagePos -threads 12 -model models.Discrete_action -hiddenSize 256 -height 84 -width 84 -batchSize 5 -momentum 0.99 -rmsEpsilon 0.1 -steps 20000000 -optimiser sharedRmsProp -eta 0.00007 -gradClip 0 -rewardClip 0 -valSteps 1875 -valFreq 25000 -tensorType torch.FloatTensor -entropyBeta 0.01 -progFreq 5000 -verbose false -foldername train_TORCS_back_nodamage_slowmulti_ushite_city -group 2 "$@"

elif [[ "$TEST" == 'eval_test' ]]; then
  th Evaluations/EvaluationMaster.lua -use_best_weight true -weightFolder train_TORCS_slowmulti_ushite_city -threads 1 -threshold 0.1 -begin 20 -finish 20 -type event "$@"
else
  echo "Invalid options"
fi
