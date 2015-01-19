// VorteX: BSP file patch
// remove unused stuff after all stages were processed

/* marker */
#define ENTITIES_C

/* dependencies */
#include "q3map2.h"

void SetCloneModelNumbers(void);

/* globals */
entity_t *patch_mapEntities;
int       patch_numMapEntities;
entity_t *patch_bspEntities;
int       patch_numBspEntities;
char      uniqueEntityKey[1024];
qboolean  patchTriggers;

/*
AllocateEntity()
*/

entity_t *AllocateEntity(entity_t *base)
{
	entity_t *e;

	if( numEntities >= MAX_MAP_ENTITIES )
		Error("numEntities == MAX_MAP_ENTITIES");

	e = &entities[numEntities];
	memset(e, 0, sizeof(entity_t));
	if (base != NULL)
		e->epairs = base->epairs;
	numEntities++;

	return e;
}

/*
PatchEntities()
patch entities in BSP using following rules:
- submodel entities are copied from BSP (not patched at all)
- all point entities are replaces from MAP
Important: this function changes entity spawn order!
*/

entity_t *FindMapEntityByUniqueKey(const char *val)
{
	entity_t *e;
	int i;

	if (!val[0] || !uniqueEntityKey[0])
		return NULL;
	for (i = 1; i < patch_numMapEntities; i++)
	{
		e = &patch_mapEntities[i];
		if (!strcmp(ValueForKey(e, uniqueEntityKey), val))
			return e;
	}
	return NULL;
}

void PatchEntities(void)
{
	entity_t *e, *bspent, *mapent;
	int i, numents, numentsext, numskipents;
	vec3_t mins, maxs;
	const char *val, *model;
	char str[512];
	brush_t *b;
	
	numEntities = 1; /* keep worldspawn */

	/* copy out submodels */
	Sys_Printf("--- PatchEntities ---\n");
	numents = 0;
	numentsext = 0;
	for (i = 1; i < patch_numBspEntities; i++)
	{
		e = &patch_bspEntities[i];
		model = ValueForKey(e, "model");
		if (strncmp(model, "*", 1))
			continue;

		/* patching triggers from map? */
		if (patchTriggers && !strncmp(ValueForKey(e, "classname"), "trigger_", 8))
				continue;

		/* patch bsp entities by saveid */
		if (uniqueEntityKey[0])
		{
			val = ValueForKey(e, uniqueEntityKey);
			if (val && val[0])
			{
				/* find matching map entity */
				mapent = FindMapEntityByUniqueKey(val);
				if (mapent)
				{
					bspent = AllocateEntity( mapent );
					SetKeyValue(bspent, "model", model); // keep model number
					numentsext++;
					continue;
				}
			}
		}

		/* add a submodel entity */
		AllocateEntity( e );
		numents++;
	}
	Sys_Printf("%9d .bsp submodel entities copied\n", numents);
	if (numentsext)
		Sys_Printf("%9d .bsp submodel entities patched\n", numentsext);
	Sys_Printf("%9d .bsp point entities skipped\n", patch_numBspEntities-numents);

	/* replace point entities, skip worldspawn */
	numents = 0;
	numentsext = 0;
	numskipents = 0;
	for (i = 1; i < patch_numMapEntities; i++)
	{
		e = &patch_mapEntities[i];

		/* add a trigger */
		if (patchTriggers && !strncmp(ValueForKey(e, "classname"), "trigger_", 8))
		{
			mapent = AllocateEntity( e );
			/* find bounds */
			ClearBounds( mins, maxs );
			for( b = mapent->brushes; b; b = b->next )
			{
				AddPointToBounds(b->mins, mins, maxs);
				AddPointToBounds(b->maxs, mins, maxs);
			}
			SetKeyValue(mapent, "model", ""); // clear model
			sprintf(str, "%f %f %f", mins[0], mins[1], mins[2]);
			SetKeyValue(mapent, "mins", str);
			sprintf(str, "%f %f %f", maxs[0], maxs[1], maxs[2]);
			SetKeyValue(mapent, "maxs", str);
			numentsext++;
			continue;
		}

		// skip bmodels
		model = ValueForKey(e, "model");
		if (!strncmp(model, "*", 1))
		{
			numskipents++;
			continue;
		}

		/* add a point entity */
		AllocateEntity( e );
		numents++;
	}
	Sys_Printf("%9d .map point entities copied\n", numents);
	if (numentsext)
		Sys_Printf("%9d .map triggers copied\n", numentsext);
	if (numskipents)
		Sys_Printf("%9d .map submodel entities skipped\n", numskipents);
	Sys_Printf("%9d entities\n", numEntities);
}


