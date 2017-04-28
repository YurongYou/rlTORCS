/***************************************************************************

    file        : raceengine.cpp
    created     : Sat Nov 23 09:05:23 CET 2002
    copyright   : (C) 2002-2014 by Eric Espie, Bernhard Wymann
    email       : eric.espie@torcs.org
    version     : $Id: raceengine.cpp,v 1.19.2.22 2014/04/15 09:34:17 berniw Exp $

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
    @version	$Id: raceengine.cpp,v 1.19.2.22 2014/04/15 09:34:17 berniw Exp $
*/

#include <stdlib.h>
#include <stdio.h>
#include <tgfclient.h>
#include <robot.h>
#include <raceman.h>
#include <racescreens.h>
#include <robottools.h>
#include <portability.h>

#include "racestate.h"
#include "racemain.h"
#include "racegl.h"
#include "raceinit.h"
#include "raceresults.h"

#include "raceengine.h"

/////////////////////////////////// by Chenyi
#define image_width 640
#define image_height 480
/////////////////////////////////// by Chenyi

static double	msgDisp;
static double	bigMsgDisp;

tRmInfo	*ReInfo = 0;

static void ReRaceRules(tCarElt *car);


/* Compute Pit stop time */
static void
ReUpdtPitTime(tCarElt *car)
{
	tSituation *s = ReInfo->s;
	tReCarInfo *info = &(ReInfo->_reCarInfo[car->index]);
	int i;

	switch (car->_pitStopType) {
		case RM_PIT_REPAIR:
			info->totalPitTime = ReInfo->raceRules.pitstopBaseTime + fabs((double)(car->_pitFuel)) / ReInfo->raceRules.refuelFuelFlow + (tdble)(fabs((double)(car->_pitRepair))) * ReInfo->raceRules.damageRepairFactor + car->_penaltyTime;
			if (ReInfo->s->raceInfo.type == RM_TYPE_PRACTICE || ReInfo->s->raceInfo.type == RM_TYPE_QUALIF) {
				// Ensure that the right min/max values are in the setup structure (could have been modified by the robot))
				RtInitCarPitSetup(car->_carHandle, &(car->pitcmd.setup), true);
			} else {
				// In case of the race no modifications are allowed, so completely reload the structure
				RtInitCarPitSetup(car->_carHandle, &(car->pitcmd.setup), false);
			}
			car->_scheduledEventTime = s->currentTime + info->totalPitTime;
			car->_penaltyTime = 0.0f;
			ReInfo->_reSimItf.reconfig(car);
			for (i=0; i<4; i++) {
				car->_tyreCondition(i) = 1.01;
				car->_tyreT_in(i) = 50.0;
				car->_tyreT_mid(i) = 50.0;
				car->_tyreT_out(i) = 50.0;
			}
			break;
		case RM_PIT_STOPANDGO:
			info->totalPitTime = car->_penaltyTime;
			car->_scheduledEventTime = s->currentTime + info->totalPitTime;
			car->_penaltyTime = 0.0f;
			break;
	}
}

/* Return from interactive pit information */
static void
ReUpdtPitCmd(void *pvcar)
{
	tCarElt *car = (tCarElt*)pvcar;

	ReUpdtPitTime(car);
	GfuiScreenActivate(ReInfo->_reGameScreen);
}

static void
ReRaceMsgUpdate(void)
{
	if (ReInfo->_reCurTime > msgDisp) {
		ReSetRaceMsg("");
	}
	if (ReInfo->_reCurTime > bigMsgDisp) {
		ReSetRaceBigMsg("");
	}
}

static void
ReRaceMsgSet(const char *msg, double life)
{
	if ((ReInfo->_displayMode != RM_DISP_MODE_NONE) && (ReInfo->_displayMode != RM_DISP_MODE_CONSOLE)) {
		ReSetRaceMsg(msg);
		msgDisp = ReInfo->_reCurTime + life;
	}
}


static void
ReRaceBigMsgSet(const char *msg, double life)
{
	if ((ReInfo->_displayMode != RM_DISP_MODE_NONE) && (ReInfo->_displayMode != RM_DISP_MODE_CONSOLE)) {
		ReSetRaceBigMsg(msg);
		bigMsgDisp = ReInfo->_reCurTime + life;
	}
}


