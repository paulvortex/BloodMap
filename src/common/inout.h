/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __INOUT__
#define __INOUT__

// inout is the only stuff relying on xml, include the headers there
#include "libxml/tree.h"
#include "mathlib.h"
#include "polylib.h"

// some useful xml routines
xmlNodePtr xml_NodeForVec( vec3_t v );
void xml_SendNode (xmlNodePtr node);

extern qboolean bNetworkBroadcast;
void Broadcast_Setup( const char *dest );
void Broadcast_Shutdown();

#define SYS_VRB 0   // verbose support (on/off)
#define SYS_STD 1   // standard print level
#define SYS_WRN 2   // warnings
#define SYS_ERR 3   // error
#define SYS_NOXML 4 // don't send that down the XML stream

extern qboolean verbose;
void Sys_Printf(const char *text, ...);
void Sys_FPrintf(int flag, const char *text, ...);
void Sys_Warning(const char *text, ...);
void Sys_Warning(vec3_t point, const char *text, ...);
void Sys_Warning(int entitynum, const char *text, ...);
void Sys_Warning(int entitynum, int brushnum, const char *text, ...);
void Sys_Warning(vec3_t windingpoints[], int numpoints, const char *text, ... );
void Sys_Warning(winding_t *winding, const char *text, ... );
void Sys_Error(const char *text, ...);
void Sys_Error(vec3_t point, const char *text, ...);
void Sys_Error(int entitynum, const char *text, ...);
void Sys_Error(int entitynum, int brushnum, const char *text, ...);
void Sys_Error(vec3_t windingpoints[], int numpoints, const char *text, ... );
void Sys_Error(winding_t *winding, const char *text, ... );
#ifdef _DEBUG
#define DBG_XML 1
#endif

#ifdef DBG_XML
void DumpXML();
#endif

#endif
