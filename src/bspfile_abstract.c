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
#define BSPFILE_ABSTRACT_C



/* dependencies */
#include "q3map2.h"




/* -------------------------------------------------------------------------------

this file was copied out of the common directory in order to not break
compatibility with the q3map 1.x tree. it was moved out in order to support
the raven bsp format (RBSP) used in soldier of fortune 2 and jedi knight 2.

since each game has its own set of particular features, the data structures
below no longer directly correspond to the binary format of a particular game.

the translation will be done at bsp load/save time to keep any sort of
special-case code messiness out of the rest of the program.

------------------------------------------------------------------------------- */



/* FIXME: remove the functions below that handle memory management of bsp file chunks */

int numBSPDrawVertsBuffer = 0;
void IncDrawVerts()
{
	numBSPDrawVerts++;

	if(bspDrawVerts == 0)
	{
		numBSPDrawVertsBuffer = MAX_MAP_DRAW_VERTS / 37;
		
		bspDrawVerts = (bspDrawVert_t *)safe_malloc_info(sizeof(bspDrawVert_t) * numBSPDrawVertsBuffer, "IncDrawVerts");

	}
	else if(numBSPDrawVerts > numBSPDrawVertsBuffer)
	{
		numBSPDrawVertsBuffer *= 3; // multiply by 1.5
		numBSPDrawVertsBuffer /= 2;

		if(numBSPDrawVertsBuffer > MAX_MAP_DRAW_VERTS)
			numBSPDrawVertsBuffer = MAX_MAP_DRAW_VERTS;

		bspDrawVerts = (bspDrawVert_t *)realloc(bspDrawVerts, sizeof(bspDrawVert_t) * numBSPDrawVertsBuffer);

		if(!bspDrawVerts)
			Error( "realloc() failed (IncDrawVerts)");
	}

	memset(bspDrawVerts + (numBSPDrawVerts - 1), 0, sizeof(bspDrawVert_t));
}

void SetDrawVerts(int n)
{
	if(bspDrawVerts != 0)
		free(bspDrawVerts);

	numBSPDrawVerts = n;
	numBSPDrawVertsBuffer = numBSPDrawVerts;

	bspDrawVerts = (bspDrawVert_t *)safe_malloc_info(sizeof(bspDrawVert_t) * numBSPDrawVertsBuffer, "IncDrawVerts");

	memset(bspDrawVerts, 0, n * sizeof(bspDrawVert_t));
}

int numBSPDrawSurfacesBuffer = 0;
void SetDrawSurfacesBuffer()
{
	if(bspDrawSurfaces != 0)
		free(bspDrawSurfaces);

	numBSPDrawSurfacesBuffer = MAX_MAP_DRAW_SURFS;

	bspDrawSurfaces = (bspDrawSurface_t *)safe_malloc_info(sizeof(bspDrawSurface_t) * numBSPDrawSurfacesBuffer, "IncDrawSurfaces");

	memset(bspDrawSurfaces, 0, MAX_MAP_DRAW_SURFS * sizeof(bspDrawVert_t));
}

void SetDrawSurfaces(int n)
{
	if(bspDrawSurfaces != 0)
		free(bspDrawSurfaces);

	numBSPDrawSurfaces = n;
	numBSPDrawSurfacesBuffer = numBSPDrawSurfaces;

	bspDrawSurfaces = (bspDrawSurface_t *)safe_malloc_info(sizeof(bspDrawSurface_t) * numBSPDrawSurfacesBuffer, "IncDrawSurfaces");

	memset(bspDrawSurfaces, 0, n * sizeof(bspDrawVert_t));
}






/*
SwapBlock()
if all values are 32 bits, this can be used to swap everything
*/

void SwapBlock( int *block, int size )
{
	int		i;
	
	
	/* dummy check */
	if( block == NULL )
		return;
	
	/* swap */
	size >>= 2;
	for( i = 0; i < size; i++ )
		block[ i ] = LittleLong( block[ i ] );
}



/*
SwapBSPFile()
byte swaps all data in the abstract bsp
*/