static void
ReManage(tCarElt *car)
{
	int i, pitok;
	tTrackSeg *sseg;
	tdble wseg;
	static float color[] = {0.0, 0.0, 1.0, 1.0};
	tSituation *s = ReInfo->s;
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];

	tReCarInfo *info = &(ReInfo->_reCarInfo[car->index]);

	if (car->_speed_x > car->_topSpeed) {
		car->_topSpeed = car->_speed_x;
	}

	// For practice and qualif.
	if (car->_speed_x > info->topSpd) {
		info->topSpd = car->_speed_x;
	}

	if (car->_speed_x < info->botSpd) {
		info->botSpd = car->_speed_x;
		car->_currentMinSpeedForLap = car->_speed_x;
	}

	// Pitstop.
	if (car->_pit) {
		if (car->ctrl.raceCmd & RM_CMD_PIT_ASKED) {
			// Pit already occupied?
			if (car->_pit->pitCarIndex == TR_PIT_STATE_FREE) {
				snprintf(car->ctrl.msg[2], 32, "Can Pit");
			} else {
				snprintf(car->ctrl.msg[2], 32, "Pit Occupied");
			}
			memcpy(car->ctrl.msgColor, color, sizeof(car->ctrl.msgColor));
		}

		if (car->_state & RM_CAR_STATE_PIT) {
			car->ctrl.raceCmd &= ~RM_CMD_PIT_ASKED; // clear the flag.
			if (car->_scheduledEventTime < s->currentTime) {
				car->_state &= ~RM_CAR_STATE_PIT;
				car->_pit->pitCarIndex = TR_PIT_STATE_FREE;
				snprintf(buf, BUFSIZE, "%s pit stop %.1fs", car->_name, info->totalPitTime);
				ReRaceMsgSet(buf, 5);
			} else {
				snprintf(car->ctrl.msg[2], 32, "in pits %.1fs", s->currentTime - info->startPitTime);
			}
		} else if ((car->ctrl.raceCmd & RM_CMD_PIT_ASKED) &&
					car->_pit->pitCarIndex == TR_PIT_STATE_FREE &&
				   (s->_maxDammage == 0 || car->_dammage <= s->_maxDammage))
		{
			tdble lgFromStart = car->_trkPos.seg->lgfromstart;

			switch (car->_trkPos.seg->type) {
				case TR_STR:
					lgFromStart += car->_trkPos.toStart;
					break;
				default:
					lgFromStart += car->_trkPos.toStart * car->_trkPos.seg->radius;
					break;
			}

			if ((lgFromStart > car->_pit->lmin) && (lgFromStart < car->_pit->lmax)) {
				pitok = 0;
				int side;
				tdble toBorder;
				if (ReInfo->track->pits.side == TR_RGT) {
					side = TR_SIDE_RGT;
					toBorder = car->_trkPos.toRight;
				} else {
					side = TR_SIDE_LFT;
					toBorder = car->_trkPos.toLeft;
				}

				sseg = car->_trkPos.seg->side[side];
				wseg = RtTrackGetWidth(sseg, car->_trkPos.toStart);
				if (sseg->side[side]) {
					sseg = sseg->side[side];
					wseg += RtTrackGetWidth(sseg, car->_trkPos.toStart);
				}
				if (((toBorder + wseg) < (ReInfo->track->pits.width - car->_dimension_y / 2.0)) &&
					(fabs(car->_speed_x) < 1.0) &&
					(fabs(car->_speed_y) < 1.0))
				{
					pitok = 1;
				}

				if (pitok) {
					car->_state |= RM_CAR_STATE_PIT;
					car->_nbPitStops++;
					for (i = 0; i < car->_pit->freeCarIndex; i++) {
						if (car->_pit->car[i] == car) {
							car->_pit->pitCarIndex = i;
							break;
						}
					}
					info->startPitTime = s->currentTime;
					snprintf(buf, BUFSIZE, "%s in pits", car->_name);
					ReRaceMsgSet(buf, 5);
					if (car->robot->rbPitCmd(car->robot->index, car, s) == ROB_PIT_MENU) {
						// the pit cmd is modified by menu.
						ReStop();
						RmPitMenuStart(car, ReInfo, (void*)car, ReUpdtPitCmd);
					} else {
						ReUpdtPitTime(car);
					}
				}
			}
		}
	}

	/* Start Line Crossing */
	if (info->prevTrkPos.seg != car->_trkPos.seg) {
		if ((info->prevTrkPos.seg->raceInfo & TR_LAST) && (car->_trkPos.seg->raceInfo & TR_START)) {
			if (info->lapFlag == 0) {
				if ((car->_state & RM_CAR_STATE_FINISH) == 0) {
					car->_laps++;
					car->_remainingLaps--;
					if (car->_laps > 1) {
						car->_lastLapTime = s->currentTime - info->sTime;
						car->_curTime += car->_lastLapTime;
						if (car->_bestLapTime != 0) {
							car->_deltaBestLapTime = car->_lastLapTime - car->_bestLapTime;
						}
						if ((car->_lastLapTime < car->_bestLapTime) || (car->_bestLapTime == 0)) {
							if (car->_commitBestLapTime) {
								car->_bestLapTime = car->_lastLapTime;
							}
						}

						car->_commitBestLapTime = true;

						if (car->_pos != 1) {
							car->_timeBehindLeader = car->_curTime - s->cars[0]->_curTime;
							car->_lapsBehindLeader = s->cars[0]->_laps - car->_laps;
							car->_timeBehindPrev = car->_curTime - s->cars[car->_pos - 2]->_curTime;
							s->cars[car->_pos - 2]->_timeBeforeNext = car->_timeBehindPrev;
						} else {
							car->_timeBehindLeader = 0;
							car->_lapsBehindLeader = 0;
							car->_timeBehindPrev = 0;

							if (ReInfo->_displayMode == RM_DISP_MODE_CONSOLE) {
								printf("Sim Time: %8.2f [s], Leader Laps: %4d, Leader Distance: %8.3f [km]\n", s->currentTime, car->_laps - 1, car->_distRaced/1000.0f);
							}
						}
						info->sTime = s->currentTime;
						switch (ReInfo->s->_raceType) {
							case RM_TYPE_PRACTICE:
								if (ReInfo->_displayMode == RM_DISP_MODE_NONE) {
									ReInfo->_refreshDisplay = 1;
									const int TIMEFMTSIZE=256;
									char t1[TIMEFMTSIZE], t2[TIMEFMTSIZE];
									GfTime2Str(t1, TIMEFMTSIZE, car->_lastLapTime, 0);
									GfTime2Str(t2, TIMEFMTSIZE, car->_bestLapTime, 0);
									snprintf(buf, BUFSIZE, "lap: %02d   time: %s  best: %s  top spd: %.2f    min spd: %.2f    damage: %d",
										car->_laps - 1, t1, t2,
										info->topSpd * 3.6, info->botSpd * 3.6, car->_dammage);
									ReResScreenAddText(buf);
								}
								/* save the lap result */
								ReSavePracticeLap(car);
								break;

							case RM_TYPE_QUALIF:
								if (ReInfo->_displayMode == RM_DISP_MODE_NONE) {
									ReUpdateQualifCurRes(car);
								}
								break;
						}
					} else {
						if ((ReInfo->_displayMode == RM_DISP_MODE_NONE) && (ReInfo->s->_raceType == RM_TYPE_QUALIF)) {
							ReUpdateQualifCurRes(car);
						}
					}

					info->topSpd = car->_speed_x;
					info->botSpd = car->_speed_x;
					car->_currentMinSpeedForLap = car->_speed_x;
					if ((car->_remainingLaps < 0) || (s->_raceState == RM_RACE_FINISHING)) {
						car->_state |= RM_CAR_STATE_FINISH;
						s->_raceState = RM_RACE_FINISHING;
						if (ReInfo->s->_raceType == RM_TYPE_RACE) {
							if (car->_pos == 1) {
								snprintf(buf, BUFSIZE, "Winner %s", car->_name);
								ReRaceBigMsgSet(buf, 10);
							} else {
								const char *numSuffix = "th";
								if (abs(12 - car->_pos) > 1) { /* leave suffix as 'th' for 11 to 13 */
									switch (car->_pos % 10) {
									case 1:
									numSuffix = "st";
									break;
									case 2:
									numSuffix = "nd";
									break;
									case 3:
									numSuffix = "rd";
									break;
									default:
									break;
									}
								}
								snprintf(buf, BUFSIZE, "%s Finished %d%s", car->_name, car->_pos, numSuffix);
								ReRaceMsgSet(buf, 5);
							}
						}
					}
				} else {
					/* prevent infinite looping of cars around track, allow one lap after finish for the first car */
					for (i = 0; i < s->_ncars; i++) {
						s->cars[i]->_state |= RM_CAR_STATE_FINISH;
					}
					return;
				}
			} else {
				info->lapFlag--;
			}
		}

		if ((info->prevTrkPos.seg->raceInfo & TR_START) && (car->_trkPos.seg->raceInfo & TR_LAST)) {
			/* going backward through the start line */
			info->lapFlag++;
		}
	}
	ReRaceRules(car);

	info->prevTrkPos = car->_trkPos;
	car->_curLapTime = s->currentTime - info->sTime;
	car->_distFromStartLine = car->_trkPos.seg->lgfromstart +
	(car->_trkPos.seg->type == TR_STR ? car->_trkPos.toStart : car->_trkPos.toStart * car->_trkPos.seg->radius);
	car->_distRaced = (car->_laps - (info->lapFlag + 1)) * ReInfo->track->length + car->_distFromStartLine;
}


