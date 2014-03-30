// diskcache system, allows to write arrays to disk and manage pages on the fly
// drops down memory requirements

#include "q3map2.h"

#define DISKCACHE_MEMTEST // use memory pools for disk cache files (use only for bug testing)

qboolean initialized = qfalse;
double   cacheSize = 0;
double   cacheCpuTime = 0;
int      numCacheFiles = 0;

/*
AllocateDiskCache()
Creates new disk cache
*/

diskCache_t *AllocateDiskCache( int pagecount )
{
	diskCache_t *dk;

	/* allocate head structure */
	dk = (diskCache_t *)safe_malloc(sizeof(diskCache_t));
	dk->maxpages = pagecount;
	tmpnam(dk->cachepath);

	/* allocate pages */
	dk->pages = (diskPage_t *)safe_malloc(dk->maxpages * sizeof(diskPage_t));
	memset(dk->pages, 0, dk->maxpages * sizeof(diskPage_t));
	
	return dk;
}

/*
SelectDiskPage()
Allocates new disk page for index
*/

void AllocateDiskPage( diskCache_t *dk, int num, int size )
{
#ifndef DISKCACHE_MEMTEST
	char tempname[MAX_QPATH];
#endif
	diskPage_t *dp;

	/* check offsets */
	if (num < 0 || num >= dk->maxpages)
		Error("AllocateDiskPage: bogus diskcache page index");
	if (size <= 0)
		Error("AllocateDiskPage: diskcache page size = 0");

	dp = &dk->pages[num];
	
	/* allocate page */
	if (!dp->file)
	{
		dp->num = num;
#ifdef DISKCACHE_MEMTEST
		dp->file = (FILE *)(1); // vortex: hack
		dp->data = (byte *)safe_malloc(size);
#else
		sprintf_s(tempname, MAX_QPATH, "%s_%i.diskcache.tmp", dk->cachepath, numCacheFiles);
		dp->filename = safe_malloc(strlen(tempname)+1);
		strcpy(dp->filename, tempname);
		dp->file = fopen(tempname, "ab+");
		fseek(dp->file, 0, SEEK_SET);
		if (!dp->file)
			Error("AllocateDiskPage: error creating cache file %s: %s", tempname, strerror(errno));
#endif
		dp->size = size;
		dp->selected = 0;
		numCacheFiles++;
		cacheSize += (float)size / (1024.0f * 1024.0f);
	}

	/* match size */
	if (dp->size != size)
		Error("AllocateDiskPage: already allocated and mismatched page size %i (%i allocated)", size, dp->size);
}

/*
SelectDiskPage()
selects disk page (loads it from diskcache if it wasnt used before)
Important: number of select calls should match number of deselect calls
*/

diskPage_t *SelectDiskPage( diskCache_t *dk, int num )
{
	diskPage_t *dp;
	double start;

	/* check offsets */
	if (num < 0 || num >= dk->maxpages)
		Error("SelectDiskPage: bogus diskcache page index");

	/* page is allocated? */
	dp = &dk->pages[num];
	if (!dp->file)
		Error("SelectDiskPage: page not allocated");

	/* load page */
	start = I_FloatTime();
#ifdef DISKCACHE_MEMTEST
#else
	if (dp->data)
		dp->selected++;
	else
	{
		dp->data = safe_malloc(dp->size);
		fseek(dp->file, 0, SEEK_SET);
		fread(dp->data, dp->size, 1, dp->file);
		dp->selected = 1;
	}
#endif
	cacheCpuTime = cacheCpuTime + (I_FloatTime() - start);
	return dp;
}

/*
DeselectDiskPage()
deselects disk page (and automatically unloads it)
*/

void DeselectDiskPage( diskCache_t *dk, int num )
{
	diskPage_t *dp;
	double start;

	/* check offsets */
	if (num < 0 || num >= dk->maxpages)
		Error("DeselectDiskPage: bogus diskcache page index");

	/* page is allocated? */
	dp = &dk->pages[num];
	if (!dp->file)
		Error("DeselectDiskPage: page not allocated");

	/* page is loaded */
	if (!dp->selected)
		Error("DeselectDiskPage: page not loaded");

	/* unload page */
	start = I_FloatTime();
#ifdef DISKCACHE_MEMTEST
#else
	dp->selected--;
	if (dp->selected <= 0)
	{

		fseek(dp->file, 0, SEEK_SET);
		fwrite(dp->data, dp->size, 1, dp->file);
		free(dp->data);
		dp->data = NULL;
		dp->selected = 0;
	}
#endif
	cacheCpuTime = cacheCpuTime + (I_FloatTime() - start);
}

/*
FreeDiskPage()
unloads disk page
*/

void FreeDiskPage( diskPage_t *dp )
{
#ifdef DISKCACHE_MEMTEST
#else
	/* unload page */
	if (dp->data)
	{
		dp->data = safe_malloc(dp->size);
		fseek(dp->file, 0, SEEK_SET);
		fread(dp->data, dp->size, 1, dp->file);
		dp->selected = 1;
	}
	dp->data = NULL;

	/* remove page file */
	if (dp->file)
	{
		fclose(dp->file);
		unlink(dp->filename);
		free(dp->filename);
	}
	dp->file = NULL;
#endif
}

/*
FreeDiskCache()
free disk cache and all pages
*/

void FreeDiskCache( diskCache_t *dk )
{
	int i;

	/* free pages */
	for (i = 0; i < dk->maxpages; i++)
		FreeDiskPage(&dk->pages[i]);
	/* free disk cache */
	free(dk->pages);
	free(dk);
}

void DiskCacheTest( void )
{
}

void DiskCacheStats( void )
{
	/*
	Sys_FPrintf( SYS_VRB, "--- DiskCacheStats ---\n" );
	Sys_FPrintf( SYS_VRB, "%9i files\n", numCacheFiles);
	Sys_FPrintf( SYS_VRB, "%9.2f Mbytes used\n", cacheSize );
	Sys_FPrintf( SYS_VRB, "%9.2f seconds elapsed\n", cacheCpuTime );
	*/
}