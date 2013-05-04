// custom decorations system
// used for creating nifty things like foliage under trees

#include "q3map2.h"

/* marker */
#define DECORATIONS_C

#define MAX_DECORE_GROUPS		128
#define MAX_DECORE_ACTIONS		64

typedef enum
{
	DECOREACTION_KEY_DEFAULT,	
	DECOREACTION_KEY_SET,		
	DECOREACTION_KEY_SETFLAG,	
	DECOREACTION_KEY_UNSETFLAG,
	DECOREACTION_KEY_TOGGLEFLAG,
}dactioncode_t;

typedef struct decoreaction_s
{
	dactioncode_t	code;			// action type
	char			parm[ 128 ];
	double			value;
	char			data[ 256 ];
}daction_t;

// node stuff
typedef struct dnode_s
{
	void *ptr; // pointer to object
	char *name;
	void *next;
	void *child;
}dnode_t;

// actions
typedef struct dactions_s
{
	daction_t	action[ MAX_DECORE_ACTIONS ];
	int			num;
}dactions_t;

// group match
typedef enum
{
	GROUPMATCH_GROUP,
	GROUPMATCH_CLASSNAME,
	GROUPMATCH_MODEL
}dmatch_t;

// group general struct
typedef struct dgroup_s
{
	// name of group
	char		name[ 1024 ]; 
	dmatch_t    match;
	// entity actions
	dactions_t	*actions;
	// misc_model merging
	int			mergeModels;
	int			mergeRadius;
	char		mergeClass[ 256 ];
	// entities
	dnode_t		*entities;
	int			numEntities;
}dgroup_t;

dgroup_t *decoreGroups Q_ASSIGN( NULL );
int numDecoreGroups Q_ASSIGN ( 0 );
int numDecoreEntities Q_ASSIGN ( 0 );

/* options */
#define MAX_SKIPSTYLES	128
qboolean	importRtlights = qfalse;
qboolean	importRtlightsSkipCubemapped = qfalse;
float		importRtlightsColorscale = 1;
float		importRtlightsSaturate = 1;
float		importRtlightsRadiusmod = 0.5;
int			importRtlightsSpawnflags = 0;
float		importRtlightsNoShadowDeviance = 0.1;
float		importRtlightsNoShadowSamples = 0.1;
float		importRtlightsNoShadowMinDist = 0.5;
float		importRtlightsShadowDeviance = 0.0;
float		importRtlightsShadowSamples = 0.0;
float		importRtlightsShadowMinDist = 0.1;
int			importRtlightsSkipStyles[MAX_SKIPSTYLES];
int			importRtlightsSkipStylesNum = 0;

/*
=========================================================

 NODES STUFF

=========================================================
*/

void PrintNode_f(dnode_t *node)
{
	dnode_t *c;

	// node + children
	Sys_Printf("node");
	if (node->child != NULL)
	{
		Sys_Printf("{");
		PrintNode_f(node->child);
		Sys_Printf("}");
	}
	// next nodes
	if (node->next != NULL)
	{
		for (c = node->next; c != NULL; c = c->next)
		{
			Sys_Printf(".next");
			if (c->child != NULL)
			{
				Sys_Printf("{");
				PrintNode_f(c->child);
				Sys_Printf("}");
			}
		}
	}
}

// debug stuff
void PrintNode(dnode_t *node)
{
	if (!node)
		Sys_Printf("{null node}");
	else
		Sys_Printf(">>");
	PrintNode_f(node);
	Sys_Printf("\n");
}

dnode_t *NewNodesArray(int size)
{
	dnode_t *nodes, *node;
	int i;

	nodes = safe_malloc( sizeof(dnode_t) * size );
	for (i = 0; i < size; i++)
	{
		node = &nodes [ i ];
		memset(node, 0, sizeof(dnode_t));
		node->ptr = NULL;
		node->child = NULL;
		node->next = NULL;
	}
	return nodes;
}

void FreeNode(dnode_t *node)
{
	dnode_t *c, *next;

	if (!node)
		return;
	
	// free child node
	if (node->child != NULL)
		FreeNode(node->child);
	// free all next items
	if (node->next != NULL)
	{
		for (c = node->next; c != NULL; c = next)
		{
			if (c->child != NULL)
				FreeNode(c->child);
			next = c->next;
			free(c);
		}
	}
	// free this node
	free(node);
}

