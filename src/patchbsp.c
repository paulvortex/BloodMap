// VorteX: BSP file patch
// remove unused stuff after all stages were processed

/* marker */
#define ENTITIES_C

/* dependencies */
#include "q3map2.h"

void SetCloneModelNumbers(void);

/* globals */
entity_t *mapEntities;
int       numMapEntities;
entity_t *bspEntities;
int       numBspEntities;

/*
AllocateEntity()
*/

entity_t *AllocateEntity(entity_t *base)
{
	entity_t *e;

	if(numEntities >= MAX_MAP_ENTITIES)
		Error("numEntities == MAX_MAP_ENTITIES");

	e = &entities[numEntities];
	memset(e, 0, sizeof(entity_t));
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

void PatchEntities(void)
{
	int i, numents;
	entity_t *e;

	numEntities = 1; /* keep worldspawn */

	/* copy out submodels */
	Sys_Printf("--- PatchEntities ---\n");
	numents = 0;
	for (i = 1; i < numBspEntities; i++)
	{
		e = &bspEntities[i];
		if (strncmp(ValueForKey(e, "model"), "*", 1))
			continue;

		/* add a submodel entity */
		AllocateEntity(e);
		numents++;
	}
	Sys_Printf("%9d submodel entities copied from BSP\n", numents);

	/* replace point entities, skip worldspawn */
	numents = 0;
	for (i = 1; i < numMapEntities; i++)
	{
		e = &mapEntities[i];
		if (!strncmp(ValueForKey(e, "model"), "*", 1))
			continue;

		/* add a point entity */
		AllocateEntity(e);
		numents++;
	}
	Sys_Printf("%9d point entities copied from MAP\n", numents);
	Sys_Printf("%9d entities\n", numEntities);
}


/*
PatchBSPMain()
entities compile
*/

int PatchBSPMain( int argc, char **argv )
{
	char path[1024], out[1024], ekey[1024];
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
	patchentities = qtrue;
	verbose = qfalse;
	strcpy(ekey, "");
	for( i = 1; i < (argc - 1); i++ )
	{
		if( !strcmp( argv[ i ],  "-custinfoparms") )
		{
			Sys_Printf( "Custom info parms enabled\n" );
			useCustomInfoParms = qtrue;
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
			strcpy(ekey, argv[i + 1]);
 			i++;
			if (strcmp(ekey, ""))
				Sys_Printf( "Using entity key \"%s\" as unique entity identifier\n", ekey );
 		}
		else if( !strcmp( argv[ i ],  "-noents" ) )
			Sys_Printf( "Not patching BSP entities\n" );
		else
			Sys_Printf( "WARNING: Unknown option \"%s\"\n", argv[ i ] );
	}
	if (patchentities)
		Sys_Printf( "Patching BSP entities by MAP entities\n" );
	if (!patchentities)
		Error("Nothing to patch\n");

	/* load shaders */
	LoadShaderInfo();

	/* load MAP file */
	Sys_Printf( "--- LoadMap ---\n" );
	LoadMapFile(name, qfalse);
	Sys_Printf( "%9d entities\n", numEntities );

	/* preprocess map */
	Sys_Printf( "--- CompileEntities ---\n" );
	ProcessDecals();
	ProcessDecorations();
	SetModelNumbers();
	SetCloneModelNumbers();
	numBSPEntities = numEntities;
	UnparseEntities(qfalse);
	ParseEntities();
	numMapEntities = numEntities;
	mapEntities = (entity_t *)safe_malloc(sizeof(entity_t) * numMapEntities);
	memcpy(mapEntities, &entities, sizeof(entity_t) * numMapEntities);
	Sys_Printf( "%9d entities\n", numMapEntities );

	/* load BSP file */
	Sys_Printf( "--- LoadBSP ---\n" );
	Sys_Printf( "Loading %s\n", out );
	LoadBSPFile( out );
	ParseEntities();
	numBspEntities = numEntities;
	bspEntities = (entity_t *)safe_malloc(sizeof(entity_t) * numBspEntities);
	memcpy(bspEntities, &entities, sizeof(entity_t) * numBspEntities);
	Sys_Printf( "%9d entities\n", numEntities );
	Sys_Printf( "%9d entDataSize\n", bspEntDataSize );

	/* patch entities */
	if (patchentities)
	{
		PatchEntities();
		/* write entities back */
		numBSPEntities = numEntities;
		UnparseEntities(qfalse);
		Sys_Printf( "%9d entDataSize\n", bspEntDataSize );
	}
	WriteBSPFile( out );

	/* return to sender */
	return 0;
}