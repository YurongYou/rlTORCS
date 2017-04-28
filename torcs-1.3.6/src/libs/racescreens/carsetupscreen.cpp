/***************************************************************************

    file        : carsetupscreen.cpp
    created     : Wed Aug 21 13:27:34 CET 2013
    copyright   : (C) 2013 Bernhard Wymann
    email       : berniw@bluewin.ch
    version     : $Id: carsetupscreen.cpp,v 1.1.2.12 2013/09/14 15:05:25 berniw Exp $                                  

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <tgfclient.h>
#include <car.h>
#include <portability.h>
#include <racescreens.h>
#include <robottools.h>
#include <vector>

static void *scrHandle = NULL;
static void	*prevHandle = NULL;

static void* rmCarHandle = NULL;
static tCarPitSetup* rmSetup = NULL;
static char* rmModName = NULL;
static int rmIdx = 0;
static char* rmTrack = NULL;
static char* rmCarName = NULL;
static int rmRaceType = RM_TYPE_PRACTICE;

static void rmSet(void *vp);
static void rmUpdateMM(void *vp);
static void rmUpdateM(void *vp);
static void rmUpdateP(void *vp);
static void rmUpdatePP(void *vp);
static void enableLoadButtons();

class cGuiSetupValue {
	private:
		void* scr;	// screen
		tCarPitSetupValue* v;
		int id;	// GUI widget id
		tdble steerincs;
		tdble steerdecs;
		tdble steerincb;
		tdble steerdecb;
		const char* unit;
		const char* format;
	
		void setValue(tdble value)
		{
			if (value > v->max) {
				value = v->max;
			} else if (value < v->min) {
				value = v->min;
			}
			v->value = value;
			value = GfParmSI2Unit(unit, value);
			const int BUFSIZE = 32;
			char buf[BUFSIZE];

			snprintf(buf, BUFSIZE, format, value);
			GfuiEditboxSetString(scr, id, buf);
		}

	public:
		void set()
		{
			const char *val = GfuiEditboxGetString(scr, id);
			tdble value = (tdble) atof(val);
			value = GfParmUnit2SI(unit, value);
			setValue(value);
		}

		void updateMM() { update(steerdecb); }
		void updateM()	{ update(steerdecs); }
		void updateP()	{ update(steerincs); }
		void updatePP() { update(steerincb); }

		void update(tdble delta)
		{
			if (fabs(v->min - v->max) >= 0.0001f) {
				tdble value = v->value;
				value += delta;
				setValue(value);
			}
		}

		cGuiSetupValue(void* scr, tCarPitSetupValue* v, const char* unit, const char* format, int font, int x, int y, int w, int len):
			scr(scr),
			v(v),
			unit(unit),
			format(format)
		{
			const int BUFSIZE = 256;
			char buf[BUFSIZE];
			int enable = GFUI_ENABLE;

			steerincb = (v->max - v->min)/10.0f;
			steerdecb = -steerincb;
			steerincs = steerincb/10.0f;
			steerdecs = -steerincs;

			// If min == max there is nothing to adjust, so we  disable the fields
			if (fabs(v->min - v->max) < 0.0001f) {
				snprintf(buf, BUFSIZE, "%s", "N/A");
				len = 3;
				enable = GFUI_DISABLE;
			} else {
				snprintf(buf, BUFSIZE, format, GfParmSI2Unit(unit, v->value));
			}

			const int sp = 3;
			const int bw = 10;
			const int minw = 30+4*(bw+sp);
			if (w < minw) w = minw; // Minimal width;

			id = GfuiEditboxCreate(scr, buf, font, x + 2*(bw+sp) + 5, y, w - 4*(bw+sp) - 10, len, this, (tfuiCallback)NULL, rmSet, 5);
			GfuiEnable(scr, id, enable);

			tdble bid;
			bid = GfuiLeanButtonCreate(scr, "-", font, x+bw/2, y, bw, GFUI_ALIGN_HC_VB, 1,
				this, rmUpdateMM, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
			GfuiEnable(scr, bid, enable);

			bid = GfuiLeanButtonCreate(scr, "-", font, x+bw/2+bw+sp, y, bw, GFUI_ALIGN_HC_VB, 1,
				this, rmUpdateM, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
			GfuiEnable(scr, bid, enable);

			bid = GfuiLeanButtonCreate(scr, "+", font, x+w-(bw+sp+bw/2), y, bw, GFUI_ALIGN_HC_VB, 1,
				this, rmUpdateP, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
			GfuiEnable(scr, bid, enable);

			bid = GfuiLeanButtonCreate(scr, "+", font, x+w-bw/2, y, bw, GFUI_ALIGN_HC_VB, 1,
				this, rmUpdatePP, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
			GfuiEnable(scr, bid, enable);
		}
	
};


static void rmSet(void *vp)
{
	cGuiSetupValue* c = static_cast<cGuiSetupValue*>(vp);
	c->set();
}

static void rmUpdateMM(void *vp)
{
	cGuiSetupValue* c = static_cast<cGuiSetupValue*>(vp);
	c->updateMM();
}

static void rmUpdateM(void *vp)
{
	cGuiSetupValue* c = static_cast<cGuiSetupValue*>(vp);
	c->updateM();
}

static void rmUpdateP(void *vp)
{
	cGuiSetupValue* c = static_cast<cGuiSetupValue*>(vp);
	c->updateP();
}

static void rmUpdatePP(void *vp)
{
	cGuiSetupValue* c = static_cast<cGuiSetupValue*>(vp);
	c->updatePP();
}


static void onSave(void *vp)
{
	rtCarPitSetupType* type = (rtCarPitSetupType*)vp;
	void* carhandle = RtLoadOriginalCarSettings(rmCarName);
	if (carhandle == 0) {
		GfError("carhandle NULL in %s, line %d\n", __FILE__, __LINE__);
		return;
	}

	RtSaveCarPitSetup(
		carhandle,
		rmSetup,
		*type,
		rmModName,
		rmIdx,
		rmTrack,
		rmCarName
	);

	GfParmReleaseHandle(carhandle);
	enableLoadButtons();
}


static void onSaveAndExit(void *vp)
{
	rtCarPitSetupType type = (rmRaceType == RM_TYPE_PRACTICE) ? PRACTICE : QUALIFYING;
	void* carhandle = RtLoadOriginalCarSettings(rmCarName);
	if (carhandle == 0) {
		GfError("carhandle NULL in %s, line %d\n", __FILE__, __LINE__);
		return;
	}

	RtSaveCarPitSetup(
		carhandle,
		rmSetup,
		type,
		rmModName,
		rmIdx,
		rmTrack,
		rmCarName
	);

	GfParmReleaseHandle(carhandle);
	if (vp != NULL) {
		GfuiScreenActivate(vp);
	}
}


static std::vector<cGuiSetupValue*> values;

static const char* unitdeg = "deg";
static const char* unitkpa = "kPa";
static const char* unitlbfin = "lbf/in";
static const char* unitmm = "mm";
static const char* unitlbfins = "lbf/in/s";
static const char* unitNm = "N.m";

static const char* f52 = "%5.2f";
static const char* f43 = "%4.3f";
static const char* d3 = "%3.0f";
static const char* d5 = "%5.0f";

static rtCarPitSetupType setuptype[6] = { PRACTICE, QUALIFYING, RACE, BACKUP1, BACKUP2, BACKUP3};
static const char* setuplabel[6] = { "Practice", "Qualifying", "Race", "Backup 1", "Backup 2", "Backup 3"};
static int loadbuttonid[6];


static void onLoad(void *vp)
{
	rtCarPitSetupType* type = (rtCarPitSetupType*)vp;
	RtLoadCarPitSetup(
		rmCarHandle, 
		rmSetup,
		*type,		
		rmModName,	
		rmIdx,				
		rmTrack,	
		rmCarName,
		false
		);
	
	// Update GUI
	for (std::vector<cGuiSetupValue*>::iterator it = values.begin(); it != values.end(); ++it) {
		(*it)->update(0.0f);
	}
}


// Enable/disable load button depending on file existence
static void enableLoadButtons()
{
	const int n = sizeof(loadbuttonid)/sizeof(loadbuttonid[0]);
	int i;
	for (i = 0; i < n; i++) {
		if (RtCarPitSetupExists(setuptype[i], rmModName, rmIdx, rmTrack, rmCarName)) {
			GfuiEnable(scrHandle, loadbuttonid[i], GFUI_ENABLE);
		} else {
			GfuiEnable(scrHandle, loadbuttonid[i], GFUI_DISABLE);
		}
	}
}


static void onActivate(void *vp)
{
	enableLoadButtons();
}


static void onLoadDefault(void* vp)
{
	if (!RtInitCarPitSetupFromDefault(rmSetup, rmCarName)) {
		GfError("failed to init from default setup in %s, line %d\n", __FILE__, __LINE__);
		return;
	}

	// Update GUI
	for (std::vector<cGuiSetupValue*>::iterator it = values.begin(); it != values.end(); ++it) {
		(*it)->update(0.0f);
	}
}


void *RmCarSetupScreenInit(void *prevMenu, tCarElt *car, tRmInfo* reInfo)
{
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];
	
	prevHandle = prevMenu;
	
	rmCarHandle = car->_carHandle;
	rmSetup = &(car->pitcmd.setup);
	rmModName = car->_modName;
	rmIdx = car->_driverIndex;
	rmTrack = reInfo->track->internalname;
	rmCarName = car->_carName;
	rmRaceType = reInfo->s->raceInfo.type;

	if (scrHandle) {
		GfuiScreenRelease(scrHandle);
		for (std::vector<cGuiSetupValue*>::iterator it = values.begin(); it != values.end(); ++it) {
			delete *it;
		}
		values.clear();
	}
	
	scrHandle = GfuiScreenCreateEx(NULL, NULL, onActivate, NULL, NULL, 1);
	snprintf(buf, BUFSIZE, "Car Setup - %s - %s - %d", rmCarName, rmTrack, rmIdx);
	GfuiLabelCreate(scrHandle, buf, GFUI_FONT_MEDIUM, 320, 450, GFUI_ALIGN_HC_VB, strlen(buf));
	GfuiMenuDefaultKeysAdd(scrHandle);

	static const int x0 = 20;
	static const int y0 = 415;
	static const int dy = -12;
	static const int xoff = 112;
	static const int xoff2 = 40;

	int i;
	static const int font = GFUI_FONT_SMALL_C;
	int col = 0;

	// Suspension/Wheel settings
	GfuiLabelCreate(scrHandle, "Ride height [mm]:", font, x0, y0 + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Camber [deg]:", font, x0, y0 + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Toe [deg]:", font, x0, y0 + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	int y = y0 + (3.3f * dy);
	col = 0;
	GfuiLabelCreate(scrHandle, "Spring [lbf/in]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Packers [mm]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Slow bump [lbf/in/s]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Slow rebound [lbf/in/s]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Fast bump [lbf/in/s]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Fast rebound [lbf/in/s]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);

	static const char* wheellabel[4] = {"Front right wheel", "Front left wheel", "Rear right wheel", "Rear left wheel"};

	for (i = 0; i < 4; i++) {
		col = 0;
		GfuiLabelCreate(scrHandle, wheellabel[i], font, x0 + xoff*(i+1) + xoff2, y0 - dy, GFUI_ALIGN_HL_VB, 0);
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->wheelrideheight[i]), unitmm, d3, font, x0 + xoff*(i+1) + xoff2, y0 + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->wheelcamber[i]), unitdeg, f52, font, x0 + xoff*(i+1) + xoff2, y0 + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->wheeltoe[i]), unitdeg, f52, font, x0 + xoff*(i+1) + xoff2, y0 + (col++ * dy), 102, 5));
		y = y0 + (3.3f * dy);
		col = 0;	
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->suspspring[i]), unitlbfin, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->susppackers[i]), unitmm, d3, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->suspslowbump[i]), unitlbfins, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->suspslowrebound[i]), unitlbfins, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->suspfastbump[i]), unitlbfins, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->suspfastrebound[i]), unitlbfins, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
	
	}

	// Steer, brake and axle settings
	y = y0 + 9.8f*dy;
	col = 1;
	GfuiLabelCreate(scrHandle, "Various settings", font, x0 + xoff + xoff2, y, GFUI_ALIGN_HL_VB, 0);

	GfuiLabelCreate(scrHandle, "Steer lock [deg]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Brake front-rear [-]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Brake pressure [kPa]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Front wing [deg]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Rear wing [deg]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	
	col = 1;
	values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->steerLock), unitdeg, f52, font, x0 + xoff + xoff2, y + (col++ * dy), 102, 5));
	values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->brakeRepartition), NULL, f43, font, x0 + xoff + xoff2, y + (col++ * dy), 102, 5));
	values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->brakePressure), unitkpa, d5, font, x0 + xoff + xoff2, y + (col++ * dy), 102, 5));
	values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->wingangle[0]), unitdeg, f52, font, x0 + xoff + xoff2, y + (col++ * dy), 102, 5));
	values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->wingangle[1]), unitdeg, f52, font, x0 + xoff + xoff2, y + (col++ * dy), 102, 5));

	col = 1;
	i = 2;
	GfuiLabelCreate(scrHandle, "ARB spring [lbf/in]:", font, x0 + xoff*i + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "3rd spring [lbf/in]:", font, x0 + xoff*i + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "3rd bump [lbf/in/s]:", font, x0 + xoff*i + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "3rd rebound [lbf/in/s]:", font, x0 + xoff*i + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "3rd X0 [mm]:", font, x0 + xoff*i + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	
	static const char* axlelabel[2] = {"Front axle", "Rear axle"};

	int j;
	for (j = 0; j < 2; j++) {
		col = 1;
		i = j + 3;

		GfuiLabelCreate(scrHandle, axlelabel[j], font, x0 + xoff*i + xoff2, y, GFUI_ALIGN_HL_VB, 0);
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->arbspring[j]), unitlbfin, d5, font, x0 + xoff*i + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->thirdspring[j]), unitlbfin, d5, font, x0 + xoff*i + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->thirdbump[j]), unitlbfins, d5, font, x0 + xoff*i + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->thirdrebound[j]), unitlbfins, d5, font, x0 + xoff*i + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->thirdX0[j]), unitmm, d3, font, x0 + xoff*i + xoff2, y + (col++ * dy), 102, 5));
	}

	// Differential and gears
	y = y + 6.8f*dy;
	col = 1;
	GfuiLabelCreate(scrHandle, "Type:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Ratio [-]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Front min bias [-]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Front max bias [-]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Slip bias [-]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Accel locking torque [Nm]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
	GfuiLabelCreate(scrHandle, "Brake locking torque [Nm]:", font, x0, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);

	// enum TDiffType { NONE = 0, SPOOL = 1, FREE = 2, LIMITED_SLIP = 3, VISCOUS_COUPLER = 4};
	static const char* diffPos[3] = {"Front differential", "Rear differential", "Center differential"};
	static const char* diffType[5] = {"None", "Spool", "Free", "1.5 way LSD", "Viscous coupler"};

	for (i = 0; i < 3; i++) {
		col = 0;
		
		GfuiLabelCreate(scrHandle, diffPos[i], font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
		GfuiLabelCreate(scrHandle, diffType[rmSetup->diffType[i]], font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->diffratio[i]), NULL, f43, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->diffmintqbias[i]), NULL, f43, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->diffmaxtqbias[i]), NULL, f43, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->diffslipbias[i]), NULL, f43, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->difflockinginputtq[i]), unitNm, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->difflockinginputbraketq[i]), unitNm, d5, font, x0 + xoff*(i+1) + xoff2, y + (col++ * dy), 102, 5));
	}

	col = 0;
	GfuiLabelCreate(scrHandle, "Gearbox ratios", font, x0 + xoff*4 + xoff2, y + (col++ * dy), GFUI_ALIGN_HL_VB, 0);

	for (i = 0; i < 8; i++) {
		snprintf(buf, BUFSIZE, "%d:", i + 1);
		GfuiLabelCreate(scrHandle, buf, font, x0 + xoff*4 + xoff2, y + (col * dy), GFUI_ALIGN_HL_VB, 0);
		values.push_back(new cGuiSetupValue(scrHandle, &(rmSetup->gearsratio[i]), NULL, f43, font, x0 + xoff*4 + xoff2 + 12, y + (col++ * dy), 90, 5));
	}

	// Save buttons
	y = 100;
	const int buttonwidth = 102;
	int x = buttonwidth/2 + x0;

	GfuiLabelCreate(scrHandle, "Save setup:", font, x0, y, GFUI_ALIGN_HL_VB, 0);
	
	for (j = 0; j < 6; j++) {
		GfuiLeanButtonCreate(scrHandle, setuplabel[j], font, x, y + (dy*(j+1)), buttonwidth, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
			(void*) &setuptype[j], onSave, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
	}	

	// Load buttons
	GfuiLabelCreate(scrHandle, "Load setup:", font, x0 + xoff + xoff2, y, GFUI_ALIGN_HL_VB, 0);
	for (j = 0; j < 6; j++) {
		loadbuttonid[j] = GfuiLeanButtonCreate(scrHandle, setuplabel[j], font, x + xoff + xoff2, y + (dy*(j+1)), buttonwidth, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
			(void*) &setuptype[j], onLoad, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
	}	

	// Reload original car setup
	GfuiLeanButtonCreate(scrHandle, "Car default", font, x + xoff + xoff2, y + (dy*7), buttonwidth, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
		NULL, onLoadDefault, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);


	// Exit/Exit and save buttons
	//GfuiButtonCreate(scrHandle, "Leave without saving", GFUI_FONT_MEDIUM, 447, 52, 306, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
	//	prevMenu, GfuiScreenActivate, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);

	const char* savebuttontext;
	if (rmRaceType == RM_TYPE_PRACTICE) {
		savebuttontext = "Save practice setup and leave";
	} else {
		savebuttontext = "Save qualifying setup and leave";
	}
	GfuiButtonCreate(scrHandle, savebuttontext, GFUI_FONT_MEDIUM, 447, 28, 306, GFUI_ALIGN_HC_VB, GFUI_MOUSE_UP,
		prevMenu, onSaveAndExit, NULL, (tfuiCallback)NULL, (tfuiCallback)NULL);
		
	return scrHandle;
}
