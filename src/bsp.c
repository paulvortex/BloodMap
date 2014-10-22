/* -------------------------------------------------------------------------------

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

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define BSP_C



/* dependencies */
#include "q3map2.h"



/* -------------------------------------------------------------------------------

functions

------------------------------------------------------------------------------- */



/*
SetCloneModelNumbers() - ydnar
sets the model numbers for brush entities
*/

void SetCloneModelNumbers( void )
{
	int			i, j;
	int			models;
	char		modelValue[ 10 ];
	const char	*value, *value2, *value3;
	
	
	/* start with 1 (worldspawn is model 0) */
	models = 1;
	for( i = 1; i < numEntities; i++ )
	{
		/* only entities with brushes or patches get a model number */
		if( entities[ i ].brushes == NULL && entities[ i ].patches == NULL && entities[ i ].forceSubmodel == qfalse)
			continue;
		
		/* is this a clone? */
		value = ValueForKey( &entities[ i ], "_clone" );
		if( value[ 0 ] != '\0' )
			continue;
		
		/* add the model key */
		sprintf( modelValue, "*%d", models );
		SetKeyValue( &entities[ i ], "model", modelValue );
		
		/* increment model count */
		models++;
	}
	
	/* fix up clones */
	for( i = 1; i < numEntities; i++ )
	{
		/* only entities with brushes or patches get a model number */
		if( entities[ i ].brushes == NULL && entities[ i ].patches == NULL && entities[ i ].forceSubmodel == qfalse)
			continue;
		
		/* is this a clone? */
		value = ValueForKey( &entities[ i ], "_ins" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( &entities[ i ], "_instance" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( &entities[ i ], "_clone" );
		if( value[ 0 ] == '\0' )
			continue;
		
		/* find an entity with matching clone name */
		for( j = 0; j < numEntities; j++ )
		{
			/* is this a clone parent? */
			value2 = ValueForKey( &entities[ j ], "_clonename" );
			if( value2[ 0 ] == '\0' )
				continue;
			
			/* do they match? */
			if( strcmp( value, value2 ) == 0 )
			{
				/* get the model num */
				value3 = ValueForKey( &entities[ j ], "model" );
				if( value3[ 0 ] == '\0' )
				{
					Sys_Printf( "WARNING: Cloned entity %s referenced entity without model\n", value2 );
					continue;
				}
				models = atoi( &value2[ 1 ] );
				
				/* add the model key */
				sprintf( modelValue, "*%d", models );
				SetKeyValue( &entities[ i ], "model", modelValue );
				
				/* nuke the brushes/patches for this entity (fixme: leak!) */
				entities[ i ].brushes = NULL;
				entities[ i ].patches = NULL;
			}
		}
	}
}



/*
FixBrushSides() - ydnar
matches brushsides back to their appropriate drawsurface and shader
*/

static void FixBrushSides( entity_t *e )
{
	int					i;
	mapDrawSurface_t	*ds;
	sideRef_t			*sideRef;
	bspBrushSide_t		*side;
	
	/* walk list of drawsurfaces */
	for( i = e->firstDrawSurf; i < numMapDrawSurfs; i++ )
	{
		/* get surface and try to early out */
		ds = &mapDrawSurfs[ i ];
		if( ds->outputNum < 0 )
			continue;
		
		/* walk sideref list */
		for( sideRef = ds->sideRef; sideRef != NULL; sideRef = sideRef->next )
		{
			/* get bsp brush side */
			if( sideRef->side == NULL || sideRef->side->outputNum < 0 )
				continue;
			side = &bspBrushSides[ sideRef->side->outputNum ];
			
			/* set drawsurface */
			side->surfaceNum = ds->outputNum;
			//%	Sys_FPrintf( SYS_VRB, "DS: %7d Side: %7d     ", ds->outputNum, sideRef->side->outputNum );
			
			/* set shader */
			if( strcmp( bspShaders[ side->shaderNum ].shader, ds->shaderInfo->shader ) )
			{
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", bspShaders[ side->shaderNum ].shader, ds->shaderInfo->shader );
				side->shaderNum = EmitShader( ds->shaderInfo->shader, &ds->shaderInfo->contentFlags, &ds->shaderInfo->surfaceFlags );
			}
		}
	}
}

/*
StitchBrushFaces() - vortex
performs a bugfixing of brush faces
*/

#define STITCH_DISTANCE         0.1f    /* lower than min grid */
#define STITCH_NORMAL_EPSILON   0.1f    /* if triangle normal is changed above this, vertex is rolled back */
#define STITCH_MAX_TRIANGLES    64      /* max triangles formed from a stiched vertex to be checked */
//#define STITCH_USE_TRIANGLE_NORMAL_CHECK

static void StitchBrushFaces( entity_t *e )
{
	mapDrawSurface_t *ds, *ds2;
	shaderInfo_t *si;
	bspDrawVert_t *dv, *dv2;
	vec3_t mins, maxs, sub;
	int i, j, k, m, best;
	int numVertsStitched = 0, numSurfacesStitched = 0;
	double dist, bestdist;
	qboolean stitched, trystitch;

#ifdef STITCH_USE_TRIANGLE_NORMAL_CHECK
	qboolean stripped, n;
	vec3_t normal, normals[STITCH_MAX_TRIANGLES];
	int t, numTriangles, indexes[STITCH_MAX_TRIANGLES][3];
#endif

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- StitchBrushFaces ---\n" );

	/* loop drawsurfaces */
	for ( i = e->firstDrawSurf ; i < numMapDrawSurfs ; i++ )
	{
		/* get surface and early out if possible */
		ds = &mapDrawSurfs[ i ];
		si = ds->shaderInfo;
		if( (si->compileFlags & C_NODRAW) || si->autosprite || ds->numVerts == 0 || ds->type != SURFACE_FACE )
			continue;

		/* get bounds, add little bevel */
		VectorCopy(ds->mins, mins);
		VectorCopy(ds->maxs, maxs);
		mins[ 0 ] -= 4;
		mins[ 1 ] -= 4;
		mins[ 2 ] -= 4;
		maxs[ 0 ] += 4;
		maxs[ 1 ] += 4;
		maxs[ 2 ] += 4;
		
		/* stitch with neighbour drawsurfaces */
		stitched = qfalse;
#ifdef STITCH_USE_TRIANGLE_NORMAL_CHECK
		stripped = qfalse;
#endif
		for ( j = e->firstDrawSurf; j < numMapDrawSurfs ; j++ )
		{
			/* get surface */
			ds2 = &mapDrawSurfs[ j ];

			/* test bounds */
			if( ds2->mins[ 0 ] > maxs[ 0 ] || ds2->maxs[ 0 ] < mins[ 0 ] ||
				ds2->mins[ 1 ] > maxs[ 1 ] || ds2->maxs[ 1 ] < mins[ 1 ] ||
				ds2->mins[ 2 ] > maxs[ 2 ] || ds2->maxs[ 2 ] < mins[ 2 ] )
				continue;

			/* loop verts */
			for( k = 0; k < ds->numVerts; k++)
			{
				dv = &ds->verts[ k ];
				trystitch = qfalse;

				/* find candidate */
				best = -1;
				for( m = 0; m < ds2->numVerts; m++)
				{
					if( VectorCompareExt( ds2->verts[ m ].xyz, dv->xyz, STITCH_DISTANCE) == qfalse )
						continue;

					/* don't stitch if origins match completely */
					dv2 = &ds2->verts[ m ];
					if( dv2->xyz[ 0 ] == dv->xyz[ 0 ] && dv2->xyz[ 1 ] == dv->xyz[ 1 ] && dv2->xyz[ 2 ] == dv->xyz[ 2 ] )
						continue;

					/* get closest one */
					VectorSubtract( dv2->xyz, dv->xyz, sub );
					dist = VectorLength( sub );
					if (best < 0 || dist < bestdist)
					{
						best = m;
						bestdist = dist;
					}
				}

				/* nothing found? */
				if ( best < 0 )
					continue;

				/* before stitching, get a list of triangles formed by this vertex */ 
#ifdef STITCH_USE_TRIANGLE_NORMAL_CHECK
				if( trystitch == qfalse )
				{
					numTriangles = 0;
					if ( stripped == qfalse )
					{
						StripFaceSurface( ds, qtrue );
						stripped = qtrue;
					}
					for (t = 0; t < ds->numIndexes; t += 3)
					{
						if( ds->indexes[ t ] == k || ds->indexes[ t + 1 ] == k || ds->indexes[ t + 2 ] == k )
						{
							indexes[ numTriangles ][ 0 ] = ds->indexes[ t ];
							indexes[ numTriangles ][ 1 ] = ds->indexes[ t + 1 ];
							indexes[ numTriangles ][ 2 ] = ds->indexes[ t + 2 ];
							NormalFromPoints( normals[ numTriangles ], ds->verts[ ds->indexes[ t ] ].xyz, ds->verts[ ds->indexes[ t + 1 ] ].xyz, ds->verts[ ds->indexes[ t + 2 ] ].xyz);
							numTriangles++;
							if (numTriangles == STITCH_MAX_TRIANGLES)
								break;
						}
					}
					trystitch = qtrue;
				}
#endif

				/* stitch */
				VectorCopy( dv->xyz, sub );
				VectorCopy( ds2->verts[ best ].xyz, dv->xyz );

#ifdef STITCH_USE_TRIANGLE_NORMAL_CHECK
				/* make sure all triangles don't get their normals perverted */
				for (t = 0; t < numTriangles; t++)
				{
					/* construct new normal */
					n = NormalFromPoints( normal, ds->verts[ indexes[ t ][ 0 ] ].xyz, ds->verts[ indexes[ t ][ 1 ] ].xyz, ds->verts[ indexes[ t ][ 2 ] ].xyz);

					/* compare, roll back if normal get perverted */
					if( !n || VectorCompareExt( normals[ t ], normal, STITCH_NORMAL_EPSILON) == qfalse )
					{
						VectorCopy( sub, dv->xyz );
						break;
					}
				}

				/* done */
				if (t == numTriangles)
				{
					numVertsStitched++;
					stitched = qtrue;
				}
#else
				numVertsStitched++;
				stitched = qtrue;
#endif
			}
		}

#ifdef STITCH_USE_TRIANGLE_NORMAL_CHECK
		/* clean up after StripFaceSurface */
		if( stripped )
		{
			ds->numIndexes = 0;
			if ( ds->indexes != NULL )
				free( ds->indexes );
			ds->indexes = NULL;
		}
#endif

		/* add to stats */
		if( stitched )
			numSurfacesStitched++;
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d verts stitched\n", numVertsStitched );
	Sys_FPrintf( SYS_VRB, "%9d surfaces stitched\n", numSurfacesStitched );
}

/*
ProcessWorldModel()
creates a full bsp + surfaces for the worldspawn entity
*/

void ProcessWorldModel( void )
{
	int			i, s;
	entity_t	*e;
	tree_t		*tree;
	face_t		*faces;
	qboolean	ignoreLeaks, leaked;
	xmlNodePtr	polyline, leaknode;
	char		level[ 2 ], shader[ MAX_OS_PATH ];
	const char	*value;

	/* note it */
	Sys_Printf ( "--- ProcessWorld ---\n" );
	
	/* sets integer blockSize from worldspawn "_blocksize" key if it exists */
	value = ValueForKey( &entities[ 0 ], "_blocksize" );
	if( value[ 0 ] == '\0' )
		value = ValueForKey( &entities[ 0 ], "blocksize" );
	if( value[ 0 ] == '\0' )
		value = ValueForKey( &entities[ 0 ], "chopsize" );	/* sof2 */
	if( value[ 0 ] != '\0' )
	{
		/* scan 3 numbers */
		s = sscanf( value, "%d %d %d", &blockSize[ 0 ], &blockSize[ 1 ], &blockSize[ 2 ] );
		
		/* handle legacy case */
		if( s == 1 )
		{
			blockSize[ 1 ] = blockSize[ 0 ];
			blockSize[ 2 ] = blockSize[ 0 ];
		}
	}
	Sys_Printf( "block size = { %d %d %d }\n", blockSize[ 0 ], blockSize[ 1 ], blockSize[ 2 ] );
	
	/* sof2: ignore leaks? */
	value = ValueForKey( &entities[ 0 ], "_ignoreleaks" );	/* ydnar */
	if( value[ 0 ] == '\0' )
		value = ValueForKey( &entities[ 0 ], "ignoreleaks" );
	if( value[ 0 ] == '1' )
		ignoreLeaks = qtrue;
	else
		ignoreLeaks = qfalse;
	
	/* begin worldspawn model */
	BeginModel();
	e = &entities[ 0 ];
	e->firstDrawSurf = 0;
	
	/* ydnar: gs mods */
	ClearMetaTriangles();

	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );

	/* build an initial bsp tree using all of the sides of all of the structural brushes */
	faces = MakeStructuralBSPFaceList( entities[ 0 ].brushes );
	tree = FaceBSP( faces );
	MakeTreePortals( tree );
	FilterStructuralBrushesIntoTree( e, tree );
	
	/* see if the bsp is completely enclosed */
	if( FloodEntities( tree ) || ignoreLeaks )
	{
		/* rebuild a better bsp tree using only the sides that are visible from the inside */
		FillOutside( tree->headnode );

		/* chop the sides to the convex hull of their visible fragments, giving us the smallest polygons */
		ClipSidesIntoTree( e, tree );
		
		/* build a visible face tree */
		faces = MakeVisibleBSPFaceList( entities[ 0 ].brushes );
		FreeTree( tree );
		tree = FaceBSP( faces );
		MakeTreePortals( tree );
		FilterStructuralBrushesIntoTree( e, tree );
		leaked = qfalse;
		
		/* ydnar: flood again for skybox */
		if( skyboxPresent )
			FloodEntities( tree );
	}
	else
	{
		Sys_FPrintf( SYS_NOXML, "**********************\n" );
		Sys_FPrintf( SYS_NOXML, "******* leaked *******\n" );
		Sys_FPrintf( SYS_NOXML, "**********************\n" );
		polyline = LeakFile( tree );
		leaknode = xmlNewNode( NULL, (xmlChar *)"message" );
		xmlNodeSetContent( leaknode, (xmlChar *)"MAP LEAKED\n" );
		xmlAddChild( leaknode, polyline );
		level[0] = (int) '0' + SYS_ERR;
		level[1] = 0;
		xmlSetProp( leaknode, (xmlChar *)"level", (xmlChar *)&level );
		xml_SendNode( leaknode );
		if( leaktest )
		{
			Sys_Printf ("--- MAP LEAKED, ABORTING LEAKTEST ---\n");
			exit( 0 );
		}
		leaked = qtrue;
		
		/* chop the sides to the convex hull of their visible fragments, giving us the smallest polygons */
		ClipSidesIntoTree( e, tree );
	}
	
	/* save out information for visibility processing */
	NumberClusters( tree );
	if( !leaked )
		WritePortalFile( tree );
	
	/* flood from entities */
	FloodAreas( tree );
	
	/* create drawsurfs for triangle models */
	AddTriangleModels( 0 );
	
	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );
	
	/* generate bsp brushes from map brushes */
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );
	
	/* add references to the detail brushes */
	FilterDetailBrushesIntoTree( e, tree );
	
	/* drawsurfs that cross fog boundaries will need to be split along the fog boundary */
	if( !nofog )
		FogDrawSurfaces( e );
	
	/* subdivide each drawsurf as required by shader tesselation */
	if( !nosubdivide )
		SubdivideFaceSurfaces( e, tree );

	/* vortex: fix degenerate brush faces */
	StitchBrushFaces( e );
	
	/* add in any vertexes required to fix t-junctions */
	if( !notjunc )
		FixTJunctions( e );

	/* ydnar: classify the surfaces */
	ClassifyEntitySurfaces( e );
	
	/* ydnar: project decals */
	MakeEntityDecals( e );
	
	/* ydnar: meta surfaces */
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();
	
	/* ydnar: debug portals */
	if( debugPortals )
		MakeDebugPortalSurfs( tree );
	
	/* ydnar: fog hull */
	value = ValueForKey( &entities[ 0 ], "_foghull" );
	if( value[ 0 ] != '\0' )
	{
		sprintf( shader, "textures/%s", value );
		MakeFogHullSurfs( e, tree, shader );
	}
	
	/* ydnar: bug 645: do flares for lights */
	for( i = 0; i < numEntities && emitFlares; i++ )
	{
		entity_t	*light, *target;
		const char	*value, *flareShader;
		vec3_t		origin, targetOrigin, normal, color;
		int			lightStyle;
		
		
		/* get light */
		light = &entities[ i ];
		value = ValueForKey( light, "classname" );
		if( !strcmp( value, "light" ) )
		{
			/* get flare shader */
			flareShader = ValueForKey( light, "_flareshader" );
			value = ValueForKey( light, "_flare" );
			if( flareShader[ 0 ] != '\0' || value[ 0 ] != '\0' )
			{
				/* get specifics */
				GetVectorForKey( light, "origin", origin );
				GetVectorForKey( light, "_color", color );
				lightStyle = IntForKey( light, "_style" );
				if( lightStyle == 0 )
					lightStyle = IntForKey( light, "style" );
				
				/* handle directional spotlights */
				value = ValueForKey( light, "target" );
				if( value[ 0 ] != '\0' )
				{
					/* get target light */
					target = FindTargetEntity( value );
					if( target != NULL )
					{
						GetVectorForKey( target, "origin", targetOrigin );
						VectorSubtract( targetOrigin, origin, normal );
						VectorNormalize( normal, normal );
					}
				}
				else
					//%	VectorClear( normal );
					VectorSet( normal, 0, 0, -1 );
				
				/* create the flare surface (note shader defaults automatically) */
				DrawSurfaceForFlare( mapEntityNum, origin, normal, color, (char*) flareShader, lightStyle );
			}
		}
	}
	
	/* add references to the final drawsurfs in the apropriate clusters */
	FilterDrawsurfsIntoTree( e, tree, qtrue );
	EmitDrawsurfsStats();

	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );
	
	/* finish */
	EndModel( e, tree->headnode );
	FreeTree( tree );
}



