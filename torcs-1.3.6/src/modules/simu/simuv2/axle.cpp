/***************************************************************************

    file                 : axle.cpp
    created              : Sun Mar 19 00:05:09 CET 2000
    copyright            : (C) 2000-2013 by Eric Espie, Bernhard Wymann
    email                : torcs@free.fr
    version              : $Id: axle.cpp,v 1.8.2.5 2013/08/29 14:21:44 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sim.h"

static const char *AxleSect[2] = {SECT_FRNTAXLE, SECT_REARAXLE};

void SimAxleConfig(tCar *car, int index)
{
	void	*hdle = car->params;
	tdble	rollCenter;
	
	tAxle *axle = &(car->axle[index]);
	
	axle->xpos = GfParmGetNum(hdle, AxleSect[index], PRM_XPOS, (char*)NULL, 0.0f);
	axle->I    = GfParmGetNum(hdle, AxleSect[index], PRM_INERTIA, (char*)NULL, 0.15f);
	rollCenter = GfParmGetNum(hdle, AxleSect[index], PRM_ROLLCENTER, (char*)NULL, 0.15f);
	car->wheel[index*2].rollCenter = car->wheel[index*2+1].rollCenter = rollCenter;
	
	tdble x0 = GfParmGetNum(hdle, AxleSect[index], PRM_SUSPCOURSE, (char*)NULL, 0.0f);
	SimSuspConfig(hdle, AxleSect[index], &(axle->thirdSusp), 0.0f, x0);

	if (index == 0) {
		axle->arbSuspSpringK = GfParmGetNum(hdle, SECT_FRNTARB, PRM_SPR, (char*)NULL, 0.0f);
	} else {
		axle->arbSuspSpringK = GfParmGetNum(hdle, SECT_REARARB, PRM_SPR, (char*)NULL, 0.0f);
	}
	
	car->wheel[index*2].feedBack.I += axle->I / 2.0;
	car->wheel[index*2+1].feedBack.I += axle->I / 2.0;
}


void SimAxleReConfig(tCar *car, int index)
{
	tAxle *axle = &(car->axle[index]);

	// Anti rollbar spring
	tCarPitSetupValue* v = &car->carElt->pitcmd.setup.arbspring[index];
	if (SimAdjustPitCarSetupParam(v)) {
		axle->arbSuspSpringK = v->value;
	}

	// Third element
	v = &car->carElt->pitcmd.setup.thirdX0[index];
	SimAdjustPitCarSetupParam(v);
	SimSuspThirdReConfig(car, index, &(axle->thirdSusp), 0.0f, v->value);
}


void SimAxleUpdate(tCar *car, int index)
{
	tAxle *axle = &(car->axle[index]);
	tdble str, stl, vr, vl;
	
	str = car->wheel[index*2].susp.x;
	stl = car->wheel[index*2+1].susp.x;
	vr = car->wheel[index*2].susp.v;
	vl = car->wheel[index*2+1].susp.v;
	
	// Anti roll bar
	tdble farb = axle->arbSuspSpringK * (stl - str);

	// Third element
	axle->thirdSusp.x = (stl + str)/2.0f;
	axle->thirdSusp.v = (vl + vr)/2.0f;
	SimSuspUpdate(&axle->thirdSusp);
	tdble fthird = 0.0f;
	if (axle->thirdSusp.x < axle->thirdSusp.spring.xMax) {
		fthird = axle->thirdSusp.force/2.0f;
	}

	// right
	car->wheel[index*2].axleFz = farb + fthird;
	// left
	car->wheel[index*2+1].axleFz = -farb + fthird;
}