/*
PatchBSPMain()
entities compile
*/

void RegionScissor( void );
int PatchBSPMain( int argc, char **argv )
{
	char path[MAX_OS_PATH], out[MAX_OS_PATH];
	qboolean patchentities;
	int	i;

	/* arg checking */
	if( argc < 1 )
	{
		Sys_Printf( "Usage: q3map -entities <mapname>\n" );
		return 0;
	}

	/* note it */
	Sys_Printf( "--- PatchBSP ---\n" );

	/* copy source name */
	i = argc - 1;
	strcpy(source, ExpandArg(argv[ i ]));
	StripExtension(source);
	sprintf(out, "%s.bsp", source);
	strcpy(name, ExpandArg(argv[ i ]));	
	if (strcmp( name + strlen( name ) - 4, ".reg" ))
	{
		/* if we are doing a full map, delete the last saved region map */
		sprintf( path, "%s.reg", source );
		remove( path );
		DefaultExtension( name, ".map" );
	}

	/* process arguments */
	Sys_Printf( "--- CommandLine ---\n" );
	patchentities = qtrue;
	verbose = qfalse;
	strcpy(uniqueEntityKey, "");
	patchTriggers = qfalse;
	for( i = 1; i < (argc - 1) && argv[ i ]; i++ )
	{
		if( !strcmp( argv[ i ],  "-entitysaveid") )
		{
			Sys_Printf( " Entity unique savegame identifiers enabled\n" );
			useEntitySaveId = qtrue;
		}
		else if( !strcmp( argv[ i ],  "-source" ) )
 		{
			strcpy(name, argv[i + 1]);
			i++;
		}
		else if( !strcmp( argv[ i ],  "-out" ) )
 		{
			strcpy(out, argv[i + 1]);
			i++;
		}
		else if( !strcmp( argv[ i ],  "-entityid" ) )
 		{
			strcpy(uniqueEntityKey, argv[i + 1]);
 			i++;
			if (strcmp(uniqueEntityKey, ""))
				Sys_Printf( " Using entity key \"%s\" as unique entity identifier\n", uniqueEntityKey );
 		}
		else if( !strcmp( argv[ i ],  "-triggers") )
		{
			Sys_Printf( " Enable patching BSP by map trigger entities\n" );
			patchTriggers = qtrue;
		}
		else
			Sys_Warning( "Unknown option \"%s\"", argv[ i ] );
	}
	if (patchentities)
		Sys_Printf( " Patching BSP entities by MAP entities\n" );
	if (!patchentities)
		Error(" Nothing to patch.\n");

	/* load shaders */
	LoadShaderInfo();

	/* load MAP file */
	Sys_Printf( "--- LoadMapFile ---\n" );
	LoadMapFile( name, qfalse, qfalse, qfalse, qfalse );
	Sys_Printf( "%9d entities\n", numEntities );

	/* check map for errors */
	CheckMapForErrors();

	/* preprocess map */
	Sys_Printf( "--- CompileEntities ---\n" );
	LoadDecorations( source );
	RegionScissor();
	ProcessDecorations();
	ProcessDecals();
	SetModelNumbers();
	SetCloneModelNumbers();
	numBSPEntities = numEntities;
	UnparseEntities(qfalse);
	ParseEntities();
	patch_numMapEntities = numEntities;
	patch_mapEntities = (entity_t *)safe_malloc(sizeof(entity_t) * patch_numMapEntities);
	memcpy(patch_mapEntities, &entities, sizeof(entity_t) * patch_numMapEntities);
	Sys_Printf( "%9d entities\n", patch_numMapEntities );

	/* load BSP file */
	Sys_Printf( "--- LoadBSPFile ---\n" );
	Sys_Printf( "loading %s\n", out );
	LoadBSPFile( out );
	ParseEntities();
	patch_numBspEntities = numEntities;
	patch_bspEntities = (entity_t *)safe_malloc(sizeof(entity_t) * patch_numBspEntities);
	memcpy(patch_bspEntities, &entities, sizeof(entity_t) * patch_numBspEntities);
	Sys_Printf( "%9d entities\n", numEntities );
	Sys_Printf( "%9d entDataSize\n", bspEntDataSize );

	/* patch entities */
	if (patchentities)
	{
		PatchEntities();
		RegionScissor();
		/* write entities back */
		numBSPEntities = numEntities;
		UnparseEntities(qfalse);
		Sys_Printf( "%9d entDataSize\n", bspEntDataSize );
	}
	WriteBSPFile( out );

	/* return to sender */
	return 0;
}