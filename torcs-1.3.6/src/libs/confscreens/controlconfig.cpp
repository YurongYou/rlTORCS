/***************************************************************************

    file        : controlconfig.cpp
    created     : Wed Mar 12 21:20:34 CET 2003
    copyright   : (C) 2003-2014 by Eric Espie, Bernhard Wymann                        
    email       : eric.espie@torcs.org   
    version     : $Id: controlconfig.cpp,v 1.5.2.8 2014/02/14 10:48:27 berniw Exp $                                  

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
    		
    @author	<a href=mailto:eric.espie@torcs.org>Eric Espie</a>
    @version	$Id: controlconfig.cpp,v 1.5.2.8 2014/02/14 10:48:27 berniw Exp $
*/


#include <stdio.h>
#include <stdlib.h>

#include <tgfclient.h>
#include <track.h>
#include <robot.h>
#include <playerpref.h>
#include <plib/js.h>
#include <portability.h>

#include "controlconfig.h"
#include "mouseconfig.h"
#include "joystickconfig.h"

static void *scrHandle = NULL;
static void	*prevHandle = NULL;
static void	*PrefHdle = NULL;

static int	MouseCalButton;
static int	JoyCalButton;

static tCtrlMouseInfo	mouseInfo;
static const int CURRENTSECTIONSIZE = 256;
static char	CurrentSection[CURRENTSECTIONSIZE];

