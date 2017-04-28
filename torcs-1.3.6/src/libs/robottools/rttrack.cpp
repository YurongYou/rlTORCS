/***************************************************************************
                      rttrack.cpp -- Track utilities functions                              
                             -------------------                                         
    created              : Sat Aug 14 23:03:22 CEST 1999
    copyright            : (C) 1999-2013 by Eric Espie, Bernhard Wymann                         
    email                : torcs@free.fr   
    version              : $Id: rttrack.cpp,v 1.21.2.12 2013/08/31 12:57:31 berniw Exp $                                  
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
    		This is a collection of useful functions for programming a robot.
    @author	<a href=mailto:torcs@free.fr>Eric Espie</a>
    @version	$Id: rttrack.cpp,v 1.21.2.12 2013/08/31 12:57:31 berniw Exp $
    @ingroup	robottools
*/

/** @defgroup tracktools	Track related tools for robots.
    All the accesses to the track structure, car position...
    @ingroup	robottools
*/

#include <portability.h>
#include <stdlib.h>
#include <math.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <tgf.h>
#include <car.h>
#include <track.h>

#include <robottools.h>

/** Get the track width at the specified point.
    @ingroup	tracktools
    @param	seg	Segment
    @param	toStart	Distance from the beginning of the segment.
    		<br>The units are:
		- meters for straights
		- radians for turns
    @return	Width of the track at this point.
    @note	The Pit lane and the track have different width, and the side segments have variable width.
 */
tdble
RtTrackGetWidth(tTrackSeg *seg, tdble toStart)
{
	return fabs(seg->startWidth + toStart * seg->Kyl);
}


/** Convert a Local position (segment, toRight, toStart)
    @ingroup	tracktools
    into a Global one (X, Y)
    The ToStart position refers to the current segment,
    the function will not search for next segment if toStart
    is greater than the segment length.
    toStart represent an angle in radian for curves
    and a length in meters for straights.
    @param	p	Local position
    @param	X	returned X position
    @param	Y	returned Y position
    @param	flag	Local position use:
			- TR_TOMIDDLE the toMiddle field is used
			- TR_TORIGHT the toRight field is used
			- TR_TOLEFT the toLeft field is used
*/
void
RtTrackLocal2Global(tTrkLocPos *p, tdble *X, tdble *Y, int flag)
{
	tdble CosA, SinA, r, a;
	tdble tr;

	tTrackSeg *seg = p->seg;
	switch (flag) {
		case TR_TOMIDDLE:
			switch(seg->type) {
				case TR_STR:
					CosA = cos(seg->angle[TR_ZS]);
					SinA = sin(seg->angle[TR_ZS]);
					/* Jussi Pajala: must be divided by two to get middle of the track ! */
					tr = p->toMiddle + seg->startWidth / 2.0; 
					*X = seg->vertex[TR_SR].x + p->toStart * CosA - tr * SinA;
					*Y = seg->vertex[TR_SR].y + p->toStart * SinA + tr * CosA;
					break;
					
				case TR_LFT:
					a = seg->angle[TR_ZS] + p->toStart;
					r = seg->radius - p->toMiddle;
					*X = seg->center.x + r * sin(a);
					*Y = seg->center.y - r * cos(a);
					break;

				case TR_RGT:
					a = seg->angle[TR_ZS] - p->toStart;
					r = seg->radius + p->toMiddle;
					*X = seg->center.x - r * sin(a);
					*Y = seg->center.y + r * cos(a);
					break;
					
			}
			break;

		case TR_TORIGHT:
			switch(seg->type) {
				case TR_STR:
					CosA = cos(seg->angle[TR_ZS]);
					SinA = sin(seg->angle[TR_ZS]);
					switch (seg->type2) {
						case TR_MAIN:
						case TR_LSIDE:
						case TR_LBORDER:
							tr = p->toRight;
							break;
						case TR_RSIDE:
						case TR_RBORDER:
							tr = p->toRight - seg->Kyl * p->toStart;
							break;
						default:
							tr = 0;
							break;
					}
					*X = seg->vertex[TR_SR].x + p->toStart * CosA - tr * SinA;
					*Y = seg->vertex[TR_SR].y + p->toStart * SinA + tr * CosA;
					break;
					
				case TR_LFT:
					a = seg->angle[TR_ZS] + p->toStart;
					switch (seg->type2) {
						case TR_MAIN:
						case TR_LSIDE:
						case TR_LBORDER:
							r = seg->radiusr - p->toRight ;
							break;
						case TR_RSIDE:
						case TR_RBORDER:
							r = seg->radiusl + seg->startWidth + seg->Kyl * p->toStart - p->toRight ;
							break;
						default:
							r = 0;
							break;
					}
					*X = seg->center.x + r * sin(a);
					*Y = seg->center.y - r * cos(a);
					break;

				case TR_RGT:
					a = seg->angle[TR_ZS] - p->toStart;
					switch (seg->type2) {
						case TR_MAIN:
						case TR_LSIDE:
						case TR_LBORDER:
							r = seg->radiusr + p->toRight ;
							break;
						case TR_RSIDE:
						case TR_RBORDER:
							r = seg->radiusl - seg->startWidth - seg->Kyl * p->toStart + p->toRight ;
							break;
						default:
							r = 0;
							break;
					}
					*X = seg->center.x - r * sin(a);
					*Y = seg->center.y + r * cos(a);
					break;
			}
			break;

		case TR_TOLEFT:
			switch(seg->type) {
				case TR_STR:
					CosA = cos(seg->angle[TR_ZS]);
					SinA = sin(seg->angle[TR_ZS]);
					tr = seg->startWidth + seg->Kyl * p->toStart - p->toLeft;
					*X = seg->vertex[TR_SR].x + p->toStart * CosA - tr * SinA;
					*Y = seg->vertex[TR_SR].y + p->toStart * SinA + tr * CosA;
					break;
					
				case TR_LFT:
					a = seg->angle[TR_ZS] + p->toStart;
					r = seg->radiusl + p->toLeft;
					*X = seg->center.x + r * sin(a);
					*Y = seg->center.y - r * cos(a);
					break;

				case TR_RGT:
					a = seg->angle[TR_ZS] - p->toStart;
					r = seg->radiusr + seg->startWidth + seg->Kyl * p->toStart - p->toLeft;
					*X = seg->center.x - r * sin(a);
					*Y = seg->center.y + r * cos(a);
					break;
			}
			break;
	}
}