static void ReSortCars(void)
{
	int i, j;
	tCarElt	*car;
	int	allfinish;
	tSituation *s = ReInfo->s;

	if ((s->cars[0]->_state & RM_CAR_STATE_FINISH) == 0) {
		allfinish = 0;
	} else {
		allfinish = 1;
	}

	for (i = 1; i < s->_ncars; i++) {
		j = i;
		while (j > 0) {
			if ((s->cars[j]->_state & RM_CAR_STATE_FINISH) == 0) {
				allfinish = 0;
				if (s->cars[j]->_distRaced > s->cars[j-1]->_distRaced) {
					car = s->cars[j];
					s->cars[j] = s->cars[j-1];
					s->cars[j-1] = car;
					s->cars[j]->_pos = j+1;
					s->cars[j-1]->_pos = j;
					j--;
					continue;
				}
			}
			j = 0;
		}
	}

	if (allfinish) {
		ReInfo->s->_raceState = RM_RACE_ENDED;
	}
}


/* Compute the race rules and penalties */
static void
ReRaceRules(tCarElt *car)
{
	tCarPenalty *penalty;
	tTrack *track = ReInfo->track;
	tRmCarRules *rules = &(ReInfo->rules[car->index]);
	tTrackSeg *seg = RtTrackGetSeg(&(car->_trkPos));
	tReCarInfo *info = &(ReInfo->_reCarInfo[car->index]);
	tTrackSeg *prevSeg = RtTrackGetSeg(&(info->prevTrkPos));
	static float color[] = {0.0, 0.0, 1.0, 1.0};

	// DNF cars which need too much time for the current lap, this is mainly to avoid
	// that a "hanging" driver can stop the quali from finishing.
	// Allowed time is longest pitstop possible + time for tracklength with speed??? (currently fixed 10 [m/s]).
	// for simplicity. Human driver is an exception to this rule, to allow explorers
	// to enjoy the landscape.

//////////////////////////////// by Yurong
	// TODO: Make it configurable.
	// if ((car->_curLapTime > 84.5 + ReInfo->track->length/10.0) &&
	// 	(car->_driverType != RM_DRV_HUMAN))
	// {
	// 	car->_state |= RM_CAR_STATE_ELIMINATED;
	//     return;
	// }
//////////////////////////////// by Yurong

	const int BUFSIZE = 1024;
	char buf[BUFSIZE];

	// If a car hits the track wall the lap time is invalidated, because of tracks where this behaviour allows much faster laps (e.g. alpine-2)
	// Invalidation and message is just shown on the first hit
	if (ReInfo->raceRules.enabled & RmRaceRules::WALL_HIT_TIME_INVALIDATE) {
		if (car->_commitBestLapTime && (car->priv.simcollision & SEM_COLLISION_XYSCENE)) {
			car->_commitBestLapTime = false;
			if (ReInfo->s->_raceType != RM_TYPE_RACE) {
				ReRaceMsgSet("Hit wall, laptime invalidated", 5);
			}
		}
	}

	// If the car cuts a corner the lap time is invalidated. Cutting a corner means: the center of gravity is more than 0.7 times the car width
	// away from the main track segment on the inside of a turn. The rule does not apply on the outside and on straights, pit entry and exit
	// count as well as track.
	tTrackSeg *mainseg = car->_trkPos.seg;
	bool pit = false;
	tTrackPitInfo pitInfo = track->pits;
	tdble toborder = 0.0f;
	tdble minradius = 1.0f;

	if (mainseg->type != TR_STR) {
		if (track->pits.type == TR_PIT_ON_TRACK_SIDE) {
			if (pitInfo.pitEntry->id < pitInfo.pitExit->id) {
				if ((mainseg->id >= pitInfo.pitEntry->id) && (mainseg->id <= pitInfo.pitExit->id)) {
					pit = true;
				}
			} else {
				if ((mainseg->id >= pitInfo.pitEntry->id) || (mainseg->id <= pitInfo.pitExit->id)) {
					pit = true;
				}
			}
		}

		if (mainseg->type == TR_LFT) {
			if (!(pit && (pitInfo.side == TR_LFT))) {
				toborder = car->_trkPos.toLeft;
				minradius = mainseg->radiusl;
			}
		} else if (mainseg->type == TR_RGT) {
			if (!(pit && (pitInfo.side == TR_RGT))) {
				toborder = car->_trkPos.toRight;
				minradius = mainseg->radiusr;
			}
		}
	}

	tdble cuttinglimit = car->_dimension_y*0.7f;
	if (toborder < -cuttinglimit) {
		if (ReInfo->raceRules.enabled & RmRaceRules::CORNER_CUTTING_TIME_INVALIDATE) {
			if (ReInfo->s->_raceType != RM_TYPE_RACE && car->_commitBestLapTime) {
				ReRaceMsgSet("Cut corner, laptime invalidated", 5);
			}
			car->_commitBestLapTime = false;
		}
		if (ReInfo->s->_raceType == RM_TYPE_RACE && ReInfo->raceRules.enabled & RmRaceRules::CORNER_CUTTING_TIME_PENALTY) {
			// In race, apply additionally corner cutting time penalty
			minradius -= cuttinglimit;
			if (minradius > 1.0f) {
				car->_penaltyTime += car->pub.speed*RCM_MAX_DT_SIMU*(-toborder-cuttinglimit)/minradius;
			}
		}
	}

	if (car->_skillLevel < 3) {
		/* only for the pros */
		return;
	}

	penalty = GF_TAILQ_FIRST(&(car->_penaltyList));
	if (penalty) {
		if (car->_laps > penalty->lapToClear) {
			/* too late to clear the penalty, out of race */
			car->_state |= RM_CAR_STATE_ELIMINATED;
			return;
		}

		switch (penalty->penalty) {
			case RM_PENALTY_DRIVETHROUGH:
				snprintf(car->ctrl.msg[3], 32, "Drive Through Penalty");
				break;
			case RM_PENALTY_STOPANDGO:
				snprintf(car->ctrl.msg[3], 32, "Stop And Go Penalty");
				break;
			default:
				*(car->ctrl.msg[3]) = 0;
				break;
		}

		memcpy(car->ctrl.msgColor, color, sizeof(car->ctrl.msgColor));
	}

	if (prevSeg->raceInfo & TR_PITSTART) {
		/* just entered the pit lane */
		if (seg->raceInfo & TR_PIT) {
			/* may be a penalty can be cleaned up */
			if (penalty) {
				switch (penalty->penalty) {
					case RM_PENALTY_DRIVETHROUGH:
						snprintf(buf, BUFSIZE, "%s DRIVE THROUGH PENALTY CLEANING", car->_name);
						ReRaceMsgSet(buf, 5);
						rules->ruleState |= RM_PNST_DRIVETHROUGH;
						break;
					case RM_PENALTY_STOPANDGO:
						snprintf(buf, BUFSIZE, "%s STOP&GO PENALTY CLEANING", car->_name);
						ReRaceMsgSet(buf, 5);
						rules->ruleState |= RM_PNST_STOPANDGO;
						break;
				}
			}
		}
    } else if (prevSeg->raceInfo & TR_PIT) {
		if (seg->raceInfo & TR_PIT) {
			/* the car stopped in pits */
			if (car->_state & RM_CAR_STATE_PIT) {
				if (rules->ruleState & RM_PNST_DRIVETHROUGH) {
					/* it's not more a drive through */
					rules->ruleState &= ~RM_PNST_DRIVETHROUGH;
				} else if (rules->ruleState & RM_PNST_STOPANDGO) {
					rules->ruleState |= RM_PNST_STOPANDGO_OK;
				}
			} else {
				if(rules->ruleState & RM_PNST_STOPANDGO_OK && car->_pitStopType != RM_PIT_STOPANDGO) {
					rules->ruleState &= ~ ( RM_PNST_STOPANDGO | RM_PNST_STOPANDGO_OK );
				}
			}
		} else if (seg->raceInfo & TR_PITEND) {
			/* went out of the pit lane, check if the current penalty is cleared */
			if (rules->ruleState & (RM_PNST_DRIVETHROUGH | RM_PNST_STOPANDGO_OK)) {
				/* clear the penalty */
				snprintf(buf, BUFSIZE, "%s penalty cleared", car->_name);
				ReRaceMsgSet(buf, 5);
				penalty = GF_TAILQ_FIRST(&(car->_penaltyList));
				GF_TAILQ_REMOVE(&(car->_penaltyList), penalty, link);
				FREEZ(penalty);
			}

			rules->ruleState = 0;
		} else {
			/* went out of the pit lane illegally... */
			/* it's a new stop and go... */
			if (!(rules->ruleState & RM_PNST_STNGO)) {
				snprintf(buf, BUFSIZE, "%s STOP&GO PENALTY", car->_name);
				ReRaceMsgSet(buf, 5);
				penalty = (tCarPenalty*)calloc(1, sizeof(tCarPenalty));
				penalty->penalty = RM_PENALTY_STOPANDGO;
				penalty->lapToClear = car->_laps + 5;
				GF_TAILQ_INSERT_TAIL(&(car->_penaltyList), penalty, link);
				rules->ruleState = RM_PNST_STNGO;
			}
		}
    } else if (seg->raceInfo & TR_PITEND) {
		rules->ruleState = 0;
    } else if (seg->raceInfo & TR_PIT) {
		/* entrered the pits not from the pit entry... */
		/* it's a new stop and go... */
		if (!(rules->ruleState & RM_PNST_STNGO)) {
			snprintf(buf, BUFSIZE, "%s STOP&GO PENALTY", car->_name);
			ReRaceMsgSet(buf, 5);
			penalty = (tCarPenalty*)calloc(1, sizeof(tCarPenalty));
			penalty->penalty = RM_PENALTY_STOPANDGO;
			penalty->lapToClear = car->_laps + 5;
			GF_TAILQ_INSERT_TAIL(&(car->_penaltyList), penalty, link);
			rules->ruleState = RM_PNST_STNGO;
		}
    }

	if (seg->raceInfo & TR_SPEEDLIMIT) {
		if (!(rules->ruleState & (RM_PNST_SPD | RM_PNST_STNGO)) && (car->_speed_x > track->pits.speedLimit)) {
			snprintf(buf, BUFSIZE, "%s DRIVE THROUGH PENALTY", car->_name);
			ReRaceMsgSet(buf, 5);
			rules->ruleState |= RM_PNST_SPD;
			penalty = (tCarPenalty*)calloc(1, sizeof(tCarPenalty));
			penalty->penalty = RM_PENALTY_DRIVETHROUGH;
			penalty->lapToClear = car->_laps + 5;
			GF_TAILQ_INSERT_TAIL(&(car->_penaltyList), penalty, link);
		}
	}
}