/*
ProcessSubModel()
creates bsp + surfaces for other brush models
*/

void ProcessSubModel( void )
{
	entity_t	*e;
	tree_t		*tree;
	brush_t		*b, *bc;
	node_t		*node;
	int         entityNum;
	
	/* start a brush model */
	BeginModel();
	entityNum = mapEntityNum;
	e = &entities[ entityNum ];
	e->firstDrawSurf = numMapDrawSurfs;
	
	/* ydnar: gs mods */
	ClearMetaTriangles();
	
	/* check for patches with adjacent edges that need to lod together */
	PatchMapDrawSurfs( e );
	
	/* allocate a tree */
	node = AllocNode();
	node->planenum = PLANENUM_LEAF;
	tree = AllocTree();
	tree->headnode = node;
	
	/* add the sides to the tree */
	ClipSidesIntoTree( e, tree );
	
	/* ydnar: create drawsurfs for triangle models */
	AddTriangleModels( entityNum );
	
	/* create drawsurfs for surface models */
	AddEntitySurfaceModels( e );
	
	/* generate bsp brushes from map brushes */
	EmitBrushes( e->brushes, &e->firstBrush, &e->numBrushes );

	/* just put all the brushes in headnode */
	for( b = e->brushes; b; b = b->next )
	{
		bc = CopyBrush( b );
		bc->next = node->brushlist;
		node->brushlist = bc;
	}
	
	/* subdivide each drawsurf as required by shader tesselation */
	if( !nosubdivide )
		SubdivideFaceSurfaces( e, tree );
	
	/* vortex: fix degenerate brush faces */
	StitchBrushFaces( e );
	
	/* add in any vertexes required to fix t-junctions */
	if( !notjunc )
		FixTJunctions( e );
	
	/* ydnar: classify the surfaces and project lightmaps */
	ClassifyEntitySurfaces( e );
	
	/* ydnar: project decals */
	MakeEntityDecals( e );
	
	/* ydnar: meta surfaces */
	MakeEntityMetaTriangles( e );
	SmoothMetaTriangles();
	FixMetaTJunctions();
	MergeMetaTriangles();
	
	/* add references to the final drawsurfs in the apropriate clusters */
	FilterDrawsurfsIntoTree( e, tree, qfalse );
	
	/* match drawsurfaces back to original brushsides (sof2) */
	FixBrushSides( e );

	/* finish */
	EndModel( e, node );
	FreeTree( tree );
}



