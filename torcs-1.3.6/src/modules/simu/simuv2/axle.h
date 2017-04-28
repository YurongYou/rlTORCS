/***************************************************************************

    file                 : axle.h
    created              : Sun Mar 19 00:05:17 CET 2000
    copyright            : (C) 2000-2013 by Eric Espie, Bernhard Wymann
    email                : torcs@free.fr
    version              : $Id: axle.h,v 1.4.2.3 2013/08/29 13:03:48 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


 
#ifndef _AXLE_H__
#define _AXLE_H__

#include "differential.h"

typedef struct
{
    tdble xpos;

	tdble arbSuspSpringK;
	tSuspension thirdSusp;	// Third element
    tdble	wheight0;

    /* dynamic */
    tdble	force[2]; /* right and left */

    tdble	I;	/* including differential inertia but not wheels */
} tAxle;


#endif /* _AXLE_H__ */ 