/** Convert a Global (segment, X, Y) position into a Local one (segment, toRight, toStart)
    @ingroup	tracktools
    The segment in the Global position is used to start the search of a good segment
    in term of toStart value.
    The segments are scanned in order to find a toStart value between 0 and the length
    of the segment for straights or the arc of the curve.
    The sides parameters is to indicate wether to use the track sides (1) or not (0) in
    the toRight computation.
    @param	segment	Current segment
    @param	X	Current X position
    @param	Y	Current Y position
    @param	p	Returned local position
    @param	type	Type of local position desired:
    			- TR_LPOS_MAIN relative to the main segment
			- TR_LPOS_SEGMENT if the point is on a side, relative to this side
			- TR_LPOS_TRACK local pos includes all the track width
 */

void
RtTrackGlobal2Local(tTrackSeg *segment, tdble X, tdble Y, tTrkLocPos *p, int type)
{
	int segnotfound = 1;
	tdble x, y;
	tTrackSeg *seg = segment;
	tTrackSeg *sseg;
	tdble theta, a2;
	int depl = 0;
	tdble curWidth;

	p->type = type;

	while (segnotfound) {

		switch(seg->type) {
			case TR_STR:
				/* rotation */
				tdble sine, cosine;
				tdble ts;

				sine = sin(seg->angle[TR_ZS]);
				cosine = cos(seg->angle[TR_ZS]);
				x = X - seg->vertex[TR_SR].x;
				y = Y - seg->vertex[TR_SR].y;
				ts = x * cosine + y * sine;
				p->seg = seg;
				p->toStart = ts;
				p->toRight = y * cosine - x * sine;
				if ((ts < 0) && (depl < 1)) {
					/* get back */
					seg = seg->prev;
					depl = -1;
				} else if ((ts > seg->length) && (depl > -1)) {
					seg = seg->next;
					depl = 1;
				} else {
					segnotfound = 0;
				}
				break;
				
			case TR_LFT:
				/* rectangular to polar */
				x = X - seg->center.x;
				y = Y - seg->center.y;
				a2 = seg->arc / 2.0;
				theta = atan2(y, x) - (seg->angle[TR_CS] + a2);
				NORM_PI_PI(theta);
				p->seg = seg;
				p->toStart = theta + a2;
				p->toRight = seg->radiusr - sqrt(x*x + y*y);
				if ((theta < -a2) && (depl < 1)) {
					seg = seg->prev;
					depl = -1;
				} else if ((theta > a2) && (depl > -1)) {
					seg = seg->next;
					depl = 1;
				} else {
					segnotfound = 0;
				}
				break;

			case TR_RGT:
				/* rectangular to polar */
				
				x = X - seg->center.x;
				y = Y - seg->center.y;
				a2 = seg->arc / 2.0;
				theta = seg->angle[TR_CS] - a2 - atan2(y, x);
				NORM_PI_PI(theta);
				p->seg = seg;
				p->toStart = theta + a2;
				p->toRight = sqrt(x*x + y*y) - seg->radiusr;
				if ((theta < -a2) && (depl < 1)) {
					seg = seg->prev;
					depl = -1;
				} else if ((theta > a2) && (depl > -1)) {
					seg = seg->next;
					depl = 1;
				} else {
					segnotfound = 0;
				}
				break;
		}
	}

	/* The track is of constant width */
	/* This is subject to change */
	p->toMiddle = p->toRight - seg->width / 2.0;
	p->toLeft = seg->width - p->toRight;

	/* Consider all the track with the sides */
	/* Stay on main segment */
	if (type == TR_LPOS_TRACK) {
		if (seg->rside != NULL) {
			sseg = seg->rside;
			p->toRight += RtTrackGetWidth(sseg, p->toStart);
			sseg = sseg->rside;
			if (sseg) {
				p->toRight += RtTrackGetWidth(sseg, p->toStart);
			}
		}
		if (seg->lside != NULL) {
			sseg = seg->lside;
			p->toLeft += RtTrackGetWidth(sseg, p->toStart);
			sseg = sseg->lside;
			if (sseg) {
				p->toLeft += RtTrackGetWidth(sseg, p->toStart);
			}
		}
	}

	/* Relative to a segment, change to the side segment if necessary */
	if (type == TR_LPOS_SEGMENT) {
		if ((p->toRight < 0) && (seg->rside != NULL)) {
			sseg = seg->rside;
			p->seg = sseg;
			curWidth = RtTrackGetWidth(sseg, p->toStart);
			p->toRight +=  curWidth;
			p->toLeft -= seg->width;
			p->toMiddle += (seg->width + curWidth) / 2.0;
			if ((p->toRight < 0) && (sseg->rside != NULL)) {
				p->toLeft -= curWidth;
				p->toMiddle += curWidth / 2.0;
				seg = sseg;
				sseg = seg->rside;
				curWidth = RtTrackGetWidth(sseg, p->toStart);
				p->seg = sseg;
				p->toRight +=  curWidth;
				p->toMiddle += curWidth / 2.0;
			}
		} else if ((p->toLeft < 0) && (seg->lside != NULL)) {
			sseg = seg->lside;
			p->seg = sseg;
			curWidth = RtTrackGetWidth(sseg, p->toStart);
			p->toRight += -seg->width;
			p->toMiddle -= (seg->width + curWidth) / 2.0;
			p->toLeft += curWidth;
			if ((p->toLeft < 0) && (sseg->lside != NULL)) {
				p->toRight -= curWidth;
				p->toMiddle -= curWidth / 2.0;
				seg = sseg;
				sseg = seg->lside;
				curWidth = RtTrackGetWidth(sseg, p->toStart);
				p->seg = sseg;
				p->toMiddle -= curWidth / 2.0;
				p->toLeft += curWidth;
			}
		}
	}
}

