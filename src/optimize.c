// VorteX: BSP file optimizer
// remove unused stuff after all stages were processed

/* marker */
#define OPTIMIZE_C

/* dependencies */
#include "q3map2.h"

/* parms */
qboolean      tidyShaders        = qtrue;
qboolean      tidyEntities       = qtrue;
vec3_t        mergeBlock         = {96.0f, 96.0f, 1024.0f};
qboolean      mergeDrawSurfaces  = qtrue;
qboolean      mergeDrawVerts     = qtrue;
qboolean      makeResMap         = qtrue;

/*
TidyShaders
reducing number of work shaders by replacing current shaders with engine-related ones (q3map_engineShader)
*/

typedef struct bspShaderMeta_s
{
	int          index;
	int          newIndex;
	char         name[MAX_OS_PATH];
	void        *mergedto; // metasurface to merge drawsurface to
	bspShader_t *sh;
}
bspShaderMeta_t;

void TidyShaders(void)
{
	int i, f, start, fOld, shadernum, modelnum, surfacenum, brushnum, sidenum;
	int numRemappedShaders, numRemappedDrawsurfaces, numRemappedBrushes, numRemappedBrushSides, numDrawSurfacesProcessed, numBrushSidesProcessed;
	bspShader_t *sh, *newShaders;
	bspShaderMeta_t *metaShaders, *msh, *msh2;
	bspModel_t *model;
	bspDrawSurface_t *ds;
	bspBrush_t *brush;
	bspBrushSide_t *side;
	shaderInfo_t *si;

	Sys_Printf( "--- TidyShaders ---\n" );

	/* create metashaders */
	Sys_FPrintf(SYS_VRB, "--- CreateMetaShaders ---\n" );
	metaShaders = (bspShaderMeta_t *)safe_malloc(sizeof(bspShaderMeta_t) * numBSPShaders);
	memset(metaShaders, 0, sizeof(bspShaderMeta_t) * numBSPShaders);
	start = I_FloatTime();
	fOld = -1;
	numRemappedShaders = 0;
	for (shadernum = 0; shadernum < numBSPShaders; shadernum++)
	{
		sh = &bspShaders[shadernum];

		/* print pacifier */
		f = 10 * shadernum / numBSPShaders;
		if( f != fOld )
		{
			fOld = f;
			Sys_FPrintf(SYS_VRB, "%d...", f );
		}

		/* allocate meta shader */
		msh = &metaShaders[shadernum];
		msh->index = shadernum;
		msh->newIndex = msh->index;
		msh->sh = sh;
		strcpy(msh->name, sh->shader);
		si = ShaderInfoForShader(sh->shader);
		if (si->engineShader)
		{
			if (strcmp(sh->shader, si->engineShader))
			{
				strcpy(msh->name, si->engineShader);
				numRemappedShaders++;
			}
		}
		else if (strcmp(sh->shader, si->shader))
		{
			strcpy(msh->name, si->shader);
			numRemappedShaders++;
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d BSP shaders\n", numBSPShaders );
	Sys_Printf( "%9d remapped shaders\n", numRemappedShaders );

	/* if there are no shaders to remap, bail */
	if (numRemappedShaders == 0)
	{
		free(metaShaders);
		return;
	}

	/* find mergable metashaders */
	Sys_FPrintf(SYS_VRB, "--- FindMergableShaders ---\n" );
	start = I_FloatTime();
	fOld = -1;
	numRemappedShaders = 0;
	for (shadernum = 0; shadernum < numBSPShaders; shadernum++)
	{
		msh = &metaShaders[shadernum];

		/* print pacifier */
		f = 10 * shadernum / numBSPShaders;
		if( f != fOld )
		{
			fOld = f;
			Sys_FPrintf(SYS_VRB, "%d...", f );
		}

		/* find shader to merge to */
		for (i = 0; i < shadernum; i++)
		{
			msh2 = &metaShaders[i];
			if (msh2->mergedto)
				continue;
			if (!strcmp(msh->name, msh2->name) && 
				msh->sh->contentFlags == msh2->sh->contentFlags && 
				msh->sh->surfaceFlags == msh2->sh->surfaceFlags)
			{
				msh->mergedto = msh2;
				numRemappedShaders++;
			}
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d shaders to be merged\n", numRemappedShaders );

	/* generate new shaders */
	Sys_FPrintf(SYS_VRB, "--- MergeShaders ---\n" );
	start = I_FloatTime();
	fOld = -1;
	newShaders = (bspShader_t *)safe_malloc(sizeof(bspShader_t) * (numBSPShaders - numRemappedShaders));
	numRemappedShaders = 0;
	for (shadernum = 0; shadernum < numBSPShaders; shadernum++)
	{
		msh = &metaShaders[shadernum];

		/* print pacifier */
		f = 10 * shadernum / numBSPShaders;
		if( f != fOld )
		{
			fOld = f;
			Sys_FPrintf(SYS_VRB, "%d...", f );
		}

		/* write shader if it is not merged */
		if (msh->mergedto)
			continue;
		msh->newIndex = numRemappedShaders;
		sh = &newShaders[numRemappedShaders];
		sh->contentFlags = msh->sh->contentFlags;
		sh->surfaceFlags = msh->sh->surfaceFlags;
		strncpy(sh->shader, msh->name, MAX_QPATH);
		numRemappedShaders++;
	}
	memcpy(bspShaders, newShaders, sizeof(bspShader_t) * numRemappedShaders);
	numBSPShaders = numRemappedShaders;
	free(newShaders);
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d shaders after merging\n", numRemappedShaders );

	/* update drawsurfaces */
	Sys_FPrintf(SYS_VRB, "--- UpdateDrawsurfaces ---\n" );
	numDrawSurfacesProcessed = 0;
	numRemappedDrawsurfaces = 0;
	start = I_FloatTime();
	fOld = -1;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];

		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < model->numBSPSurfaces; surfacenum++ )
		{
			/* print pacifier */
			f = 10 * numDrawSurfacesProcessed / numBSPDrawSurfaces;
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}
			numDrawSurfacesProcessed++;

			/* update drawsurface shader */
			ds = &bspDrawSurfaces[ model->firstBSPSurface + surfacenum ];
			msh = &metaShaders[ds->shaderNum];
			if (msh->mergedto)
			{
				ds->shaderNum = ((bspShaderMeta_t *)msh->mergedto)->newIndex;
				numRemappedDrawsurfaces++;
			}
			else
				ds->shaderNum = msh->newIndex;
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d drawsurfaces remapped\n", numRemappedDrawsurfaces );

	/* update brushes */
	Sys_FPrintf(SYS_VRB, "--- UpdateBrushes ---\n" );
	start = I_FloatTime();
	fOld = -1;
	numBrushSidesProcessed = 0;
	numRemappedBrushes = 0;
	numRemappedBrushSides = 0;
	for (brushnum = 0; brushnum < numBSPBrushes; brushnum++)
	{
		brush = &bspBrushes[brushnum];

		/* update brush sides */
		for (sidenum = 0; sidenum < brush->numSides; sidenum++)
		{
			/* print pacifier */
			f = 10 * numBrushSidesProcessed / numBSPDrawSurfaces;
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}
			numBrushSidesProcessed++;

			/* update brushside shader */
			side = &bspBrushSides[brush->firstSide + sidenum];
			msh = &metaShaders[side->shaderNum];
			if (msh->mergedto)
			{
				side->shaderNum = ((bspShaderMeta_t *)msh->mergedto)->newIndex;
				numRemappedBrushSides++;
			}
			else
				side->shaderNum = msh->newIndex;
		}

		/* update brush shader */
		msh = &metaShaders[brush->shaderNum];
		if (msh->mergedto)
		{
			brush->shaderNum = ((bspShaderMeta_t *)msh->mergedto)->newIndex;
			numRemappedBrushes++;
		}
		else
			brush->shaderNum = msh->newIndex;
	}
	Sys_Printf( "%9d brushes remapped\n", numRemappedBrushes );
	Sys_Printf( "%9d brushsides remapped\n", numRemappedBrushSides );
	free(metaShaders);
}

/*
TidyEntities
reduce entity lump size by stripping q3map2-related key pairs
*/

void TidyEntities(void)
{
	int dataSize;

	Sys_Printf("--- TidyEntities ---\n");
	dataSize = bspEntDataSize;
	Sys_Printf( "%9d entity data size\n", dataSize );
	ParseEntities();
	numBSPEntities = numEntities;
	UnparseEntities(qtrue);
	Sys_Printf( "%9d new entity data size (reduced by %i kb)\n", bspEntDataSize, (dataSize - bspEntDataSize) / 1024 );
}

/*
MergeDrawSurfaces
optimizes BSP by merging near drawsurfaces that has exactly same appearance
*/

typedef struct bspDrawSurfaceMeta_s
{
	int               index;
	int               newIndex;
	qboolean          mergeable;
	void             *next;
	void             *last;
	void             *mergedto; // metasurface to merge drawsurface to
	vec3_t            absmin;
	vec3_t            absmax;
	bspDrawSurface_t *ds;
	shaderInfo_t     *si;
}
bspDrawSurfaceMeta_t;

void MergeDrawSurfaces(void)
{
	int i, f, start, fOld, modelnum, surfacenum, leafnum, leafsurfacenum;
	int numNewSurfaces, numSkipSurfaces, numMergedSurfaces, numDrawSurfacesProcessed, numDrawVertsProcessed, numDrawIndexesProcessed, numLeafSurfacesProcessed;
	bspDrawSurfaceMeta_t *metaSurfaces, *ms, *ms2;
	bspModel_t *model;
	bspLeaf_t *leaf;
	bspDrawSurface_t *ds, *ds2, *newDrawSurfaces;
	bspDrawVert_t *newDrawVerts, *dv, *dv2;
	int *newDrawIndexes, *newLeafSurfaces, *di, *di2, firstDS, numDS;
	vec3_t newmins, newmaxs, newsize;

	Sys_Printf( "--- MergeDrawSurfaces ---\n" );

	/* allocate merged surfaces */
	numMergedSurfaces = 0;
	metaSurfaces = (bspDrawSurfaceMeta_t *)safe_malloc(sizeof(bspDrawSurfaceMeta_t) * numBSPDrawSurfaces);
	memset(metaSurfaces, 0, sizeof(bspDrawSurfaceMeta_t) * numBSPDrawSurfaces);

	/* create metasurfaces */
	Sys_FPrintf(SYS_VRB, "--- CreateMetaSurfaces ---\n" );
	numDrawSurfacesProcessed = 0;
	start = I_FloatTime();
	fOld = -1;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];

		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < model->numBSPSurfaces; surfacenum++ )
		{
			ds = &bspDrawSurfaces[ model->firstBSPSurface + surfacenum ];

			/* print pacifier */
			f = 10 * numDrawSurfacesProcessed / numBSPDrawSurfaces;
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}
			numDrawSurfacesProcessed++;

			/* allocate meta surface */
			ms = &metaSurfaces[ model->firstBSPSurface + surfacenum ];
			ms->si = ShaderInfoForShader( bspShaders[ ds->shaderNum ].shader );
			ms->index = model->firstBSPSurface + surfacenum;
			ms->ds = ds;

			/* find min/max bounds */
			for (i = 0; i < ds->numVerts; i++)
			{
				dv = &bspDrawVerts[ ds->firstVert + i ];
				if (i == 0)
				{
					ms->absmax[0] = dv->xyz[0];
					ms->absmax[1] = dv->xyz[1];
					ms->absmax[2] = dv->xyz[2];
					ms->absmin[0] = dv->xyz[0];
					ms->absmin[1] = dv->xyz[1];
					ms->absmin[2] = dv->xyz[2];
				}
				else
				{
					ms->absmax[0] = max(ms->absmax[0], dv->xyz[0]);
					ms->absmax[1] = max(ms->absmax[1], dv->xyz[1]);
					ms->absmax[2] = max(ms->absmax[2], dv->xyz[2]);
					ms->absmin[0] = min(ms->absmin[0], dv->xyz[0]);
					ms->absmin[1] = min(ms->absmin[1], dv->xyz[1]);
					ms->absmin[2] = min(ms->absmin[2], dv->xyz[2]);
				}
			}
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d BSP draw surfaces\n", numBSPDrawSurfaces );
	Sys_Printf( "%9d BSP draw verts\n", numBSPDrawVerts );
	Sys_Printf( "%9d BSP draw indexes\n", numBSPDrawIndexes );

	/* find out surfaces to be merged */
	Sys_FPrintf(SYS_VRB, "--- FindMergableSurfaces ---\n" );
	start = I_FloatTime();
	fOld = -1;
	numDrawSurfacesProcessed = 0;
	numDrawVertsProcessed = 0;
	numDrawIndexesProcessed = 0;
	numNewSurfaces = 0;
	numSkipSurfaces = 0;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];

		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < model->numBSPSurfaces; surfacenum++ )
		{
			ds = &bspDrawSurfaces[ model->firstBSPSurface + surfacenum ];

			/* print pacifier */
			f = 10 * numDrawSurfacesProcessed / numBSPDrawSurfaces;
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}
			numDrawSurfacesProcessed++;

			/* get meta surface */
			ms = &metaSurfaces[ model->firstBSPSurface + surfacenum ];
			numDrawVertsProcessed += ds->numVerts;
			numDrawIndexesProcessed += ds->numIndexes;

			/* only check for meshes and brush sides */
			if (ds->surfaceType != MST_PLANAR && ds->surfaceType != MST_TRIANGLE_SOUP)
			{
				ms->newIndex = numNewSurfaces;
				numNewSurfaces++;
				continue;
			}

			/* not mergeable? */
			if( ms->si->noMerge == qtrue )
			{
				ms->mergeable = qtrue;
				ms->newIndex = numNewSurfaces;
				numNewSurfaces++;
				numSkipSurfaces++;
				continue;
			}

			/* check if this surface can be merged to other surfaces */
			for( i = 0; i < model->numBSPSurfaces; i++ )
			{
				ms2 = &metaSurfaces[ model->firstBSPSurface + i ];
				if( ms == ms2 || ms2->mergeable == qfalse )
					continue;
				if( ms2->si->noMerge == qtrue )
					continue;
				ds2 = ms2->ds;

				/* surface should match shader and lightmapnum */
				if (ds->shaderNum == ds2->shaderNum && ds->lightmapNum[ 0 ] == ds2->lightmapNum[ 0 ])
				{
					vec3_t testBlock;

					/* calc new size */
					VectorCopy(ms2->absmin, newmins);
					VectorCopy(ms2->absmax, newmaxs);
					testBlock[ 0 ] = max(mergeBlock[ 0 ], fabs(newmaxs[ 0 ] - newmins[ 0 ]));
					testBlock[ 1 ] = max(mergeBlock[ 1 ], fabs(newmaxs[ 1 ] - newmins[ 1 ]));
					testBlock[ 2 ] = max(mergeBlock[ 2 ], fabs(newmaxs[ 2 ] - newmins[ 2 ]));
					AddPointToBounds(ms->absmin, newmins, newmaxs);
					AddPointToBounds(ms->absmax, newmins, newmaxs);
					newsize[ 0 ] = fabs(newmaxs[ 0 ] - newmins[ 0 ]);
					newsize[ 1 ] = fabs(newmaxs[ 1 ] - newmins[ 1 ]);
					newsize[ 2 ] = fabs(newmaxs[ 2 ] - newmins[ 2 ]);

					/* test size */
					if (newsize[ 0 ] <= testBlock[ 0 ] && newsize[ 1 ] <= testBlock[ 1 ] && newsize[ 2 ] <= testBlock[ 2 ])
					{
						/* allow merge */
						if (!ms2->last)
							ms2->next = ms2->last = ms;
						else
						{
							((bspDrawSurfaceMeta_t *)ms2->last)->next = ms;
							ms2->last = ms;
						}
						ms->mergedto = ms2;
						numMergedSurfaces++;
						break;
					}
				}
			}

			/* failed to find any surface to merge, so this surface could be a merge host */
			if (i >= model->numBSPSurfaces)
			{
				ms->mergeable = qtrue;
				ms->newIndex = numNewSurfaces;
				numNewSurfaces++;
			}
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );

	/* print some stats */
	Sys_Printf( "%9d unique surfaces\n", numDrawSurfacesProcessed );
	Sys_Printf( "%9d unique verts\n", numDrawVertsProcessed );
	Sys_Printf( "%9d unique indexes\n", numDrawIndexesProcessed );
	Sys_Printf( "%9d surfaces to be merged\n", numMergedSurfaces );
	Sys_Printf( "%9d skipped (nomerge) surfaces\n", numSkipSurfaces );

	/* if there are no verts to merge, bail */
	if (numMergedSurfaces == 0)
	{
		free(metaSurfaces);
		return;
	}

	/* generate new drawsurfaces */
	Sys_FPrintf(SYS_VRB, "--- MergeDrawSurfaces ---\n" );
	start = I_FloatTime();
	fOld = -1;
	newDrawSurfaces = (bspDrawSurface_t *)safe_malloc(sizeof(bspDrawSurface_t) * (numBSPDrawSurfaces - numMergedSurfaces));
	newDrawVerts = (bspDrawVert_t *)safe_malloc(sizeof(bspDrawVert_t) * numDrawVertsProcessed);
	newDrawIndexes = (int *)safe_malloc(sizeof(int) * numDrawIndexesProcessed);
	numDrawSurfacesProcessed = 0;
	numDrawVertsProcessed = 0;
	numDrawIndexesProcessed = 0;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];
		firstDS = model->firstBSPSurface;
		numDS = model->numBSPSurfaces;
		model->firstBSPSurface = numDrawSurfacesProcessed;

		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < numDS; surfacenum++ )
		{
			ms = &metaSurfaces[ firstDS + surfacenum ];

			/* already merged? */
			if (ms->mergedto)
				continue;

			/* construct new surface */
			ds = &bspDrawSurfaces[ firstDS + surfacenum ];
			ds2 = &newDrawSurfaces[ numDrawSurfacesProcessed ];
			numDrawSurfacesProcessed++;

			/* print pacifier */
			f = 10 * numDrawSurfacesProcessed / (numBSPDrawSurfaces - numMergedSurfaces);
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}

			/* copy structure of base surface */
			memcpy(ds2, ds, sizeof(bspDrawSurface_t));
			
			/* copy draw indexes */
			ds2->firstIndex = numDrawIndexesProcessed;
			if (ds->numIndexes)
			{
				di = &bspDrawIndexes[ ds->firstIndex ];
				di2 = &newDrawIndexes[ ds2->firstIndex ];
				memcpy(di2, di, sizeof(int) * ds->numIndexes);
				numDrawIndexesProcessed += ds->numIndexes;
			}

			/* copy draw verts */
			ds2->firstVert = numDrawVertsProcessed;
			if (ds->numVerts)
			{
				dv = &bspDrawVerts[ ds->firstVert ];
				dv2 = &newDrawVerts[ ds2->firstVert ];
				memcpy(dv2, dv, sizeof(bspDrawVert_t) * ds->numVerts);
				numDrawVertsProcessed += ds->numVerts;
			}

			/* not mergeable surface are only copied */
			if( ms->si->noMerge == qfalse )
			{
				/* merge surfaces */
				for (ms2 = (bspDrawSurfaceMeta_t *)ms->next; ms2; ms2 = (bspDrawSurfaceMeta_t *)ms2->next)
				{
					/* copy draw indexes */
					if (ms2->ds->numIndexes)
					{
						di = &bspDrawIndexes[ ms2->ds->firstIndex ];
						di2 = &newDrawIndexes[ numDrawIndexesProcessed ];
						ds2->numIndexes += ms2->ds->numIndexes;
						for(i = 0; i < ms2->ds->numIndexes; i += 3)
						{
							di2[0] = di[0] + ds2->numVerts;
							di2[1] = di[1] + ds2->numVerts;
							di2[2] = di[2] + ds2->numVerts;
							di2 += 3;
							di += 3;
						}
						numDrawIndexesProcessed += ms2->ds->numIndexes;
					}

					/* copy draw verts */
					if ( ms2->ds->numVerts)
					{
						dv = &bspDrawVerts[ ms2->ds->firstVert ];
						dv2 = &newDrawVerts[ numDrawVertsProcessed ];
						ds2->numVerts += ms2->ds->numVerts;
						memcpy(dv2, dv, sizeof(bspDrawVert_t) * ms2->ds->numVerts);
						numDrawVertsProcessed += ms2->ds->numVerts;
					}
				}
			}
		}
		model->numBSPSurfaces = numDrawSurfacesProcessed - model->firstBSPSurface;
	}

	/* merge into loader structures */
	memcpy(bspDrawSurfaces, newDrawSurfaces, sizeof(bspDrawSurface_t) * numDrawSurfacesProcessed);
	numBSPDrawSurfaces = numDrawSurfacesProcessed;
	free(newDrawSurfaces);
	memcpy(bspDrawVerts, newDrawVerts, sizeof(bspDrawVert_t) * numDrawVertsProcessed);
	numBSPDrawVerts = numDrawVertsProcessed;
	free(newDrawVerts);
	memcpy(bspDrawIndexes, newDrawIndexes, sizeof(int) * numDrawIndexesProcessed);
	numBSPDrawIndexes = numDrawIndexesProcessed;
	free(newDrawIndexes);
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );

	/* generate new leaf surfaces */
	// todo: remove duplicate surfaces
	Sys_FPrintf(SYS_VRB, "--- MergeLeafSurfaces ---\n" );
	newLeafSurfaces = (int *)safe_malloc(sizeof(int) * (numBSPLeafSurfaces));
	numLeafSurfacesProcessed = 0;
	start = I_FloatTime();
	fOld = -1;
	for( leafnum = 0; leafnum < numBSPLeafs; leafnum++ )
	{
		leaf = &bspLeafs[ leafnum ];

		/* test leaf surfaces */
		for( leafsurfacenum = 0; leafsurfacenum < leaf->numBSPLeafSurfaces; leafsurfacenum++ )
		{
			/* print pacifier */
			f = 10 * (numLeafSurfacesProcessed / numBSPLeafSurfaces);
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}
			numLeafSurfacesProcessed++;

			/* set new surface indexes */
			ms = &metaSurfaces[ bspLeafSurfaces[ leaf->firstBSPLeafSurface + leafsurfacenum ] ];
			if (ms->mergedto)
				bspLeafSurfaces[ leaf->firstBSPLeafSurface + leafsurfacenum ] = ((bspDrawSurfaceMeta_t *)ms->mergedto)->newIndex;
			else
				bspLeafSurfaces[ leaf->firstBSPLeafSurface + leafsurfacenum ] = ms->newIndex;
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d surfaces after merging\n", numBSPDrawSurfaces );
	free(newLeafSurfaces);
	free(metaSurfaces);
}