/*
ProcessModels()
process world + other models into the bsp
*/

void ProcessModels( void )
{
	int			f, fOld, start, submodel, submodels;
	qboolean	oldVerbose;
	entity_t	*entity;
	
	/* preserve -v setting */
	oldVerbose = verbose;
	
	/* start a new bsp */
	BeginBSPFile();
	
	/* create map fogs */
	CreateMapFogs();

	/* process world model */
	ProcessWorldModel();
	
	/* process submodels */
	verbose = qfalse;
	start = I_FloatTime();
	fOld = -1;

	Sys_Printf ( "--- ProcessModels ---\n" );

	/* count */
	for( submodels = 0, mapEntityNum = 1; mapEntityNum < numEntities; mapEntityNum++ )
		if( entities[ mapEntityNum ].brushes != NULL || entities[ mapEntityNum ].patches != NULL || entities[ mapEntityNum ].forceSubmodel != qfalse )
			submodels++;

	/* process */
	for( submodel = 0, mapEntityNum = 1; mapEntityNum < numEntities; mapEntityNum++ )
	{
		entity = &entities[ mapEntityNum ];
		if( entity->brushes == NULL && entity->patches == NULL && entity->forceSubmodel == qfalse )
			continue;
		submodel++;

		/* print pacifier */
		if ( submodels > 10 )
		{
			f = 10 * submodel / submodels;
			if( f != fOld )
			{
				fOld = f;
				Sys_Printf( "%d...", f );
			}
		}
		ProcessSubModel();
	}

	/* print overall time */
	if ( submodels > 10 )
		Sys_Printf (" (%d)\n", (int) (I_FloatTime() - start) );

	/* restore -v setting */
	verbose = oldVerbose;

	/* emit stats */
	Sys_Printf( "%9i submodels\n", submodels);
	EmitDrawsurfsStats();

	/* write fogs */
	EmitFogs();

	/* vortex: emit meta stats */
	EmitMetaStats();
}