/** Returns the absolute height in meters of the road
    at the Local position p.
    If the point lies outside the track (and sides)
    the height is computed using the tangent to the banking
    of the segment (or side).
    @verbatim
|                + Point given
|               .^
|              . |
|             .  |
|            .   |
|           /    | heigth
|          /     |
|   ______/      v
|   ^    ^^  ^
|   |    ||  |
|    track side
    @endverbatim
    @ingroup	tracktools
    @param	p	Local position
    @return	Height in meters
 */
tdble
RtTrackHeightL(tTrkLocPos *p)
{
	tdble lg;
	tdble tr = p->toRight;
	tTrackSeg *seg = p->seg;
	
	//bool left_side = true;
	if ((tr < 0) && (seg->rside != NULL)) {
		//left_side = false;

		seg = seg->rside;
		tr += seg->width;

		if ((tr < 0) && (seg->rside != NULL)) {
			seg = seg->rside;
			tr += RtTrackGetWidth(seg, p->toStart);
		}   
	} else if ((tr > seg->width) && (seg->lside != NULL)) {
		tr -= seg->width;
		seg = seg->lside;
		if ((tr > seg->width) && (seg->lside != NULL)) {
			tr -= RtTrackGetWidth(seg, p->toStart);
			seg = seg->lside;
		}
	}

	switch (seg->type) {
		case TR_STR:
			lg = p->toStart;
			break;
		default:
			lg = p->toStart * seg->radius;
			break;
	}
	
	if (seg->style == TR_CURB) {
		// The final height = starting height + height difference due
		// to track angle + height difference due to curb (this seems
		// to be the way it is implemented in the graphics too: the
		// curb does not adding an angle to the main track, but a
		// height in global coords).
		if (seg->type2 == TR_RBORDER) {
			// alpha shows how far we've moved into this segment.
			tdble alpha = seg->width - tr;
			tdble angle = seg->angle[TR_XS] + p->toStart * seg->Kzw;
			tdble noise = seg->surface->kRoughness * sin(seg->surface->kRoughWaveLen * lg) * alpha / seg->width;
			tdble start_height = seg->vertex[TR_SR].z + p->toStart * seg->Kzl;
			return start_height + tr * tan(angle) + alpha * atan2(seg->height, seg->width) + noise;
		}

		return
			seg->vertex[TR_SR].z + p->toStart * seg->Kzl +
			tr * (tan(seg->angle[TR_XS] + p->toStart * seg->Kzw) +
			atan2(seg->height, seg->width)) +
			seg->surface->kRoughness * sin(seg->surface->kRoughWaveLen * lg) * tr / seg->width;
	}

	return seg->vertex[TR_SR].z + p->toStart * seg->Kzl + tr * tan(seg->angle[TR_XS] + p->toStart * seg->Kzw) +
	seg->surface->kRoughness * sin(seg->surface->kRoughWaveLen * tr) * sin(seg->surface->kRoughWaveLen * lg);
}

/* get the real segment */
tTrackSeg *
RtTrackGetSeg(tTrkLocPos *p)
{
	tdble tr = p->toRight;
	tTrackSeg *seg = p->seg;

	if ((tr < 0) && (seg->rside != NULL)) {
		seg = seg->rside;
		tr += seg->width;
		if ((tr < 0) && (seg->rside != NULL)) {
			seg = seg->rside;
			tr += RtTrackGetWidth(seg, p->toStart);
		}   
	} else if ((tr > seg->width) && (seg->lside != NULL)) {
		tr -= seg->width;
		seg = seg->lside;
		if ((tr > seg->width) && (seg->lside != NULL)) {
			tr -= RtTrackGetWidth(seg, p->toStart);
			seg = seg->lside;
		}
	}
	return seg;
}

/** Returns the absolute height in meters of the road
    at the Global position (segment, X, Y)
    @ingroup	tracktools
    @param	seg	Segment
    @param	X	Global X position
    @param	Y	Global Y position
    @return	Height in meters
 */
tdble
RtTrackHeightG(tTrackSeg *seg, tdble X, tdble Y)
{
	tTrkLocPos p;

	RtTrackGlobal2Local(seg, X, Y, &p, TR_LPOS_SEGMENT);
	return RtTrackHeightL(&p);
}

/** Give the normal vector of the border of the track
    including the sides.
    The side parameter is used to indicate the right (TR_RGT)
    of the left (TR_LFT) side to consider.
    The Global position given (segment, X, Y) is used
    to project the point on the border, it is not necessary
    to give a point directly on the border itself.
    The vector is normalized.
    @ingroup	tracktools
    @param	seg	Current segment
    @param	X	Global X position
    @param	Y	Global Y position
    @param	side	Side where the normal is wanted
			- TR_LFT for left side
			- TR_RGT for right side
    @param	norm	Returned normalized side normal vector
    @todo	RtTrackSideNormalG: Give the correct normal for variable width sides.
 */