void SwapBSPFile( void )
{
	int		i, j;
	
	
	/* models */
	SwapBlock( (int*) bspModels, numBSPModels * sizeof( bspModels[ 0 ] ) );

	/* shaders (don't swap the name) */
	for( i = 0; i < numBSPShaders ; i++ )
	{
		bspShaders[ i ].contentFlags = LittleLong( bspShaders[ i ].contentFlags );
		bspShaders[ i ].surfaceFlags = LittleLong( bspShaders[ i ].surfaceFlags );
	}

	/* planes */
	SwapBlock( (int*) bspPlanes, numBSPPlanes * sizeof( bspPlanes[ 0 ] ) );
	
	/* nodes */
	SwapBlock( (int*) bspNodes, numBSPNodes * sizeof( bspNodes[ 0 ] ) );

	/* leafs */
	SwapBlock( (int*) bspLeafs, numBSPLeafs * sizeof( bspLeafs[ 0 ] ) );

	/* leaffaces */
	SwapBlock( (int*) bspLeafSurfaces, numBSPLeafSurfaces * sizeof( bspLeafSurfaces[ 0 ] ) );

	/* leafbrushes */
	SwapBlock( (int*) bspLeafBrushes, numBSPLeafBrushes * sizeof( bspLeafBrushes[ 0 ] ) );

	// brushes
	SwapBlock( (int*) bspBrushes, numBSPBrushes * sizeof( bspBrushes[ 0 ] ) );

	// brushsides
	SwapBlock( (int*) bspBrushSides, numBSPBrushSides * sizeof( bspBrushSides[ 0 ] ) );

	// vis
	((int*) &bspVisBytes)[ 0 ] = LittleLong( ((int*) &bspVisBytes)[ 0 ] );
	((int*) &bspVisBytes)[ 1 ] = LittleLong( ((int*) &bspVisBytes)[ 1 ] );

	/* drawverts (don't swap colors) */
	for( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[ i ].xyz[ 0 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 0 ] );
		bspDrawVerts[ i ].xyz[ 1 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 1 ] );
		bspDrawVerts[ i ].xyz[ 2 ] = LittleFloat( bspDrawVerts[ i ].xyz[ 2 ] );
		bspDrawVerts[ i ].normal[ 0 ] = LittleFloat( bspDrawVerts[ i ].normal[ 0 ] );
		bspDrawVerts[ i ].normal[ 1 ] = LittleFloat( bspDrawVerts[ i ].normal[ 1 ] );
		bspDrawVerts[ i ].normal[ 2 ] = LittleFloat( bspDrawVerts[ i ].normal[ 2 ] );
		bspDrawVerts[ i ].st[ 0 ] = LittleFloat( bspDrawVerts[ i ].st[ 0 ] );
		bspDrawVerts[ i ].st[ 1 ] = LittleFloat( bspDrawVerts[ i ].st[ 1 ] );
		for( j = 0; j < MAX_LIGHTMAPS; j++ )
		{
			bspDrawVerts[ i ].lightmap[ j ][ 0 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 0 ] );
			bspDrawVerts[ i ].lightmap[ j ][ 1 ] = LittleFloat( bspDrawVerts[ i ].lightmap[ j ][ 1 ] );
		}
	}
	
	/* drawindexes */
	SwapBlock( (int*) bspDrawIndexes, numBSPDrawIndexes * sizeof( bspDrawIndexes[0] ) );

	/* drawsurfs */
	/* note: rbsp files (and hence q3map2 abstract bsp) have byte lightstyles index arrays, this follows sof2map convention */
	SwapBlock( (int*) bspDrawSurfaces, numBSPDrawSurfaces * sizeof( bspDrawSurfaces[ 0 ] ) );

	/* fogs */
	for( i = 0; i < numBSPFogs; i++ )
	{
		bspFogs[ i ].brushNum = LittleLong( bspFogs[ i ].brushNum );
		bspFogs[ i ].visibleSide = LittleLong( bspFogs[ i ].visibleSide );
	}
}



/*
GetLumpElements()
gets the number of elements in a bsp lump
*/

int GetLumpElements( bspHeader_t *header, int lump, int size )
{
	/* check for odd size */
	if( header->lumps[ lump ].length % size )
	{
		if( force )
		{
			Sys_Warning( "GetLumpElements: odd lump size (%d) in lump %d", header->lumps[ lump ].length, lump );
			return 0;
		}
		else
			Sys_Error( "GetLumpElements: odd lump size (%d) in lump %d", header->lumps[ lump ].length, lump );
	}
	
	/* return element count */
	return header->lumps[ lump ].length / size;
}



/*
GetLump()
returns a pointer to the specified lump
*/