static tCmdInfo Cmd[] = {
    {HM_ATT_GEAR_R,     {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_GEAR_N,     {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_UP_SHFT,    {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_DN_SHFT,    {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_LIGHT1_CMD, {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_ASR_CMD,    {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_LEFTSTEER,  {1,  GFCTRL_TYPE_MOUSE_AXIS},   0, HM_ATT_LEFTSTEER_MIN,  0, HM_ATT_LEFTSTEER_MAX,  0, HM_ATT_LEFTSTEER_POW,  1.0, 1},
    {HM_ATT_RIGHTSTEER, {2,  GFCTRL_TYPE_MOUSE_AXIS},   0, HM_ATT_RIGHTSTEER_MIN, 0, HM_ATT_RIGHTSTEER_MAX, 0, HM_ATT_RIGHTSTEER_POW, 1.0, 1},
    {HM_ATT_THROTTLE,   {1,  GFCTRL_TYPE_MOUSE_BUT},    0, HM_ATT_THROTTLE_MIN,   0, HM_ATT_THROTTLE_MAX,   0, HM_ATT_THROTTLE_POW,   1.0, 1},
    {HM_ATT_BRAKE,      {2,  GFCTRL_TYPE_MOUSE_BUT},    0, HM_ATT_BRAKE_MIN,      0, HM_ATT_BRAKE_MAX,      0, HM_ATT_BRAKE_POW,      1.0, 1},
    {HM_ATT_CLUTCH,     {3,  GFCTRL_TYPE_MOUSE_BUT},    0, HM_ATT_CLUTCH_MIN,     0, HM_ATT_CLUTCH_MAX,     0, HM_ATT_CLUTCH_POW,     1.0, 1},
    {HM_ATT_ABS_CMD,    {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1},
    {HM_ATT_SPDLIM_CMD, {-1, GFCTRL_TYPE_NOT_AFFECTED}, 0, 0, 0, 0, 0, 0, 0, 1}
};

static int maxCmd = sizeof(Cmd) / sizeof(Cmd[0]);

static jsJoystick *js[NUM_JOY] = {NULL};
static float ax[_JS_MAX_AXES * NUM_JOY] = {0};
static float axCenter[_JS_MAX_AXES * NUM_JOY];
static int rawb[NUM_JOY] = {0};
static int ReloadValues = 1;


typedef struct {
	const char *key;
	const char *label;
	int id;
	float value;
} EditboxValue;

static EditboxValue editBoxValues[] = {
	{ HM_ATT_STEER_SENS,	"Steer sensitivity",	0, 0.0f},
	{ HM_ATT_LEFTSTEER_POW,	"Steer power",			0, 0.0f},
	{ HM_ATT_STEER_DEAD,	"Steer dead zone",		0, 0.0f},
	{ HM_ATT_STEER_SPD,		"Steer speed factor",	0, 0.0f},
	{ HM_ATT_BRAKE_SENS,	"Brake sensitivity",	0, 0.0f},
	{ HM_ATT_BRAKE_POW,		"Brake power",			0, 0.0f},
	{ HM_ATT_THROTTLE_SENS,	"Throttle sensitivity",	0, 0.0f},
	{ HM_ATT_CLUTCH_SENS,	"Clutch sensitivity",	0, 0.0f},
};

static const int IDX_LEFTSTEER_POW = 1;	// Index for the list above
static const int IDX_BRAKE_POW = 5;		// Index for the list above
static const int nbEditboxValues = sizeof(editBoxValues)/sizeof(editBoxValues[0]);


static void onValueChange(void* v)
{
	EditboxValue* editBoxValue = (EditboxValue*) v;
	float fv;
	const int BUFSIZE = 10;
	char buf[BUFSIZE];

	char* val = GfuiEditboxGetString(scrHandle, editBoxValue->id);

	if (sscanf(val, "%f", &fv) == 1) {
		snprintf(buf, BUFSIZE, "%6.4f", fv);
		editBoxValue->value = fv;
		GfuiEditboxSetString(scrHandle, editBoxValue->id, buf);
	} else {
		GfuiEditboxSetString(scrHandle, editBoxValue->id, "");
	}
}


static void onSave(void * /* dummy */)
{
	int i;
	const char *str;
	
	// First write the command values
	for (i = 0; i < maxCmd; i++) {
		str = GfctrlGetNameByRef(Cmd[i].ref.type, Cmd[i].ref.index);
		if (str) {
			GfParmSetStr(PrefHdle, CurrentSection, Cmd[i].name, str);
		} else {
			GfParmSetStr(PrefHdle, CurrentSection, Cmd[i].name, "");
		}

		if (Cmd[i].minName) {
			GfParmSetNum(PrefHdle, CurrentSection, Cmd[i].minName, NULL, Cmd[i].min);
		}

		if (Cmd[i].maxName) {
			GfParmSetNum(PrefHdle, CurrentSection, Cmd[i].maxName, NULL, Cmd[i].max);
		}

		if (Cmd[i].powName) {
			GfParmSetNum(PrefHdle, CurrentSection, Cmd[i].powName, NULL, Cmd[i].pow);
		}
	}

	// The editbox values must be written after the command values, otherwise the wrong "power" values are saved
	for (i = 0; i < nbEditboxValues; i++) {
		GfParmSetNum(PrefHdle, CurrentSection, editBoxValues[i].key, NULL, editBoxValues[i].value);
		if (strcmp(HM_ATT_LEFTSTEER_POW, editBoxValues[i].key) == 0) {
			GfParmSetNum(PrefHdle, CurrentSection, HM_ATT_RIGHTSTEER_POW, NULL, editBoxValues[i].value); // In GUI we set left == right
		}
	}

	GfParmWriteFile(NULL, PrefHdle, "preferences");
	GfuiScreenActivate(prevHandle);
}


static void
updateButtonText(void)
{
	int i;
	const char *str;
	int displayMouseCal = GFUI_INVISIBLE;
	int displayJoyCal = GFUI_INVISIBLE;
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];
	
	for (i = 0; i < maxCmd; i++) {
		str = GfctrlGetNameByRef(Cmd[i].ref.type, Cmd[i].ref.index);
		if (str) {
			GfuiButtonSetText (scrHandle, Cmd[i].Id, str);
		} else {
			GfuiButtonSetText (scrHandle, Cmd[i].Id, "---");
		}

		if (Cmd[i].ref.type == GFCTRL_TYPE_MOUSE_AXIS) {
			displayMouseCal = GFUI_VISIBLE;
		} else if (Cmd[i].ref.type == GFCTRL_TYPE_JOY_AXIS) {
			displayJoyCal = GFUI_VISIBLE;
		}
	}

	for (i = 0; i < nbEditboxValues; i++) {
		snprintf(buf, BUFSIZE, "%6.4f", editBoxValues[i].value);
		GfuiEditboxSetString(scrHandle, editBoxValues[i].id, buf);
	}

	GfuiVisibilitySet(scrHandle, MouseCalButton, displayMouseCal);
	GfuiVisibilitySet(scrHandle, JoyCalButton, displayJoyCal);
}


static void
onFocusLost(void * /* dummy */)
{
	updateButtonText();
}


static tCmdInfo* CurrentCmd;

static int InputWaited = 0;


static int
onKeyAction(unsigned char key, int /* modifier */, int state)
{
	const char *name;
	
	if (!InputWaited || (state == GFUI_KEY_UP)) {
		return 0;
	}

	if (key == 27) {
		/* escape */
		CurrentCmd->ref.index = -1;
		CurrentCmd->ref.type = GFCTRL_TYPE_NOT_AFFECTED;
		GfParmSetStr(PrefHdle, CurrentSection, CurrentCmd->name, "");
	} else {
		name = GfctrlGetNameByRef(GFCTRL_TYPE_KEYBOARD, (int)key);
		CurrentCmd->ref.index = (int)key;
		CurrentCmd->ref.type = GFCTRL_TYPE_KEYBOARD;
		GfParmSetStr(PrefHdle, CurrentSection, CurrentCmd->name, name);
	}
	
	glutIdleFunc(GfuiIdle);
	InputWaited = 0;
	updateButtonText();
	return 1;
}


static int
onSKeyAction(int key, int /* modifier */, int state)
{
	const char *name;
	
	if (!InputWaited || (state == GFUI_KEY_UP)) {
		return 0;
	}

	name = GfctrlGetNameByRef(GFCTRL_TYPE_SKEYBOARD, key);
	CurrentCmd->ref.index = key;
	CurrentCmd->ref.type = GFCTRL_TYPE_SKEYBOARD;
	GfParmSetStr(PrefHdle, CurrentSection, CurrentCmd->name, name);
	
	glutIdleFunc(GfuiIdle);
	InputWaited = 0;
	updateButtonText();
	return 1;
}


static int
getMovedAxis(void)
{
	int	i;
	int	Index = -1;
	float maxDiff = 0.3;

	for (i = 0; i < _JS_MAX_AXES * NUM_JOY; i++) {
		if (maxDiff < fabs(ax[i] - axCenter[i])) {
			maxDiff = fabs(ax[i] - axCenter[i]);
			Index = i;
		}
	}
	return Index;
}


static void
Idle(void)
{
	int mask;
	int	b, i;
	int	index;
	const char *str;
	int	axis;
	
	GfctrlMouseGetCurrent(&mouseInfo);
	
	/* Check for a mouse button pressed */
	for (i = 0; i < 3; i++) {
		if (mouseInfo.edgedn[i]) {
			glutIdleFunc(GfuiIdle);
			InputWaited = 0;
			str = GfctrlGetNameByRef(GFCTRL_TYPE_MOUSE_BUT, i);
			CurrentCmd->ref.index = i;
			CurrentCmd->ref.type = GFCTRL_TYPE_MOUSE_BUT;
			GfuiButtonSetText (scrHandle, CurrentCmd->Id, str);
			glutPostRedisplay();
			return;
		}
	}
	
	/* Check for a mouse axis moved */
	for (i = 0; i < 4; i++) {
		if (mouseInfo.ax[i] > 20.0) {
			glutIdleFunc(GfuiIdle);
			InputWaited = 0;
			str = GfctrlGetNameByRef(GFCTRL_TYPE_MOUSE_AXIS, i);
			CurrentCmd->ref.index = i;
			CurrentCmd->ref.type = GFCTRL_TYPE_MOUSE_AXIS;
			GfuiButtonSetText (scrHandle, CurrentCmd->Id, str);
			glutPostRedisplay();
			return;
		}
	}
	
	/* Check for a Joystick button pressed */
	for (index = 0; index < NUM_JOY; index++) {
		if (js[index]) {
			js[index]->read(&b, &ax[index * _JS_MAX_AXES]);
		
			/* Joystick buttons */
			for (i = 0, mask = 1; i < 32; i++, mask *= 2) {
				if (((b & mask) != 0) && ((rawb[index] & mask) == 0)) {
					/* Button i fired */
					glutIdleFunc(GfuiIdle);
					InputWaited = 0;
					str = GfctrlGetNameByRef(GFCTRL_TYPE_JOY_BUT, i + 32 * index);
					CurrentCmd->ref.index = i + 32 * index;
					CurrentCmd->ref.type = GFCTRL_TYPE_JOY_BUT;
					GfuiButtonSetText (scrHandle, CurrentCmd->Id, str);
					glutPostRedisplay();
					rawb[index] = b;
					return;
				}
			}
			rawb[index] = b;
		}
	}

	/* detect joystick movement */
	axis = getMovedAxis();
	if (axis != -1) {
		glutIdleFunc(GfuiIdle);
		InputWaited = 0;
		CurrentCmd->ref.type = GFCTRL_TYPE_JOY_AXIS;
		CurrentCmd->ref.index = axis;
		str = GfctrlGetNameByRef(GFCTRL_TYPE_JOY_AXIS, axis);
		GfuiButtonSetText (scrHandle, CurrentCmd->Id, str);
		glutPostRedisplay();
		return;
	}
}


static void
onPush(void *vi)
{
	int	index;    

	CurrentCmd = (tCmdInfo*) vi;
	GfuiButtonSetText (scrHandle, CurrentCmd->Id, "");
	CurrentCmd->ref.index = -1;
	CurrentCmd->ref.type = GFCTRL_TYPE_NOT_AFFECTED;
	GfParmSetStr(PrefHdle, CurrentSection, CurrentCmd->name, "");
	
	if (CurrentCmd->keyboardPossible) {
		InputWaited = 1;
	}
	
	glutIdleFunc(Idle);
	GfctrlMouseInitCenter();
	memset(&mouseInfo, 0, sizeof(mouseInfo));
	GfctrlMouseGetCurrent(&mouseInfo);

	for (index = 0; index < NUM_JOY; index++) {
		if (js[index]) {
			js[index]->read(&rawb[index], &ax[index * _JS_MAX_AXES]); /* initial value */
		}
	}
	memcpy(axCenter, ax, sizeof(axCenter));
}


static void
onActivate(void * /* dummy */)
{
	int cmd;
	const char *prm;
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];


	if (ReloadValues) {
		snprintf(buf, BUFSIZE, "%s%s", GetLocalDir(), HM_PREF_FILE);
		PrefHdle = GfParmReadFile(buf, GFPARM_RMODE_STD | GFPARM_RMODE_CREAT);
	
		for (cmd = 0; cmd < maxCmd; cmd++) {
			prm = GfctrlGetNameByRef(Cmd[cmd].ref.type, Cmd[cmd].ref.index);
			if (!prm) {
				prm = "---";
			}

			prm = GfParmGetStr(PrefHdle, HM_SECT_MOUSEPREF, Cmd[cmd].name, prm);
			prm = GfParmGetStr(PrefHdle, CurrentSection, Cmd[cmd].name, prm);
			GfctrlGetRefByName(prm, &Cmd[cmd].ref);

			if (Cmd[cmd].minName) {
				Cmd[cmd].min = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(Cmd[cmd].ref.type), Cmd[cmd].minName, NULL, Cmd[cmd].min);
				Cmd[cmd].min = GfParmGetNum(PrefHdle, CurrentSection, Cmd[cmd].minName, NULL, Cmd[cmd].min);
			}

			if (Cmd[cmd].maxName) {
				Cmd[cmd].max = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(Cmd[cmd].ref.type), Cmd[cmd].maxName, NULL, Cmd[cmd].max);
				Cmd[cmd].max = GfParmGetNum(PrefHdle, CurrentSection, Cmd[cmd].maxName, NULL, Cmd[cmd].max);
			}

			if (Cmd[cmd].powName) {
				Cmd[cmd].pow = GfParmGetNum(PrefHdle, GfctrlGetDefaultSection(Cmd[cmd].ref.type), Cmd[cmd].powName, NULL, Cmd[cmd].pow);
				Cmd[cmd].pow = GfParmGetNum(PrefHdle, CurrentSection, Cmd[cmd].powName, NULL, Cmd[cmd].pow);
			}
		}
	
		int i;
		for (i = 0; i < nbEditboxValues; i++) {
			editBoxValues[i].value = GfParmGetNum(PrefHdle, HM_SECT_MOUSEPREF, editBoxValues[i].key, NULL, 0);
			editBoxValues[i].value = GfParmGetNum(PrefHdle, CurrentSection, editBoxValues[i].key, NULL, editBoxValues[i].value);
		}
	}

	// Update GUI after calibration to avoid nonsense values (inefficient)
	for (cmd = 0; cmd < maxCmd; cmd++) {
		if (strcmp(Cmd[cmd].name, HM_ATT_LEFTSTEER) == 0) {
			editBoxValues[IDX_LEFTSTEER_POW].value = Cmd[cmd].pow;
		}
		if (strcmp(Cmd[cmd].name, HM_ATT_BRAKE) == 0) {
			editBoxValues[IDX_BRAKE_POW].value = Cmd[cmd].pow;
		}
	}

	updateButtonText();
}

static void
DevCalibrate(void *menu)
{
	ReloadValues = 0;
	GfuiScreenActivate(menu);
}


void *
TorcsControlMenuInit(void *prevMenu, int idx)
{
	int x, y, x2, dy, i;
	int index;
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];
	
	ReloadValues = 1;
	snprintf(CurrentSection, CURRENTSECTIONSIZE, "%s/%d", HM_SECT_DRVPREF, idx);
	
	prevHandle = prevMenu;
	snprintf(buf, BUFSIZE, "%s%s", GetLocalDir(), HM_PREF_FILE);
	PrefHdle = GfParmReadFile(buf, GFPARM_RMODE_STD | GFPARM_RMODE_CREAT);
	
	if (scrHandle) {
		return scrHandle;
	}
	
	for (index = 0; index < NUM_JOY; index++) {
		if (js[index] == NULL) {
			js[index] = new jsJoystick(index);
		}
		
		if (js[index]->notWorking()) {
			/* don't configure the joystick */
			js[index] = NULL;
		}
	}
	
	scrHandle = GfuiScreenCreateEx((float*)NULL, NULL, onActivate, NULL, (tfuiCallback)NULL, 1);
	GfuiTitleCreate(scrHandle, "Control Configuration", 0);
	
	GfuiScreenAddBgImg(scrHandle, "data/img/splash-mouseconf.png");
	
	GfuiMenuDefaultKeysAdd(scrHandle);
	
	x = 10;
	x2 = 210;
	y = 390;
	dy = 30;
	
	for (i = 0; i < maxCmd; i++) {
		GfuiLabelCreate(scrHandle, Cmd[i].name, GFUI_FONT_MEDIUM, x, y, GFUI_ALIGN_HL_VB, 0);
		Cmd[i].Id = GfuiButtonStateCreate (scrHandle, "MOUSE_MIDDLE_BUTTON", GFUI_FONT_MEDIUM_C, x+x2, y, 0, GFUI_ALIGN_HC_VB, GFUI_MOUSE_DOWN, 
							(void*)&Cmd[i], onPush, NULL, (tfuiCallback)NULL, onFocusLost);
		y -= dy;
		if (i == (maxCmd / 2 - 1)) {
			x = 320;
			y = 390;
			x2 = 220;
		}
	}
	
	int y0 = 180;

	for (i = 0; i < nbEditboxValues; i++) {
		y = y0 - (i%4)*dy;
		GfuiLabelCreate(scrHandle, editBoxValues[i].label, GFUI_FONT_MEDIUM, 10 + (i/4*310), y, GFUI_ALIGN_HL_VB, 0);
		editBoxValues[i].id = GfuiEditboxCreate(scrHandle, "", GFUI_FONT_MEDIUM_C, 10 + (i/4*310) + 190, y, 80, 6, &editBoxValues[i], (tfuiCallback)NULL, onValueChange);
	}
	
	GfuiAddKey(scrHandle, 13, "Save", NULL, onSave, NULL);
	GfuiButtonCreate(
		scrHandle, "Save", GFUI_FONT_LARGE, 160, 40, 150, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
		NULL, onSave, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL
	);
	
	MouseCalButton = GfuiButtonCreate(
		scrHandle, "Calibrate", GFUI_FONT_LARGE, 320, 40, 150, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
		MouseCalMenuInit(scrHandle, Cmd, maxCmd), DevCalibrate, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL
	);
	
	JoyCalButton = GfuiButtonCreate(
		scrHandle, "Calibrate", GFUI_FONT_LARGE, 320, 40, 150, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
		JoyCalMenuInit(scrHandle, Cmd, maxCmd), DevCalibrate, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL
	);
	
	GfuiAddKey(scrHandle, 27, "Cancel", prevMenu, GfuiScreenActivate, NULL);
	GfuiButtonCreate(
		scrHandle, "Cancel", GFUI_FONT_LARGE, 480, 40, 150, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
		prevMenu, GfuiScreenActivate, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL
	);
	
	GfuiKeyEventRegister(scrHandle, onKeyAction);
	GfuiSKeyEventRegister(scrHandle, onSKeyAction);
	
	return scrHandle;
}