void
RtTrackSideNormalG(tTrackSeg *seg, tdble X, tdble Y, int side, t3Dd *norm)
{
	tdble lg;

	switch (seg->type) {
		case TR_STR:
			if (side == TR_RGT) {
				norm->x = seg->rgtSideNormal.x;
				norm->y = seg->rgtSideNormal.y;
			} else {
				norm->x = -seg->rgtSideNormal.x;
				norm->y = -seg->rgtSideNormal.y;
			}
			break;
		case TR_RGT:
			if (side == TR_LFT) {
				norm->x = seg->center.x - X;
				norm->y = seg->center.y - Y;
			} else {
				norm->x = X - seg->center.x;
				norm->y = Y - seg->center.y;
			}
			lg = 1.0 / sqrt(norm->x * norm->x + norm->y * norm->y);
			norm->x *= lg;
			norm->y *= lg;
			break;
		case TR_LFT:
			if (side == TR_RGT) {
				norm->x = seg->center.x - X;
				norm->y = seg->center.y - Y;   
			} else {
				norm->x = X - seg->center.x;
				norm->y = Y - seg->center.y;
			}
			lg = 1.0 / sqrt(norm->x * norm->x + norm->y * norm->y);
			norm->x *= lg;
			norm->y *= lg;
		break;
	}
}

/** Used to get the tangent angle for a track position
    The angle is given in radian.
    the angle 0 is parallel to the first segment start.
    @ingroup	tracktools
    @param	p	Local position
    @return	Tagent angle in radian.
    @note	For side segment, the track side is used for the tangent.
 */

tdble
RtTrackSideTgAngleL(tTrkLocPos *p)
{
	switch (p->seg->type) {
		case TR_STR:
			return p->seg->angle[TR_ZS];
			break;
		case TR_RGT:
			return p->seg->angle[TR_ZS] - p->toStart;
			break;
		case TR_LFT:
			return p->seg->angle[TR_ZS] + p->toStart;
			break;
	}
	return 0;
}


/** Used to get the normal vector of the road (pointing upward).
    Local coordinates are used to locate the point where to
    get the road normal vector.
    The vector is normalized. 
    @ingroup	tracktools
    @param	p	Local position
    @param	norm	Returned normalized road normal vector
 */
void
RtTrackSurfaceNormalL(tTrkLocPos *p, t3Dd *norm)
{
	tTrkLocPos p1;
	t3Dd px1, px2, py1, py2;
	t3Dd v1, v2;
	tdble lg;

	p1.seg = p->seg;

	p1.toStart = 0;
	p1.toRight = p->toRight;
	RtTrackLocal2Global(&p1, &px1.x, &px1.y, TR_TORIGHT);
	px1.z = RtTrackHeightL(&p1);

	if (p1.seg->type == TR_STR) {
		p1.toStart = p1.seg->length;
	} else {
		p1.toStart = p1.seg->arc;
	}
	
	RtTrackLocal2Global(&p1, &px2.x, &px2.y, TR_TORIGHT);
	px2.z = RtTrackHeightL(&p1);

	p1.toRight = 0;
	p1.toStart = p->toStart;
	RtTrackLocal2Global(&p1, &py1.x, &py1.y, TR_TORIGHT);
	py1.z = RtTrackHeightL(&p1);

	p1.toRight = p1.seg->width;
	RtTrackLocal2Global(&p1, &py2.x, &py2.y, TR_TORIGHT);
	py2.z = RtTrackHeightL(&p1);


	v1.x = px2.x - px1.x;
	v1.y = px2.y - px1.y;
	v1.z = px2.z - px1.z;
	v2.x = py2.x - py1.x;
	v2.y = py2.y - py1.y;
	v2.z = py2.z - py1.z;

	norm->x = v1.y * v2.z - v2.y * v1.z;
	norm->y = v2.x * v1.z - v1.x * v2.z;
	norm->z = v1.x * v2.y - v2.x * v1.y;
	lg = sqrt(norm->x * norm->x + norm->y * norm->y + norm->z * norm->z);
	
	if (lg == 0.0) {
		lg = 1.0;
	} else {
		lg = 1.0 / lg;
	}
	
	norm->x *= lg;
	norm->y *= lg;
	norm->z *= lg;
}


/** Get the distance from the start lane.
    @ingroup	tracktools
    @param	car 	the concerned car.
    @return	The distance between the start lane and the car.
 */
tdble
RtGetDistFromStart(tCarElt *car)
{
	tTrackSeg *seg;
	tdble lg;

	seg = car->_trkPos.seg;
	lg = seg->lgfromstart;

	switch (seg->type) {
		case TR_STR:
			lg += car->_trkPos.toStart;
			break;
		default:
			lg += car->_trkPos.toStart * seg->radius;
			break;
	}

	return lg;
}

/** Get the distance from the start lane.
    @ingroup	tracktools
    @param	p	Local position
    @return	The distance between the start lane and the car.
 */
tdble
RtGetDistFromStart2(tTrkLocPos *p)
{
	tTrackSeg *seg;
	tdble lg;

	seg = p->seg;
	lg = seg->lgfromstart;

	switch (seg->type) {
		case TR_STR:
			lg += p->toStart;
			break;
		default:
			lg += p->toStart * seg->radius;
			break;
	}

	return lg;
}


/** Get the distance to the pit stop.
    @ingroup	tracktools
    @param	car 	The concerned car.
    @param	track	The current Track
    @param	dL	Length to the pits
    @param	dW	Width to the pits
    @return	0
    @note	dW > 0 if the pit is on the right
*/
int
RtDistToPit(struct CarElt *car, tTrack *track, tdble *dL, tdble *dW)
{
	tTrkLocPos *pitpos;
	tTrkLocPos *carpos;
	tdble pitts;
	tdble carts;

	if (car->_pit == NULL) return 1;

	pitpos = &(car->_pit->pos);
	carpos = &(car->_trkPos);

	if (carpos->seg->radius) {
		carts = carpos->toStart * carpos->seg->radius;
	} else {
		carts = carpos->toStart;
	}
	
	if (pitpos->seg->radius) {
		pitts = pitpos->toStart * pitpos->seg->radius;
	} else {
		pitts = pitpos->toStart;
	}

	*dL = pitpos->seg->lgfromstart - carpos->seg->lgfromstart + pitts - carts;
	if (*dL < 0.0f) {
		*dL += track->length;
	} else if (*dL > track->length) {
		*dL -= track->length;
	}
	
	*dW = pitpos->toRight - carpos->toRight;

	return 0;
}