void *GetLump( bspHeader_t *header, int lump )
{
	return (void*)( (byte*) header + header->lumps[ lump ].offset);
}



/*
CopyLump()
copies a bsp file lump into a destination buffer
*/

int CopyLump( bspHeader_t *header, int lump, void *dest, int size )
{
	int		length, offset;
	
	
	/* get lump length and offset */
	length = header->lumps[ lump ].length;
	offset = header->lumps[ lump ].offset;
	
	/* handle erroneous cases */
	if( length == 0 )
		return 0;
	if( length % size )
	{
		if( force )
		{
			Sys_Warning( "CopyLump: odd lump size (%d) in lump %d\n", length, lump );
			return 0;
		}
		else
			Sys_Error( "CopyLump: odd lump size (%d) in lump %d", length, lump );
	}
	
	/* copy block of memory and return */
	memcpy( dest, (byte*) header + offset, length );
	return length / size;
}



/*
AddLump()
adds a lump to an outgoing bsp file
*/

void AddLump( FILE *file, bspHeader_t *header, int lumpNum, const void *data, int length )
{
	bspLump_t	*lump;
	
	
	/* add lump to bsp file header */
	lump = &header->lumps[ lumpNum ];
	lump->offset = LittleLong( ftell( file ) );
	lump->length = LittleLong( length );
	
	/* write lump to file */
	SafeWrite( file, data, (length + 3) & ~3 );
}



/*
LoadBSPFile()
loads a bsp file into memory
*/

void LoadBSPFile( const char *filename )
{
	/* dummy check */
	if( game == NULL || game->load == NULL )
		Error( "LoadBSPFile: unsupported BSP file format" );
	
	/* load it, then byte swap the in-memory version */
	game->load( filename );
	SwapBSPFile();
}



/*
WriteBSPFile()
writes a bsp file
*/

void WriteBSPFile( const char *filename )
{
	char	tempname[ MAX_OS_PATH ];
	time_t	tm;
	
	
	/* dummy check */
	if( game == NULL || game->write == NULL )
		Error( "WriteBSPFile: unsupported BSP file format" );
	
	/* make fake temp name so existing bsp file isn't damaged in case write process fails */
	time( &tm );
	sprintf( tempname, "%s.%08X", filename, (int) tm );
	
	/* byteswap, write the bsp, then swap back so it can be manipulated further */
	SwapBSPFile();
	game->write( tempname );
	SwapBSPFile();
	
	/* replace existing bsp file */
	remove( filename );
	rename( tempname, filename );
}



/*
PrintBSPFileSizes()
dumps info about current file
*/