/*
MergeDrawVerts
optimizes BSP by merging coincident vertices
*/

typedef struct bspDrawVertMeta_s
{
	int               index;
	int               newIndex;
	void             *mergedto; // metavert to merge drawsurface to
	bspDrawVert_t    *dv;
}
bspDrawVertMeta_t;

#define VERTMERGE_ORIGIN_EPSILON  0.05
#define VERTMERGE_NORMAL_EPSILON  0.05
#define VERTMERGE_TC_EPSILON      0.0001
#define VERTMERGE_LMTC_EPSILON    0.00001
#define VERTMERGE_COLOR_EPSILON   8
#define VERTMERGE_ALPHA_EPSILON   8

qboolean Vec3ByteCompareExt(byte n1[3], byte n2[3], byte epsilon)
{
	int i;
	for (i= 0; i < 3; i++)
		if (fabs((float)(n1[i] - n2[i])) > epsilon)
			return qfalse;
	return qtrue;
}

qboolean Vec1ByteCompareExt(byte n1, byte n2, byte epsilon)
{
	if (fabs((float)(n1 - n2)) > epsilon)
		return qfalse;
	return qtrue;
}

qboolean Vec3CompareExt(vec3_t n1, vec3_t n2, float epsilon)
{
	int i;
	for (i= 0; i < 3; i++)
		if (fabs(n1[i] - n2[i]) > epsilon)
			return qfalse;
	return qtrue;
}