static void RtReadCarPitSetupEntry(tCarPitSetupValue* v, const char* path, const char* key, void *hdle, bool minmaxonly)
{
	if (!minmaxonly) {
		v->value = GfParmGetNum(hdle, path, key, (char*)NULL, 0.0f);
	}
	GfParmGetNumBoundaries(hdle, path, key, &v->min, &v->max);
}


void RtInitCarPitSetup(void *hdle,  tCarPitSetup* s, bool minmaxonly)
{
	static const char *WheelSect[4] = {SECT_FRNTRGTWHEEL, SECT_FRNTLFTWHEEL, SECT_REARRGTWHEEL, SECT_REARLFTWHEEL};
	static const char *SuspSect[4] = {SECT_FRNTRGTSUSP, SECT_FRNTLFTSUSP, SECT_REARRGTSUSP, SECT_REARLFTSUSP};
	const int BUFSIZE = 256;
	char path[BUFSIZE];

	// Steer
	RtReadCarPitSetupEntry(&s->steerLock, SECT_STEER, PRM_STEERLOCK, hdle, minmaxonly);

	int i;
	for (i=0; i < 4; i++) {
		// Wheel
		RtReadCarPitSetupEntry(&s->wheelcamber[i], WheelSect[i], PRM_CAMBER, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->wheeltoe[i], WheelSect[i], PRM_TOE, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->wheelrideheight[i], WheelSect[i], PRM_RIDEHEIGHT, hdle, minmaxonly);

		// Suspension
		RtReadCarPitSetupEntry(&s->suspspring[i], SuspSect[i], PRM_SPR, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->susppackers[i], SuspSect[i], PRM_PACKERS, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->suspslowbump[i], SuspSect[i], PRM_SLOWBUMP, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->suspslowrebound[i], SuspSect[i], PRM_SLOWREBOUND, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->suspfastbump[i], SuspSect[i], PRM_FASTBUMP, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->suspfastrebound[i], SuspSect[i], PRM_FASTREBOUND, hdle, minmaxonly);
	}

	// Brake
	RtReadCarPitSetupEntry(&s->brakeRepartition, SECT_BRKSYST, PRM_BRKREP, hdle, minmaxonly);
	RtReadCarPitSetupEntry(&s->brakePressure, SECT_BRKSYST, PRM_BRKPRESS, hdle, minmaxonly);

	// Anti roll bar
	static const char *ArbSect[2] = {SECT_FRNTARB, SECT_REARARB};
	for (i=0; i < 2; i++) { 
		RtReadCarPitSetupEntry(&s->arbspring[i], ArbSect[i], PRM_SPR, hdle, minmaxonly);	
	}

	// Third element
	static const char *AxleSect[2] = {SECT_FRNTAXLE, SECT_REARAXLE};
	for (i=0; i < 2; i++) { 
		RtReadCarPitSetupEntry(&s->thirdspring[i], AxleSect[i], PRM_SPR, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->thirdbump[i], AxleSect[i], PRM_SLOWBUMP, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->thirdrebound[i], AxleSect[i], PRM_SLOWREBOUND, hdle, minmaxonly);
		RtReadCarPitSetupEntry(&s->thirdX0[i], AxleSect[i], PRM_SUSPCOURSE, hdle, minmaxonly);
	}

	// Gears
	for (i=0; i < 8; i++) { 
		snprintf(path, BUFSIZE, "%s/%s/%d", SECT_GEARBOX, ARR_GEARS, i+1);
		RtReadCarPitSetupEntry(&s->gearsratio[i], path, PRM_RATIO, hdle, minmaxonly);
	}

	// Wings
	static const char *WingSect[2] = {SECT_FRNTWING, SECT_REARWING};
	for (i=0; i < 2; i++) { 
		RtReadCarPitSetupEntry(&s->wingangle[i], WingSect[i], PRM_WINGANGLE, hdle, minmaxonly);
	}

	// Differentials
	static const char *DiffSect[3] = {SECT_FRNTDIFFERENTIAL, SECT_REARDIFFERENTIAL, SECT_CENTRALDIFFERENTIAL};
	for (i=0; i < 3; i++) { 
		RtReadCarPitSetupEntry(&s->diffratio[i], DiffSect[i], PRM_RATIO, hdle, minmaxonly);	
		RtReadCarPitSetupEntry(&s->diffmintqbias[i], DiffSect[i], PRM_MIN_TQ_BIAS, hdle, minmaxonly);	
		RtReadCarPitSetupEntry(&s->diffmaxtqbias[i], DiffSect[i], PRM_MAX_TQ_BIAS, hdle, minmaxonly);	
		RtReadCarPitSetupEntry(&s->diffslipbias[i], DiffSect[i], PRM_MAX_SLIP_BIAS, hdle, minmaxonly);	
		RtReadCarPitSetupEntry(&s->difflockinginputtq[i], DiffSect[i], PRM_LOCKING_TQ, hdle, minmaxonly);	
		RtReadCarPitSetupEntry(&s->difflockinginputbraketq[i], DiffSect[i], PRM_LOCKINGBRAKE_TQ, hdle, minmaxonly);	
	
		const char* type = GfParmGetStr(hdle, DiffSect[i], PRM_TYPE, VAL_DIFF_NONE);
		if (strcmp(type, VAL_DIFF_LIMITED_SLIP) == 0) {
			s->diffType[i] = tCarPitSetup::LIMITED_SLIP; 
		} else if (strcmp(type, VAL_DIFF_VISCOUS_COUPLER) == 0) {
			s->diffType[i] = tCarPitSetup::VISCOUS_COUPLER;
		} else if (strcmp(type, VAL_DIFF_SPOOL) == 0) {
			s->diffType[i] = tCarPitSetup::SPOOL;
		}  else if (strcmp(type, VAL_DIFF_FREE) == 0) {
			s->diffType[i] = tCarPitSetup::FREE;
		} else {
			s->diffType[i] = tCarPitSetup::NONE; 
		}
	}
}