void PrintBSPFileSizes( void )
{
	bspModel_t *m;
	bspDrawSurface_t *s;
	bspShader_t *sh;
	int i;

	/* parse entities first */
	if( numEntities <= 0 )
		ParseEntities();
	
	/* note that this is abstracted */
	Sys_Printf( "Abstracted BSP file components (*actual sizes may differ)\n" );
	
	/* print various and sundry bits */
	Sys_Printf( "%9d models        %9d\n",
		numBSPModels, (int) (numBSPModels * sizeof( bspModel_t )) );
	Sys_Printf( "%9d shaders       %9d\n",
		numBSPShaders, (int) (numBSPShaders * sizeof( bspShader_t )) );
	Sys_Printf( "%9d brushes       %9d\n",
		numBSPBrushes, (int) (numBSPBrushes * sizeof( bspBrush_t )) );
	Sys_Printf( "%9d brushsides    %9d *\n",
		numBSPBrushSides, (int) (numBSPBrushSides * sizeof( bspBrushSide_t )) );
	Sys_Printf( "%9d fogs          %9d\n",
		numBSPFogs, (int) (numBSPFogs * sizeof( bspFog_t ) ) );
	Sys_Printf( "%9d planes        %9d\n",
		numBSPPlanes, (int) (numBSPPlanes * sizeof( bspPlane_t )) );
	Sys_Printf( "%9d entdata       %9d\n",
		numEntities, bspEntDataSize );
	Sys_Printf( "\n");
	
	Sys_Printf( "%9d nodes         %9d\n",
		numBSPNodes, (int) (numBSPNodes * sizeof( bspNode_t)) );
	Sys_Printf( "%9d leafs         %9d\n",
		numBSPLeafs, (int) (numBSPLeafs * sizeof( bspLeaf_t )) );
	Sys_Printf( "%9d leafsurfaces  %9d\n",
		numBSPLeafSurfaces, (int) (numBSPLeafSurfaces * sizeof( *bspLeafSurfaces )) );
	Sys_Printf( "%9d leafbrushes   %9d\n",
		numBSPLeafBrushes, (int) (numBSPLeafBrushes * sizeof( *bspLeafBrushes )) );
	Sys_Printf( "\n");
	
	Sys_Printf( "%9d drawsurfaces  %9d *\n",
		numBSPDrawSurfaces, (int) (numBSPDrawSurfaces * sizeof( *bspDrawSurfaces )) );
	Sys_Printf( "%9d drawverts     %9d *\n",
		numBSPDrawVerts, (int) (numBSPDrawVerts * sizeof( *bspDrawVerts )) );
	Sys_Printf( "%9d drawindexes   %9d\n",
		numBSPDrawIndexes, (int) (numBSPDrawIndexes * sizeof( *bspDrawIndexes )) );
	Sys_Printf( "\n");
	
	Sys_Printf( "%9d lightmaps     %9d\n",
		numBSPLightBytes / (game->lightmapSize * game->lightmapSize * 3), numBSPLightBytes );
	Sys_Printf( "%9d lightgrid     %9d *\n",
		numBSPGridPoints, (int) (numBSPGridPoints * sizeof( *bspGridPoints )) );
	Sys_Printf( "          visibility    %9d\n",
		numBSPVisBytes );

	Sys_FPrintf( SYS_VRB, "--- BSP models ---\n" );
	Sys_FPrintf( SYS_VRB, "modelno : brushes  drawsurfs  firstshader\n" );
	for (i = 0; i < numBSPModels; i++)
	{
		m = &bspModels[i];
		s = &bspDrawSurfaces[m->firstBSPSurface];
		sh = &bspShaders[s->shaderNum];
		Sys_FPrintf( SYS_VRB, "*%4i  : %4i   %4i   %s \n", i, m->numBSPBrushes, m->numBSPSurfaces, sh->shader);
	}
}


/* -------------------------------------------------------------------------------

entity data handling

------------------------------------------------------------------------------- */


/*
StripTrailing()
strips low byte chars off the end of a string
*/

void StripTrailing( char *e )
{
	char	*s;
	
	
	s = e + strlen( e ) - 1;
	while( s >= e && *s <= 32 )
	{
		*s = 0;
		s--;
	}
}



/*
ParseEpair()
parses a single quoted "key" "value" pair into an epair struct
*/

epair_t *ParseEPair( void )
{
	epair_t		*e;
	
	
	/* allocate and clear new epair */
	e = (epair_t *)safe_malloc( sizeof( epair_t ) );
	memset( e, 0, sizeof( epair_t ) );
	
	/* handle key */
	if( strlen( token ) >= (MAX_KEY - 1) )
		Error( "ParseEPair: token too long" );
	
	e->key = copystring( token );
	GetToken( qfalse );
	
	/* handle value */
	if( strlen( token ) >= MAX_VALUE - 1 )
		Error( "ParseEpar: token too long" );
	e->value = copystring( token );
	
	/* strip trailing spaces that sometimes get accidentally added in the editor */
	StripTrailing( e->key );
	StripTrailing( e->value );
	
	/* return it */
	return e;
}



/*
ParseEntity()
parses an entity's epairs
*/

qboolean ParseEntity( void )
{
	epair_t		*e;
	
	
	/* dummy check */
	if( !GetToken( qtrue ) )
		return qfalse;
	if( strcmp( token, "{" ) )
		Sys_Error( "ParseEntity: { not found" );
	if( numEntities == MAX_MAP_ENTITIES )
		Sys_Error( "ParseEntity: MAX_MAP_ENTITIES (%d) exceeded", MAX_MAP_ENTITIES );
	
	/* create new entity */
	mapEnt = &entities[ numEntities ];
	memset(mapEnt, 0, sizeof(entity_t));
	numEntities++;
	
	/* parse */
	while( 1 )
	{
		if( !GetToken( qtrue ) )
			Error( "ParseEntity: EOF without closing brace" );
		if( !EPAIR_STRCMP( token, "}" ) )
			break;
		e = ParseEPair();
		e->next = mapEnt->epairs;
		mapEnt->epairs = e;
	}

	/* vortex: restore mapEntityNum */
	mapEnt->mapEntityNum = IntForKey( mapEnt, "_mapEntityNum" );
	
	/* return to sender */
	return qtrue;
}



