/***************************************************************************

    file                 : pref.h
    created              : Sat Apr 29 16:52:41 CEST 2000
    copyright            : (C) 2000-2014 by Eric Espie, Bernhard Wymann
    email                : torcs@free.fr
    version              : $Id: pref.h,v 1.12.2.3 2014/02/14 10:39:13 berniw Exp $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 


#ifndef _PREF_H_
#define _PREF_H_


typedef struct {
    const char	*name;		/* Name of the control */
    GfCtrlType type;		/* Type of control (analog, joy button, keyboard) */
    int		val;		/* control index */
    const char	*minName;	/* Name of the min attribute */
    tdble	min;		/* min value for analog control */
    tdble	minVal;		/* another min value for analog control*/
    const char	*maxName;	/* Name of the max attribute */
    tdble	max;		/* max value for analog control */
    const char	*sensName;	/* Name of the sens attribute */
    tdble	sens;		/* sensitivity of control */
    const char	*powName;	/* Name of the pow attribute */
    tdble	pow;		/* power of curve command = pow(control, power) */
    const char	*spdSensName;	/* Name of the speed sensitivity attribute */
    tdble	spdSens;	/* speed sensitivity */
    const char	*deadZoneName;	/* Name of the dead zone attribute */
    tdble	deadZone;	/* dead zone (% of the available zone) */
} tControlCmd;

extern void	*PrefHdle;


extern const int nbCmdControl;


extern const char *Yn[];

#define	CMD_UP_SHFT	0
#define	CMD_DN_SHFT	1
#define	CMD_ASR		2
#define	CMD_ABS		3
#define	CMD_GEAR_R	4
#define	CMD_GEAR_N	5
#define	CMD_GEAR_1	6
#define	CMD_GEAR_2	7
#define	CMD_GEAR_3	8
#define	CMD_GEAR_4	9
#define	CMD_GEAR_5	10
#define	CMD_GEAR_6	11
#define CMD_THROTTLE	12
#define CMD_BRAKE	13
#define CMD_LEFTSTEER	14
#define CMD_RIGHTSTEER	15
#define CMD_LIGHT1	16
#define CMD_CLUTCH	17
#define CMD_SPDLIM	18


extern void HmReadPrefs(int index);


#endif /* _PREF_H_ */ 



