/***************************************************************************

    file        : controlconfig.h
    created     : Wed Mar 12 22:09:01 CET 2003
    copyright   : (C) 2003-2014 by Eric Espie, Bernhard Wymann                   
    email       : eric.espie@torcs.org   
    version     : $Id: controlconfig.h,v 1.3.2.2 2014/02/14 10:39:12 berniw Exp $                                  

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
    @version	$Id: controlconfig.h,v 1.3.2.2 2014/02/14 10:39:12 berniw Exp $
*/

#ifndef _CONTROLCONFIG_H_
#define _CONTROLCONFIG_H_

extern void *TorcsControlMenuInit(void *prevMenu, int index);


typedef struct
{
	const char *name;
	tCtrlRef ref;
	int Id;
	const char *minName;
	float min;
	const char *maxName;
	float max;
	const char *powName;
	float pow;
	int keyboardPossible;
} tCmdInfo;

#endif /* _CONTROLCONFIG_H_ */ 