/*
RegionScissor()
remove all stuff outside map region
*/
void RegionScissor( void )
{
	int k, entityNum, scissorEntity, c, frame;
	int removedBrushes = 0, removedPatches = 0, removedModels = 0, removedEntities = 0, removedLights = 0;
	const char *classname, *model, *value;
	float temp, intensity;
	m4x4_t transform;
	entity_t *e;
	brush_t *b, *bs, *bl, *nb;
	parseMesh_t *p, *ps, *pl, *np;
	vec3_t mins, maxs, origin, scale, angles, color;
	picoModel_t *picomodel;
	bspDrawVert_t *v;

	/* early out */
	if ( mapRegion == qfalse )
		return;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- RegionScissor ---\n" );

	/* scissor world brushes */
	e = &entities[ 0 ];
	bs = NULL;
	for( b = e->brushes; b; b = nb )
	{
		nb = b->next;
		if( b->mins[ 0 ] > mapRegionMaxs[ 0 ] || b->maxs[ 0 ] < mapRegionMins[ 0 ] ||
			b->mins[ 1 ] > mapRegionMaxs[ 1 ] || b->maxs[ 1 ] < mapRegionMins[ 1 ] ||
			b->mins[ 2 ] > mapRegionMaxs[ 2 ] || b->maxs[ 2 ] < mapRegionMins[ 2 ] )
		{
			// remove brush
			b->next = NULL;
			FreeBrush(b);
			removedBrushes++;
			continue;
		}
		// keep brush
		if (bs == NULL)
		{
			bs = b;
			bl = b;
		}
		else
		{
			bl->next = b;
			b->next = NULL;
			bl = b;
		}
	}
	e->brushes = bs;

	/* scissor world patches */
	e = &entities[ 0 ];
	ps = NULL;
	for( p = e->patches; p; p = np )
	{
		np = p->next;
		// calc patch bounds
		ClearBounds( mins, maxs );
		c = p->mesh.width * p->mesh.height;
		v = p->mesh.verts;
		for( k = 0; k < c; k++, v++ )
			AddPointToBounds( v->xyz, mins, maxs );
		if( mins[ 0 ] > mapRegionMaxs[ 0 ] || maxs[ 0 ] < mapRegionMins[ 0 ] ||
			mins[ 1 ] > mapRegionMaxs[ 1 ] || maxs[ 1 ] < mapRegionMins[ 1 ] ||
			mins[ 2 ] > mapRegionMaxs[ 2 ] || maxs[ 2 ] < mapRegionMins[ 2 ] )
		{
			// remove patch
			// FIXME: leak
			p->next = NULL;
			removedPatches++;
			continue;
		}
		// keep patch
		if (ps == NULL)
		{
			ps = p;
			pl = p;
		}
		else
		{
			pl->next = p;
			p->next = NULL;
			pl = p;
		}
	}
	e->patches = ps;

	/* scissor entities */
	scissorEntity = 1;
	for( entityNum = 1; entityNum < numEntities; entityNum++ )
	{
		e = &entities[ entityNum ];
		classname = ValueForKey( e, "classname" );

		/* scissor misc_models by model box */
		if( !Q_stricmp( classname, "misc_model" ) || !Q_stricmp( classname, "misc_gamemodel" ) )
		{
			/* get model name */
			model = ValueForKey( e, "_model" );	
			if( model[ 0 ] == '\0' )
				model = ValueForKey( e, "model" );
			if( model[ 0 ] == '\0' )
				goto scissorentity;
					
			/* get model frame */
			frame = 0;
			if( KeyExists(e, "frame" ) )
				frame = IntForKey( e, "frame" );
			if( KeyExists(e, "_frame" ) )
				frame = IntForKey( e, "_frame" );

			/* get origin */
			GetVectorForKey( e, "origin", origin );

			/* get scale */
			scale[ 0 ] = scale[ 1 ] = scale[ 2 ] = 1.0f;
			temp = FloatForKey( e, "modelscale" );
			if( temp != 0.0f )
				scale[ 0 ] = scale[ 1 ] = scale[ 2 ] = temp;
			value = ValueForKey( e, "modelscale_vec" );
			if( value[ 0 ] != '\0' )
				sscanf( value, "%f %f %f", &scale[ 0 ], &scale[ 1 ], &scale[ 2 ] );

			/* get "angle" (yaw) or "angles" (pitch yaw roll) */
			angles[ 0 ] = angles[ 1 ] = angles[ 2 ] = 0.0f;
			angles[ 2 ] = FloatForKey( e, "angle" );
			value = ValueForKey( e, "angles" );
			if( value[ 0 ] != '\0' )
				sscanf( value, "%f %f %f", &angles[ 1 ], &angles[ 2 ], &angles[ 0 ] );

			/* get model */
			picomodel = LoadModel( model, frame );
			if( picomodel == NULL )
				goto scissorentity;
			VectorCopy(picomodel->mins, mins);
			VectorCopy(picomodel->maxs, maxs);

			/* transform */
			m4x4_identity( transform );
			m4x4_pivoted_transform_by_vec3( transform, origin, angles, eXYZ, scale, vec3_origin );
			m4x4_transform_point( transform, mins );
			m4x4_transform_point( transform, maxs );

			/* scissor */
			if( mins[ 0 ] > mapRegionMaxs[ 0 ] || maxs[ 0 ] < mapRegionMins[ 0 ] ||
				mins[ 1 ] > mapRegionMaxs[ 1 ] || maxs[ 1 ] < mapRegionMins[ 1 ] ||
				mins[ 2 ] > mapRegionMaxs[ 2 ] || maxs[ 2 ] < mapRegionMins[ 2 ] )
			{
				removedEntities++;
				removedModels++;
				continue;
			}
			goto keepentity;
		}

		/* scissor bmodel entity */
		if ( e->brushes || e->patches )
		{
			/* calculate entity bounds */
			ClearBounds(mins, maxs);
			for( b = e->brushes; b; b = b->next )
			{
				AddPointToBounds(b->mins, mins, maxs);
				AddPointToBounds(b->maxs, mins, maxs);
			}
			for( p = e->patches; p; p = p->next )
			{
				c = p->mesh.width * p->mesh.height;
				v = p->mesh.verts;
				for( k = 0; k < c; k++, v++ )
					AddPointToBounds( v->xyz, mins, maxs );
			}

			/* adjust by origin */
			GetVectorForKey( e, "origin", origin );
			VectorAdd(mins, origin, mins);
			VectorAdd(maxs, origin, maxs);

			/* scissor */
			if( mins[ 0 ] > mapRegionMaxs[ 0 ] || maxs[ 0 ] < mapRegionMins[ 0 ] ||
				mins[ 1 ] > mapRegionMaxs[ 1 ] || maxs[ 1 ] < mapRegionMins[ 1 ] ||
				mins[ 2 ] > mapRegionMaxs[ 2 ] || maxs[ 2 ] < mapRegionMins[ 2 ] )
			{
				removedEntities++;
				continue;
			}

			goto keepentity;
		}

		/* scissor lights by distance */
		if( !Q_strncasecmp( classname, "light", 5 ) )
		{
			/* get origin */
			GetVectorForKey( e, "origin", origin );

			/* get intensity */
			intensity = FloatForKey( e, "_light" );
			if( intensity == 0.0f )
				intensity = FloatForKey( e, "light" );
			if( intensity == 0.0f)
				intensity = 300.0f;

			/* get light color */
			color[ 0 ] = color[ 1 ] = color[ 2 ] = 1.0f;
			value = ValueForKey( e, "_color" );
			if( value && value[ 0 ] )
				sscanf( value, "%f %f %f", &color[ 0 ], &color[ 1 ], &color[ 2 ] );

			/* get distance */
			temp = 0;
			if (origin[ 0 ] > mapRegionMaxs[ 0 ])
				temp = max(temp, origin[ 0 ] - mapRegionMaxs[ 0 ]);
			if (origin[ 1 ] > mapRegionMaxs[ 1 ])
				temp = max(temp, origin[ 1 ] - mapRegionMaxs[ 1 ]);
			if (origin[ 2 ] > mapRegionMaxs[ 2 ])
				temp = max(temp, origin[ 2 ] - mapRegionMaxs[ 2 ]);
			if (origin[ 0 ] < mapRegionMins[ 0 ])
				temp = max(temp, mapRegionMins[ 0 ] - origin[ 0 ]);
			if (origin[ 1 ] < mapRegionMins[ 1 ])
				temp = max(temp, mapRegionMins[ 1 ] - origin[ 1 ]);
			if (origin[ 2 ] < mapRegionMins[ 2 ])
				temp = max(temp, mapRegionMins[ 2 ] - origin[ 2 ]);

			/* cull */
			temp = intensity / ( temp * temp );
			intensity = max(color[0] * temp, max(color[1] * temp, color[2] * temp));
			if (intensity <= 0.002)
			{
				removedLights++;
				removedEntities++;
				continue;
			}
			goto keepentity;
		}

		/* scissor point entities by origin */
scissorentity:
		GetVectorForKey( e, "origin", origin );
		if( VectorCompare( origin, vec3_origin ) == qfalse ) 
		{
			if( origin[ 0 ] > mapRegionMaxs[ 0 ] || origin[ 0 ] < mapRegionMins[ 0 ] ||
				origin[ 1 ] > mapRegionMaxs[ 1 ] || origin[ 1 ] < mapRegionMins[ 1 ] ||
				origin[ 2 ] > mapRegionMaxs[ 2 ] || origin[ 2 ] < mapRegionMins[ 2 ] )
			{
				removedEntities++;
				continue;
			}
		}

		/* entity is keeped */
keepentity:
		if (scissorEntity != entityNum)
			memcpy(&entities[scissorEntity], &entities[entityNum], sizeof(entity_t));
		scissorEntity++;
	}
	numEntities = scissorEntity;


	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9d brushes removed\n", removedBrushes );
	Sys_FPrintf( SYS_VRB, "%9d patches removed\n", removedPatches );
	Sys_FPrintf( SYS_VRB, "%9d models removed\n", removedModels );
	Sys_FPrintf( SYS_VRB, "%9d lights removed\n", removedLights );
	Sys_FPrintf( SYS_VRB, "%9d entities removed\n", removedEntities );
}

