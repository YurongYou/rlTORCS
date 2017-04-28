// -*- Mode: c++ -*-
/***************************************************************************

    file                 : grsound.h
    created              : Thu Aug 17 23:57:35 CEST 2000
    copyright            : (C) 2000-2004 by Eric Espie, Christos Dimitrakakis
    email                : torcs@free.fr
    version              : $Id: grsound.h,v 1.7.2.1 2013/09/01 10:24:22 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _GRSOUND_H_
#define _GRSOUND_H_

#include "grcam.h"
#include <raceman.h>
#include <plib/sl.h>

extern void grInitSound(tSituation* s, int ncars);
extern void grShutdownSound(int ncars);
extern float grRefreshSound(tSituation *s, cGrCamera *camera);
extern void grMuteForMenu(void);


#endif /* _GRSOUND_H_ */ 