/*
ParseEntities()
parses the bsp entity data string into entities
*/

void ParseEntities( void )
{
	numEntities = 0;
	ParseFromMemory( bspEntData, bspEntDataSize );
	while( ParseEntity() );
	
	/* ydnar: set number of bsp entities in case a map is loaded on top */
	numBSPEntities = numEntities;
}



/*
UnparseEntities()
generates the dentdata string from all the entities.
this allows the utilities to add or remove key/value
pairs to the data created by the map editor
*/

void UnparseEntities( qboolean removeQ3map2keys )
{
	int			i;
	char		*buf, *end;
	epair_t		*ep;
	char		line[ 2048 ];
	char		key[ 1024 ], value[ 1024 ];
	const char	*value2;
	
	/* setup */
	buf = bspEntData;
	end = buf;
	*end = 0;
	
	/* run through entity list */
	for( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* get epair */
		ep = entities[ i ].epairs;
		if( ep == NULL )
			continue;	/* ent got removed */
		
		/* ydnar: certain entities get stripped from bsp file */
		/* vortex: misc_models are only stripped if they are not forced submodel */
		value2 = ValueForKey( &entities[ i ], "classname" );
		if( !Q_stricmp( value2, "misc_model" ) ||
			!Q_stricmp( value2, "_decal" ) || !Q_stricmp( value2, "misc_decal" ) ||
			!Q_stricmp( value2, "_skybox" ) || !Q_stricmp( value2, "misc_skybox"  ))
			continue;

		/* add beginning brace */
		strcat( end, "{\n" );
		end += 2;
		
		/* walk epair list */
		for( ep = entities[ i ].epairs; ep != NULL; ep = ep->next )
		{
			if( removeQ3map2keys && !strncmp(ep->key, "_", 1) )
				continue;

			/* copy and clean */
			strcpy( key, ep->key );
			StripTrailing( key );
			strcpy( value, ep->value );
			StripTrailing( value );
			
			/* add to buffer */
			sprintf( line, "\"%s\" \"%s\"\n", key, value );
			strcat( end, line );
			end += strlen( line );
		}
		
		/* add trailing brace */
		strcat( end,"}\n" );
		end += 2;
		
		/* check for overflow */
		if( end > buf + MAX_MAP_ENTSTRING )
			Error( "Entity text too long" );
	}
	
	/* set size */
	bspEntDataSize = end - buf + 1;
}



/*
PrintEntity()
prints an entity's epairs to the console
*/

void PrintEntity( const entity_t *ent )
{
	epair_t	*ep;
	

	Sys_Printf( "------- entity %p -------\n", ent );
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
		Sys_Printf( "%s = %s\n", ep->key, ep->value );

}



/*
SetKeyValue()
sets an epair in an entity
*/

void SetKeyValue( entity_t *ent, const char *key, const char *value )
{
	epair_t	*ep;
	
	
	/* check for existing epair */
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if( !EPAIR_STRCMP( ep->key, key ) )
		{
			free( ep->value );
			ep->value = copystring( value );
			return;
		}
	}
	
	/* create new epair */
	ep = (epair_t *)safe_malloc( sizeof( *ep ) );
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = copystring( key );
	ep->value = copystring( value );
}

/*
KeyExists()
returns true if entity has this key
*/

qboolean KeyExists( const entity_t *ent, const char *key )
{
	epair_t	*ep;
	
	/* walk epair list */
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if( !EPAIR_STRCMP( ep->key, key ) )
			return qtrue;
	}

	/* no match */
	return qfalse;
}

/*
ValueForKey()
gets the value for an entity key
*/

const char *ValueForKey( const entity_t *ent, const char *key )
{
	epair_t	*ep;
	
	
	/* dummy check */
	if( ent == NULL )
		return "";
	
	/* walk epair list */
	for( ep = ent->epairs; ep != NULL; ep = ep->next )
	{
		if( !EPAIR_STRCMP( ep->key, key ) )
			return ep->value;
	}
	
	/* if no match, return empty string */
	return "";
}



/*
IntForKey()
gets the integer point value for an entity key
*/

int IntForKey( const entity_t *ent, const char *key )
{
	const char	*k;
	
	
	k = ValueForKey( ent, key );
	return atoi( k );
}