void FreeNodesArray(dnode_t *nodes, int size)
{
	dnode_t *node;
	int i;

	for (i = 0; i < size; i++)
	{
		node = &nodes [ i ];
		if ( node->next != NULL )
			FreeNode( node->next );
		if ( node->child != NULL )
			FreeNode( node->child );
	}
}

dnode_t *NewNode(char *name, void *ptr)
{	
	dnode_t *node;

	node = safe_malloc(sizeof(dnode_t));
	memset(node, 0, sizeof(dnode_t));
	node->name = name;
	node->ptr = ptr;
	node->child = NULL;
	node->next = NULL;

	return node;
}

// add node in the end of chain of current node
void AddNextNode(dnode_t *node, char *name, void *ptr)
{
	dnode_t *n;

	n = NewNode(name, ptr);
	while(1)
	{
		if (node->next == NULL)
			break;
		node = node->next;
	}
	node->next = n;
}

// add child node
void AddChildNode(dnode_t *node, char *name, void *ptr)
{
	dnode_t *n;

	n = NewNode(name, ptr);
	if (node->child != NULL) // add a next after last child
	{
		node = node->child;
		while(1)
		{
			if (node->next == NULL)
				break;
			node = node->next;
		}
		node->next = n;
		return;
	}
	node->child = n;
}

// add node to random position
void AddNextNodeRandom(dnode_t *node, char *name, void *ptr)
{
	int numnodes = 0, num;
	dnode_t *last, *n;

	n = NewNode(name, ptr);

	/* get all nodes */
	for(last = node; last->next != NULL; last = last->next)
		numnodes++;

	/* select random node */
	num = (int)(((double)(rand() + 0.5) / ((double)RAND_MAX + 1)) * (numnodes + 0.5));
	for (last = node; num > 0; num--)
		last = last->next;

	/* add new node right after selected node */
	n->next = last->next;
	last->next = n;
}

/*
=========================================================

 DECORATOR

=========================================================
*/

/*
NewDecoreGroup()
allocate new empty decore group and return it
*/

dgroup_t *NewDecoreGroup(char *name, dmatch_t match)
{
	dgroup_t *group;
	int i;

	/* range check */
	if( numDecoreGroups >= MAX_DECORE_GROUPS )
		Error( "NewDecoreGroup: numDecoreGroups == MAX_DECORE_GROUPS" );

	/* redefinition check */
	for ( i = 0; i < numDecoreGroups; i++ )
	{
		group = &decoreGroups[ i ];
		if ( !Q_stricmp( group->name, name ) )
			Error( "NewDecoreGroup: %s redefined\n" , name );
	}

	/* set and return */
	group = &decoreGroups[numDecoreGroups];	
	group->match = match;
	group->entities = NULL;
	group->actions = NULL;
	group->mergeRadius = 128;
	strcpy( group->name, name );
	strcpy( group->mergeClass, "" );

	numDecoreGroups++;
	return group;
}

/*
PopulateDecoreGroup()
get a decoration group for entity
returns NULL if no group
*/

qboolean PopulateDecoreGroup(entity_t *e, dmatch_t match, char *keyname)
{
	const char *value;
	dgroup_t *group;
	int	i;

	/* get key */
	value = ValueForKey( e, keyname );
	if( value[ 0 ] == '\0' )
		return qfalse;

	/* find group */
	for (i = 0; i < numDecoreGroups; i++)
	{
		group = &decoreGroups[i];
		if (group->match == match && !Q_stricmp( value, group->name) )
		{
			if (group->entities == NULL)
				group->entities = NewNode("", e);
			else
				AddNextNode(group->entities, "", e );
			group->numEntities++;
			return qtrue;
		}
	}
	return qfalse;
}

/*
FreeDecoreGroup()
free all allocated fiels on decoregroup struct
*/

void FreeDecoreGroup( dgroup_t *group )
{
	// free ents
	if ( group->entities != NULL )
		FreeNode( group->entities );
}

/*
DecoreParseParm()
finds all entities with _decore groups and process them
*/

char *DecoreParseParm(char *opname, int parmnum)
{
	if ( !GetToken( qfalse ) )
		Sys_Printf( "%s: unexpected end of line on line %i", opname, token, scriptline);
	if ( token [ 0 ] == '\0' )
		Sys_Printf( "%s: bad parm %i on line %i", opname, parmnum, token, scriptline);
	return token;
}

