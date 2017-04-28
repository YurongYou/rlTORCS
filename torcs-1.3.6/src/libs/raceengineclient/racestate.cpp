/***************************************************************************

    file        : racestate.cpp
    created     : Sat Nov 16 12:00:42 CET 2002
    copyright   : (C) 2002-2014 by Eric Espie, Bernhard Wymann
    email       : eric.espie@torcs.org
    version     : $Id: racestate.cpp,v 1.5.2.5 2014/02/04 14:03:03 berniw Exp $

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
    @version	$Id: racestate.cpp,v 1.5.2.5 2014/02/04 14:03:03 berniw Exp $
*/

/* The Race Engine State Automaton */

#include <stdlib.h>
#include <stdio.h>
#include <tgfclient.h>
#include <raceman.h>
#include <racescreens.h>

#include "racemain.h"
#include "raceinit.h"
#include "raceengine.h"
#include "racegl.h"
#include "raceresults.h"
#include "racemanmenu.h"

#include "racestate.h"

static void *mainMenu;


/* State Automaton Init */
void
ReStateInit(void *prevMenu)
{
	mainMenu = prevMenu;
}


#include <execinfo.h>
void print_stacktrace()
{
	printf("\n\n\n\n\nstackstrace begin:\n");
    int size = 16;
    void * array[16];
    int stack_num = backtrace(array, size);
    char ** stacktrace = backtrace_symbols(array, stack_num);
    for (int i = 0; i < stack_num; ++i)
    {
        printf("%s\n", stacktrace[i]);
    }
    free(stacktrace);
    printf("stackstrace begin:\n\n\n\n\n");
}





/* State Automaton Management         */
/* Called when a race menu is entered */
void
ReStateManage(void)
{
	// print_stacktrace();
	int mode = RM_SYNC | RM_NEXT_STEP;

	do {
		switch (ReInfo->_reState) {
			case RE_STATE_CONFIG:
				GfOut("RaceEngine: state = RE_STATE_CONFIG\n");
				/* Display the race specific menu */
				mode = ReRacemanMenu();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_EVENT_INIT;
				}
				break;

			case RE_STATE_EVENT_INIT:
				GfOut("RaceEngine: state = RE_STATE_EVENT_INIT\n");
				/* Load the event description (track and drivers list) */
				mode = ReRaceEventInit();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_PRE_RACE;
				}
				break;

			case RE_STATE_PRE_RACE:
				GfOut("RaceEngine: state = RE_STATE_PRE_RACE\n");
				mode = RePreRace();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_RACE_START;
				}
				break;

			case RE_STATE_RACE_START:
				GfOut("RaceEngine: state = RE_STATE_RACE_START\n");
				mode = ReRaceStart();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_RACE;
				}
				break;

			case RE_STATE_RACE:
				mode = ReUpdate();
				if (ReInfo->s->_raceState == RM_RACE_ENDED) {
					/* race finished */
					ReInfo->_reState = RE_STATE_RACE_END;
				} else if (mode & RM_END_RACE) {
					/* interrupt by player */
					ReInfo->_reState = RE_STATE_RACE_STOP;
				}
/////////////////////////// by Yurong
				else if (mode == RE_STATE_PRE_RACE) {
					ReInfo->_reState = RE_STATE_PRE_RACE;
				}
/////////////////////////// end by Yurong
				break;

			case RE_STATE_RACE_STOP:
				GfOut("RaceEngine: state = RE_STATE_RACE_STOP\n");
				/* Interrupted by player */
				mode = ReRaceStop();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_RACE_END;
				}
				break;

			case RE_STATE_RACE_END:
				GfOut("RaceEngine: state = RE_STATE_RACE_END\n");
				mode = ReRaceEnd();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_POST_RACE;
				} else if (mode & RM_NEXT_RACE) {
					ReInfo->_reState = RE_STATE_RACE_START;
				}

				break;

			case RE_STATE_POST_RACE:
				GfOut("RaceEngine: state = RE_STATE_POST_RACE\n");
				mode = RePostRace();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_EVENT_SHUTDOWN;
				} else if (mode & RM_NEXT_RACE) {
					ReInfo->_reState = RE_STATE_PRE_RACE;
				}
				break;

			case RE_STATE_EVENT_SHUTDOWN:
				GfOut("RaceEngine: state = RE_STATE_EVENT_SHUTDOWN\n");
				mode = ReEventShutdown();
				if (mode & RM_NEXT_STEP) {
					ReInfo->_reState = RE_STATE_SHUTDOWN;
				} else if (mode & RM_NEXT_RACE) {
					ReInfo->_reState = RE_STATE_EVENT_INIT;
				}
				break;

			case RE_STATE_SHUTDOWN:
				if (ReInfo->_displayMode == RM_DISP_MODE_CONSOLE) {
					return;
				}
			case RE_STATE_ERROR:
				GfOut("RaceEngine: state = RE_STATE_SHUTDOWN\n");
				/* Back to race menu */
				ReInfo->_reState = RE_STATE_CONFIG;
				mode = RM_SYNC;
				break;

			case RE_STATE_EXIT:
				printf("RaceEngine: state = RE_STATE_EXIT");
				GfScrShutdown();
				exit (0);		/* brutal isn't it ? */
				break;
		}
		// printf("========================= while condition is %d ================================\n", (mode & (RM_SYNC | RM_QUIT)) == RM_SYNC);
		// fflush(stdout);
	} while ((mode & (RM_SYNC | RM_QUIT)) == RM_SYNC);
	if (mode & RM_QUIT) {
		GfScrShutdown();
		exit (0);		/* brutal isn't it ? */
	}

	if (mode & RM_ACTIVGAMESCR) {
		GfuiScreenActivate(ReInfo->_reGameScreen);
	}
}

/* Change and Execute a New State  */
void
ReStateApply(void *vstate)
{
	long state = (long)vstate;

	ReInfo->_reState = (int)state;
	ReStateManage();
}