/*
FloatForKey()
gets the floating point value for an entity key
*/

vec_t FloatForKey( const entity_t *ent, const char *key )
{
	const char	*k;
	
	
	k = ValueForKey( ent, key );
	return atof( k );
}



/*
GetVectorForKey()
gets a 3-element vector value for an entity key
*/

void GetVectorForKey( const entity_t *ent, const char *key, vec3_t vec )
{
	const char	*k;
	double		v1, v2, v3;
	

	/* get value */
	k = ValueForKey( ent, key );
	
	/* scanf into doubles, then assign, so it is vec_t size independent */
	v1 = v2 = v3 = 0.0;
	sscanf( k, "%lf %lf %lf", &v1, &v2, &v3 );
	vec[ 0 ] = v1;
	vec[ 1 ] = v2;
	vec[ 2 ] = v3;
}



/*
FindTargetEntity()
finds an entity target
*/

entity_t *FindTargetEntity( const char *target )
{
	int			i;
	const char	*n;

	
	/* walk entity list */
	for( i = 0; i < numEntities; i++ )
	{
		n = ValueForKey( &entities[ i ], "targetname" );
		if ( !strcmp( n, target ) )
			return &entities[ i ];
	}
	
	/* nada */
	return NULL;
}

/*
GetEntityShadowFlags() - ydnar
gets an entity's shadow flags
note: does not set them to defaults if the keys are not found!
*/