// parse actions sequence
void DecoreParseActions ( dactions_t *actions )
{
	daction_t actiondata;

	MatchToken( "{" );

	while ( 1 )
	{
		if ( !GetToken( qtrue ) )
		{
			Error( "DecoreParseActions: unexpected end of line at line %i\n", scriptline );
			break;
		}

		/* { is not allowed */
		if( !Q_stricmp( token, "{" ) )
		{
			Error( "DecoreParseActions: unexpected { at line %i\n", scriptline );
			break;
		}

		/* end? */
		if( !Q_stricmp( token, "}" ) )
			break;

		/* check limits */
		if (actions->num >= MAX_DECORE_ACTIONS)
			Error( "DecoreParseActions: numActions == MAX_DECORE_ACTIONS at line %i", scriptline );

		/* nullify previous action */
		memset(&actiondata, 0, sizeof(daction_t));
		actiondata.code = -1;

		/* parse new action */
		if( !Q_stricmp( token, "default" ) )
		{
			actiondata.code = DECOREACTION_KEY_DEFAULT;
			strcpy( actiondata.parm, DecoreParseParm("default", 1) );
			strcpy( actiondata.data, DecoreParseParm("default", 2) );
		}
		else if ( !Q_stricmp( token, "set" ) )
		{
			actiondata.code = DECOREACTION_KEY_SET;
			strcpy( actiondata.parm, DecoreParseParm("set", 1) );
			strcpy( actiondata.data, DecoreParseParm("set", 2) );
		}
		else if ( !Q_stricmp( token, "flagset" ) )
		{
			actiondata.code = DECOREACTION_KEY_SETFLAG;
			strcpy( actiondata.parm, DecoreParseParm("flagset", 1) );
			actiondata.value = (double)(atoi( DecoreParseParm("flagset", 2) ));
		}
		else if ( !Q_stricmp( token, "flagunset" ) )
		{
			actiondata.code = DECOREACTION_KEY_UNSETFLAG;
			strcpy( actiondata.parm, DecoreParseParm("flagunset", 1) );
			actiondata.value = (double)(atoi( DecoreParseParm("flagunset", 2) ));
		}
		else if ( !Q_stricmp( token, "flagtoggle" ) )
		{
			actiondata.code = DECOREACTION_KEY_TOGGLEFLAG;
			strcpy( actiondata.parm, DecoreParseParm("flagtoggle", 1) );
			actiondata.value = (double)(atoi( DecoreParseParm("flagtoggle", 2) ));
		}
		
		/* add action */
		if (actiondata.code >= 0)
		{
			memcpy(&actions->action[actions->num], &actiondata, sizeof(daction_t));
			actions->num++;
		}

		/* skip rest of line */
		while (TokenAvailable())
			GetToken( qfalse );
	}
}

// parse qboolean out of string
qboolean BooleanFromString(const char *str)
{
	if( !Q_stricmp( str, "yes" ) || !Q_stricmp( str, "true" ) || !Q_stricmp( str, "on" ) || !Q_stricmp( str, "1" ) )
		return qtrue;
	return qfalse;
}

// parse options
void DecoreParseOptions( void )
{
	int numbrace;

	numbrace = 1;
	while ( 1 )
	{
		if ( !GetToken( qtrue ) )
			break;
		if( !Q_stricmp( token, "{" ) )
		{
			numbrace++;
			continue;
		}
		if( !Q_stricmp( token, "}" ) )
		{
			numbrace--;
			if (numbrace <= 0)
				break;
			continue;
		}

		/* options */
		if( !Q_stricmp( token, "importrtlights" ) )
		{
			MatchToken( "{" );
			while ( 1 )
			{
				if ( !GetToken( qtrue ) )
					break;	
				if( !Q_stricmp( token, "}" ) )
					break;
				
				if( !Q_stricmp( token, "active" ) )
				{
					GetToken( qfalse );
					importRtlights = BooleanFromString( token );
					continue;
				}
				if( !Q_stricmp( token, "colorscale" ) )
				{
					GetToken( qfalse );
					importRtlightsColorscale = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "saturate" ) )
				{
					GetToken( qfalse );
					importRtlightsSaturate = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "radiusmod" ) )
				{
					GetToken( qfalse );
					importRtlightsRadiusmod = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "spawnflags" ) )
				{
					GetToken( qfalse );
					importRtlightsSpawnflags = atoi( token );
					continue;
				}
				if( !Q_stricmp( token, "noshadowdeviance" ) )
				{
					GetToken( qfalse );
					importRtlightsNoShadowDeviance = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "noshadowsamples" ) )
				{
					GetToken( qfalse );
					importRtlightsNoShadowSamples = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "noshadowmindist" ) )
				{
					GetToken( qfalse );
					importRtlightsNoShadowMinDist = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "shadowdeviance" ) )
				{
					GetToken( qfalse );
					importRtlightsShadowDeviance = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "shadowsamples" ) )
				{
					GetToken( qfalse );
					importRtlightsShadowSamples = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "shadowmindist" ) )
				{
					GetToken( qfalse );
					importRtlightsShadowMinDist = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "skipcubemapped" ) )
				{
					GetToken( qfalse );
					if ( atoi( token ) )
						importRtlightsSkipCubemapped = qtrue;
					else
						importRtlightsSkipCubemapped = qfalse;
					continue;
				}
				if( !Q_stricmp( token, "skipstyle" ) )
				{
					GetToken( qfalse );
					if (importRtlightsSkipStylesNum >= MAX_SKIPSTYLES)
						Error( "DecoreParseActions: SkipStylesNum == MAX_SKIPSTYLES at line %i", scriptline );
					importRtlightsSkipStyles[importRtlightsSkipStylesNum] = atoi( token );
					importRtlightsSkipStylesNum++;
					continue;
				}
			}
		}
	}
}

