/***************************************************************************

    file                 : pref.cpp
    created              : Sat Apr 29 16:51:03 CEST 2000
    copyright            : (C) 2000-2014 by Eric Espie, Bernhard Wymann
    email                : torcs@free.fr
    version              : $Id: pref.cpp,v 1.19.2.8 2014/02/14 10:48:27 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/** @file   
    		
    @author	<a href=mailto:torcs@free.fr>Eric Espie</a>
    @version	$Id: pref.cpp,v 1.19.2.8 2014/02/14 10:48:27 berniw Exp $
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <tgfclient.h>
#include <portability.h>

#include <track.h>
#include <car.h>
#include <raceman.h>
#include <robottools.h>
#include <robot.h>
#include <playerpref.h>
#include <plib/js.h>

#include "pref.h"
#include "human.h"

void	*PrefHdle;

tControlCmd	CmdControlRef[] = {
    {HM_ATT_UP_SHFT,    GFCTRL_TYPE_JOY_BUT,       0, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_DN_SHFT,    GFCTRL_TYPE_JOY_BUT,       1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_ASR_CMD,    GFCTRL_TYPE_JOY_BUT,       2, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_ABS_CMD,    GFCTRL_TYPE_JOY_BUT,       3, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_R,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_N,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_1,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_2,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_3,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_4,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_5,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_GEAR_6,     GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},

    {HM_ATT_THROTTLE,   GFCTRL_TYPE_JOY_AXIS, 1, HM_ATT_THROTTLE_MIN,   0.0, 0.0, HM_ATT_THROTTLE_MAX,   1.0, HM_ATT_THROTTLE_SENS, 1.0, HM_ATT_THROTTLE_POW, 2.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_BRAKE,      GFCTRL_TYPE_JOY_AXIS, 1, HM_ATT_BRAKE_MIN,      0.0, 0.0, HM_ATT_BRAKE_MAX,      1.0, HM_ATT_BRAKE_SENS,    1.0, HM_ATT_BRAKE_POW,    2.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_LEFTSTEER,  GFCTRL_TYPE_JOY_AXIS, 0, HM_ATT_LEFTSTEER_MIN,  0.0, 0.0, HM_ATT_LEFTSTEER_MAX,  1.0, HM_ATT_STEER_SENS,    2.0, HM_ATT_LEFTSTEER_POW,    1.0, HM_ATT_STEER_SPD, 0.0, HM_ATT_STEER_DEAD, 0.0},
    {HM_ATT_RIGHTSTEER, GFCTRL_TYPE_JOY_AXIS, 0, HM_ATT_RIGHTSTEER_MIN, 0.0, 0.0, HM_ATT_RIGHTSTEER_MAX, 1.0, HM_ATT_STEER_SENS,    2.0, HM_ATT_RIGHTSTEER_POW,    1.0, HM_ATT_STEER_SPD, 0.0, HM_ATT_STEER_DEAD, 0.0},
    {HM_ATT_LIGHT1_CMD, GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_CLUTCH,      GFCTRL_TYPE_JOY_AXIS, 1, HM_ATT_CLUTCH_MIN,      0.0, 0.0, HM_ATT_CLUTCH_MAX,      1.0, HM_ATT_CLUTCH_SENS,    1.0, HM_ATT_CLUTCH_POW,    2.0, NULL, 0.0, NULL, 0.0},
    {HM_ATT_SPDLIM_CMD,  GFCTRL_TYPE_NOT_AFFECTED, -1, NULL, 0.0, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0, NULL, 0.0}
};

const int nbCmdControl = sizeof(CmdControlRef) / sizeof(CmdControlRef[0]);
const char *Yn[] = {HM_VAL_YES, HM_VAL_NO};


void HmReadPrefs(int index)
{
    const char	*prm;
    const int BUFSIZE = 1024;
	char sstring[BUFSIZE];
    int	cmd;
    float tmp;
    int	idx = index - 1;
    tControlCmd	*cmdCtrl;

    HCtx[idx]->CmdControl = (tControlCmd *) calloc (nbCmdControl, sizeof (tControlCmd));
    cmdCtrl = HCtx[idx]->CmdControl;
    memcpy(cmdCtrl, CmdControlRef, nbCmdControl * sizeof (tControlCmd));

    snprintf(sstring, BUFSIZE, "%s%s", GetLocalDir(), HM_PREF_FILE);
    PrefHdle = GfParmReadFile(sstring, GFPARM_RMODE_REREAD | GFPARM_RMODE_CREAT);

    snprintf(sstring, BUFSIZE, "%s/%s/%d", HM_SECT_PREF, HM_LIST_DRV, index);
    prm = GfParmGetStr(PrefHdle, sstring, HM_ATT_TRANS, HM_VAL_AUTO);
    if (strcmp(prm, HM_VAL_AUTO) == 0) {
		HCtx[idx]->Transmission = 0;
    } else {
		HCtx[idx]->Transmission = 1;
    }

    /* Parameters Settings */
    prm = GfParmGetStr(PrefHdle, sstring, HM_ATT_ABS, Yn[HCtx[idx]->ParamAbs]);
    if (strcmp(prm, Yn[0]) == 0) {
		HCtx[idx]->ParamAbs = 1;
    } else {
		HCtx[idx]->ParamAbs = 0;
    }

    prm = GfParmGetStr(PrefHdle, sstring, HM_ATT_ASR, Yn[HCtx[idx]->ParamAsr]);
    if (strcmp(prm, Yn[0]) == 0) {
		HCtx[idx]->ParamAsr = 1;
    } else {
		HCtx[idx]->ParamAsr = 0;
    }

    /* Command Settings */
    for (cmd = 0; cmd < nbCmdControl; cmd++) {
		prm = GfctrlGetNameByRef(cmdCtrl[cmd].type, cmdCtrl[cmd].val);
		prm = GfParmGetStr(PrefHdle, HM_SECT_MOUSEPREF, cmdCtrl[cmd].name, prm);
		prm = GfParmGetStr(PrefHdle, sstring, cmdCtrl[cmd].name, prm);
		if (!prm || (strlen(prm) == 0)) {
			cmdCtrl[cmd].type = GFCTRL_TYPE_NOT_AFFECTED;
			GfOut("%s -> NONE (-1)\n", cmdCtrl[cmd].name);
			continue;
		}

		tCtrlRef ref;
		GfctrlGetRefByName(prm, &ref);
		cmdCtrl[cmd].type = ref.type;
		cmdCtrl[cmd].val = ref.index;
		GfOut("%s -> %s\n", cmdCtrl[cmd].name, prm);

		if (cmdCtrl[cmd].minName) {
			cmdCtrl[cmd].min = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(cmdCtrl[cmd].type), cmdCtrl[cmd].minName, (char*)NULL, cmdCtrl[cmd].min);
			cmdCtrl[cmd].min = cmdCtrl[cmd].minVal = GfParmGetNum(PrefHdle, sstring, cmdCtrl[cmd].minName, (char*)NULL, cmdCtrl[cmd].min);
		}
		if (cmdCtrl[cmd].maxName) {
			cmdCtrl[cmd].max = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(cmdCtrl[cmd].type), cmdCtrl[cmd].maxName, (char*)NULL, cmdCtrl[cmd].max);
			cmdCtrl[cmd].max = GfParmGetNum(PrefHdle, sstring, cmdCtrl[cmd].maxName, (char*)NULL, cmdCtrl[cmd].max);
		}	
		if (cmdCtrl[cmd].sensName) {
			cmdCtrl[cmd].sens = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(cmdCtrl[cmd].type), cmdCtrl[cmd].sensName, (char*)NULL, cmdCtrl[cmd].sens);
			cmdCtrl[cmd].sens = GfParmGetNum(PrefHdle, sstring, cmdCtrl[cmd].sensName, (char*)NULL, cmdCtrl[cmd].sens);
			cmdCtrl[cmd].sens = 1.0f / cmdCtrl[cmd].sens;
		}	
		if (cmdCtrl[cmd].powName) {
			cmdCtrl[cmd].pow = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(cmdCtrl[cmd].type), cmdCtrl[cmd].powName, (char*)NULL, cmdCtrl[cmd].pow);
			cmdCtrl[cmd].pow = GfParmGetNum(PrefHdle, sstring, cmdCtrl[cmd].powName, (char*)NULL, cmdCtrl[cmd].pow);
		}
		if (cmdCtrl[cmd].spdSensName) {
			cmdCtrl[cmd].spdSens = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(cmdCtrl[cmd].type), cmdCtrl[cmd].spdSensName, (char*)NULL, cmdCtrl[cmd].spdSens);
			cmdCtrl[cmd].spdSens = GfParmGetNum(PrefHdle, sstring, cmdCtrl[cmd].spdSensName, (char*)NULL, cmdCtrl[cmd].spdSens);
			cmdCtrl[cmd].spdSens = cmdCtrl[cmd].spdSens / 100.0;
		}
		if (cmdCtrl[cmd].deadZoneName) {
			cmdCtrl[cmd].deadZone = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(cmdCtrl[cmd].type), cmdCtrl[cmd].deadZoneName, (char*)NULL, cmdCtrl[cmd].deadZone);
			cmdCtrl[cmd].deadZone = GfParmGetNum(PrefHdle, sstring, cmdCtrl[cmd].deadZoneName, (char*)NULL, cmdCtrl[cmd].deadZone);
		}
		if (cmdCtrl[cmd].min > cmdCtrl[cmd].max) {
			tmp = cmdCtrl[cmd].min;
			cmdCtrl[cmd].min = cmdCtrl[cmd].max;
			cmdCtrl[cmd].max = tmp;
		}
		cmdCtrl[cmd].deadZone = (cmdCtrl[cmd].max - cmdCtrl[cmd].min) * cmdCtrl[cmd].deadZone;
		if (cmdCtrl[cmd].type == GFCTRL_TYPE_MOUSE_AXIS) {
			HCtx[idx]->MouseControlUsed = 1;
		}
    }

    prm = GfParmGetStr(PrefHdle, HM_SECT_MOUSEPREF, HM_ATT_REL_BUT_NEUTRAL, Yn[HCtx[idx]->RelButNeutral]);
    prm = GfParmGetStr(PrefHdle, sstring, HM_ATT_REL_BUT_NEUTRAL, prm);
    if (strcmp(prm, Yn[0]) == 0) {
		HCtx[idx]->RelButNeutral = 1;
    } else {
		HCtx[idx]->RelButNeutral = 0;
    }

    prm = GfParmGetStr(PrefHdle, HM_SECT_MOUSEPREF, HM_ATT_SEQSHFT_ALLOW_NEUTRAL, Yn[HCtx[idx]->SeqShftAllowNeutral]);
    prm = GfParmGetStr(PrefHdle, sstring, HM_ATT_SEQSHFT_ALLOW_NEUTRAL, prm);
    if (strcmp(prm, Yn[0]) == 0) {
		HCtx[idx]->SeqShftAllowNeutral = 1;
    } else {
		HCtx[idx]->SeqShftAllowNeutral = 0;
    }

    prm = GfParmGetStr(PrefHdle, sstring, HM_ATT_AUTOREVERSE, Yn[HCtx[idx]->AutoReverse]);
    if (strcmp(prm, Yn[0]) == 0) {
		HCtx[idx]->AutoReverse = 1;
    } else {
		HCtx[idx]->AutoReverse = 0;
    }
}