qboolean Vec2CompareExt(float n1[2], float n2[2], float epsilon)
{
	int i;
	for (i = 0; i < 2; i++)
		if (fabs(n1[i] - n2[i]) > epsilon)
			return qfalse;
	return qtrue;
}

qboolean Vec1CompareExt(float n1, float n2, float epsilon)
{
	if (fabs(n1 - n2) > epsilon)
		return qfalse;
	return qtrue;
}

void MergeDrawVerts(void)
{
	int i, j, k, start, f, fOld, modelnum, surfacenum, vertnum, indexnum, a, b, c;
	int numVertsProcessed, numVertsToMerge, numNewVerts, numNewIndexes, numRemovedTriangles, *newDrawIndexes;
	int firstVert, firstIndex;
	bspModel_t *model;
	bspDrawSurface_t *ds;
	bspDrawVert_t *dv, *dv2, *newDrawVerts;
	bspDrawVertMeta_t *metaVerts, *mv, *mv2;

	Sys_Printf( "--- MergeDrawVerts ---\n" );
	
	/* fill meta verts */
	Sys_FPrintf(SYS_VRB, "--- CreateMetaVerts ---\n" );
	start = I_FloatTime();
	fOld = -1;
	metaVerts = (bspDrawVertMeta_t *)safe_malloc(sizeof(bspDrawVertMeta_t) * numBSPDrawVerts);
	memset(metaVerts, 0, sizeof(bspDrawVertMeta_t) * numBSPDrawVerts);
	numVertsProcessed = 0;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];

		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < model->numBSPSurfaces; surfacenum++ )
		{
			ds = &bspDrawSurfaces[ model->firstBSPSurface + surfacenum ];
			/* walk all draw verts */
			for (vertnum = 0; vertnum < ds->numVerts; vertnum++)
			{
				dv = &bspDrawVerts[ds->firstVert + vertnum];

				/* create meta vert */
				mv = &metaVerts[ds->firstVert + vertnum];
				mv->index = ds->firstVert + vertnum;
				mv->dv = dv;

				/* print pacifier */
				f = 10 * numVertsProcessed / numBSPDrawVerts;
				if( f != fOld )
				{
					fOld = f;
					Sys_FPrintf(SYS_VRB, "%d...", f );
				}
				numVertsProcessed++;
			}
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d BSP draw surfaces\n", numBSPDrawSurfaces );
	Sys_Printf( "%9d BSP draw verts\n", numBSPDrawVerts );
	Sys_Printf( "%9d BSP draw indexes\n", numBSPDrawIndexes );

	/* find out verts to be merged */
	Sys_FPrintf(SYS_VRB, "--- FindMergableVerts ---\n" );
	start = I_FloatTime();
	fOld = -1;
	numVertsProcessed = 0;
	numVertsToMerge = 0;
	numNewVerts = 0;
	numNewIndexes = 0;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];
		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < model->numBSPSurfaces; surfacenum++ )
		{
			ds = &bspDrawSurfaces[ model->firstBSPSurface + surfacenum ];
			numNewIndexes += ds->numIndexes;
			/* walk all draw verts */
			for (vertnum = 0; vertnum < ds->numVerts; vertnum++)
			{
				mv = &metaVerts[ds->firstVert + vertnum];
				dv = mv->dv;

				/* print pacifier */
				f = 10 * numVertsProcessed / numBSPDrawVerts;
				if( f != fOld )
				{
					fOld = f;
					Sys_FPrintf(SYS_VRB, "%d...", f );
				}
				numVertsProcessed++;

				/* find coincident vertices */
				if (ds->surfaceType == MST_PLANAR || ds->surfaceType == MST_TRIANGLE_SOUP)
				{
					for (i = 0; i < vertnum; i++)
					{
						mv2 = &metaVerts[ds->firstVert + i];
						dv2 = mv2->dv;

						if (mv2->mergedto)
							continue;
						if (Vec3CompareExt(dv->xyz, dv2->xyz, VERTMERGE_ORIGIN_EPSILON) == qfalse)
							continue;
						if (Vec3CompareExt(dv->normal, dv2->normal, VERTMERGE_NORMAL_EPSILON) == qfalse)
							continue;
						if (Vec2CompareExt(dv->st, dv2->st, VERTMERGE_TC_EPSILON) == qfalse)
							continue;
						if (Vec2CompareExt(dv->lightmap[0], dv2->lightmap[0], VERTMERGE_LMTC_EPSILON) == qfalse)
							continue;
						if (Vec3ByteCompareExt(dv->color[0], dv2->color[0], VERTMERGE_COLOR_EPSILON) == qfalse)
							continue;
						if (Vec1ByteCompareExt(dv->color[0][3], dv2->color[0][3], VERTMERGE_ALPHA_EPSILON) == qfalse)
							continue;

						/* allow merge */
						mv->mergedto = mv2;
						numVertsToMerge++;
						break;
					}
					/* no coincident verts found, so this would be a merge target */
					if (i >= vertnum)
					{
						mv->newIndex = numNewVerts;
						numNewVerts++;
					}
				}
				else
				{
					/* no merging */
					mv->newIndex = numNewVerts;
					numNewVerts++;
				}
			}
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_Printf( "%9d verts to be merged\n", numVertsToMerge );

	/* if there are no verts to merge, bail */
	if (numVertsToMerge == 0)
	{
		free(metaVerts);
		return;
	}

	/* generate new drawverts and indexes */
	Sys_FPrintf(SYS_VRB, "--- MergeDrawVerts ---\n" );
	start = I_FloatTime();
	fOld = -1;
	newDrawVerts = (bspDrawVert_t *)safe_malloc(sizeof(bspDrawVert_t) * numNewVerts);
	newDrawIndexes = (int *)safe_malloc(sizeof(int) * numNewIndexes);
	numVertsProcessed = 0;
	numNewVerts = 0;
	numNewIndexes = 0;
	numRemovedTriangles = 0;
	for( modelnum = 0; modelnum < numBSPModels; modelnum++ )
	{
		model = &bspModels[ modelnum ];
		/* walk all surfaces of model */
		for( surfacenum = 0; surfacenum < model->numBSPSurfaces; surfacenum++ )
		{
			ds = &bspDrawSurfaces[ model->firstBSPSurface + surfacenum ];
			firstVert = numNewVerts;
			firstIndex = numNewIndexes;
	
			/* walk all draw verts */
			for (vertnum = 0; vertnum < ds->numVerts; vertnum++)
			{
				mv = &metaVerts[ds->firstVert + vertnum];
				if (mv->mergedto)
					continue;
				dv = mv->dv;
				dv2 = &newDrawVerts[numNewVerts];
				memcpy(dv2, dv, sizeof(bspDrawVert_t));
				mv->newIndex = numNewVerts;
				numNewVerts++;
			}
			/* walk all triangles */
			for (indexnum = 0; indexnum < ds->numIndexes; indexnum += 3)
			{
				i = ds->firstVert + bspDrawIndexes[ds->firstIndex + indexnum];
				j = ds->firstVert + bspDrawIndexes[ds->firstIndex + indexnum + 1];
				k = ds->firstVert + bspDrawIndexes[ds->firstIndex + indexnum + 2];
				if (i < 0 || i > numBSPDrawVerts)
					Sys_Warning("Drawvert %i out of range 0 - %i", i, numBSPDrawVerts);
				else if (j < 0 || j > numBSPDrawVerts)
					Sys_Warning("Drawvert %i out of range 0 - %i", j, numBSPDrawVerts);
				else if (k < 0 || k > numBSPDrawVerts)
					Sys_Warning("Drawvert %i out of range 0 - %i", k, numBSPDrawVerts);
				else
				{
					a = ((metaVerts[i].mergedto) ? ((bspDrawVertMeta_t *)metaVerts[i].mergedto)->newIndex : metaVerts[i].newIndex) - firstVert;
					b = ((metaVerts[j].mergedto) ? ((bspDrawVertMeta_t *)metaVerts[j].mergedto)->newIndex : metaVerts[j].newIndex) - firstVert;
					c = ((metaVerts[k].mergedto) ? ((bspDrawVertMeta_t *)metaVerts[k].mergedto)->newIndex : metaVerts[k].newIndex) - firstVert;
					if (a < 0 || b < 0 || c < 0)
						Sys_Warning("Degraded new index %i %i %i", a, b, c);
					else if (a == b || a == c || b == c)
						numRemovedTriangles++;
					else
					{
						newDrawIndexes[numNewIndexes] = a;
						newDrawIndexes[numNewIndexes+1] = b;
						newDrawIndexes[numNewIndexes+2] = c;
						numNewIndexes += 3;
					}
				}
			}
			/* link new verts/indexes */
			ds->firstVert = firstVert;
			ds->numVerts = numNewVerts - ds->firstVert;
			ds->firstIndex = firstIndex;
			ds->numIndexes = numNewIndexes - ds->firstIndex;
		}
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	memcpy(bspDrawVerts, newDrawVerts, sizeof(bspDrawVert_t) * numNewVerts);
	numBSPDrawVerts = numNewVerts;
	free(newDrawVerts);
	memcpy(bspDrawIndexes, newDrawIndexes, sizeof(int) * numNewIndexes);
	numBSPDrawIndexes = numNewIndexes;
	free(newDrawIndexes);
	Sys_Printf( "%9d verts after merge\n", numBSPDrawVerts );
	Sys_Printf( "%9d indexes after merge\n", numBSPDrawIndexes );
	Sys_Printf( "%9d triangles removed\n", numRemovedTriangles );
}

/*
PrintBSP()
test stuff
*/

void PrintBSP()
{
	int i, j;

	Sys_Printf("bspModel_t *m;\n" );
	Sys_Printf("bspShader_t *sh;\n" );
	Sys_Printf("bspBrush_t *b;\n" );
	Sys_Printf("bspBrushSide_t *bs;\n" );
	Sys_Printf("bspPlane_t *p;\n" );
	Sys_Printf("bspNode_t *n;\n" );
	Sys_Printf("bspLeaf_t *lf;\n" );
	Sys_Printf("bspDrawSurface_t *ds;\n" );
	Sys_Printf("bspDrawVert_t *dv;\n" );
	Sys_Printf("\n" );

	/* models */
	Sys_Printf("/* models */\n" );
	Sys_Printf("numBSPModels = %i;\n", numBSPModels);
	for (i = 0; i < numBSPModels; i++)
	{
		Sys_Printf( "m = &bspModels[ %i ];\n", i );
		Sys_Printf( "	VectorSet( m->mins, %f, %f, %f );\n", bspModels[ i ].mins[ 0 ],  bspModels[ i ].mins[ 1 ], bspModels[ i ].mins[ 2 ] );
		Sys_Printf( "	VectorSet( m->maxs, %f, %f, %f );\n", bspModels[ i ].maxs[ 0 ],  bspModels[ i ].maxs[ 1 ], bspModels[ i ].maxs[ 2 ] );
		Sys_Printf( "	m->firstBSPSurface = %i;\n", bspModels[ i ].firstBSPSurface );
		Sys_Printf( "	m->numBSPSurfaces = %i;\n", bspModels[ i ].numBSPSurfaces );
		Sys_Printf( "	m->firstBSPBrush = %i;\n;", bspModels[ i ].firstBSPBrush );
		Sys_Printf( "	m->numBSPBrushes = %i;\n", bspModels[ i ].numBSPBrushes );
	}
	Sys_Printf( "\n" );

	/* shaders */
	Sys_Printf("/* shaders */\n" );
	Sys_Printf("numBSPShaders = %i;\n", numBSPShaders);
	for (i = 0; i < numBSPShaders; i++)
	{
		Sys_Printf( "sh = &bspShaders[ %i ]; ", i );
		Sys_Printf( "strcpy( sh->shader, \"%s\" ); ", bspShaders[ i ].shader );
		Sys_Printf( "sh->surfaceFlags = %i; ", bspShaders[ i ].surfaceFlags );
		Sys_Printf( "sh->contentFlags = %i;\n", bspShaders[ i ].contentFlags );
	}
	Sys_Printf( "\n" );

	/* brushes */
	Sys_Printf("/* brushes */\n" );
	Sys_Printf("numBSPBrushes = %i;\n", numBSPBrushes);
	for (i = 0; i < numBSPBrushes; i++)
	{
		Sys_Printf( "b = &bspBrushes[ %i ]; ", i );
		Sys_Printf( "b->firstSide = %i; ", bspBrushes[ i ].firstSide );
		Sys_Printf( "b->numSides = %i; ", bspBrushes[ i ].numSides );
		Sys_Printf( "b->shaderNum = %i;\n", bspBrushes[ i ].shaderNum );
	}
	Sys_Printf( "\n" );

	/* brushsides */
	Sys_Printf("/* brush sides */\n" );
	Sys_Printf("numBSPBrushSides = %i;\n", numBSPBrushSides);
	for (i = 0; i < numBSPBrushSides; i++)
	{
		Sys_Printf( "bs = &bspBrushSides[ %i ]; ", i );
		Sys_Printf( "bs->planeNum = %i; ", bspBrushSides[ i ].planeNum );
		Sys_Printf( "bs->shaderNum = %i; ", bspBrushSides[ i ].shaderNum );
		Sys_Printf( "bs->surfaceNum = %i;\n", bspBrushSides[ i ].surfaceNum );
	}
	Sys_Printf( "\n" );

	/* planes */
	Sys_Printf("/* planes */\n" );
	Sys_Printf("numBSPPlanes = %i;\n", numBSPPlanes);
	for (i = 0; i < numBSPPlanes; i++)
	{
		Sys_Printf( "p = &bspPlanes[ %i ]; ", i );
		Sys_Printf( "VectorSet( p->normal, %f, %f, %f ); ", bspPlanes[ i ].normal[ 0 ], bspPlanes[ i ].normal[ 1 ], bspPlanes[ i ].normal[ 2 ] );
		Sys_Printf( "p->dist = %i;\n", bspPlanes[ i ].dist );
	}
	Sys_Printf( "\n" );

	/* nodes */
	Sys_Printf("/* nodes */\n" );
	Sys_Printf("numBSPNodes = %i;\n", numBSPNodes);
	for (i = 0; i < numBSPNodes; i++)
	{
		Sys_Printf( "n = &bspNodes[ %i ]; ", i );
		Sys_Printf( "n->planeNum = %i; ", bspNodes[ i ].planeNum );
		Sys_Printf( "n->children[ 0 ] = %i; ", bspNodes[ i ].children[ 0 ] );
		Sys_Printf( "n->children[ 1 ] = %i; ", bspNodes[ i ].children[ 1 ] );
		Sys_Printf( "VectorSet( n->mins, %f, %f, %f ); ", bspNodes[ i ].mins[ 0 ], bspNodes[ i ].mins[ 1 ], bspNodes[ i ].mins[ 2 ] );
		Sys_Printf( "VectorSet( n->maxs, %f, %f, %f );\n", bspNodes[ i ].maxs[ 0 ], bspNodes[ i ].maxs[ 1 ], bspNodes[ i ].maxs[ 2 ] );
	}
	Sys_Printf( "\n" );

	/* leafs */
	Sys_Printf("/* leafs */\n" );
	Sys_Printf("numBSPLeafs = %i;\n", numBSPLeafs);
	for (i = 0; i < numBSPLeafs; i++)
	{
		Sys_Printf( "lf = &bspLeafs[ %i ]; ", i );
		Sys_Printf( "lf->cluster = %i; ", bspLeafs[ i ].cluster );
		Sys_Printf( "lf->area = %i; ", bspLeafs[ i ].area );
		Sys_Printf( "VectorSet( lf->mins, %f, %f, %f ); ", bspLeafs[ i ].mins[ 0 ],  bspLeafs[ i ].mins[ 1 ], bspLeafs[ i ].mins[ 2 ] );
		Sys_Printf( "VectorSet( lf->maxs, %f, %f, %f ); ", bspLeafs[ i ].maxs[ 0 ],  bspLeafs[ i ].maxs[ 1 ], bspLeafs[ i ].maxs[ 2 ] );
		Sys_Printf( "lf->firstBSPLeafSurface = %i; ", bspLeafs[ i ].firstBSPLeafSurface );
		Sys_Printf( "lf->numBSPLeafSurfaces = %i; ", bspLeafs[ i ].numBSPLeafSurfaces );
		Sys_Printf( "lf->firstBSPLeafBrush = %i; ", bspLeafs[ i ].firstBSPLeafBrush );
		Sys_Printf( "lf->numBSPLeafBrushes = %i;\n", bspLeafs[ i ].numBSPLeafBrushes );
	}
	Sys_Printf( "\n" );

	/* leafsurfaces */
	Sys_Printf("/* leafsurfaces */\n" );
	Sys_Printf("numBSPLeafSurfaces = %i;\n", numBSPLeafSurfaces);
	for (i = 0; i < numBSPLeafSurfaces; i++)
		Sys_Printf( "bspLeafSurfaces[ %i ] = %i;\n", i, bspLeafSurfaces[ i ] );
	Sys_Printf( "\n" );

	/* leafs */
	Sys_Printf("/* leafbrushes */\n" );
	Sys_Printf("numBSPLeafBrushes = %i;\n", numBSPLeafBrushes);
	for (i = 0; i < numBSPLeafBrushes; i++)
		Sys_Printf( "bspLeafBrushes[ %i ] = %i;\n", i, bspLeafBrushes[ i ] );
	Sys_Printf( "\n" );

	/* drawsurfaces */
	Sys_Printf("/* drawsurfaces */\n" );
	Sys_Printf("numBSPDrawSurfaces = %i;\n", numBSPDrawSurfaces);
	for (i = 0; i < numBSPDrawSurfaces; i++)
	{
		Sys_Printf( "ds = &bspDrawSurfaces[ %i ];\n", i );
		Sys_Printf( "ds->shaderNum = %i;\n", bspDrawSurfaces[ i ].shaderNum );
		Sys_Printf( "ds->fogNum = %i;\n", bspDrawSurfaces[ i ].fogNum );
		Sys_Printf( "ds->surfaceType = %i;\n", bspDrawSurfaces[ i ].surfaceType );
		Sys_Printf( "ds->firstVert = %i; ", bspDrawSurfaces[ i ].firstVert );
		Sys_Printf( "ds->numVerts = %i;\n", bspDrawSurfaces[ i ].numVerts );
		Sys_Printf( "ds->firstIndex = %i; ", bspDrawSurfaces[ i ].firstIndex );
		Sys_Printf( "ds->numIndexes = %i;\n", bspDrawSurfaces[ i ].numIndexes );
		for (j = 0; j < MAX_LIGHTMAPS; j++)
		{
			Sys_Printf( "ds->lightmapStyles[ %i ] = %i; ", j, bspDrawSurfaces[ i ].lightmapStyles[ j ] );
			Sys_Printf( "ds->vertexStyles[ %i ] = %i; ", j, bspDrawSurfaces[ i ].vertexStyles[ j ] );
			Sys_Printf( "ds->lightmapNum[ %i ] = %i; ", j, bspDrawSurfaces[ i ].lightmapNum[ j ] );
			Sys_Printf( "ds->lightmapX[ %i ] = %i; ", j, bspDrawSurfaces[ i ].lightmapX[ j ] );
			Sys_Printf( "ds->lightmapY[ %i ] = %i;\n", j, bspDrawSurfaces[ i ].lightmapY[ j ] );
		}
		Sys_Printf( "ds->lightmapWidth = %i; ", bspDrawSurfaces[ i ].lightmapWidth );
		Sys_Printf( "ds->lightmapHeight = %i;\n", bspDrawSurfaces[ i ].lightmapHeight );
		Sys_Printf( "VectorSet( ds->lightmapOrigin, %f, %f, %f );\n", bspDrawSurfaces[ i ].lightmapOrigin[ 0 ], bspDrawSurfaces[ i ].lightmapOrigin[ 1 ], bspDrawSurfaces[ i ].lightmapOrigin[ 2 ] );
		Sys_Printf( "VectorSet( ds->lightmapVecs[ 0 ], %f, %f, %f );\n", bspDrawSurfaces[ i ].lightmapVecs[ 0 ][ 0 ], bspDrawSurfaces[ i ].lightmapVecs[ 0 ][ 1 ], bspDrawSurfaces[ i ].lightmapVecs[ 0 ][ 2 ] );
		Sys_Printf( "VectorSet( ds->lightmapVecs[ 1 ], %f, %f, %f );\n", bspDrawSurfaces[ i ].lightmapVecs[ 1 ][ 0 ], bspDrawSurfaces[ i ].lightmapVecs[ 1 ][ 1 ], bspDrawSurfaces[ i ].lightmapVecs[ 1 ][ 2 ] );
		Sys_Printf( "VectorSet( ds->lightmapVecs[ 2 ], %f, %f, %f );\n", bspDrawSurfaces[ i ].lightmapVecs[ 2 ][ 0 ], bspDrawSurfaces[ i ].lightmapVecs[ 2 ][ 1 ], bspDrawSurfaces[ i ].lightmapVecs[ 2 ][ 2 ] );
		Sys_Printf( "ds->patchWidth = %i; ", bspDrawSurfaces[ i ].patchWidth );
		Sys_Printf( "ds->patchHeight = %i;\n", bspDrawSurfaces[ i ].patchHeight );
	}
	Sys_Printf( "\n" );

	/* drawverts */
	Sys_Printf("/* drawverts */\n" );
	Sys_Printf("numBSPDrawVerts = %i;\n", numBSPDrawVerts);
	for (i = 0; i < numBSPDrawVerts; i++)
	{
		Sys_Printf( "dv = &bspDrawVerts[ %i ]; ", i );
		Sys_Printf( "VectorSet( dv->xyz, %f, %f, %f ); ", bspDrawVerts[ i ].xyz[ 0 ], bspDrawVerts[ i ].xyz[ 1 ], bspDrawVerts[ i ].xyz[ 2 ] );
		Sys_Printf( "VectorSet( dv->normal, %f, %f, %f ); ", bspDrawVerts[ i ].normal[ 0 ], bspDrawVerts[ i ].normal[ 1 ], bspDrawVerts[ i ].normal[ 2 ] );
		Sys_Printf( "dv->st[ 0 ] = %f; ", bspDrawVerts[ i ].st[ 0 ] );
		Sys_Printf( "dv->st[ 1 ] = %f;\n", bspDrawVerts[ i ].st[ 1 ] );
		for (j = 0; j < MAX_LIGHTMAPS; j++)
		{
			Sys_Printf( "	dv->lightmap[ %i ][ 0 ] = %f; ", j, bspDrawVerts[ i ].lightmap[ j ][ 0 ] );
			Sys_Printf( "	dv->lightmap[ %i ][ 1 ] = %f; ", j, bspDrawVerts[ i ].lightmap[ j ][ 1 ] );
			Sys_Printf( "	dv->color[ %i ][ 0 ] = %i; ", j, bspDrawVerts[ i ].color[ j ][ 0 ] );
			Sys_Printf( "	dv->color[ %i ][ 1 ] = %i; ", j, bspDrawVerts[ i ].color[ j ][ 1 ] );
			Sys_Printf( "	dv->color[ %i ][ 2 ] = %i; ", j, bspDrawVerts[ i ].color[ j ][ 2 ] );
			Sys_Printf( "	dv->color[ %i ][ 3 ] = %i;\n", j, bspDrawVerts[ i ].color[ j ][ 3 ] );
		}
	}
	Sys_Printf( "\n" );

	/* drawindexes */
	Sys_Printf("/* drawindexes */\n" );
	Sys_Printf("numBSPDrawIndexes = %i;\n", numBSPDrawIndexes);
	for (i = 0; i < numBSPDrawIndexes; i++)
		Sys_Printf( "bspDrawIndexes[ %i ] = %i;\n", i, bspDrawIndexes[ i ] );
	Sys_Printf( "\n" );

	/* visibility */
	Sys_Printf("/* visibility */\n" );
	Sys_Printf("numBSPVisBytes = %i;\n", numBSPVisBytes);
	for (i = 0; i < numBSPVisBytes; i++)
		Sys_Printf( "bspVisBytes[ %i ] = %i;\n", i, bspVisBytes[ i ] );
	Sys_Printf( "\n" );
}


/*
WriteResourceBSPFile()
generate fake bsp file with only shaders, to load up a resources map is using with precahce_model()
*/

void WriteResourceBSPFile( const char *filename )
{
	bspModel_t *m;
	bspBrush_t *b;
	bspBrushSide_t *bs;
	bspPlane_t *p;
	bspNode_t *n;
	bspLeaf_t *lf;
	bspDrawSurface_t *ds;
	bspDrawVert_t *dv;
	int i;

	/* empty data */
	numBSPLightBytes = 0;
	numBSPGridPoints = 0;
	bspEntDataSize = 0;
	numBSPFogs = 0;
	numBSPVisBytes = 0;

	/* models */
	numBSPModels = 1;
	m = &bspModels[ 0 ]; 
	VectorSet( m->mins, -8.000000, -8.000000, -8.000000 ); 
	VectorSet( m->maxs, 8.000000, 8.000000, 8.000000 ); 
	m->firstBSPSurface = 0; 
	m->numBSPSurfaces = numBSPShaders; 
	m->firstBSPBrush = 0; 
	m->numBSPBrushes = 1;

	/* brushes */
	numBSPBrushes = 1;
	b = &bspBrushes[ 0 ]; b->firstSide = 0; b->numSides = 6; b->shaderNum = 0;

	/* brush sides */
	numBSPBrushSides = 6;
	bs = &bspBrushSides[ 0 ]; bs->planeNum = 11; bs->shaderNum = 0; bs->surfaceNum = -1;
	bs = &bspBrushSides[ 1 ]; bs->planeNum = 4; bs->shaderNum = 0; bs->surfaceNum = -1;
	bs = &bspBrushSides[ 2 ]; bs->planeNum = 9; bs->shaderNum = 0; bs->surfaceNum = -1;
	bs = &bspBrushSides[ 3 ]; bs->planeNum = 2; bs->shaderNum = 0; bs->surfaceNum = -1;
	bs = &bspBrushSides[ 4 ]; bs->planeNum = 7; bs->shaderNum = 0; bs->surfaceNum = -1;
	bs = &bspBrushSides[ 5 ]; bs->planeNum = 0; bs->shaderNum = 0; bs->surfaceNum = -1;

	/* planes */
	numBSPPlanes = 18;
	p = &bspPlanes[ 0 ]; VectorSet( p->normal, 0.000000, 0.000000, 1.000000 ); p->dist = 0;
	p = &bspPlanes[ 1 ]; VectorSet( p->normal, -0.000000, -0.000000, -1.000000 ); p->dist = 0;
	p = &bspPlanes[ 2 ]; VectorSet( p->normal, 0.000000, 1.000000, 0.000000 ); p->dist = 0;
	p = &bspPlanes[ 3 ]; VectorSet( p->normal, -0.000000, -1.000000, -0.000000 ); p->dist = 0;
	p = &bspPlanes[ 4 ]; VectorSet( p->normal, 1.000000, 0.000000, 0.000000 ); p->dist = 0;
	p = &bspPlanes[ 5 ]; VectorSet( p->normal, -1.000000, -0.000000, -0.000000 ); p->dist = 0;
	p = &bspPlanes[ 6 ]; VectorSet( p->normal, -0.000000, -0.000000, 1.000000 ); p->dist = 0;
	p = &bspPlanes[ 7 ]; VectorSet( p->normal, 0.000000, 0.000000, -1.000000 ); p->dist = 0;
	p = &bspPlanes[ 8 ]; VectorSet( p->normal, -0.000000, 1.000000, -0.000000 ); p->dist = 0;
	p = &bspPlanes[ 9 ]; VectorSet( p->normal, 0.000000, -1.000000, 0.000000 ); p->dist = 0;
	p = &bspPlanes[ 10 ]; VectorSet( p->normal, 1.000000, -0.000000, -0.000000 ); p->dist = 0;
	p = &bspPlanes[ 11 ]; VectorSet( p->normal, -1.000000, 0.000000, 0.000000 ); p->dist = 0;
	p = &bspPlanes[ 12 ]; VectorSet( p->normal, 1.000000, 0.000000, 0.000000 ); p->dist = 0;
	p = &bspPlanes[ 13 ]; VectorSet( p->normal, -1.000000, -0.000000, -0.000000 ); p->dist = 0;
	p = &bspPlanes[ 14 ]; VectorSet( p->normal, 0.000000, 1.000000, 0.000000 ); p->dist = 0;
	p = &bspPlanes[ 15 ]; VectorSet( p->normal, -0.000000, -1.000000, -0.000000 ); p->dist = 0;
	p = &bspPlanes[ 16 ]; VectorSet( p->normal, 0.000000, 0.000000, 1.000000 ); p->dist = 0;
	p = &bspPlanes[ 17 ]; VectorSet( p->normal, -0.000000, -0.000000, -1.000000 ); p->dist = 0;

	/* nodes */
	numBSPNodes = 31;
	n = &bspNodes[ 0 ]; n->planeNum = 12; n->children[ 0 ] = 1; n->children[ 1 ] = 16; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 1 ]; n->planeNum = 14; n->children[ 0 ] = 2; n->children[ 1 ] = 9; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 2 ]; n->planeNum = 16; n->children[ 0 ] = 3; n->children[ 1 ] = 6; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 3 ]; n->planeNum = 4; n->children[ 0 ] = -2; n->children[ 1 ] = 4; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 4 ]; n->planeNum = 0; n->children[ 0 ] = -3; n->children[ 1 ] = 5; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 5 ]; n->planeNum = 2; n->children[ 0 ] = -4; n->children[ 1 ] = -5; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 6 ]; n->planeNum = 4; n->children[ 0 ] = -6; n->children[ 1 ] = 7; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 7 ]; n->planeNum = 6; n->children[ 0 ] = 8; n->children[ 1 ] = -9; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 8 ]; n->planeNum = 2; n->children[ 0 ] = -7; n->children[ 1 ] = -8; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 9 ]; n->planeNum = 16; n->children[ 0 ] = 10; n->children[ 1 ] = 13; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 10 ]; n->planeNum = 4; n->children[ 0 ] = -10; n->children[ 1 ] = 11; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 11 ]; n->planeNum = 0; n->children[ 0 ] = -11; n->children[ 1 ] = 12; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 12 ]; n->planeNum = 8; n->children[ 0 ] = -12; n->children[ 1 ] = -13; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 13 ]; n->planeNum = 4; n->children[ 0 ] = -14; n->children[ 1 ] = 14; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 14 ]; n->planeNum = 6; n->children[ 0 ] = 15; n->children[ 1 ] = -17; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 15 ]; n->planeNum = 8; n->children[ 0 ] = -15; n->children[ 1 ] = -16; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 16 ]; n->planeNum = 14; n->children[ 0 ] = 17; n->children[ 1 ] = 24; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 17 ]; n->planeNum = 16; n->children[ 0 ] = 18; n->children[ 1 ] = 21; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 18 ]; n->planeNum = 10; n->children[ 0 ] = 19; n->children[ 1 ] = -21; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 19 ]; n->planeNum = 0; n->children[ 0 ] = -18; n->children[ 1 ] = 20; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 20 ]; n->planeNum = 2; n->children[ 0 ] = -19; n->children[ 1 ] = -20; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 21 ]; n->planeNum = 10; n->children[ 0 ] = 22; n->children[ 1 ] = -25; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 22 ]; n->planeNum = 6; n->children[ 0 ] = 23; n->children[ 1 ] = -24; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 23 ]; n->planeNum = 2; n->children[ 0 ] = -22; n->children[ 1 ] = -23; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 24 ]; n->planeNum = 16; n->children[ 0 ] = 25; n->children[ 1 ] = 28; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 25 ]; n->planeNum = 10; n->children[ 0 ] = 26; n->children[ 1 ] = -29; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 26 ]; n->planeNum = 0; n->children[ 0 ] = -26; n->children[ 1 ] = 27; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 27 ]; n->planeNum = 8; n->children[ 0 ] = -27; n->children[ 1 ] = -28; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 28 ]; n->planeNum = 10; n->children[ 0 ] = 29; n->children[ 1 ] = -33; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 29 ]; n->planeNum = 6; n->children[ 0 ] = 30; n->children[ 1 ] = -32; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );
	n = &bspNodes[ 30 ]; n->planeNum = 8; n->children[ 0 ] = -30; n->children[ 1 ] = -31; VectorSet( n->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( n->maxs, 0.000000, 0.000000, 0.000000 );

	/* leafs */
	numBSPLeafs = 33;
	lf = &bspLeafs[ 0 ]; lf->cluster = 0; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 0; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 1 ]; lf->cluster = 0; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 0; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 2 ]; lf->cluster = 1; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 3; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 0; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 3 ]; lf->cluster = 2; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 6; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 0; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 4 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 0; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 5 ]; lf->cluster = 3; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 9; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 1; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 6 ]; lf->cluster = 4; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 12; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 1; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 7 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 1; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 8 ]; lf->cluster = 5; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 15; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 2; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 9 ]; lf->cluster = 6; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 18; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 2; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 10 ]; lf->cluster = 7; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 21; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 2; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 11 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 2; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 12 ]; lf->cluster = 8; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 24; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 3; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 13 ]; lf->cluster = 9; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 27; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 3; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 14 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 3; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 15 ]; lf->cluster = 10; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 30; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 4; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 16 ]; lf->cluster = 11; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 33; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 4; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 17 ]; lf->cluster = 12; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 36; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 4; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 18 ]; lf->cluster = 13; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 39; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 4; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 19 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 4; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 20 ]; lf->cluster = 14; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 42; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 5; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 21 ]; lf->cluster = 15; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 45; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 5; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 22 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 5; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 23 ]; lf->cluster = 16; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 48; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 6; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 24 ]; lf->cluster = 17; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 51; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 6; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 25 ]; lf->cluster = 18; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 54; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 6; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 26 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 6; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 27 ]; lf->cluster = 19; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 57; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 7; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 28 ]; lf->cluster = 20; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 60; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 7; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 29 ]; lf->cluster = -1; lf->area = -1; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 0; lf->numBSPLeafSurfaces = 0; lf->firstBSPLeafBrush = 7; lf->numBSPLeafBrushes = 1;
	lf = &bspLeafs[ 30 ]; lf->cluster = 21; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 63; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 8; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 31 ]; lf->cluster = 22; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 66; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 8; lf->numBSPLeafBrushes = 0;
	lf = &bspLeafs[ 32 ]; lf->cluster = 23; lf->area = 0; VectorSet( lf->mins, 0.000000, 0.000000, 0.000000 ); VectorSet( lf->maxs, 0.000000, 0.000000, 0.000000 ); lf->firstBSPLeafSurface = 69; lf->numBSPLeafSurfaces = 3; lf->firstBSPLeafBrush = 8; lf->numBSPLeafBrushes = 0;

	/* leafsurfaces */
	numBSPLeafSurfaces = 72;
	bspLeafSurfaces[ 0 ] = 0;
	bspLeafSurfaces[ 1 ] = 0;
	bspLeafSurfaces[ 2 ] = 0;
	bspLeafSurfaces[ 3 ] = 0;
	bspLeafSurfaces[ 4 ] = 0;
	bspLeafSurfaces[ 5 ] = 0;
	bspLeafSurfaces[ 6 ] = 0;
	bspLeafSurfaces[ 7 ] = 0;
	bspLeafSurfaces[ 8 ] = 0;
	bspLeafSurfaces[ 9 ] = 0;
	bspLeafSurfaces[ 10 ] = 0;
	bspLeafSurfaces[ 11 ] = 0;
	bspLeafSurfaces[ 12 ] = 0;
	bspLeafSurfaces[ 13 ] = 0;
	bspLeafSurfaces[ 14 ] = 0;
	bspLeafSurfaces[ 15 ] = 0;
	bspLeafSurfaces[ 16 ] = 0;
	bspLeafSurfaces[ 17 ] = 0;
	bspLeafSurfaces[ 18 ] = 0;
	bspLeafSurfaces[ 19 ] = 0;
	bspLeafSurfaces[ 20 ] = 0;
	bspLeafSurfaces[ 21 ] = 0;
	bspLeafSurfaces[ 22 ] = 0;
	bspLeafSurfaces[ 23 ] = 0;
	bspLeafSurfaces[ 24 ] = 0;
	bspLeafSurfaces[ 25 ] = 0;
	bspLeafSurfaces[ 26 ] = 0;
	bspLeafSurfaces[ 27 ] = 0;
	bspLeafSurfaces[ 28 ] = 0;
	bspLeafSurfaces[ 29 ] = 0;
	bspLeafSurfaces[ 30 ] = 0;
	bspLeafSurfaces[ 31 ] = 0;
	bspLeafSurfaces[ 32 ] = 0;
	bspLeafSurfaces[ 33 ] = 0;
	bspLeafSurfaces[ 34 ] = 0;
	bspLeafSurfaces[ 35 ] = 0;
	bspLeafSurfaces[ 36 ] = 0;
	bspLeafSurfaces[ 37 ] = 0;
	bspLeafSurfaces[ 38 ] = 0;
	bspLeafSurfaces[ 39 ] = 0;
	bspLeafSurfaces[ 40 ] = 0;
	bspLeafSurfaces[ 41 ] = 0;
	bspLeafSurfaces[ 42 ] = 0;
	bspLeafSurfaces[ 43 ] = 0;
	bspLeafSurfaces[ 44 ] = 0;
	bspLeafSurfaces[ 45 ] = 0;
	bspLeafSurfaces[ 46 ] = 0;
	bspLeafSurfaces[ 47 ] = 0;
	bspLeafSurfaces[ 48 ] = 0;
	bspLeafSurfaces[ 49 ] = 0;
	bspLeafSurfaces[ 50 ] = 0;
	bspLeafSurfaces[ 51 ] = 0;
	bspLeafSurfaces[ 52 ] = 0;
	bspLeafSurfaces[ 53 ] = 0;
	bspLeafSurfaces[ 54 ] = 0;
	bspLeafSurfaces[ 55 ] = 0;
	bspLeafSurfaces[ 56 ] = 0;
	bspLeafSurfaces[ 57 ] = 0;
	bspLeafSurfaces[ 58 ] = 0;
	bspLeafSurfaces[ 59 ] = 0;
	bspLeafSurfaces[ 60 ] = 0;
	bspLeafSurfaces[ 61 ] = 0;
	bspLeafSurfaces[ 62 ] = 0;
	bspLeafSurfaces[ 63 ] = 0;
	bspLeafSurfaces[ 64 ] = 0;
	bspLeafSurfaces[ 65 ] = 0;
	bspLeafSurfaces[ 66 ] = 0;
	bspLeafSurfaces[ 67 ] = 0;
	bspLeafSurfaces[ 68 ] = 0;
	bspLeafSurfaces[ 69 ] = 0;
	bspLeafSurfaces[ 70 ] = 0;
	bspLeafSurfaces[ 71 ] = 0;

	/* leafbrushes */
	numBSPLeafBrushes = 8;
	bspLeafBrushes[ 0 ] = 0;
	bspLeafBrushes[ 1 ] = 0;
	bspLeafBrushes[ 2 ] = 0;
	bspLeafBrushes[ 3 ] = 0;
	bspLeafBrushes[ 4 ] = 0;
	bspLeafBrushes[ 5 ] = 0;
	bspLeafBrushes[ 6 ] = 0;
	bspLeafBrushes[ 7 ] = 0;

	/* drawsurfaces */
	numBSPDrawSurfaces = numBSPShaders;
	for (i = 0; i < numBSPShaders; i++)
	{
		ds = &bspDrawSurfaces[ i ];
		ds->shaderNum = i;
		ds->fogNum = -1;
		ds->surfaceType = 1;
		ds->firstVert = 0; ds->numVerts = 24;
		ds->firstIndex = 0; ds->numIndexes = 36;
		ds->lightmapStyles[ 0 ] = 0; ds->vertexStyles[ 0 ] = 0; ds->lightmapNum[ 0 ] = -3; ds->lightmapX[ 0 ] = 0; ds->lightmapY[ 0 ] = 0;
		ds->lightmapStyles[ 1 ] = 255; ds->vertexStyles[ 1 ] = 255; ds->lightmapNum[ 1 ] = -3; ds->lightmapX[ 1 ] = 0; ds->lightmapY[ 1 ] = 0;
		ds->lightmapWidth = 0; ds->lightmapHeight = 0;
		VectorSet( ds->lightmapOrigin, 0.000000, 0.000000, 0.000000 );
		VectorSet( ds->lightmapVecs[ 0 ], 0.000000, 0.000000, 0.000000 );
		VectorSet( ds->lightmapVecs[ 1 ], 0.000000, 0.000000, 0.000000 );
		VectorSet( ds->lightmapVecs[ 2 ], 1.000000, 0.000000, 0.000000 );
		ds->patchWidth = 0; ds->patchHeight = 0;
	}

	/* drawverts */
	numBSPDrawVerts = 24;
	dv = &bspDrawVerts[ 0 ];
	VectorSet( dv->xyz, 8.000000, 8.000000, -8.000000 ); VectorSet( dv->normal, 1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 1 ];
	VectorSet( dv->xyz, 8.000000, -8.000000, -8.000000 ); VectorSet( dv->normal, 1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 2 ];
	VectorSet( dv->xyz, 8.000000, 8.000000, 8.000000 ); VectorSet( dv->normal, 1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 3 ];
	VectorSet( dv->xyz, 8.000000, -8.000000, 8.000000 ); VectorSet( dv->normal, 1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 4 ];
	VectorSet( dv->xyz, 8.000000, 8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, 1.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 5 ];
	VectorSet( dv->xyz, -8.000000, 8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, 1.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 6 ];
	VectorSet( dv->xyz, 8.000000, 8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, 1.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 7 ];
	VectorSet( dv->xyz, -8.000000, 8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, 1.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 8 ];
	VectorSet( dv->xyz, 8.000000, -8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, 1.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 9 ];
	VectorSet( dv->xyz, -8.000000, -8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, 1.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 10 ];
	VectorSet( dv->xyz, 8.000000, 8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, 1.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 11 ];
	VectorSet( dv->xyz, -8.000000, 8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, 1.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 12 ];
	VectorSet( dv->xyz, 8.000000, -8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, -1.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 13 ];
	VectorSet( dv->xyz, -8.000000, -8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, -1.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 14 ];
	VectorSet( dv->xyz, 8.000000, -8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, -1.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 15 ];
	VectorSet( dv->xyz, -8.000000, -8.000000, 8.000000 ); VectorSet( dv->normal, 0.000000, -1.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 16 ];
	VectorSet( dv->xyz, -8.000000, 8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, -1.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 17 ];
	VectorSet( dv->xyz, -8.000000, -8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, -1.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 18 ];
	VectorSet( dv->xyz, 8.000000, 8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, -1.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 19 ];
	VectorSet( dv->xyz, 8.000000, -8.000000, -8.000000 ); VectorSet( dv->normal, 0.000000, 0.000000, -1.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 20 ];
	VectorSet( dv->xyz, -8.000000, 8.000000, 8.000000 ); VectorSet( dv->normal, -1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 21 ];
	VectorSet( dv->xyz, -8.000000, -8.000000, -8.000000 ); VectorSet( dv->normal, -1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 22 ];
	VectorSet( dv->xyz, -8.000000, 8.000000, -8.000000 ); VectorSet( dv->normal, -1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 1.000000; dv->st[ 1 ] = 1.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;
	dv = &bspDrawVerts[ 23 ];
	VectorSet( dv->xyz, -8.000000, -8.000000, 8.000000 ); VectorSet( dv->normal, -1.000000, 0.000000, 0.000000 ); dv->st[ 0 ] = 0.000000; dv->st[ 1 ] = 0.000000;
	dv->lightmap[ 0 ][ 0 ] = -1; dv->lightmap[ 0 ][ 1 ] = -1; dv->color[ 0 ][ 0 ] = 255; dv->color[ 0 ][ 1 ] = 255; dv->color[ 0 ][ 2 ] = 255; dv->color[ 0 ][ 3 ] = 255;
	dv->lightmap[ 1 ][ 0 ] = -1; dv->lightmap[ 1 ][ 1 ] = -1; dv->color[ 1 ][ 0 ] = 0; dv->color[ 1 ][ 1 ] = 0; dv->color[ 1 ][ 2 ] = 0; dv->color[ 1 ][ 3 ] = 0;

	/* drawindexes */
	numBSPDrawIndexes = 36;
	bspDrawIndexes[ 0 ] = 0;
	bspDrawIndexes[ 1 ] = 1;
	bspDrawIndexes[ 2 ] = 2;
	bspDrawIndexes[ 3 ] = 2;
	bspDrawIndexes[ 4 ] = 1;
	bspDrawIndexes[ 5 ] = 3;
	bspDrawIndexes[ 6 ] = 4;
	bspDrawIndexes[ 7 ] = 5;
	bspDrawIndexes[ 8 ] = 6;
	bspDrawIndexes[ 9 ] = 7;
	bspDrawIndexes[ 10 ] = 5;
	bspDrawIndexes[ 11 ] = 4;
	bspDrawIndexes[ 12 ] = 8;
	bspDrawIndexes[ 13 ] = 9;
	bspDrawIndexes[ 14 ] = 10;
	bspDrawIndexes[ 15 ] = 10;
	bspDrawIndexes[ 16 ] = 9;
	bspDrawIndexes[ 17 ] = 11;
	bspDrawIndexes[ 18 ] = 12;
	bspDrawIndexes[ 19 ] = 13;
	bspDrawIndexes[ 20 ] = 14;
	bspDrawIndexes[ 21 ] = 14;
	bspDrawIndexes[ 22 ] = 13;
	bspDrawIndexes[ 23 ] = 15;
	bspDrawIndexes[ 24 ] = 16;
	bspDrawIndexes[ 25 ] = 17;
	bspDrawIndexes[ 26 ] = 18;
	bspDrawIndexes[ 27 ] = 18;
	bspDrawIndexes[ 28 ] = 17;
	bspDrawIndexes[ 29 ] = 19;
	bspDrawIndexes[ 30 ] = 20;
	bspDrawIndexes[ 31 ] = 21;
	bspDrawIndexes[ 32 ] = 22;
	bspDrawIndexes[ 33 ] = 23;
	bspDrawIndexes[ 34 ] = 21;
	bspDrawIndexes[ 35 ] = 20;

	/* write out */
	Sys_Printf( "Writing resource map %s\n", source );
	WriteBSPFile( source );
}