void LoadDecorationScript( void )
{
	dgroup_t *group;
	int i;

	/* file exists? */
	if( vfsGetFileCount( "scripts/decorations.txt" ) == 0 )
		return;

	/* note */
	Sys_FPrintf( SYS_VRB, "--- LoadDecorationScript ---\n" );

	/* create the array */
	decoreGroups = safe_malloc(sizeof(dgroup_t) * MAX_DECORE_GROUPS);
	memset(decoreGroups, 0, sizeof(dgroup_t) * MAX_DECORE_GROUPS );
	numDecoreGroups = 0;

	/* load it */
	LoadScriptFile( "scripts/decorations.txt", 0 );

	/* parse */
	group = NULL;
	while ( 1 )
	{
		if ( !GetToken( qtrue ) )
			break;

		/* end of decore group or action list */
		if( !Q_stricmp( token, "}" ) )
		{
			if (group == NULL)
				Error( "LoadDecorationScript(): unexpected %s on line %i", token, scriptline);
			group = NULL;
		}

		/* new decore group */
		if( !Q_stricmp( token, "group" ) )
		{
			GetToken( qfalse );
			group = NewDecoreGroup( token, GROUPMATCH_GROUP );	
			MatchToken( "{" );
		}

		/* new class group */
		if( !Q_stricmp( token, "class" ) )
		{
			GetToken( qfalse );
			group = NewDecoreGroup( token, GROUPMATCH_CLASSNAME );	
			MatchToken( "{" );
		}

		/* new class group */
		if( !Q_stricmp( token, "model" ) )
		{
			GetToken( qfalse );
			group = NewDecoreGroup( token, GROUPMATCH_MODEL );	
			MatchToken( "{" );
		}

		/* options group */
		if( !Q_stricmp( token, "options" ) )
		{
			MatchToken( "{" );
			DecoreParseOptions();
		}

		/* parse group-related parm */
		if (group == NULL)
			continue;

		if( !Q_stricmp( token, "mergemodels" ) )
		{
			group->mergeModels = 1;
			GetToken( qfalse );
			group->mergeRadius = atoi (token);
			GetToken( qfalse );
			strcpy( group->mergeClass , token );
		}
		else if( !Q_stricmp( token, "entity" ) )
		{
			if ( group->actions != NULL )
				Error ( "LoadDecorationScript: double 'entity' definition at line %i", scriptline );
			group->actions = safe_malloc( sizeof(dactions_t) );
			memset( group->actions, 0, sizeof(dactions_t) );
			DecoreParseActions( group->actions );
		}
	}

	/* find all map entities */
	numDecoreEntities = 0;
	for (i = 1; i < numEntities; i++)
		if (PopulateDecoreGroup( &entities[ i ], GROUPMATCH_GROUP, "_decore" ) || 
		    PopulateDecoreGroup( &entities[ i ], GROUPMATCH_CLASSNAME, "classname" ) ||
		    PopulateDecoreGroup( &entities[ i ], GROUPMATCH_MODEL, "model" ) )
			numDecoreEntities++;

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9i decoration groups\n", numDecoreGroups );
	for (i = 0; i < numDecoreGroups; i++)
	{
		group = &decoreGroups[ i ];
		if ( group->numEntities )
			Sys_FPrintf( SYS_VRB, "%9i entities in '%s'\n", group->numEntities, group->name);
	}
}

