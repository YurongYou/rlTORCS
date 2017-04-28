#!/bin/bash
LB="\033[1;34m"
NC='\033[0m'
cd `dirname "$0"`
if [[ -z "$LC_ALL" ]]; then
	export LC_ALL=C
fi
printf "${LB}>> Begin installing dependencies\n${NC}"
sudo apt-get upgrade -y
sudo apt-get update -y
sudo apt-get install build-essential -y
sudo apt-get install xvfb -y
if ! hash git 2>/dev/null; then sudo apt-get install git -y >/dev/null; fi
printf "${LB}>> All dependencies installed\n${NC}"

# installing TORCS
printf "${LB}>> Begin installing TORCS\n${NC}"
sudo apt-get build-dep torcs -y
sudo apt-get update -y
cd torcs-1.3.6
make
sudo make install
sudo make datainstall
cd ..
printf "${LB}>> TORCS installed\n${NC}"

# install torch packages
printf "${LB}>> Begin installing lua packages\n${NC}"
lua_packages=(luaposix logroll classic tds)
for name in ${lua_packages[@]};
do
	printf "\t${LB}>> Try finding ${name}\n${NC}"
	if ! $(luarocks list 2>/dev/null | grep ${name} >/dev/null); then
		printf "\t${LB}>> Installing ${name}\n${NC}"
		luarocks install ${name}
	fi
	printf "\t${LB}>> ${name} installed\n${NC}"
done

# compile torcs controller
printf "${LB}>> Begin Compiling TORCSctrl.so \n${NC}"
cd TORCS
make
cd ..
printf "${LB}>> Compilation Finish\n${NC}"