void GetEntityShadowFlags( const entity_t *ent, const entity_t *ent2, char *castShadows, char *recvShadows, qboolean isWorldspawn )
{
	const char	*value;
	
	/* get cast shadows */
	if( castShadows != NULL )
	{
		/* default shadow cast groups */
		if( isWorldspawn == qtrue )
			*castShadows = WORLDSPAWN_CAST_SHADOWS;
		else  
			*castShadows = ENTITY_CAST_SHADOWS;

		/* explicit shadow cast group */
		value = ValueForKey( ent, "_castShadows" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( ent, "_cs" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( ent2, "_castShadows" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( ent2, "_cs" );
		if( value[ 0 ] != '\0' )
			*castShadows = atoi( value );
	}
	
	/* receive */
	if( recvShadows != NULL )
	{
		/* default shadow receive groups */
		if( isWorldspawn == qtrue )
			*recvShadows = WORLDSPAWN_RECV_SHADOWS;
		else
			*recvShadows = ENTITY_RECV_SHADOWS;

		/* explicit shadow receive group */
		value = ValueForKey( ent, "_receiveShadows" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( ent, "_rs" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( ent2, "_receiveShadows" );
		if( value[ 0 ] == '\0' )
			value = ValueForKey( ent2, "_rs" );
		if( value[ 0 ] != '\0' )
			*recvShadows = atoi( value );
	}
}

/*
GetEntityLightmapScale() - vortex
gets an entity's lightmap scale
*/

void GetEntityLightmapScale( const entity_t *ent, float *lightmapScale, float baseScale )
{
	*lightmapScale = baseScale;
	if( KeyExists( ent, "_ls" ) )
		*lightmapScale = FloatForKey( ent, "_ls" );
	if( KeyExists( ent, "_lightmapscale" ) )
		*lightmapScale = FloatForKey( ent, "_lightmapscale" );
	if( KeyExists( ent, "lightmapscale" ) )
		*lightmapScale = FloatForKey( ent, "lightmapscale" );

	/* fix negative */
	if( *lightmapScale <= 0.0f )
		*lightmapScale = 0.0f;
}

/*
GetEntityLightmapAxis() - vortex
gets an entity's lightmap axis
*/

void GetEntityLightmapAxis( const entity_t *ent, vec3_t lightmapAxis, vec3_t baseAxis )
{
	const char *value;
	int a;

	value = NULL;
	if( KeyExists( ent, "_la" ) )
		value = ValueForKey( ent, "_la" );
	if( KeyExists( ent, "_lightmapaxis" ) )
		value = ValueForKey( ent, "_lightmapaxis" );
	if( KeyExists( ent, "lightmapaxis" ) )
		value = ValueForKey( ent, "lightmapaxis" );

	/* get axis */
	if( value == NULL )
	{
		if (baseAxis == NULL)
			VectorClear( lightmapAxis );
		else
			VectorCopy( baseAxis, lightmapAxis );
		return;
	}
	if( !Q_stricmp( value, "z" ) )
		VectorSet( lightmapAxis, 0, 0, 1 );
	else if( !Q_stricmp( value, "-z" ) )
		VectorSet( lightmapAxis, 0, 0, -1 );
	else if( !Q_stricmp( value, "y" ) )
		VectorSet( lightmapAxis, 0, 1, 0 );
	else if( !Q_stricmp( value, "-y" ) )
		VectorSet( lightmapAxis, 0, -1, 0 );
	else if( !Q_stricmp( value, "x" ) )
		VectorSet( lightmapAxis, 1, 0, 0 );
	else if( !Q_stricmp( value, "-x" ) )
		VectorSet( lightmapAxis, -1, 0, 0 );
	else
	{
		a = sscanf_s(value, "%f %f %f", &lightmapAxis[0], &lightmapAxis[1], &lightmapAxis[2]);
		if (a != 3)
		{
			if (baseAxis == NULL)
				VectorClear( lightmapAxis );
			else
				VectorCopy( baseAxis, lightmapAxis );
		}
		else
			VectorNormalize( lightmapAxis, lightmapAxis );
	}
}

/*
GetEntityNormalSmoothing() - vortex
gets an entity's normal smoothing
*/

void GetEntityNormalSmoothing( const entity_t *ent, int *smoothNormals, int baseSmoothing )
{
	*smoothNormals = baseSmoothing;
	if( KeyExists( ent, "_np" ) )
		*smoothNormals = IntForKey(ent, "_np");
	if( KeyExists( ent, "_smoothnormals" ) )
		*smoothNormals  = IntForKey(ent, "_smoothnormals");
}

/*
GetEntityMinlightAmbientColor() - vortex
gets an entity's minlight, ambient light, color mod
*/

void GetEntityMinlightAmbientColor( const entity_t *ent, vec3_t color, vec3_t minlight, vec3_t minvertexlight, vec3_t ambient, vec3_t colormod, qboolean setDefaults )
{
	vec3_t _color;
	float f;

	/* find the optional _color key (used for _ambient and _minlight) */
	GetVectorForKey( ent, "_color", _color );
	if( VectorIsNull( _color ) )
		VectorSet( _color, 1.0, 1.0, 1.0 );
	if( color != NULL )
		VectorCopy( _color, color );

	/* minlight/_minlight key */
	if( minlight != NULL )
	{
		if( setDefaults == qtrue )
			VectorSet( minlight, 0, 0, 0 );
		if( KeyExists( ent, "minlight" ) )
		{
			if( sscanf( ValueForKey( ent, "minlight"), "%f %f %f", &minlight[0], &minlight[1], &minlight[2] ) != 3 )
			{
				f = FloatForKey( ent, "minlight" );
				VectorScale( _color, f, minlight );
			}
			if( minvertexlight != NULL )
				VectorCopy( minlight, minvertexlight );
		}
		if( KeyExists( ent, "_minlight" ) )
		{
			if( sscanf( ValueForKey( ent, "_minlight"), "%f %f %f", &minlight[0], &minlight[1], &minlight[2] ) != 3 )
			{
				f = FloatForKey( ent, "_minlight" );
				VectorScale( _color, f, minlight );
			}
			if( minvertexlight != NULL )
				VectorCopy( minlight, minvertexlight );
		}
	}

	/* minvertexlight/_minvertexlight key */
	if( minvertexlight != NULL )
	{
		if( setDefaults == qtrue )
			VectorSet( minvertexlight, 0, 0, 0 );
		if( KeyExists( ent, "minvertexlight" ) )
		{
			if( sscanf( ValueForKey( ent, "minvertexlight"), "%f %f %f", &minvertexlight[0], &minvertexlight[1], &minvertexlight[2] ) != 3 )
			{
				f = FloatForKey( ent, "minvertexlight" );
				VectorScale( _color, f, minvertexlight );
			}
		}
		if( KeyExists( ent, "_minvertexlight" ) )
		{
			if( sscanf( ValueForKey( ent, "_minvertexlight"), "%f %f %f", &minvertexlight[0], &minvertexlight[1], &minvertexlight[2] ) != 3 )
			{
				f = FloatForKey( ent, "_minvertexlight" );
				VectorScale( _color, f, minvertexlight );
			}
		}
	}

	/* ambient/_ambient key */
	if( ambient != NULL )
	{
		if( setDefaults == qtrue )
			VectorSet( ambient, 0, 0, 0 );
		if( KeyExists( ent, "ambient" ) )
		{
			if( sscanf( ValueForKey( ent, "ambient"), "%f %f %f", &ambient[0], &ambient[1], &ambient[2] ) != 3 )
			{
				f = FloatForKey( ent, "ambient" );
				VectorScale( _color, f, ambient );
			}
		}
		if( KeyExists( ent, "_ambient" ) )
		{
			if( sscanf( ValueForKey( ent, "_ambient"), "%f %f %f", &ambient[0], &ambient[1], &ambient[2] ) != 3 )
			{
				f = FloatForKey( ent, "_ambient" );
				VectorScale( _color, f, ambient );
			}
		}
	}
	
	/* colormod/_colormod key */
	if( colormod != NULL )
	{
		if( setDefaults == qtrue )
			VectorSet( colormod, 1, 1, 1 );
		if( KeyExists( ent, "_colormod" ) )
		{
			if( sscanf( ValueForKey( ent, "_colormod"), "%f %f %f", &colormod[0], &colormod[1], &colormod[2] ) != 3 )
			{
				f = FloatForKey( ent, "_colormod" );
				VectorSet( colormod, f, f, f );
			}
		}
	}

	/* convert from sRGB to linear colorspace */
	if ( colorsRGB ) 
	{
		if( color != NULL )
		{
			color[0] = srgb_to_linear( color[0] );
			color[1] = srgb_to_linear( color[1] );
			color[2] = srgb_to_linear( color[2] );
		}
		if( minlight != NULL )
		{
			minlight[0] = srgb_to_linear( minlight[0] );
			minlight[1] = srgb_to_linear( minlight[1] );
			minlight[2] = srgb_to_linear( minlight[2] );
		}
		if( minvertexlight != NULL )
		{
			minvertexlight[0] = srgb_to_linear( minvertexlight[0] );
			minvertexlight[1] = srgb_to_linear( minvertexlight[1] );
			minvertexlight[2] = srgb_to_linear( minvertexlight[2] );
		}
		if( ambient != NULL )
		{
			ambient[0] = srgb_to_linear( ambient[0] );
			ambient[1] = srgb_to_linear( ambient[1] );
			ambient[2] = srgb_to_linear( ambient[2] );
		}
		if( colormod != NULL )
		{
			colormod[0] = srgb_to_linear( colormod[0] );
			colormod[1] = srgb_to_linear( colormod[1] );
			colormod[2] = srgb_to_linear( colormod[2] );
		}
	}
}

/*
GetEntityPatchMeta() - vortex
gets an entity's patch meta settings
*/

void GetEntityPatchMeta( const entity_t *ent, qboolean *forceMeta, float *patchQuality, float *patchSubdivision, float baseQuality, float baseSubdivisions )
{
	/* vortex: _patchMeta */
	*forceMeta = IntForKey(ent, "_patchMeta" ) > 0 ? qtrue : qfalse;
	if (!*forceMeta)
		*forceMeta = IntForKey(ent, "patchMeta" ) > 0 ? qtrue : qfalse;
	if (!*forceMeta)
		*forceMeta = IntForKey(ent, "_pm" ) > 0 ? qtrue : qfalse;
	/* vortex: _patchQuality */
	if( patchQuality )
	{
		*patchQuality = FloatForKey(ent, "_patchQuality" );
		if (*patchQuality == 0)
			*patchQuality = FloatForKey(ent, "patchQuality" );
		if (*patchQuality == 0)
			*patchQuality = FloatForKey(ent, "_pq" );
		if (*patchQuality == 0)
			*patchQuality = baseQuality;
		if (*patchQuality == 0)
			*patchQuality = 1.0;
	}
	/* vortex: _patchSubdivide */
	if( patchSubdivision )
	{
		*patchSubdivision = FloatForKey(ent, "_patchSubdivide" );
		if (*patchSubdivision == 0)
			*patchSubdivision = FloatForKey(ent, "patchSubdivide" );
		if (*patchSubdivision == 0)
			*patchSubdivision = FloatForKey(ent, "_ps" );
		if (*patchSubdivision == 0)
			*patchSubdivision = baseSubdivisions;
		if (*patchSubdivision == 0)
			*patchSubdivision = patchSubdivisions;
	}
}