/*
EntityProcessActions()
processes actions on specific entity
*/

void EntityProcessActions(entity_t *e, dactions_t *actions)
{
	daction_t *action;
	char string[ 256 ];
	int i, f;

	/* nothing to process? */
	if ( actions == NULL )
		return;

	for ( i = 0; i < actions->num; i++ )
	{
		action = &actions->action[ i ];
		switch( action->code )
		{
		case DECOREACTION_KEY_DEFAULT:
			if (!KeyExists(e, action->parm))
				SetKeyValue( e, action->parm, action->data );
			break;
		case DECOREACTION_KEY_SET:
			SetKeyValue( e, action->parm, action->data );
			break;
		case DECOREACTION_KEY_SETFLAG:
			f = IntForKey(e, action->parm);
			sprintf(string, "%i", (f | (int)(action->value)));
			SetKeyValue( e, action->parm, string );
			break;
		case DECOREACTION_KEY_UNSETFLAG:
			f = IntForKey(e, action->parm);
			f = f - (f & (int)action->value);
			sprintf(string, "%i", f);
			SetKeyValue( e, action->parm, string );
			break;
		case DECOREACTION_KEY_TOGGLEFLAG:
			f = IntForKey(e, action->parm);
			if ((f & (int)action->value) == (int)action->value)
				f = f - (f & (int)action->value);
			else
				f = f | (int)action->value;
			sprintf(string, "%i", f);
			SetKeyValue( e, action->parm, string );
			break;
		}
	}
}

/*
CreateEntity()
create an empty entity of given class and returns it
*/

entity_t *CreateEntity(char *classname)
{
	entity_t *e;

	/* range check */
	if( numEntities >= MAX_MAP_ENTITIES )
		Error( "numEntities == MAX_MAP_ENTITIES" );

	/* create */
	e = &entities[ numEntities ];
	numEntities++;
	memset( e, 0, sizeof( *e ) );
	SetKeyValue( e, "classname", classname );
	return e;
}

/*
MergeModelsForGroup()
performs a merging tests for all misc_models on group
*/

void DebugMergeModels(dgroup_t *group, dnode_t *dstnodes, int numSrcNodes)
{
	int numents, numgents, entsmin, entsmax, numgroups, totalents;
	dnode_t *testnode, *mergenode;
	int i;

	/* debug stats */
	Sys_Printf( "################################################\n" );
	Sys_Printf( "Merging group %s with radius %i passes %i\n", group->name, group->mergeRadius, numSrcNodes );
	totalents = 0;
	for ( i = 0; i < numSrcNodes; i++ )
	{
		numents = 0;
		numgroups = 0;
		entsmin = MAX_MAP_ENTITIES;
		entsmax = 0;
		/* cycle groups */
		for (mergenode = dstnodes[i].next; mergenode != NULL; mergenode = mergenode->next)
		{
			numgroups++;
			numgents = 1;
			for (testnode = mergenode->child; testnode != NULL; testnode = testnode->next)
				numgents++;
			if (numgents < entsmin)
				entsmin = numgents;
			if (numgents > entsmax)
				entsmax = numgents;
			numents += numgents;
		}
		Sys_Printf( "pass %i = %i groups, %i ents, min %i, max %i, avg %i \n", i, numgroups, numents, entsmin, entsmax, numents / numgroups);
		totalents += numents;
	}
	Sys_Printf( "################################################\n" );
}

