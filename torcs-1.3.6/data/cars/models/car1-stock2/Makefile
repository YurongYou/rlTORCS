##############################################################################
#
#    file                 : Makefile
#    created              : Wed Feb 5 07:29:59 CET 2012
#    copyright            : (C) 2013 by Bernhard Wymann                         
#    email                : berniw@bluewin.ch
#    version              : $Id: Makefile,v 1.1.2.1 2013/02/05 06:52:47 berniw Exp $                                  
#
##############################################################################
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
##############################################################################

CAR       = car1-stock2

DATADIR   = cars/$(CAR)

DATA      = $(CAR).xml readme.txt \
            $(CAR).rgb tex-wheel.rgb shadow.rgb driver.rgb carbon-128.rgb \
            $(shell find *.acc -maxdepth 0 -type f -print)

data_PKGFILES	= $(DATA) $(shell find *.xcf -maxdepth 0 -type f -print)

data_PKGDIR	= $(DATADIR)

include ${MAKE_DEFAULT}


