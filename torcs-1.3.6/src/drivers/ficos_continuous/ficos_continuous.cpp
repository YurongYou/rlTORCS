/***************************************************************************

    file                 : ficos_continuous.cpp
    created              : Sat Dec 10 17:09:02 CST 2016
    copyright            : (C) 2002 YurongYou

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <tgf.h>
#include <track.h>
#include <car.h>
#include <raceman.h>
#include <robottools.h>
#include <robot.h>

static tTrack	*curTrack;

static void initTrack(int index, tTrack* track, void *carHandle, void **carParmHandle, tSituation *s);
static void newrace(int index, tCarElt* car, tSituation *s);
static void drive(int index, tCarElt* car, tSituation *s);
static void endrace(int index, tCarElt *car, tSituation *s);
static void shutdown(int index);
static int  InitFuncPt(int index, void *pt);


/*
 * Module entry point
 */
extern "C" int
ficos_continuous(tModInfo *modInfo)
{
    memset(modInfo, 0, 10*sizeof(tModInfo));

    modInfo->name    = strdup("ficos_continuous");		/* name of the module (short) */
    modInfo->desc    = strdup("");	/* description of the module (can be long) */
    modInfo->fctInit = InitFuncPt;		/* init function */
    modInfo->gfId    = ROB_IDENT;		/* supported framework version */
    modInfo->index   = 1;

    return 0;
}

/* Module interface initialization. */
static int
InitFuncPt(int index, void *pt)
{
    tRobotItf *itf  = (tRobotItf *)pt;

    itf->rbNewTrack = initTrack; /* Give the robot the track view called */
				 /* for every track change or new race */
    itf->rbNewRace  = newrace; 	 /* Start a new race */
    itf->rbDrive    = drive;	 /* Drive during race */
    itf->rbPitCmd   = NULL;
    itf->rbEndRace  = endrace;	 /* End of the current race */
    itf->rbShutdown = shutdown;	 /* Called before the module is unloaded */
    itf->index      = index; 	 /* Index used if multiple interfaces */
    return 0;
}

/* Called for every track change or new race. */
static void
initTrack(int index, tTrack* track, void *carHandle, void **carParmHandle, tSituation *s)
{
    curTrack = track;
    *carParmHandle = NULL;
}

static double clutchtime = 0;

static double getAutoClutch(int gear, int newgear, tCarElt *car)
{
    if (newgear != 0 && newgear < car->_gearNb) {
        if (newgear != gear) {
            clutchtime = 0.332f - ((double) newgear / 65.0f);
        }

        if (clutchtime > 0.0f)
            clutchtime -= RCM_MAX_DT_ROBOTS;
        return 2.0f * clutchtime;
    }

    return 0.0f;
}

/* Start a new race. */
static void
newrace(int index, tCarElt* car, tSituation *s)
{
    clutchtime = 0;
}



/* Drive during race. */
extern double* psteerCmd;
extern double* paccelCmd;
extern double* pbrakeCmd;

extern double* pspeed;
extern double* pangle_in_rad;
extern int* pdamage;
extern double* ppos;
extern int* psegtype;
extern double* pradius;
extern int* _pisEnd;
extern double* _pdist;

extern int* pfrontCarNum;
extern double* pfrontDist;

// Compute the length to the start of the segment.
float getDistToSegStart(tCarElt *ocar)
{
    if (ocar->_trkPos.seg->type == TR_STR) {
        return ocar->_trkPos.toStart;
    } else {
        return ocar->_trkPos.toStart*ocar->_trkPos.seg->radius;
    }
}

#define CMD_GEAR_R  4
#define CMD_GEAR_N  5
#define CMD_GEAR_1  6