// returns num of created groups
int MergeModelsForGroup(dgroup_t *group, dnode_t *srcnodes, int numSrcNodes)
{
	dnode_t *src, *dst, *testnode, *mergenode, *dstnodes;
	entity_t *e, *e2;
	vec3_t delta;
	char nullname[2], str[128];
	int i, num, avgmin, avgmax, best;
	int *stats;

	/* allocate destination nodes */
	dstnodes = NewNodesArray( numSrcNodes );
	strcpy(nullname, "");

	/* walk all test node chains */
	for ( i = 0; i < numSrcNodes; i++ )
	{
		src = &srcnodes[ i ];
		dst = &dstnodes[ i ];
		/* walk all entities for node */
		for ( testnode = src->next; testnode != NULL; testnode = testnode->next )
		{
			if (testnode->name[ 0 ] != '-')
				continue; // already grouped

			e = (entity_t *)testnode->ptr;

			/* test entity against merging groups */
			for (mergenode = dst->next; mergenode != NULL; mergenode = mergenode->next)
			{
				e2 = (entity_t *)mergenode->ptr;
				VectorSubtract (e->origin, e2->origin, delta);
				if (VectorLength(delta) < group->mergeRadius)
				{
					testnode->name = nullname;
					AddChildNode(mergenode, "", e );
					break;
				}	
			}

			if (testnode->name[ 0 ] != '-')
				continue;

			/* failed to merge for now - create new group */
			testnode->name = nullname;
			AddNextNode( dst, "", e );
		}
	}

	/* calc badly-balanced groups which has less than (avg / 0.75) or greater than avg 1.25 entities count for each pass */
	stats = safe_malloc( sizeof(int) * numSrcNodes * 2);
	memset( stats, 0, sizeof(int) * numSrcNodes * 2 );
	for ( i = 0; i < numSrcNodes; i++ )
	{
		for (mergenode = dstnodes[i].next; mergenode != NULL; mergenode = mergenode->next)
			stats[i*2]++;
		avgmin = ( group->numEntities / stats[i*2] ) * 0.75;
		avgmax = ( group->numEntities / stats[i*2] ) * 1.25;

		/* cals unbalanced groups */
		for (mergenode = dstnodes[i].next; mergenode != NULL; mergenode = mergenode->next)
		{
			num = 1;
			for (testnode = mergenode->child; testnode != NULL; testnode = testnode->next)
				num++;
			if (num < avgmin || num > avgmax)
				stats[i*2 + 1]++;
		}
	}

	/* pick best result */
	best = 0;
	for ( i = 0; i < numSrcNodes; i++ )
	{
		if ( stats[i*2 + 1] > stats[best*2 + 1] )
			continue;
		// prefere with lesser 'less groups' count
		if ( stats[i*2 + 1] < stats[best*2 + 1] )
			best = i;
		// equal - compare by groups count
		if ( stats[i*2] < stats[best*2] )
			best = i;
	}
	
	/* make actual merging (create new target chains) */
	num = 0;
	for (mergenode = dstnodes[ best ].next; mergenode != NULL; mergenode = mergenode->next)
	{
		num++;

		/* make main entity be a target for others */
		sprintf(str, "__decgrp_%s_%i", group->name, num);
		e = CreateEntity( group->mergeClass );
		EntityProcessActions( e, group->actions );
		SetKeyValue( e, "targetname", str );
		e->forceSubmodel = qtrue;

		/* target head node entity */
		e2 = (entity_t *)mergenode->ptr;
		SetKeyValue( e2, "target", str );
		VectorSet( delta, e2->origin[ 0 ], e2->origin[ 1 ], e2->origin[ 2 ] );

		/* target all other ents */
		for ( testnode = mergenode->child; testnode != NULL; testnode = testnode->next )
		{
			e2 = (entity_t *)testnode->ptr;
			SetKeyValue( e2, "target", str );

			VectorAdd( delta, e2->origin, delta );
			VectorScale( delta, 0.5, delta );
		}

		/* set averaged origin */
		sprintf( str, "%f %f %f", delta[ 0 ], delta[ 1 ], delta[ 2 ] );
		SetKeyValue( e, "origin", str );
		VectorSet( e->origin, delta[ 0 ], delta[ 1 ], delta[ 2 ] );
	}

	/* free allocated memory */
	FreeNodesArray( dstnodes, numSrcNodes );
	free(stats);

	return num;
}

/*
ImportRtlights()
import lights from .rtlights file
*/