/*
BSPMain() - ydnar
handles creation of a bsp from a map file
*/

int BSPMain( int argc, char **argv )
{
	int			i;
	char		path[ MAX_OS_PATH ], tempSource[ MAX_OS_PATH ];

	/* note it */
	Sys_Printf( "--- BSP ---\n" );
	
	SetDrawSurfacesBuffer();
	mapDrawSurfs = (mapDrawSurface_t *)safe_malloc( sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	memset( mapDrawSurfs, 0, sizeof( mapDrawSurface_t ) * MAX_MAP_DRAW_SURFS );
	numMapDrawSurfs = 0;

	tempSource[ 0 ] = '\0';
	
	/* set standard game flags */
	maxSurfaceVerts = game->maxSurfaceVerts;
	maxLMSurfaceVerts = game->maxLMSurfaceVerts;
	maxSurfaceIndexes = game->maxSurfaceIndexes;
	emitFlares = game->emitFlares;
	Sys_Printf( "--- GameSpecific ---\n" );
	Sys_Printf( " Max surface verts: %i\n" , game->maxSurfaceVerts);
	Sys_Printf( " Max lightmapped surface verts: %i\n" , game->maxLMSurfaceVerts);
	Sys_Printf( " Max surface indexes: %i\n" , game->maxSurfaceIndexes);
	if (emitFlares)
		Sys_Printf( " Emit flares: enabled\n");
	else
		Sys_Printf( " Emit flares: disabled\n");

#if 0
	{
		unsigned int n, numCycles, f, fOld, start;
		vec3_t testVec;
		numCycles = 4000000000;
		Sys_Printf( "--- Test Speeds ---\n" );
		Sys_Printf(  "%9i cycles\n", numCycles );

		/* pass 1 */
		Sys_Printf( "--- Pass 1 ---\n" );
		start = I_FloatTime();
		fOld = -1;
		VectorSet(testVec, 5.0f, 5.0f, 5.0f);
		for( n = 0; n < numCycles; n++ )
		{
			/* print pacifier */
			f = 10 * n / numCycles;
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}

			/* process */
			VectorCompare(testVec, testVec);
		}
		Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );

		/* pass 2 */
		Sys_Printf( "--- Pass 2 ---\n" );
		start = I_FloatTime();
		fOld = -1;
		VectorSet(testVec, 5.0f, 5.0f, 5.0f);
		for( n = 0; n < numCycles; n++ )
		{
			/* print pacifier */
			f = 10 * n / numCycles;
			if( f != fOld )
			{
				fOld = f;
				Sys_FPrintf(SYS_VRB, "%d...", f );
			}

			/* process */
			VectorCompare2(testVec, testVec);
		}
		Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
		Error("***");
	}