static const char* CarPitSetupFilenames[6] = { "practice", "qualifying", "race", "backup1", "backup2", "backup3" };

void RtGetCarPitSetupFilename(
	rtCarPitSetupType type,		// the setup type
	int robidx,					// player/robot instance
	const char* carname,		// car filename
	const char* trackname,		// track file name
	char* filename,				// buffer for result								  
	const int len				// buffer size						  							  
)
{
	snprintf(filename, len, "%s_%s_%d_%s" , carname, trackname, robidx, CarPitSetupFilenames[type]);
}


/**	Robottool internal: Set parameter if min != max, save as well min and max values	
	@ingroup	tracktools
	@param hdlesetup	Handle to set parameter into
	@param	key			key	name
	@param	unit		unit to convert the result to (NULL if SI wanted)	
	@param	v			tCarPitSetupValue to set	
*/
static void RtParmSetNum(void* hdlesetup, const char* path, const char* key, const char* unit, tCarPitSetupValue* v)
{
	// If min == max there is nothing to adjust, so we do not need to write the value.
	if (fabs(v->min - v->max) >= 0.0001f) {
		GfParmSetNumEx(hdlesetup, path, key, unit, GfParmSI2Unit(unit, v->value), GfParmSI2Unit(unit, v->min), GfParmSI2Unit(unit, v->max));
	}
}


/**	Save a custom car setup to a given filename. The setup is validated against the setup given in hdlecar.
	@ingroup	tracktools
	@param hdlecar		Handle to master setup to validate against (min/max and other checks)
	@param s			Pointer to tCarPitSetup struct to save
	@param filepath		Full path to setup file
	@param carname		TORCS internal name car name
 */