void ImportRtlights( void )
{
	int		i, n, a, style, shadow, flags, size, intensity, numimported;
	char	tempchar, *s, *t, cubemapname[1024], value[128];
	float	origin[3], radius, color[3], basecolor[3], angles[3], corona, coronasizescale, ambientscale, diffusescale, specularscale, d;
	char	filename [MAX_QPATH];
	char	*lightsstring;
	entity_t *e;

	// flags for rtlight rendering
	#define DARKPLACES_LIGHTFLAG_NORMALMODE		1
	#define DARKPLACES_LIGHTFLAG_REALTIMEMODE	2

	/* this mode requires _keepLights to be set */
	SetKeyValue( &entities[ 0 ], "_keepLights", "1" );

	/* note it */
	Sys_FPrintf(SYS_VRB, "--- ImportRtlights ---\n" );

	/* open file */
	strcpy( filename, source );
	StripExtension( filename );
	DefaultExtension( filename, ".rtlights" );
	if (!FileExists(filename))
	{
		Sys_FPrintf(SYS_VRB, " map has no .rtlights file\n" );
		return;
	}
	size = LoadFile( filename, (void **) &lightsstring );

	/* this code is picked up from Darkplaces engine sourcecode and it's written by Forest [LordHavoc] Hale */
	numimported = 0;
	s = lightsstring;
	n = 0;
	while (*s)
	{
		
		t = s;
		while (*s && *s != '\n' && *s != '\r')
			s++;
		if (!*s)
			break;
		tempchar = *s;
		shadow = 1;
		// check for modifier flags
		if (*t == '!')
		{
			shadow = 0;
			t++;
		}
		*s = 0;
		cubemapname[sizeof(cubemapname)-1] = 0;
		a = sscanf_s(t, "%f %f %f %f %f %f %f %d %127s %f %f %f %f %f %f %f %f %i", &origin[0], &origin[1], &origin[2], &radius, &color[0], &color[1], &color[2], &style, cubemapname, sizeof(cubemapname), &corona, &angles[0], &angles[1], &angles[2], &coronasizescale, &ambientscale, &diffusescale, &specularscale, &flags);
		*s = tempchar;
		if (a < 18)
			flags = DARKPLACES_LIGHTFLAG_REALTIMEMODE;
		if (a < 17)
			specularscale = 1;
		if (a < 16)
			diffusescale = 1;
		if (a < 15)
			ambientscale = 0;
		if (a < 14)
			coronasizescale = 0.25f;
		if (a < 13)
			VectorClear(angles);
		if (a < 10)
			corona = 0;
		if (a < 9 || !strcmp(cubemapname, "\"\""))
			cubemapname[0] = 0;
		// remove quotes on cubemapname
		if (cubemapname[0] == '"' && cubemapname[strlen(cubemapname) - 1] == '"')
		{
			size_t namelen;
			namelen = strlen(cubemapname) - 2;
			memmove(cubemapname, cubemapname + 1, namelen);
			cubemapname[namelen] = '\0';
		}
		if (*s == '\r')
			s++;
		if (*s == '\n')
			s++;
		n++;
	
		/* process light properties */
		intensity = (diffusescale + ambientscale)*importRtlightsColorscale;
		if (intensity <= 0)
			continue; // corona-only light
		if (cubemapname[0] && importRtlightsSkipCubemapped == qtrue)
			continue; // skip cubemapped lights
		for (i = 0; i < importRtlightsSkipStylesNum; i++) // skipstyle directive
			if (importRtlightsSkipStyles[ i ] == style)
				break;
		if (i < importRtlightsSkipStylesNum)
			continue;

		VectorScale( color, intensity, color );
		VectorSet( basecolor, 0.299f, 0.587f, 0.114f );
		d = DotProduct( color, basecolor );
		color[ 0 ] = color[ 0 ] * importRtlightsSaturate + d * max(0, (1 - importRtlightsSaturate));
		color[ 1 ] = color[ 1 ] * importRtlightsSaturate + d * max(0, (1 - importRtlightsSaturate));
		color[ 2 ] = color[ 2 ] * importRtlightsSaturate + d * max(0, (1 - importRtlightsSaturate));
		radius *= importRtlightsRadiusmod;

		/* create a "light" entity for this light */
		e = CreateEntity( "light" );
		sprintf(value, "%f %f %f", origin[ 0 ], origin[ 1 ], origin[ 2 ]);
		SetKeyValue( e, "origin", value );
		sprintf(value, "%f %f %f", color[ 0 ], color[ 1 ], color[ 2 ]);
		SetKeyValue( e, "_color", value );
		sprintf(value, "%f", radius);
		SetKeyValue( e, "light", value );
		if (importRtlightsSpawnflags)
		{
			sprintf(value, "%f", importRtlightsSpawnflags);
			SetKeyValue( e, "spawnflags", value );
		}
		if (shadow)
		{
			if (importRtlightsShadowDeviance && importRtlightsShadowSamples)
			{
				sprintf(value, "%i", (int)(importRtlightsShadowDeviance));
				SetKeyValue( e, "_deviance", value );
				sprintf(value, "%i", (int)(importRtlightsShadowSamples));
				SetKeyValue( e, "_samples", value );
			}
			if (importRtlightsShadowMinDist)
			{
				sprintf(value, "%i", (int)(radius * importRtlightsShadowMinDist));
				SetKeyValue( e, "_mindist", value );
			}
		}
		else
		{
			if (importRtlightsNoShadowDeviance && importRtlightsNoShadowSamples)
			{
				sprintf(value, "%i", (int)(importRtlightsNoShadowDeviance));
				SetKeyValue( e, "_deviance", value );
				sprintf(value, "%i", (int)(importRtlightsNoShadowSamples));
				SetKeyValue( e, "_samples", value );
			}
			if (importRtlightsNoShadowMinDist)
			{
				sprintf(value, "%i", (int)(radius * importRtlightsNoShadowMinDist));
				SetKeyValue( e, "_mindist", value );
			}
		}
		numimported++;
	}
	if (*s)
		Sys_FPrintf (SYS_VRB, " invalid rtlights file\n" );
	free(lightsstring);

	/* emit some stats */
	Sys_FPrintf(SYS_VRB, " %6i lights imported\n", numimported );
	
	// flags for rtlight rendering
	#undef DARKPLACES_LIGHTFLAG_NORMALMODE
	#undef DARKPLACES_LIGHTFLAG_REALTIMEMODE
}