/*
WriteResourceOBJFile()
generate fake obj file with only shaders, to load up a resources map is using with precahce_model()
*/

typedef struct
{
	union
	{
		char classname[64];
		char shadername[128];
		char model[MAX_OS_PATH];
		char sound[MAX_OS_PATH];
		char entryname[MAX_OS_PATH];
	};
}RefEntry_t;

static int CompareRefEntry( const void *a, const void *b ) { return strcmp(((RefEntry_t *)a)->entryname, ((RefEntry_t *)b)->entryname); }

#define MAX_REFERENCED 512

void WriteResourceOBJFile( const char *objfilename )
{
	FILE *f;
	entity_t *entity;
	const char *classname, *model, *noise, *shadername;
	char dirname[MAX_OS_PATH], /*lightmapfile[MAX_OS_PATH], */mapname[MAX_OS_PATH];
	static RefEntry_t RefShaders[MAX_REFERENCED] = { 0 };
	static RefEntry_t RefModels[MAX_REFERENCED] = { 0 };
	static RefEntry_t RefSounds[MAX_REFERENCED] = { 0 };
	static RefEntry_t RefClasses[MAX_REFERENCED] = { 0 };
	int numShaders, numModels, numSounds, numClasses;
	int i, j, k, spawnflags;
	/*qboolean any_found;*/

	strcpy( dirname, source );
	StripExtension( dirname );
	ExtractFileBase( dirname, mapname );

	f = SafeOpenWrite( objfilename );
	ParseEntities();
	fprintf(f, "# Wavefront Objectfile containing all resourced map is using\n");
	fprintf(f, "# Resource listing:\n");
	
	/* get materials listing */
	numShaders = 0;
	for (i = 0; i < numBSPShaders; i++)
	{
		/* register material */
		shadername = bspShaders[ i ].shader;
		for (j = 0; j < numShaders; j++)
			if (!strncmp(RefShaders[ j ].shadername, shadername, 128) )
				break;
		if (j >= numShaders && numShaders < MAX_REFERENCED)
		{
			strncpy(RefShaders[ numShaders ].shadername, shadername, 128);
			numShaders++;
		}
	}

	/* get models, classes, sounds in resource listing */
	numModels = 0;
	numSounds = 0;
	numClasses = 0;
	for (i = 0; i < numEntities; i++)
	{
		entity = &entities[ i ];

		/* register class */
		classname = ValueForKey(entity, "classname");
		if( strcmp( classname, "worldspawn" ) && strcmp( classname, "misc_model" ) &&
			strcmp( classname, "_decal" ) && strcmp( classname, "misc_decal" ) &&
			strcmp( classname, "_skybox" ) && strcmp( classname, "misc_skybox" ) &&
			strncmp( classname, "info_", 5 ) ) // info_ classes does not not include any precaches
		{
			for (j = 0; j < numClasses; j++)
				if (!strncmp(RefClasses[ j ].classname, classname, 64) )
					break;
			if (j >= numClasses && numClasses < MAX_REFERENCED)
			{
				strncpy(RefClasses[ numClasses ].classname, classname, 64);
				numClasses++;
			}
		}

		/* register sound */
		static char *soundkeys[4] = { "noise", "noise2", "noise3", "noise4" };
		for (k = 0; k < 4; k++)
		{
			noise = ValueForKey(entity, soundkeys[k]);
			if (noise && noise[0] && !strncmp(noise, "sound/", 6))
			{
				noise += 6;
				/* register sound */
				for (j = 0; j < numSounds; j++)
					if (!strcmp(RefSounds[ j ].sound, noise) )
						break;
				if (j >= numSounds && numSounds < MAX_REFERENCED)
				{
					strcpy(RefSounds[ numSounds ].sound, noise);
					numSounds++;
				}
			}
		}

		/* register model */
		if (!strcmp(classname, "misc_gamemodel"))
		{
			/* get model and spawnflags */
			model = ValueForKey(entity, "model");
			spawnflags = IntForKey(entity, "spawnflags");
			if (strncmp(model, "*", 1))
			{
				/* register model */
				for (j = 0; j < numModels; j++)
					if (!strcmp(RefModels[ j ].model, model) )
						break;
				if (j >= numModels && numModels < MAX_REFERENCED)
				{
					strcpy(RefModels[ numModels ].model, model);
					numModels++;
				}
			}
		}
	}

	/* write models, sounds and classes */
	qsort(RefModels, numModels, sizeof(RefEntry_t), CompareRefEntry);
	for (i = 0; i < numModels; i++)
		fprintf(f, "# m \"%s\"\n", RefModels[ i ].model );
	qsort(RefSounds, numSounds, sizeof(RefEntry_t), CompareRefEntry);
	for (i = 0; i < numSounds; i++)
		fprintf(f, "# s \"%s\"\n", RefSounds[ i ].sound );
	qsort(RefClasses, numClasses, sizeof(RefEntry_t), CompareRefEntry);
	for (i = 0; i < numClasses; i++)
		fprintf(f, "# c \"%s\"\n", RefClasses[ i ].classname );

	/* write loadable geometry containing shaders */
	fprintf(f, "# Loadable geometry (texture references):\n");
	fprintf(f, "g base\n" );
	fprintf(f, "v -0.000000 0.000000 7.000000\n" );
	fprintf(f, "v -7.000000 0.000000 -4.000000\n" );
	fprintf(f, "v 7.000000 0.000000 -4.000000\n" );
	fprintf(f, "vn 0 0 1\n" );
	fprintf(f, "vn 0 0 1\n" );
	fprintf(f, "vn 0 0 1\n" );
	fprintf(f, "vt -0.000000 -0.218750\n" );
	fprintf(f, "vt -0.218750 0.125000\n" );
	fprintf(f, "vt 0.218750 0.125000\n" );
	qsort(RefShaders, numShaders, sizeof(RefEntry_t), CompareRefEntry);
	for (i = 0; i < numShaders; i++)
	{
		fprintf(f, "usemtl %s\n", RefShaders[ i ].shadername  );
		fprintf(f, "f 3/3 2/2 1/1\n" );
	}
#if 0
	for( i = 0; 1; i++ )
	{
		any_found = qfalse;

		/* check .tga */
		GetExternalLightmapPath( source, NULL, i, ".tga", lightmapfile );
		if( FileExists( lightmapfile ) )
		{
			fprintf(f, "usemtl %smaps/%s/" EXTERNAL_LIGHTMAP "\n", externalLightmapsPath, mapname, i );
			fprintf(f, "f 3/3 2/2 1/1\n" );
			any_found = qtrue;
		}

		/* check .png */
		if (!any_found)
		{
			GetExternalLightmapPath( source, NULL, i, ".png", lightmapfile );
			if( FileExists( lightmapfile ) )
			{
				fprintf(f, "usemtl %smaps/%s/" EXTERNAL_LIGHTMAP "\n", externalLightmapsPath, mapname, i );
				fprintf(f, "f 3/3 2/2 1/1\n" );
				any_found = qtrue;
			}
		}

		/* check .jpg */
		if (!any_found)
		{
			GetExternalLightmapPath( source, NULL, i, ".jpg", lightmapfile );
			if( FileExists( lightmapfile ) )
			{
				fprintf(f, "usemtl %smaps/%s/" EXTERNAL_LIGHTMAP "\n", externalLightmapsPath, mapname, i );
				fprintf(f, "f 3/3 2/2 1/1\n" );
				any_found = qtrue;
			}
		}

		/* check .dds */
		if (!any_found)
		{
			GetExternalLightmapPath( source, "dds/", i, ".dds", lightmapfile );
			if( FileExists( lightmapfile ) )
			{
				fprintf(f, "usemtl %smaps/%s/" EXTERNAL_LIGHTMAP "\n", externalLightmapsPath, mapname, i );
				fprintf(f, "f 3/3 2/2 1/1\n" );
				any_found = qtrue;
			}
		}

		if (!any_found)
			break;
	}
#endif
	fclose(f);
	numBSPEntities = numEntities;
	UnparseEntities(qfalse);
}