/* Drive during race. */
static void
drive(int index, tCarElt* car, tSituation *s)
{
    memset(&car->ctrl, 0, sizeof(tCarCtrl));

/////////////////////////////////////// by Yurong
    car->ctrl.steer = *psteerCmd;
    car->ctrl.accelCmd = *paccelCmd;
    car->ctrl.brakeCmd = *pbrakeCmd;

    // ABS
    if (fabs(car->_speed_x) > 10.0)
    {
        int i;

        tdble skidAng = atan2(car->_speed_Y, car->_speed_X) - car->_yaw;
        NORM_PI_PI(skidAng);

        if (car->_speed_x > 5 && fabs(skidAng) > 0.2)
            car->_brakeCmd = MIN(car->_brakeCmd, 0.10 + 0.70 * cos(skidAng));

        if (fabs(car->_steerCmd) > 0.1)
        {
            tdble decel = ((fabs(car->_steerCmd)-0.1) * (1.0 + fabs(car->_steerCmd)) * 0.6);
            car->_brakeCmd = MIN(car->_brakeCmd, MAX(0.35, 1.0 - decel));
        }

        const tdble abs_slip = 2.5;
        const tdble abs_range = 5.0;

        tdble slip = 0;
        for (i = 0; i < 4; i++) {
            slip += car->_wheelSpinVel(i) * car->_wheelRadius(i);
        }
        slip = car->_speed_x - slip/4.0f;

        if (slip > abs_slip)
            car->_brakeCmd = car->_brakeCmd - MIN(car->_brakeCmd*0.8, (slip - abs_slip) / abs_range);
    }

    // ASR
    tdble trackangle = RtTrackSideTgAngleL(&(car->_trkPos));
    tdble asr_angle = trackangle - car->_yaw;
    NORM_PI_PI(asr_angle);

    tdble maxaccel = 0.0;
    if (car->_trkPos.seg->type == TR_STR)
        maxaccel = MIN(car->_accelCmd, 0.2);
    else if (car->_trkPos.seg->type == TR_LFT && asr_angle < 0.0)
        maxaccel = MIN(car->_accelCmd, MIN(0.6, -asr_angle));
    else if (car->_trkPos.seg->type == TR_RGT && asr_angle > 0.0)
        maxaccel = MIN(car->_accelCmd, MIN(0.6, asr_angle));

    tdble origaccel = car->_accelCmd;
    tdble skidAng = atan2(car->_speed_Y, car->_speed_X) - car->_yaw;
    NORM_PI_PI(skidAng);

    if (car->_speed_x > 5 && fabs(skidAng) > 0.2)
    {
        car->_accelCmd = MIN(car->_accelCmd, 0.15 + 0.70 * cos(skidAng));
        car->_accelCmd = MAX(car->_accelCmd, maxaccel);
    }

    if (fabs(car->_steerCmd) > 0.1)
    {
        tdble decel = ((fabs(car->_steerCmd)-0.1) * (1.0 + fabs(car->_steerCmd)) * 0.8);
        car->_accelCmd = MIN(car->_accelCmd, MAX(0.35, 1.0 - decel));
    }

    tdble drivespeed = (car->_wheelSpinVel(REAR_RGT) + car->_wheelSpinVel(REAR_LFT)) *
                          car->_wheelRadius(REAR_LFT) / 2.0;
    tdble slip = drivespeed - fabs(car->_speed_x);
    if (slip > 2.0)
        car->_accelCmd = MIN(car->_accelCmd, origaccel - MIN(origaccel-0.1, ((slip - 2.0)/10.0)));


    // auto gear
    int gear = car->_gear;
    gear += car->_gearOffset;
    car->_gearCmd = car->_gear;


    tdble omega = car->_enginerpmRedLine * car->_wheelRadius(2) * 0.95;
    tdble shiftThld = 10000.0f;
    if (car->_gearRatio[gear] != 0) {
        shiftThld = omega / car->_gearRatio[gear];
    }

    if (car->pub.speed > shiftThld) {
        car->_gearCmd++;
    } else if (car->_gearCmd > 1) {
        if (car->pub.speed < (omega / car->_gearRatio[gear-1] - 4.0)) {
            car->_gearCmd--;
        }
    }

    if (car->_gearCmd <= 0) {
        car->_gearCmd++;
    }

    if (car->_clutchCmd == 0.0f)
        car->_clutchCmd = getAutoClutch(car->_gear, car->_gearCmd, car);

    double angle = RtTrackSideTgAngleL(&(car->_trkPos)) - car->_yaw;
    NORM_PI_PI(angle);
    *pspeed = car->_speed_x;
    *pangle_in_rad = angle;
    *pdamage = car->_dammage;
    *ppos = car->_trkPos.toMiddle;
    *_pdist = car->_distRaced;
    *psegtype = car->_trkPos.seg->type;
    *pradius = car->_trkPos.seg->radius;

    // get the nearest car distance
    float min_dist=99999;
    float distance;
    tCarElt* min_car;
    int min_car_num;
    const float max_dist_range=60.0;

    for (int i = 0; i < s->_ncars; i++) {
        if (s->cars[i] != car) {
            distance = s->cars[i]->_trkPos.seg->lgfromstart + getDistToSegStart(s->cars[i]) - car->_distFromStartLine;
            if (distance > 0 && distance < min_dist){
                min_dist = distance;
                min_car = s->cars[i];
                min_car_num = i;
            }
        }
    }
    *pfrontCarNum = min_car_num;
    *pfrontDist = min_dist;
}

/* End of the current race */
static void
endrace(int index, tCarElt *car, tSituation *s)
{
}

/* Called before the module is unloaded */
static void
shutdown(int index)
{
}

