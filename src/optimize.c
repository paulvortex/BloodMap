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

/*
TidyShaders
reducing number of work shaders by replacing current shaders with engine-related ones (q3map_engineShader)
*/

typedef struct bspShaderMeta_s
{
	int          index;
	int          newIndex;
	char         name[1024];
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
}
bspDrawSurfaceMeta_t;

void MergeDrawSurfaces(void)
{
	int i, f, start, fOld, modelnum, surfacenum, leafnum, leafsurfacenum;
	int numNewSurfaces, numMergedSurfaces, numDrawSurfacesProcessed, numDrawVertsProcessed, numDrawIndexesProcessed, numLeafSurfacesProcessed;
	bspDrawSurfaceMeta_t *metaSurfaces, *ms, *ms2;
	bspModel_t *model;
	bspLeaf_t *leaf;
	bspDrawSurface_t *ds, *ds2, *newDrawSurfaces;
	bspDrawVert_t *newDrawVerts, *dv, *dv2;
	int *newDrawIndexes, *newLeafSurfaces, *di, *di2, firstDS, numDS;
	vec3_t newsize;

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

			/* check if this surface can be merged to other surfaces */
			for( i = 0; i < model->numBSPSurfaces; i++ )
			{
				ms2 = &metaSurfaces[ model->firstBSPSurface + i ];
				if (ms == ms2 || ms2->mergeable == qfalse)
					continue;
				ds2 = ms2->ds;

				/* surface should match shader and lightmapnum */
				if (ds->shaderNum == ds2->shaderNum && ds->lightmapNum[0] == ds2->lightmapNum[0])
				{
					/* check min/max bounds */
					newsize[0] = max(ms->absmax[0], ms2->absmax[0]) - min(ms->absmin[0], ms2->absmin[0]);
					newsize[1] = max(ms->absmax[1], ms2->absmax[1]) - min(ms->absmin[1], ms2->absmin[1]);
					newsize[2] = max(ms->absmax[2], ms2->absmax[2]) - min(ms->absmin[2], ms2->absmin[2]);
					if (newsize[0] < max(mergeBlock[0], ms2->absmax[0] - ms2->absmin[0]) && 
						newsize[1] < max(mergeBlock[1], ms2->absmax[1] - ms2->absmin[1]) &&
						newsize[2] < max(mergeBlock[2], ms2->absmax[2] - ms2->absmin[2]))
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

			/* failed to find any surface to merge, so this surface could be a merge candidate */
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

			/* merge surfaces */
			for (ms2 = ms->next; ms2; ms2 = ms2->next)
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
			f = 10 * numLeafSurfacesProcessed / numBSPLeafSurfaces;
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

#define VERTMERGE_ORIGIN_EPSILON  0.01
#define VERTMERGE_NORMAL_EPSILON  0.2
#define VERTMERGE_TC_EPSILON      0.02
#define VERTMERGE_LMTC_EPSILON    0.0005
#define VERTMERGE_COLOR_EPSILON   32
#define VERTMERGE_ALPHA_EPSILON   32

qboolean Vec3ByteCompareExt(byte n1[3], byte n2[3], byte epsilon)
{
	int i;
	for (i= 0; i < 3; i++)
		if (fabs(n1[i] - n2[i]) > epsilon)
			return qfalse;
	return qtrue;
}

qboolean Vec1ByteCompareExt(byte n1, byte n2, byte epsilon)
{
	if (fabs(n1 - n2) > epsilon)
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
	int i, start, f, fOld, modelnum, surfacenum, vertnum, indexnum, a, b, c;
	int numVertsProcessed, numVertsToMerge, numNewVerts, numNewIndexes, numRemovedTriangles, *newDrawIndexes;
	int firstVert, numVerts, firstIndex, numIndexes;
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
				if (ds->surfaceType == MST_PLANAR && ds->surfaceType == MST_TRIANGLE_SOUP)
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
			numVerts = 0;
			firstIndex = numNewIndexes;
			numIndexes = 0;
			/* walk all draw verts */
			for (vertnum = 0; vertnum < ds->numVerts; vertnum++)
			{
				mv = &metaVerts[ds->firstVert + vertnum];
				if (mv->mergedto)
					continue;
				dv = mv->dv;
				dv2 = &newDrawVerts[numNewVerts];
				memcpy(dv2, dv, sizeof(bspDrawVert_t));
				numNewVerts++;
			}
			/* walk all draw indexes */
			for (indexnum = 0; indexnum < ds->numIndexes; indexnum += 3)
			{
				i = ds->firstVert + bspDrawIndexes[ds->firstIndex + indexnum];
				if (i < 0 || i > numBSPDrawVerts)
					Sys_Printf("WARNING: drawvert %i out of range 0 - %i\n", i, numBSPDrawVerts);
				else
				{
					a = ((metaVerts[i].mergedto) ? ((bspDrawVertMeta_t *)metaVerts[i].mergedto)->newIndex : metaVerts[i].newIndex) - firstVert;
					b = ((metaVerts[i+1].mergedto) ? ((bspDrawVertMeta_t *)metaVerts[i+1].mergedto)->newIndex : metaVerts[i+1].newIndex) - firstVert;
					c = ((metaVerts[i+2].mergedto) ? ((bspDrawVertMeta_t *)metaVerts[i+2].mergedto)->newIndex : metaVerts[i+2].newIndex) - firstVert;
					if (a == b || a == c || b == c)
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
OptimizeBSPMain()
main routine for bsp optimizer processing
*/

int OptimizeBSPMain( int argc, char **argv )
{
	int	i;

	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -optimize <mapname>\n" );
		return 0;
	}

	/* note it */
	Sys_Printf( "--- Optimize ---\n" );

	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
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
			Sys_Printf( "Merge block set to {%.1f %.1f %.1f}\n", mergeBlock[0], mergeBlock[1], mergeBlock[2]);
		}
		/* -onlyshaders */
		else if( !strcmp( argv[ i ],  "-onlyshaders") )
		{
			tidyShaders = qtrue;
			mergeDrawSurfaces = qfalse;
			mergeDrawVerts = qfalse;
			Sys_Printf( "Only optimizing material references\n" );
		}
		/* -tidyshaders */
		else if( !strcmp( argv[ i ],  "-tidyshaders") )
		{
			tidyShaders = qtrue;
			Sys_Printf( "Enabled optimisation of material references\n" );
		}
		/* -notidyshaders */
		else if( !strcmp( argv[ i ],  "-notidyshaders") )
		{
			tidyShaders = qfalse;
			Sys_Printf( "Disabled optimisation of material references\n" );
		}
		/* -mergesurfaces */
		else if( !strcmp( argv[ i ],  "-mergesurfaces") )
		{
			mergeDrawSurfaces = qtrue;
			Sys_Printf( "Enabled merging of same-shader draw surfaces\n" );
		}
		/* -nomergesurfaces */
		else if( !strcmp( argv[ i ],  "-nomergesurfaces") )
		{
			mergeDrawSurfaces = qfalse;
			Sys_Printf( "Disabled merging of same-shader draw surfaces\n" );
		}
		/* -mergeverts */
		else if( !strcmp( argv[ i ],  "-mergeverts") )
		{
			mergeDrawVerts = qtrue;
			Sys_Printf( "Enabled merging of coincident draw vertices\n" );
		}
		/* -nomergeverts */
		else if( !strcmp( argv[ i ],  "-nomergeverts") )
		{
			mergeDrawVerts = qfalse;
			Sys_Printf( "Disabled merging of coincident draw vertices\n" );
		}
		/* -tidyentities */
		else if( !strcmp( argv[ i ],  "-tidyents") )
		{
			tidyEntities = qtrue;
			Sys_Printf( "Enabled optimisation of entity data lump\n" );
		}
		/* -notidyentities */
		else if( !strcmp( argv[ i ],  "-notidyents") )
		{
			tidyEntities = qfalse;
			Sys_Printf( "Disabled optimisation of entity data lump\n" );
		}
		/* -custinfoparms */
		else if( !strcmp( argv[ i ],  "-custinfoparms") )
		{
			useCustomInfoParms = qtrue;
			Sys_Printf( "Custom info parms enabled\n" );
		}
		else
			Sys_Printf( "WARNING: Unknown option \"%s\"\n", argv[ i ] );
	}

	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	
	/* load shaders */
	LoadShaderInfo();
	
	/* load map */
	Sys_Printf( "Loading %s\n", source );
	
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
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}