//////////////////////////////// by Yurong
extern int* pwritten;
extern uint8_t* pdata;
extern uint8_t* pdata_remove_side;
extern uint8_t* pdata_remove_middle;
extern uint8_t* pdata_remove_car;
extern double* psteerCmd_ghost;
extern double* paccelCmd_ghost;
extern double* pbrakeCmd_ghost;
extern double* pspeed_ghost;
extern double* pangle_in_rad_ghost;
extern int* pdamage_ghost;
extern double* ppos_ghost;
extern int* psegtype_ghost;
extern double* pradius_ghost;
extern int* pisEnd;
extern int* pisStuck;
extern double* pdist;
extern bool auto_back;

extern int* pfrontCarNum_ghost;
extern double* pfrontDist_ghost;

extern bool isCollectSeg;

double* psteerCmd;
double* paccelCmd;
double* pbrakeCmd;
double* pspeed;
double* pangle_in_rad;
int* pdamage;
double* ppos;
int* psegtype;
double* pradius;
int* _pisEnd;
double* _pdist;
bool pauto_back;

int* pfrontCarNum;
double* pfrontDist;

#ifdef COLLECTSEG
int drawIndicator = 0;
#endif

int count=0;
/////////////////////////////// end by Yurong
static void
ReOneStep(double deltaTimeIncrement)
{
/////////////////////////// by Yurong
     if (psteerCmd==NULL) {
        psteerCmd=psteerCmd_ghost;
        paccelCmd=paccelCmd_ghost;
        pbrakeCmd=pbrakeCmd_ghost;

		pspeed = pspeed_ghost;
		pangle_in_rad = pangle_in_rad_ghost;
		pdamage = pdamage_ghost;
		ppos = ppos_ghost;
		psegtype = psegtype_ghost;
		pradius = pradius_ghost;
		_pisEnd = pisEnd;
		_pdist = pdist;
		pauto_back = auto_back;
		pfrontCarNum = pfrontCarNum_ghost;
		pfrontDist = pfrontDist_ghost;
     }
/////////////////////////// by Yurong
/////////////////////////// by Yurong, output 1 image per 0.1 second
     // if (ReInfo->s->currentTime >= 0)
     // {
     //    count++;
     //    if (count>100)
     //    {
     //       count=1;
     //       // glReadBuffer(GL_COLOR_ATTACHMENT0);
     //       glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)pdata);
     //       *pwritten=1;

     //       	while (*pwritten == 1){
     //       		usleep(1);
     //       		// printf("waiting!!\n");
     //       	}
     //    }
     // }
/////////////////////////// by Yurong, output 1 image per 0.1 second
	int i;
	tRobotItf *robot;
	tSituation *s = ReInfo->s;

/////////////////////////// by Yurong
	static int zombieID = -1;
	if (zombieID < 0){
		for (i = 0; i < s->_ncars; i++)
			if (strcmp(s->cars[i]->_name, "chenyi") == 0) zombieID = i;
		if (zombieID < 0) zombieID = s->_ncars + 1;
	}
/////////////////////////// end by Yurong

	if ((ReInfo->_displayMode != RM_DISP_MODE_NONE) && (ReInfo->_displayMode != RM_DISP_MODE_CONSOLE)) {
		if (floor(s->currentTime) == -2.0) {
			ReRaceBigMsgSet("Ready", 1.0);
		} else if (floor(s->currentTime) == -1.0) {
			ReRaceBigMsgSet("Set", 1.0);
		} else if (floor(s->currentTime) == 0.0) {
			ReRaceBigMsgSet("Go", 1.0);
		}
	}

	ReInfo->_reCurTime += deltaTimeIncrement * ReInfo->_reTimeMult; /* "Real" time */
	s->currentTime += deltaTimeIncrement; /* Simulated time */

	if (s->currentTime < 0) {
		/* no simu yet */
		ReInfo->s->_raceState = RM_RACE_PRESTART;
	} else if (ReInfo->s->_raceState == RM_RACE_PRESTART) {
		ReInfo->s->_raceState = RM_RACE_RUNNING;
		s->currentTime = 0.0; /* resynchronize */
		ReInfo->_reLastTime = 0.0;
	}

	START_PROFILE("rbDrive*");
	if ((s->currentTime - ReInfo->_reLastTime) >= RCM_MAX_DT_ROBOTS) {
		s->deltaTime = s->currentTime - ReInfo->_reLastTime;
		for (i = 0; i < s->_ncars; i++) {
			if ((s->cars[i]->_state & RM_CAR_STATE_NO_SIMU) == 0) {
				robot = s->cars[i]->robot;
				robot->rbDrive(robot->index, s->cars[i], s);
			}
/////////////////////////// by Yurong
			else if (i == zombieID){
				*pisEnd = 1;
				*pwritten = 1;
				while (true) usleep(1);
			}
/////////////////////////// end by Yurong
		}
		ReInfo->_reLastTime = s->currentTime;
	}
	STOP_PROFILE("rbDrive*");

	START_PROFILE("_reSimItf.update*");
	ReInfo->_reSimItf.update(s, deltaTimeIncrement, -1);
	for (i = 0; i < s->_ncars; i++) {
		ReManage(s->cars[i]);
	}
	STOP_PROFILE("_reSimItf.update*");

	if ((ReInfo->_displayMode != RM_DISP_MODE_NONE) && (ReInfo->_displayMode != RM_DISP_MODE_CONSOLE)) {
		ReRaceMsgUpdate();
	}
	ReSortCars();
}