void RtSaveCarPitSetupFile(
	void *hdlecar,			// handle to car definition file, for min/max merge
	tCarPitSetup* s,		// the setup data to save
	const char* filepath,	// full path including filename and extension
	const char* carname
)	
{
	const int BUFSIZE = 256;
	char path[BUFSIZE];
	void* hdlesetup = GfParmReadFile(filepath, GFPARM_RMODE_STD | GFPARM_RMODE_CREAT);
	
	// Steer
	RtParmSetNum(hdlesetup, SECT_STEER, PRM_STEERLOCK, "deg", &s->steerLock);

	static const char *WheelSect[4] = {SECT_FRNTRGTWHEEL, SECT_FRNTLFTWHEEL, SECT_REARRGTWHEEL, SECT_REARLFTWHEEL};
	static const char *SuspSect[4] = {SECT_FRNTRGTSUSP, SECT_FRNTLFTSUSP, SECT_REARRGTSUSP, SECT_REARLFTSUSP};

	int i;
	for (i=0; i < 4; i++) {
		// Wheel
		RtParmSetNum(hdlesetup, WheelSect[i], PRM_CAMBER, "deg", &s->wheelcamber[i]);
		RtParmSetNum(hdlesetup, WheelSect[i], PRM_TOE, "deg", &s->wheeltoe[i]);
		RtParmSetNum(hdlesetup, WheelSect[i], PRM_RIDEHEIGHT, "mm", &s->wheelrideheight[i]);

		// Suspension
		RtParmSetNum(hdlesetup, SuspSect[i], PRM_SPR, "lbs/in", &s->suspspring[i]);
		RtParmSetNum(hdlesetup, SuspSect[i], PRM_PACKERS, "mm", &s->susppackers[i]);
		RtParmSetNum(hdlesetup, SuspSect[i], PRM_SLOWBUMP, "lbs/in/s", &s->suspslowbump[i]);
		RtParmSetNum(hdlesetup, SuspSect[i], PRM_SLOWREBOUND, "lbs/in/s", &s->suspslowrebound[i]);
		RtParmSetNum(hdlesetup, SuspSect[i], PRM_FASTBUMP, "lbs/in/s", &s->suspfastbump[i]);
		RtParmSetNum(hdlesetup, SuspSect[i], PRM_FASTREBOUND, "lbs/in/s", &s->suspfastrebound[i]);
	}

	// Brake
	RtParmSetNum(hdlesetup, SECT_BRKSYST, PRM_BRKREP, NULL, &s->brakeRepartition);
	RtParmSetNum(hdlesetup, SECT_BRKSYST, PRM_BRKPRESS, "kPa", &s->brakePressure);

	// Anti roll bar
	static const char *ArbSect[2] = {SECT_FRNTARB, SECT_REARARB};
	for (i=0; i < 2; i++) {
		RtParmSetNum(hdlesetup, ArbSect[i], PRM_SPR, "lbs/in", &s->arbspring[i]);
	}

	// Third element
	static const char *AxleSect[2] = {SECT_FRNTAXLE, SECT_REARAXLE};
	for (i=0; i < 2; i++) {
		RtParmSetNum(hdlesetup, AxleSect[i], PRM_SPR, "lbs/in", &s->thirdspring[i]);
		RtParmSetNum(hdlesetup, AxleSect[i], PRM_SLOWBUMP, "lbs/in/s", &s->thirdbump[i]);
		RtParmSetNum(hdlesetup, AxleSect[i], PRM_SLOWREBOUND, "lbs/in/s", &s->thirdrebound[i]);
		RtParmSetNum(hdlesetup, AxleSect[i], PRM_SUSPCOURSE, "mm", &s->thirdX0[i]);
	}

	// Gears
	for (i=0; i < 8; i++) { 
		snprintf(path, BUFSIZE, "%s/%s/%d", SECT_GEARBOX, ARR_GEARS, i+1);
		RtParmSetNum(hdlesetup, path, PRM_RATIO, NULL, &s->gearsratio[i]);
	}

	// Wings
	static const char *WingSect[2] = {SECT_FRNTWING, SECT_REARWING};
	for (i=0; i < 2; i++) { 
		RtParmSetNum(hdlesetup, WingSect[i], PRM_WINGANGLE, "deg", &s->wingangle[i]);
	}

	// Differentials
	static const char *DiffSect[3] = {SECT_FRNTDIFFERENTIAL, SECT_REARDIFFERENTIAL, SECT_CENTRALDIFFERENTIAL};
	static const char *DiffType[5] = {VAL_DIFF_NONE, VAL_DIFF_SPOOL, VAL_DIFF_FREE, VAL_DIFF_LIMITED_SLIP, VAL_DIFF_VISCOUS_COUPLER};
	for (i=0; i < 3; i++) { 
		RtParmSetNum(hdlesetup, DiffSect[i], PRM_RATIO, NULL, &s->diffratio[i]);
		RtParmSetNum(hdlesetup, DiffSect[i], PRM_MIN_TQ_BIAS, NULL, &s->diffmintqbias[i]);
		RtParmSetNum(hdlesetup, DiffSect[i], PRM_MAX_TQ_BIAS, NULL, &s->diffmaxtqbias[i]);
		RtParmSetNum(hdlesetup, DiffSect[i], PRM_MAX_SLIP_BIAS, NULL, &s->diffslipbias[i]);
		RtParmSetNum(hdlesetup, DiffSect[i], PRM_LOCKING_TQ, "N.m", &s->difflockinginputtq[i]);
		RtParmSetNum(hdlesetup, DiffSect[i], PRM_LOCKINGBRAKE_TQ, "N.m", &s->difflockinginputbraketq[i]);

		if (s->diffType[i] != tCarPitSetup::NONE) {
			GfParmSetStr(hdlesetup, DiffSect[i], PRM_TYPE, DiffType[s->diffType[i]]);
		}
	} 

	hdlesetup = GfParmMergeHandles(hdlecar, hdlesetup, GFPARM_MMODE_DST | GFPARM_MMODE_RELDST);
	GfParmWriteFile(filepath, hdlesetup, carname);
	GfParmReleaseHandle(hdlesetup);
}

			
/**	Save a custom car setup for a given robot, car, track and session (race, practice, qualifying, ...) type.
	The setup is validated against the setup given in hdlecar.
	@ingroup	tracktools
	@param hdlecar		Handle to master setup to validate against (min/max and other checks)
	@param s			Pointer to tCarPitSetup struct to save
	@param type			Setup type
	@param modulename	name of robot module without extension
	@param robidx		index of robot
	@param trackname	TORCS internal name name of track
	@param carname		TORCS internal name car name
	@note robot, car, track and session information are used to compose a standard setup filename
	@see RtSaveCarPitSetupFile
 */
void RtSaveCarPitSetup(
	void *hdlecar,				// handle to car definition file, for min/max merge
	tCarPitSetup* s,			// the setup data to save
	rtCarPitSetupType type,		// the setup type
	const char* modulename,		// modulename
	int robidx,					// player/robot instance
	const char* trackname,		// track file name without extension
	const char* carname			// car file name without extension
)
{
	const int filelen = 256;
	char filename[filelen];
	RtGetCarPitSetupFilename(type, robidx, carname, trackname, filename, filelen);

	const int pathlen = 1024;
	char path[pathlen];

	snprintf(path, pathlen, "%sdrivers/%s/setups", GetLocalDir(), modulename);
	if (GfCreateDir(path) == GF_DIR_CREATED) {
		snprintf(path, pathlen, "%sdrivers/%s/setups/%s.xml", GetLocalDir(), modulename, filename);
		RtSaveCarPitSetupFile(hdlecar, s, path, carname);		
	} else {
		GfError("RtSaveCarPitSetup, could not create %s\n", path);
	}
}


/**	Checks if a specific car setup is available
	@ingroup	tracktools
	@param type			Setup type
	@param modulename	name of robot module without extension
	@param robidx		index of robot
	@param trackname	TORCS internal name name of track
	@param carname		TORCS internal name car name
	@return	true on success, false on failure
 */
bool RtCarPitSetupExists(
	rtCarPitSetupType type,		// the setup type
	const char* modulename,		// modulename
	int robidx,					// player/robot instance
	const char* trackname,		// track file name without extension
	const char* carname			// car file name without extension	 
)
{
	const int filelen = 256;
	char filename[filelen];
	RtGetCarPitSetupFilename(type, robidx, carname, trackname, filename, filelen);

	const int pathlen = 1024;
	char path[pathlen];

	snprintf(path, pathlen, "%sdrivers/%s/setups/%s.xml", GetLocalDir(), modulename, filename);
	FILE* file = fopen(path, "r");	// TODO: maybe "stat" would be enough here
	if (file) {
		fclose(file);
		return true;
	}

	return false;
}


/**	Load a custom car setup from a given filename. The setup is validated against the setup given in hdlecar.
	@ingroup	tracktools
	@param hdlecar		Handle to master setup to validate against (min/max and other checks)
	@param filepath		Full path to setup file
	@param s			Pointer to tCarPitSetup struct to fill/initialize
	@param minmaxonly	If true, just the min/max values of s are modified
	@return	true on success, false on failure
 */
