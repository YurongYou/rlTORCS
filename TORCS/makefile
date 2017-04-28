.PHONY: clean

LIBOPTS = -I$(HOME)/torch/install/include -L$(HOME)/torch/install/lib -lTH
CFLAGS = -fPIC -shared -g
CC = gcc

TORCSctrl.so : TORCSctrl.cpp
	$(CC) $< $(LIBOPTS) $(CFLAGS) -o $@

clean :
	@rm *.so