/*
OptimizeBSPMain()
main routine for bsp optimizer processing
*/

int OptimizeBSPMain( int argc, char **argv )
{
	char mapname[MAX_OS_PATH];
	int	i;

	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -optimize <mapname>\n" );
		return 0;
	}

	/* note it */
	Sys_Printf( "--- OptimizeBSP ---\n" );

	/* process arguments */
	Sys_Printf( "--- CommandLine ---\n" );
	strcpy(externalLightmapsPath, "");
	for( i = 1; i < (argc - 1) && argv[ i ]; i++ )
	{
		/* -mergeblock */
		if( !strcmp( argv[ i ],  "-mergeblock") )
		{
			i++;
			if (i < argc)
			{
				mergeBlock[0] = atof(argv[ i ]);
				i++;
				if (i < argc)
				{
					mergeBlock[1] = atof(argv[ i ]);
					i++;
					if (i < argc)
					{
						mergeBlock[2] = atof(argv[ i ]);
						i++;
					}
				}
			}
			Sys_Printf( " Merge block set to {%.1f %.1f %.1f}\n", mergeBlock[0], mergeBlock[1], mergeBlock[2]);
		}
		/* -onlyshaders */
		else if( !strcmp( argv[ i ],  "-onlyshaders") )
		{
			tidyShaders = qtrue;
			mergeDrawSurfaces = qfalse;
			mergeDrawVerts = qfalse;
			Sys_Printf( " Only optimizing material references\n" );
		}
		/* -tidyshaders */
		else if( !strcmp( argv[ i ],  "-tidyshaders") )
		{
			tidyShaders = qtrue;
			Sys_Printf( " Enabled optimisation of material references\n" );
		}
		/* -notidyshaders */
		else if( !strcmp( argv[ i ],  "-notidyshaders") )
		{
			tidyShaders = qfalse;
			Sys_Printf( " Disabled optimisation of material references\n" );
		}
		/* -mergesurfaces */
		else if( !strcmp( argv[ i ],  "-mergesurfaces") )
		{
			mergeDrawSurfaces = qtrue;
			Sys_Printf( " Enabled merging of same-shader draw surfaces\n" );
		}
		/* -nomergesurfaces */
		else if( !strcmp( argv[ i ],  "-nomergesurfaces") )
		{
			mergeDrawSurfaces = qfalse;
			Sys_Printf( " Disabled merging of same-shader draw surfaces\n" );
		}
		/* -mergeverts */
		else if( !strcmp( argv[ i ],  "-mergeverts") )
		{
			mergeDrawVerts = qtrue;
			Sys_Printf( " Enabled merging of coincident draw vertices\n" );
		}
		/* -nomergeverts */
		else if( !strcmp( argv[ i ],  "-nomergeverts") )
		{
			mergeDrawVerts = qfalse;
			Sys_Printf( " Disabled merging of coincident draw vertices\n" );
		}
		/* -tidyentities */
		else if( !strcmp( argv[ i ],  "-tidyents") )
		{
			tidyEntities = qtrue;
			Sys_Printf( " Enabled optimisation of entity data lump\n" );
		}
		/* -notidyentities */
		else if( !strcmp( argv[ i ],  "-notidyents") )
		{
			tidyEntities = qfalse;
			Sys_Printf( " Disabled optimisation of entity data lump\n" );
		}
		/* -resmap */
		else if( !strcmp( argv[ i ],  "-resmap") )
		{
			makeResMap = qtrue;
			Sys_Printf( " Generating resource map\n" );
		}
		/* -entitysaveid */
		else if( !strcmp( argv[ i ],  "-entitysaveid") )
		{
			Sys_Printf( " Entity unique savegame identifiers enabled\n" );
			useEntitySaveId = qtrue;
		}
		/* -externalpath */
		else if( !strcmp( argv[ i ], "-externalpath" ) )
		{
			i++;
			if (i < argc)
			{
				SanitizePath(argv[ i ], externalLightmapsPath);
				Sys_Printf( " External are stored under %s\n" , externalLightmapsPath );
			}
		}
		else
			Sys_Warning( "Unknown option \"%s\"", argv[ i ] );
	}

	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	/* load shaders */
	LoadShaderInfo();
	
	/* load map */
	Sys_Printf( "--- LoadBSPFile ---\n" );
	Sys_Printf( "loading %s\n", source );
	
	/* load surface file */
	LoadBSPFile( source );

	/* optimize */
	if (tidyShaders)
		TidyShaders();
	if (mergeDrawSurfaces)
		MergeDrawSurfaces();
	if (mergeDrawVerts)
		MergeDrawVerts();
	if (tidyEntities)
		TidyEntities();

	/* write back */
	Sys_Printf( "--- WriteBSPFile ---\n" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* transform and write a resource map */
	if ( makeResMap )
	{
		Sys_Printf( "--- WriteResourceOBJFile ---\n" );
		strcpy( source, ExpandArg( argv[ i ] ) );
		strcpy( mapname, source );
		StripExtension( mapname );
		DefaultExtension( mapname, ".res.obj" );
		Sys_Printf( "Writing resmap %s\n", mapname );
		WriteResourceOBJFile( mapname );
	}

	/* return to sender */
	return 0;
}