void
ReStart(void)
{
    ReInfo->_reRunning = 1;
    ReInfo->_reCurTime = GfTimeClock() - RCM_MAX_DT_SIMU;
}

void
ReStop(void)
{
	ReInfo->_reGraphicItf.muteformenu();
    ReInfo->_reRunning = 0;
}

static void
reCapture(void)
{
	unsigned char *img;
	int sw, sh, vw, vh;
	tRmMovieCapture	*capture = &(ReInfo->movieCapture);
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];

	GfScrGetSize(&sw, &sh, &vw, &vh);
	img = (unsigned char*)malloc(vw * vh * 3);
	if (img == NULL) {
		return;
	}

	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_FRONT);
	glReadPixels((sw-vw)/2, (sh-vh)/2, vw, vh, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)img);

	snprintf(buf, BUFSIZE, "%s/torcs-%4.4d-%8.8d.png", capture->outputBase, capture->currentCapture, capture->currentFrame++);
	GfImgWritePng(img, buf, vw, vh);
	free(img);
}


int
ReUpdate(void)
{
	double t;
	tRmMovieCapture	*capture;
	int mode = RM_ASYNC;
	int i;
	const int MAXSTEPS = 2000;

	START_PROFILE("ReUpdate");
	ReInfo->_refreshDisplay = 0;
	switch (ReInfo->_displayMode) {
		case RM_DISP_MODE_NORMAL:
			t = GfTimeClock();
			i = 0;
			// printf("begin simu at time %lf\n", GfTimeClock());
			START_PROFILE("ReOneStep*");
			while ((ReInfo->_reRunning && ((t - ReInfo->_reCurTime) > RCM_MAX_DT_SIMU)) && MAXSTEPS > i++) {
				ReOneStep(RCM_MAX_DT_SIMU);
				// printf("reOneStep %d, t: %lf, ReInfo->_reCurTime: %lf\n", i, t, ReInfo->_reCurTime);
			}
			STOP_PROFILE("ReOneStep*");
			// printf("end simu at time %lf\n", GfTimeClock());

			if (i > MAXSTEPS) {
				// Cannot keep up with time warp, reset time to avoid lag when running slower again
				ReInfo->_reCurTime = GfTimeClock();
			}

			GfuiDisplay();
			ReInfo->_reGraphicItf.refresh(ReInfo->s);
			glutPostRedisplay();	/* Callback -> reDisplay */
			// printf("finish update\n");
			break;

/////////////////////////// by Yurong
		case RM_DISP_MODE_TRAIN:
			// frame skip included
			// t = ReInfo->_reCurTime + 40 * RCM_MAX_DT_SIMU + 0.00004;
			START_PROFILE("ReOneStep*");
			// t = ReInfo->_reCurTime;
			for (int i = 0; ReInfo->_reRunning && i < 40; ++i){
				ReOneStep(RCM_MAX_DT_SIMU);
			}
			// while (ReInfo->_reRunning && ((t - ReInfo->_reCurTime) >= RCM_MAX_DT_SIMU)) {
			// 	ReOneStep(RCM_MAX_DT_SIMU);
			// }
			STOP_PROFILE("ReOneStep*");
			GfuiDisplay();

			glutPostRedisplay();	/* Callback -> reDisplay */
			if (ReInfo->s->currentTime >= 0){
				// printf("now time: %f\n", ReInfo->s->currentTime);
				// usleep(100);
				// glReadBuffer(GL_COLOR_ATTACHMENT0);
#ifdef COLLECTSEG
					drawIndicator = 3;
					ReInfo->_reGraphicItf.refresh(ReInfo->s);
					glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)pdata_remove_car);
					drawIndicator = 1;
					ReInfo->_reGraphicItf.refresh(ReInfo->s);
					glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)pdata_remove_side);
					drawIndicator = 2;
					ReInfo->_reGraphicItf.refresh(ReInfo->s);
					glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)pdata_remove_middle);
					drawIndicator = 0;