/*
ProcessDecorations()
does all decoration job
*/

void ProcessDecorations( void )
{
	dgroup_t	*group;
	dnode_t		*node, *tnode, *testnodes;
	int			i, f, fOld, start, walkEnts, numTestNodes;

	/* load decoration scripts */
	decoreGroups = NULL;
	LoadDecorationScript();
	if (!decoreGroups)
		return;
	
	/* get number of test nodes */
	numTestNodes = IntForKey( &entities[ 0 ], "_mergetests" );
	if (numTestNodes <= 0)
		numTestNodes = 4;
	if (numTestNodes < 2)
		numTestNodes = 2;
	if (numTestNodes > 256)
		numTestNodes = 256;
	Sys_FPrintf( SYS_VRB, "%9i merge tests will be performed\n", numTestNodes );

	/* note it */
	Sys_FPrintf (SYS_VRB, "--- ProcessDecorations ---\n" );

	/* model merging requires at least 2 ents */
	for ( i = 0; i < numDecoreGroups; i++ )
		if ( decoreGroups[ i ].numEntities < 2 )
			decoreGroups[ i ].mergeModels = 0;

	/* walk all groups and entities */
	fOld = -1;
	walkEnts = 0;
	start = I_FloatTime();
	for ( i = 0; i < numDecoreGroups; i++ )
	{
		group = &decoreGroups[ i ];
		if (!group->numEntities)
			continue;

		/* initiate merging nodes array */
		if ( group->mergeModels )
			testnodes = NewNodesArray(numTestNodes);

		/* walk entities */
		for( node = group->entities; node != NULL; node = node->next )
		{
			walkEnts++;

			/* print pacifier */
			if ( numDecoreEntities > 10 )
			{
				f = 10 * walkEnts / numDecoreEntities;
				if( f != fOld )
				{
					fOld = f;
					Sys_FPrintf (SYS_VRB, "%d...", f );
				}
			}

			/* add to test nodes */
			if ( group->mergeModels && !Q_stricmp( ValueForKey( node->ptr, "classname" ), "misc_model" ) )
			{
				// vortex: only if not targeted
				if (!strcmp( ValueForKey(node->ptr, "target" ), "" ) )
				{
					for ( f = 0; f < numTestNodes; f++ )
					{
						tnode = &testnodes[ f ];
						if (tnode->next == NULL)
							tnode->next = NewNode( "-", node->ptr );
						else
							AddNextNodeRandom( tnode->next, "-", node->ptr);
					}
				}
			}

			/* process key actions */
			if ( group->actions != NULL )
				EntityProcessActions( (entity_t *)node->ptr, group->actions );
		}

		/* merge misc_models */
		if ( group->mergeModels )
		{
			/* do merging, clear stuff, reuse mergeModels for stats */
			group->mergeModels = MergeModelsForGroup( group, testnodes, numTestNodes );
			FreeNodesArray( testnodes, numTestNodes );
		}
	}

	/* print time */
	if ( numDecoreEntities > 10 )
		Sys_FPrintf (SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );

	/* emit some stats */
	for ( i = 0; i < numDecoreGroups; i++ )
	{
		group = &decoreGroups[ i ];
		if ( group->mergeModels )
			Sys_FPrintf( SYS_VRB, "%9i models merged to %i in '%s'\n", group->numEntities, group->mergeModels, group->name );

		// free group-allocated stuff
		FreeDecoreGroup ( group );
	}
	free( decoreGroups );
	numDecoreGroups = 0;


	/* import rtlights */
	if (importRtlights)
		ImportRtlights();
}