bool RtLoadCarPitSetupFilename(void* hdlecar, const char* filepath,  tCarPitSetup* s, bool minmaxonly)
{
	void* hdlesetup = GfParmReadFile(filepath, GFPARM_RMODE_STD);
	if (hdlesetup) {
		hdlesetup = GfParmMergeHandles(hdlecar, hdlesetup, GFPARM_MMODE_DST | GFPARM_MMODE_RELDST);
		RtInitCarPitSetup(hdlesetup, s, minmaxonly);
		GfParmReleaseHandle(hdlesetup);
		return true;
	} else {
		return false;
	}
}


/**	Load a custom car setup for a given robot, car, track and session (race, practice, qualifying, ...) type.
	The setup is validated against the setup given in hdlecar.
	@ingroup	tracktools
	@param hdlecar		Handle to master setup to validate against (min/max and other checks)
	@param s			Pointer to tCarPitSetup struct to fill/initialize
	@param type			Setup type
	@param modulename	name of robot module without extension
	@param robidx		index of robot
	@param trackname	TORCS internal name name of track
	@param carname		TORCS internal name car name
	@param minmaxonly	If true, just the min/max values of s are modified
	@return	true on success, false on failure
	@note robot, car, track and session information are used to compose a standard setup filename
	@see RtLoadCarPitSetupFilename
 */
bool RtLoadCarPitSetup(
	void* hdlecar, 
	tCarPitSetup* s,
	rtCarPitSetupType type,		
	const char* modulename,	
	int robidx,				
	const char* trackname,	
	const char* carname,
	bool minmaxonly
)
{
	const int filelen = 256;
	char filename[filelen];
	RtGetCarPitSetupFilename(type, robidx, carname, trackname, filename, filelen);

	const int pathlen = 1024;
	char path[pathlen];

	snprintf(path, pathlen, "%sdrivers/%s/setups/%s.xml", GetLocalDir(), modulename, filename);
	return RtLoadCarPitSetupFilename(hdlecar, path, s, minmaxonly);
}


/** Gets a handle to a parameter file containing the original TORCS car setup, that means the car setup
	merged with the cars category setup
    @ingroup	tracktools
	@param	carname TORCS internal name of the car (directory/filename)
    @return	NULL on failure, a valid handle otherwise
 */
void* RtLoadOriginalCarSettings(const char* carname)
{
	const int BUFSIZE = 1024;
	char buf[BUFSIZE];

	// Fetch car handle
	snprintf(buf, BUFSIZE, "%scars/%s/%s.xml", GetDataDir(), carname, carname);
	void* carhdle = GfParmReadFile(buf, GFPARM_RMODE_STD);
	if (carhdle == 0) {
		GfError("carhdle NULL in %s, line %d\n", __FILE__, __LINE__);
		return NULL;
	}

	// Get category
	const char* category = GfParmGetStr(carhdle, SECT_CAR, PRM_CATEGORY, NULL);
	if (category == 0) {
		GfError("category string NULL in %s, line %d\n", __FILE__, __LINE__);
		GfParmReleaseHandle(carhdle);
		return NULL;
	}

	// Fetch category handle
	snprintf(buf, BUFSIZE, "%scategories/%s.xml", GetDataDir(), category);
	void* cathdle = GfParmReadFile(buf, GFPARM_RMODE_STD);
	if (cathdle == 0) {
		GfError("cathdle NULL in %s, line %d\n", __FILE__, __LINE__);
		GfParmReleaseHandle(carhdle);
		return NULL;
	}

	// Compose final result, MergeHandles releases source handles with given parameters
	cathdle = GfParmReadFile(buf, GFPARM_RMODE_STD | GFPARM_RMODE_CREAT);
	carhdle = GfParmMergeHandles(cathdle, carhdle, GFPARM_MMODE_SRC | GFPARM_MMODE_DST | GFPARM_MMODE_RELSRC | GFPARM_MMODE_RELDST);

	return carhdle;
}


/** Initialize the given tCarPitSetup with the original TORCS setup, that means the car setup
	merged with the cars category setup
    @ingroup	tracktools
    @param	s	Pointer to tCarPitSetup struct to fill/initialize
	@param	carname TORCS internal name of the car (directory/filename)
    @return	true on success, false on failure
 */
bool RtInitCarPitSetupFromDefault(tCarPitSetup* s, const char* carname)
{
	void* carhandle = RtLoadOriginalCarSettings(carname);
	if (carhandle == 0) {
		GfError("carhandle NULL in %s, line %d\n", __FILE__, __LINE__);
		return false;
	}

	RtInitCarPitSetup(carhandle, s, false);
	GfParmReleaseHandle(carhandle);
	return true;
}


/**	Load a custom car setup file for a given robot, car, track and session (race, practice, qualifying, ...) type.
	@ingroup	tracktools
	@param type			Setup type
	@param modulename	name of robot module without extension
	@param robidx		index of robot
	@param trackname	TORCS internal name name of track
	@param carname		TORCS internal name car name
	@return	Handle to data, or NULL on failure (e.g. if file is not available) 
	@note robot, car, track and session information are used to compose a standard setup filename
 */
void* RtParmReadSetup(
	rtCarPitSetupType type,		
	const char* modulename,	
	int robidx,				
	const char* trackname,	
	const char* carname
)
{
	const int filelen = 256;
	char filename[filelen];
	RtGetCarPitSetupFilename(type, robidx, carname, trackname, filename, filelen);

	const int pathlen = 1024;
	char path[pathlen];

	snprintf(path, pathlen, "%sdrivers/%s/setups/%s.xml", GetLocalDir(), modulename, filename);
	return GfParmReadFile(path, GFPARM_RMODE_STD);
}
