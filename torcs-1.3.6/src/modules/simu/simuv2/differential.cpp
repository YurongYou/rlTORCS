/***************************************************************************

    file                 : differential.cpp
    created              : Sun Mar 19 00:06:33 CET 2000
    copyright            : (C) 2000-2013 by Eric Espie, Bernhard Wymann
    email                : torcs@free.fr
    version              : $Id: differential.cpp,v 1.11.2.7 2013/09/03 13:49:50 berniw Exp $

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

void SimDifferentialConfig(void *hdle, const char *section, tDifferential *differential)
{
	differential->I		= GfParmGetNum(hdle, section, PRM_INERTIA, (char*)NULL, 0.1f);
	differential->efficiency	= GfParmGetNum(hdle, section, PRM_EFFICIENCY, (char*)NULL, 1.0f);
	differential->ratio		= GfParmGetNum(hdle, section, PRM_RATIO, (char*)NULL, 1.0f);
	differential->dTqMin	= GfParmGetNum(hdle, section, PRM_MIN_TQ_BIAS, (char*)NULL, 0.05f);
	differential->dTqMax	= GfParmGetNum(hdle, section, PRM_MAX_TQ_BIAS, (char*)NULL, 0.80f) - differential->dTqMin;
	if (differential->dTqMax < 0.0f) differential->dTqMax = 0.0f;
	differential->dSlipMax	= GfParmGetNum(hdle, section, PRM_MAX_SLIP_BIAS, (char*)NULL, 0.03f);
	differential->lockInputTq	= GfParmGetNum(hdle, section, PRM_LOCKING_TQ, (char*)NULL, 3000.0f);
	differential->lockBrakeInputTq = GfParmGetNum(hdle, section, PRM_LOCKINGBRAKE_TQ, (char*)NULL, differential->lockInputTq*0.33f);
	differential->viscosity	= GfParmGetNum(hdle, section, PRM_VISCOSITY_FACTOR, (char*)NULL, 1.0f);
	
	const char* type = GfParmGetStr(hdle, section, PRM_TYPE, VAL_DIFF_NONE);
	if (strcmp(type, VAL_DIFF_LIMITED_SLIP) == 0) {
		differential->type = DIFF_LIMITED_SLIP; 
	} else if (strcmp(type, VAL_DIFF_VISCOUS_COUPLER) == 0) {
		differential->type = DIFF_VISCOUS_COUPLER;
	} else if (strcmp(type, VAL_DIFF_SPOOL) == 0) {
		differential->type = DIFF_SPOOL;
	}  else if (strcmp(type, VAL_DIFF_FREE) == 0) {
		differential->type = DIFF_FREE;
	} else {
		differential->type = DIFF_NONE; 
	}
		
	differential->feedBack.I = differential->I * differential->ratio * differential->ratio +
		(differential->inAxis[0]->I + differential->inAxis[1]->I) / differential->efficiency;
}


void SimDifferentialReConfig(tCar* car, int index)
{	
	tDifferential *differential = &car->transmission.differential[index];

	// Ratio
	tCarPitSetupValue* v = &car->carElt->pitcmd.setup.diffratio[index];
	if (SimAdjustPitCarSetupParam(v)) {
		differential->ratio = v->value;
		differential->feedBack.I = differential->I * differential->ratio * differential->ratio +
			(differential->inAxis[0]->I + differential->inAxis[1]->I) / differential->efficiency;
	}

	// Min torque bias
	v = &car->carElt->pitcmd.setup.diffmintqbias[index];
	if (SimAdjustPitCarSetupParam(v)) {
		differential->dTqMin = v->value;
	}

	// Max torque bias
	v = &car->carElt->pitcmd.setup.diffmaxtqbias[index];
	if (SimAdjustPitCarSetupParam(v)) {
		differential->dTqMax = v->value - differential->dTqMin;
		if (differential->dTqMax < 0.0f) {
			differential->dTqMax = 0.0f;
			v->value = differential->dTqMin;
		}
	}

	// Slip bias
	v = &car->carElt->pitcmd.setup.diffslipbias[index];
	if (SimAdjustPitCarSetupParam(v)) {
		differential->dSlipMax = v->value;
	}

	// Locking input torque
	v = &car->carElt->pitcmd.setup.difflockinginputtq[index];
	if (SimAdjustPitCarSetupParam(v)) {
		differential->lockInputTq = v->value;
	}

	// Locking brake input torque
	v = &car->carElt->pitcmd.setup.difflockinginputbraketq[index];
	if (SimAdjustPitCarSetupParam(v)) {
		differential->lockBrakeInputTq = v->value;
	}
}


static void updateSpool(tCar *car, tDifferential *differential, int first)
{
	tdble	DrTq;
	tdble	ndot;
	tdble	spinVel;
	tdble	BrTq;
	tdble	engineReaction;
	tdble	I;
	tdble	inTq, brkTq;
	
	DrTq = differential->in.Tq;
	
	I = differential->outAxis[0]->I + differential->outAxis[1]->I;
	inTq = differential->inAxis[0]->Tq + differential->inAxis[1]->Tq;
	brkTq = differential->inAxis[0]->brkTq + differential->inAxis[1]->brkTq;
	
	ndot = SimDeltaTime * (DrTq - inTq) / I;
	spinVel = differential->inAxis[0]->spinVel + ndot;
	
	BrTq = - SIGN(spinVel) * brkTq;
	ndot = SimDeltaTime * BrTq / I;
	
	if (((ndot * spinVel) < 0.0) && (fabs(ndot) > fabs(spinVel))) {
		ndot = -spinVel;
	}
	if ((spinVel == 0.0) && (ndot < 0.0)) ndot = 0;
	
	spinVel += ndot;
	if (first) {
		engineReaction = SimEngineUpdateRpm(car, spinVel);
		if (engineReaction != 0.0) {
			spinVel = engineReaction;
		}
	}
	differential->outAxis[0]->spinVel = differential->outAxis[1]->spinVel = spinVel;
	
	differential->outAxis[0]->Tq = (differential->outAxis[0]->spinVel - differential->inAxis[0]->spinVel) / SimDeltaTime * differential->outAxis[0]->I;
	differential->outAxis[1]->Tq = (differential->outAxis[1]->spinVel - differential->inAxis[1]->spinVel) / SimDeltaTime * differential->outAxis[1]->I;
}


void 
SimDifferentialUpdate(tCar *car, tDifferential *differential, int first)
{
	tdble	DrTq, DrTq0, DrTq1;
	tdble	ndot0, ndot1;
	tdble	spinVel0, spinVel1;
	tdble	inTq0, inTq1;
	tdble	spdRatioMax, commomSpinVel;
	tdble	deltaSpd, deltaTq, bias, lockTq, biassign;
	tdble	BrTq;
	tdble	engineReaction;
	tdble	meanv;
	
	if (differential->type == DIFF_SPOOL) {
		updateSpool(car, differential, first);
		return;
	}
	
	DrTq = differential->in.Tq;
	
	spinVel0 = differential->inAxis[0]->spinVel;
	spinVel1 = differential->inAxis[1]->spinVel;
	
	inTq0 = differential->inAxis[0]->Tq;
	inTq1 = differential->inAxis[1]->Tq;

	commomSpinVel = fabs(spinVel0) + fabs(spinVel1);
	if (commomSpinVel != 0) {
		tdble spdRatio = fabs(spinVel0 - spinVel1) / commomSpinVel;
		
		switch (differential->type) {
		case DIFF_FREE:				
			{
				float spiderTq = inTq1 - inTq0;
				DrTq0 = (DrTq + spiderTq)*0.5f;
				DrTq1 = (DrTq - spiderTq)*0.5f;
			}
			break;
		case DIFF_LIMITED_SLIP:
			if (DrTq > differential->lockInputTq || DrTq < -differential->lockBrakeInputTq) {
				updateSpool(car, differential, first);
				return;
			}

			if (DrTq >= 0.0f) {
				lockTq = differential->lockInputTq;
				biassign = 1.0f;
			} else {
				lockTq = -differential->lockBrakeInputTq;
				biassign = -1.0f;
			}

			spdRatioMax = differential->dSlipMax - DrTq * differential->dSlipMax / lockTq;
			bias = 0.0f;
			if (spdRatio > spdRatioMax) {
				deltaSpd = (spdRatio - spdRatioMax) * commomSpinVel / 2.0;
				if (spinVel0 > spinVel1) {
					spinVel0 -= deltaSpd;
					spinVel1 += deltaSpd;
					bias = -(spdRatio - spdRatioMax);
				} else {
					spinVel0 += deltaSpd;
					spinVel1 -= deltaSpd;
					bias = (spdRatio - spdRatioMax);
				}
			}

			{
				float spiderTq = inTq1 - inTq0;
				DrTq0 = (DrTq*(1.0f + bias*biassign) + spiderTq)*0.5f;
				DrTq1 = (DrTq*(1.0f - bias*biassign) - spiderTq)*0.5f;
			}			
			break;
		case DIFF_VISCOUS_COUPLER:
			if (spinVel0 >= spinVel1) {
				DrTq0 = DrTq * differential->dTqMin;
				DrTq1 = DrTq * (1 - differential->dTqMin);
			} else {
				deltaTq = differential->dTqMin + (1.0 - exp(-fabs(differential->viscosity * (spinVel0 - spinVel1)))) * differential->dTqMax;
				DrTq0 = DrTq * deltaTq;
				DrTq1 = DrTq * (1 - deltaTq);
			}
		
			break;
		default: /* NONE ? */
			DrTq0 = DrTq1 = 0;
			break;
		}
	} else {
		DrTq0 = DrTq / 2.0;
		DrTq1 = DrTq / 2.0;
	}
	
	ndot0 = SimDeltaTime * (DrTq0 - inTq0) / differential->outAxis[0]->I;
	spinVel0 += ndot0;
	ndot1 = SimDeltaTime * (DrTq1 - inTq1) / differential->outAxis[1]->I;
	spinVel1 += ndot1;

	BrTq = - SIGN(spinVel0) * differential->inAxis[0]->brkTq;
	ndot0 = SimDeltaTime * BrTq / differential->outAxis[0]->I;
	if (((ndot0 * spinVel0) < 0.0) && (fabs(ndot0) > fabs(spinVel0))) {
		ndot0 = -spinVel0;
	}
	if ((spinVel0 == 0.0) && (ndot0 < 0.0)) ndot0 = 0;
	spinVel0 += ndot0;
	
	BrTq = - SIGN(spinVel1) * differential->inAxis[1]->brkTq;
	ndot1 = SimDeltaTime * BrTq / differential->outAxis[1]->I;
	if (((ndot1 * spinVel1) < 0.0) && (fabs(ndot1) > fabs(spinVel1))) {
		ndot1 = -spinVel1;
	}
	if ((spinVel1 == 0.0) && (ndot1 < 0.0)) ndot1 = 0;
	spinVel1 += ndot1;
	
	if (first) {
		meanv = (spinVel0 + spinVel1) / 2.0;
		engineReaction = SimEngineUpdateRpm(car, meanv);
		if (meanv != 0.0) {
			engineReaction = engineReaction / meanv;
			if (engineReaction != 0.0) {
				spinVel1 *= engineReaction;
				spinVel0 *= engineReaction;
			}
		}
	}
	
	differential->outAxis[0]->spinVel = spinVel0;
	differential->outAxis[1]->spinVel = spinVel1;
	
	differential->outAxis[0]->Tq = (differential->outAxis[0]->spinVel - differential->inAxis[0]->spinVel) / SimDeltaTime * differential->outAxis[0]->I;
	differential->outAxis[1]->Tq = (differential->outAxis[1]->spinVel - differential->inAxis[1]->spinVel) / SimDeltaTime * differential->outAxis[1]->I;	
}


