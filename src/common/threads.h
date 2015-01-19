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


extern int numthreads;

/* threads */
void ThreadSetDefault (void);
void ThreadStats (void);
int	 GetThreadWork (void);
void RunThreadsOnIndividual (int workcnt, qboolean showpacifier, void(*func)(int));
void RunThreadsOn (int workcnt, qboolean showpacifier, void(*func)(int));
void RunSameThreadOn(int workcnt, qboolean showpacifier, void(*threadfunc)(int));
void RunSameThreadOnIndividual(int workcnt, qboolean showpacifier, void(*threadfunc)(int));
void ThreadLock (void);
void ThreadUnlock (void);

/* mutex */
#if defined(WIN32) || defined(WIN64)
	#define	USED
	#include <windows.h>
	typedef struct ThreadMutex_s
	{
		qboolean locked;
		char *file;
		int line;
		HANDLE handle;
	}ThreadMutex;
	#define ThreadMutexInit(m) _ThreadMutexInit(m, __FILE__, __LINE__)
	#define ThreadMutexLock(m) _ThreadMutexLock(m, __FILE__, __LINE__)
	#define ThreadMutexUnlock(m) _ThreadMutexUnlock(m, __FILE__, __LINE__)
	#define ThreadMutexDelete(m) _ThreadMutexDelete(m, __FILE__, __LINE__)
	void _ThreadMutexInit(ThreadMutex *mutex, char *file, int line);
	void _ThreadMutexLock(ThreadMutex *mutex, char *file, int line);
	void _ThreadMutexUnlock(ThreadMutex *mutex, char *file, int line);
	void _ThreadMutexDelete(ThreadMutex *mutex, char *file, int line);
#else
	typedef struct ThreadMutex_s
	{
		qboolean locked;
	}ThreadMutex;
	// hack: use critical section
	#define ThreadMutexInit(m)
	#define ThreadMutexLock(m) ThreadLock()
	#define ThreadMutexUnlock(m) ThreadUnlock()
	#define ThreadMutexDelete(m)
#endif