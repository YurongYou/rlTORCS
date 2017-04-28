/***************************************************************************

    file                 : susp.cpp
    created              : Sun Mar 19 00:08:41 CET 2000
    copyright            : (C) 2000 by Eric Espie
    email                : torcs@free.fr
    version              : $Id: susp.cpp,v 1.10.2.7 2014/02/10 07:53:35 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include "sim.h"

/*
 * b2 and b3 calculus
 */
static void initDamper(tSuspension *susp)
{
	tDamper *damp;
	
	damp = &(susp->damper);	
	damp->bump.b2 = (damp->bump.C1 - damp->bump.C2) * damp->bump.v1;
	damp->rebound.b2 = (damp->rebound.C1 - damp->rebound.C2) * damp->rebound.v1;
}




/*
 * get damper force
 */
static tdble damperForce(tSuspension *susp)
{
	tDamperDef *dampdef;
	tdble     f;
	tdble     av;
	tdble     v;

	v = susp->v;
	
	if (fabs(v) > 10.0f) {
		v = SIGN(v) * 10.0f;
	}
	
	if (v < 0.0f) {
		/* rebound */
		dampdef = &(susp->damper.rebound);
	} else {
		/* bump */
		dampdef = &(susp->damper.bump);
	}
	
	av = fabs(v);
	if (av < dampdef->v1) {
		f = (dampdef->C1 * av);
	} else {
		f = (dampdef->C2 * av + dampdef->b2);
	}
	
	f *= SIGN(v);
	
	return f;
}




/*
 * get spring force
 */
static tdble springForce(tSuspension *susp)
{
	tSpring *spring = &(susp->spring);
	tdble f;
	
	/* K is < 0 */
	f = spring->K * (susp->x - spring->x0) + spring->F0;
	if (f < 0.0f) {
		f = 0.0f;
	}
	
	return f;
}




void SimSuspCheckIn(tSuspension *susp)
{
	susp->state = 0;
	if (susp->x < susp->spring.packers) {
		susp->x = susp->spring.packers;
		susp->state = SIM_SUSP_COMP;
	}
	susp->x *= susp->spring.bellcrank;
	if (susp->x > susp->spring.xMax) {
		susp->x = susp->spring.xMax;
		susp->state = SIM_SUSP_EXT;
	}
}




void SimSuspUpdate(tSuspension *susp)
{
	susp->force = (springForce(susp) + damperForce(susp)) * susp->spring.bellcrank;
}




void SimSuspConfig(void *hdle, const char *section, tSuspension *susp, tdble F0, tdble X0)
{
	susp->spring.K          = GfParmGetNum(hdle, section, PRM_SPR, (char*)NULL, 175000.0f);
	susp->spring.xMax       = GfParmGetNum(hdle, section, PRM_SUSPCOURSE, (char*)NULL, 0.5f);
	susp->spring.bellcrank  = GfParmGetNum(hdle, section, PRM_BELLCRANK, (char*)NULL, 1.0f);
	susp->spring.packers    = GfParmGetNum(hdle, section, PRM_PACKERS, (char*)NULL, 0.0f);
	susp->damper.bump.C1    = GfParmGetNum(hdle, section, PRM_SLOWBUMP, (char*)NULL, 0.0f);
	susp->damper.rebound.C1 = GfParmGetNum(hdle, section, PRM_SLOWREBOUND, (char*)NULL, 0.0f);
	susp->damper.bump.C2    = GfParmGetNum(hdle, section, PRM_FASTBUMP, (char*)NULL, susp->damper.bump.C1);
	susp->damper.rebound.C2 = GfParmGetNum(hdle, section, PRM_FASTREBOUND, (char*)NULL, susp->damper.rebound.C1);
	susp->damper.bump.v1	= GfParmGetNum(hdle, section, PRM_BUMPTHRESHOLD, (char*)NULL, 0.5f);
	susp->damper.rebound.v1	= GfParmGetNum(hdle, section, PRM_REBOUNDTHRESHOLD, (char*)NULL, 0.5f);

	susp->spring.x0 = susp->spring.bellcrank * X0;
	susp->spring.F0 = F0 / susp->spring.bellcrank;
	susp->spring.K = - susp->spring.K;
	
	initDamper(susp);
}


void SimSuspReConfig(tCar* car, int index, tSuspension *susp, tdble F0, tdble X0)
{
	// Spring
	tCarPitSetupValue* v = &car->carElt->pitcmd.setup.suspspring[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->spring.K = - v->value;	
	}

	// Packers
	v = &car->carElt->pitcmd.setup.susppackers[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->spring.packers = v->value;
	}

	// Slow bump
	v = &car->carElt->pitcmd.setup.suspslowbump[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->damper.bump.C1 = v->value;
	}

	// Slow rebound
	v = &car->carElt->pitcmd.setup.suspslowrebound[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->damper.rebound.C1 = v->value;
	}

	// Fast bump
	v = &car->carElt->pitcmd.setup.suspfastbump[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->damper.bump.C2 = v->value;
	}

	// Fast rebound
	v = &car->carElt->pitcmd.setup.suspfastrebound[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->damper.rebound.C2 = v->value;
	}

	susp->spring.x0 = susp->spring.bellcrank * X0;
	susp->spring.F0 = F0 / susp->spring.bellcrank;

	initDamper(susp);
}


void SimSuspThirdReConfig(tCar* car, int index, tSuspension *susp, tdble F0, tdble X0)
{
	// Spring
	tCarPitSetupValue* v = &car->carElt->pitcmd.setup.thirdspring[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->spring.K = - v->value;
	}

	// Bump
	v = &car->carElt->pitcmd.setup.thirdbump[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->damper.bump.C1 = v->value;
		susp->damper.bump.C2 = v->value;
	}

	// Rebound
	v = &car->carElt->pitcmd.setup.thirdrebound[index];
	if (SimAdjustPitCarSetupParam(v)) {
		susp->damper.rebound.C1 = v->value;
		susp->damper.rebound.C2 = v->value;
	}

	susp->spring.xMax = X0;

	susp->spring.x0 = susp->spring.bellcrank * X0;
	susp->spring.F0 = F0 / susp->spring.bellcrank;

	initDamper(susp);
}