#endif
					ReInfo->_reGraphicItf.refresh(ReInfo->s);
					glReadPixels(0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, (GLvoid*)pdata);
				*pwritten=1;
				// printf("%f\n", ReInfo->_reCurTime - t);
				while (*pwritten == 1){
					usleep(1);
					// printf("waiting!!\n");
				}
			}
		break;
/////////////////////////// end by Yurong

		case RM_DISP_MODE_NONE:
			// Just update view once per 2 seconds simulation time to avoid trouble with graphics cards
			// which are bad in buffer switching (e.g. ATI fglrx driver on Linux).
			t = ReInfo->_reCurTime;
			while ((t - ReInfo->_reCurTime + 2.0) > 0.0) {
				ReOneStep(RCM_MAX_DT_SIMU);
			}

			GfuiDisplay();
			glutPostRedisplay();	/* Callback -> reDisplay */
			break;

		case RM_DISP_MODE_CAPTURE:
			capture = &(ReInfo->movieCapture);
			while ((ReInfo->_reCurTime - capture->lastFrame) < capture->deltaFrame) {
				ReOneStep(capture->deltaSimu);
			}
			capture->lastFrame = ReInfo->_reCurTime;

			GfuiDisplay();
			ReInfo->_reGraphicItf.refresh(ReInfo->s);
			reCapture();
			glutPostRedisplay();	/* Callback -> reDisplay */
			break;

		case RM_DISP_MODE_CONSOLE:
			t = ReInfo->_reCurTime;
			while ((t - ReInfo->_reCurTime + 2.0) > 0.0) {
				ReOneStep(RCM_MAX_DT_SIMU);
			}
			mode = RM_SYNC;
			break;

	}
	STOP_PROFILE("ReUpdate");
/////////////////////////// by Yurong
	// if (*pisEnd) {
 //       	*pisEnd = 0;
 //        ReRaceCleanup();
	// 	mode = RE_STATE_PRE_RACE;
 //    }
/////////////////////////// end by Yurong
	return mode;
}

void
ReTimeMod (void *vcmd)
{
	long cmd = (long)vcmd;

	switch ((int)cmd) {
		case 0:
			ReInfo->_reTimeMult *= 2.0;
			if (ReInfo->_reTimeMult > 64.0) {
				ReInfo->_reTimeMult = 64.0;
			}
			break;
		case 1:
			ReInfo->_reTimeMult *= 0.5;
			if (ReInfo->_reTimeMult < 1.0f/128.0f) {
				ReInfo->_reTimeMult = 1.0f/128.0f;
			}
			break;
		case 2:
			default:
			ReInfo->_reTimeMult = 1.0;
			break;
	}

	const int BUFSIZE = 1024;
	char buf[BUFSIZE];

	snprintf(buf, BUFSIZE, "Time x%.2f", 1.0 / ReInfo->_reTimeMult);
	ReRaceMsgSet(buf, 5);
}