#endif


	Sys_Printf( "--- CommandLine ---\n" );
	
	/* process arguments */
	for( i = 1; i < (argc - 1); i++ )
	{
		if( !strcmp( argv[ i ], "-tempname" ) )
			strcpy( tempSource, argv[ ++i ] );
		else if( !strcmp( argv[ i ], "-tmpout" ) )
			strcpy( outbase, "/tmp" );
		else if( !strcmp( argv[ i ],  "-nowater" ) )
		{
			Sys_Printf( "Disabling water\n" );
			nowater = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-nodetail" ) )
		{
			Sys_Printf( "Ignoring detail brushes\n") ;
			nodetail = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-fulldetail" ) )
		{
			Sys_Printf( "Turning detail brushes into structural brushes\n" );
			fulldetail = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-nodetailcollision" ) )
		{
			Sys_Printf( "Disabling collision for detail brushes\n" );
			nodetailcollision = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-nofoliage" ) )
		{
			Sys_Printf( "Disabling foliage\n" );
			nofoliage = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-nodecals" ) )
		{
			Sys_Printf( "Disabling decals\n" );
			nodecals = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-nofog" ) )
		{
			Sys_Printf( "Fog volumes disabled\n" );
			nofog = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-nosubdivide" ) )
		{
			Sys_Printf( "Disabling brush face subdivision\n" );
			nosubdivide = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-noclipmodel" ) )
		{
			Sys_Printf( "Disabling misc_model autoclip feature\n" );
			noclipmodel = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-leaktest" ) )
		{
			Sys_Printf( "Leaktest enabled\n" );
			leaktest = qtrue;
		}
		else if( !strcmp( argv[ i ], "-nocurves" ) )
		{
			Sys_Printf( "Ignoring curved surfaces (patches)\n" );
			noCurveBrushes = qtrue;
		}
		else if( !strcmp( argv[ i ], "-notjunc" ) )
		{
			Sys_Printf( "Disabling T-junction fixing\n" );
			notjunc = qtrue;
		}
		else if( !strcmp( argv[ i ], "-noclip" ) )
		{
			Sys_Printf( "Disabling face clipping by BSP tree\n" );
			noclip = qtrue;
		}
		else if( !strcmp( argv[ i ], "-fakemap" ) )
		{
			Sys_Printf( "Generating fakemap.map\n" );
			fakemap = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-samplesize" ) )
 		{
			sampleSize = atof( argv[ i + 1 ] );
			if( sampleSize < MIN_LIGHTMAP_SAMPLE_SIZE )
				sampleSize = MIN_LIGHTMAP_SAMPLE_SIZE;
 			i++;
			Sys_Printf( "Lightmap sample size set to %fx%f units\n", sampleSize, sampleSize );
 		}
		else if( !strcmp( argv[ i ],  "-custinfoparms") )
		{
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-entitysaveid") )
		{
			Sys_Printf( "Entity unique savegame identifiers enabled\n" );
			useEntitySaveId = qtrue;
		}
		/* sof2 args */
		else if( !strcmp( argv[ i ], "-rename" ) )
		{
			Sys_Printf( "Appending _bsp suffix to misc_model shaders (SOF2)\n" );
			renameModelShaders = qtrue;
		}
		
		/* ydnar args */
		else if( !strcmp( argv[ i ],  "-ne" ) )
 		{
			normalEpsilon = atof( argv[ i + 1 ] );
 			i++;
			Sys_Printf( "Normal epsilon set to %f\n", normalEpsilon );
 		}
		else if( !strcmp( argv[ i ],  "-de" ) )
 		{
			distanceEpsilon = atof( argv[ i + 1 ] );
 			i++;
			Sys_Printf( "Distance epsilon set to %f\n", distanceEpsilon );
 		}
		else if( !strcmp( argv[ i ],  "-mv" ) )
 		{
			maxLMSurfaceVerts = atoi( argv[ i + 1 ] );
			if( maxLMSurfaceVerts < 3 )
				maxLMSurfaceVerts = 3;
			if( maxLMSurfaceVerts > maxSurfaceVerts )
				maxSurfaceVerts = maxLMSurfaceVerts;
 			i++;
			Sys_Printf( "Maximum lightmapped surface vertex count set to %d\n", maxLMSurfaceVerts );
 		}
		else if( !strcmp( argv[ i ],  "-mi" ) )
 		{
			maxSurfaceIndexes = atoi( argv[ i + 1 ] );
			if( maxSurfaceIndexes < 3 )
				maxSurfaceIndexes = 3;
 			i++;
			Sys_Printf( "Maximum per-surface index count set to %d\n", maxSurfaceIndexes );
 		}
		else if( !strcmp( argv[ i ], "-np" ) )
		{
			npDegrees = atof( argv[ i + 1 ] );
			if( npDegrees < 0.0f )
				shadeAngleDegrees = 0.0f;
			else if( npDegrees > 0.0f )
				Sys_Printf( "Forcing nonplanar surfaces with a breaking angle of %f degrees\n", npDegrees );
			i++;
		}
		else if( !strcmp( argv[ i ],  "-snap" ) )
 		{
			bevelSnap = atoi( argv[ i + 1 ]);
			if( bevelSnap < 0 )
				bevelSnap = 0;
 			i++;
			if( bevelSnap > 0 )
				Sys_Printf( "Snapping brush bevel planes to %d units\n", bevelSnap );
 		}
		else if( !strcmp( argv[ i ],  "-texrange" ) )
 		{
			texRange = atoi( argv[ i + 1 ]);
			if( texRange < 0 )
				texRange = 0;
 			i++;
			Sys_Printf( "Limiting per-surface texture range to %d texels\n", texRange );
 		}
		else if( !strcmp( argv[ i ], "-nohint" ) )
		{
			Sys_Printf( "Hint brushes disabled\n" );
			noHint = qtrue;
		}
		else if( !strcmp( argv[ i ], "-flat" ) )
		{
			Sys_Printf( "Flatshading enabled\n" );
			flat = qtrue;
		}
		else if( !strcmp( argv[ i ], "-meta" ) )
		{
			Sys_Printf( "Creating meta surfaces from brush faces\n" );
			meta = qtrue;
		}
		else if( !strcmp( argv[ i ], "-broadmerge" ) )
		{
			broadMerge = qtrue;
			Sys_Printf( "Reducing drawsurfaces count by extensive merging\n" );
		}
		else if( !strcmp( argv[ i ], "-patchmeta" ) )
		{
			Sys_Printf( "Creating meta surfaces from patches\n" );
			patchMeta = qtrue;
		}
		else if( !strcmp( argv[ i ], "-flares" ) )
		{
			Sys_Printf( "Flare surfaces enabled\n" );
			emitFlares = qtrue;
		}
		else if( !strcmp( argv[ i ], "-noflares" ) )
		{
			Sys_Printf( "Flare surfaces disabled\n" );
			emitFlares = qfalse;
		}
		else if( !strcmp( argv[ i ], "-skyfix" ) )
		{
			Sys_Printf( "GL_CLAMP sky fix/hack/workaround enabled\n" );
			skyFixHack = qtrue;
		}
		else if( !strcmp( argv[ i ], "-debugsurfaces" ) )
		{
			Sys_Printf( "emitting debug surfaces\n" );
			debugSurfaces = qtrue;
		}
		else if( !strcmp( argv[ i ], "-debuginset" ) )
		{
			Sys_Printf( "Debug surface triangle insetting enabled\n" );
			debugInset = qtrue;
		}
		else if( !strcmp( argv[ i ], "-debugportals" ) )
		{
			Sys_Printf( "Debug portal surfaces enabled\n" );
			debugPortals = qtrue;
		}
		else if( !strcmp( argv[ i ], "-bsp" ) )
			Sys_Printf( "-bsp argument unnecessary\n" );
		else
			Sys_Printf( "WARNING: Unknown option \"%s\"\n", argv[ i ] );
	}
	
	/* fixme: print more useful usage here */
	if( i != (argc - 1) )
		Error( "usage: q3map [options] mapfile" );
	
	/* copy source name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	
	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );
	
	/* delete portal, line and surface files */
	sprintf( path, "%s.prt", source );
	remove( path );
	sprintf( path, "%s.lin", source );
	remove( path );
	//%	sprintf( path, "%s.srf", source );	/* ydnar */
	//%	remove( path );
	
	/* expand mapname */
	strcpy( name, ExpandArg( argv[ i ] ) );	
	if( !strcmp( name + strlen( name ) - 4, ".reg" ) )
	{
		/* building as region map */
		mapRegion = qtrue;
		mapRegionBrushes = qfalse; // disable region brushes
		Sys_Printf( "Running region compile\n" );
	}
	else
	{
		/* if we are doing a full map, delete the last saved region map */
		sprintf( path, "%s.reg", source );
		remove( path );
		DefaultExtension( name, ".map" );	/* might be .reg */
	}

	/* load shaders */
	LoadShaderInfo();

	/* load original file from temp spot in case it was renamed by the editor on the way in */
	if( strlen( tempSource ) > 0 )
		LoadMapFile( tempSource, qfalse, qfalse );
	else
		LoadMapFile( name, qfalse, qfalse  );

	/* check map for errors */
	CheckMapForErrors();

	/* load up decorations */
	LoadDecorations();

	/* import decorations */
	ImportDecorations( source );

	/* vortex: preload triangle models */
	LoadTriangleModels();

	/* cull stuff for region */
	RegionScissor();
	
	/* vortex: decorator */
	ProcessDecorations();

	/* process decals */
	ProcessDecals();

	/* ydnar: cloned brush model entities */
	SetCloneModelNumbers();
	
	/* process world and submodels */
	ProcessModels();
	
	/* set light styles from targetted light entities */
	SetLightStyles();
	
	/* finish and write bsp */
	EndBSPFile();
	
	/* remove temp map source file if appropriate */
	if( strlen( tempSource ) > 0)
		remove( tempSource );
	
	/* return to sender */
	return 0;
}

