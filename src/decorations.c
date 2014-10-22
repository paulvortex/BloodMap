// custom decorations system
// used for creating nifty things like foliage under trees

#include "q3map2.h"

// marker
#define DECORATIONS_C

// limits
#define MAX_DECORE_GROUPS		1024
#define MAX_DECORE_ACTIONS		128
#define MAX_DECORE_TESTNODES	512

// defaults
#define DEFAULT_DECORE_TESTNODES 64
#define MIN_DECORE_TESTNODES     10

// action codes
typedef enum
{
	DECOREACTION_NULL,
	DECOREACTION_KEY_DEFAULT,	
	DECOREACTION_KEY_SET,		
	DECOREACTION_KEY_SETFLAG,	
	DECOREACTION_KEY_UNSETFLAG,
	DECOREACTION_KEY_TOGGLEFLAG,
}decoreActionCode_t;

// action
typedef struct decoreaction_s
{
	decoreActionCode_t code;
	char			   parm[ 128 ];
	double			   value;
	char			   data[ 256 ];
}decoreAction_t;

// node
typedef struct dnode_s
{
	void *ptr; // pointer to object
	void *next;
	void *child;
}decoreNode_t;

// actions array
typedef struct dactions_s
{
	decoreAction_t	action[ MAX_DECORE_ACTIONS ];
	int			    num;
}decoreActions_t;

// group match parm
typedef enum
{
	GROUPMATCH_GROUP,
	GROUPMATCH_CLASSNAME,
	GROUPMATCH_MODEL
}decoreMatchParm_t;

// entities array
typedef struct dgroupents_s
{
	entity_t  **entities;
	int			numEntities;
	int         maxEntities;
}decoreEnts_t;

// merge axis
typedef enum
{
	MERGE_XYZ,
	MERGE_XY
}mergeAxis_t;

// group general struct
typedef struct dgroup_s
{
	// name of group
	char		      name[ 1024 ]; 
	decoreMatchParm_t match;
	// entity actions
	decoreActions_t	 *actions;
	// misc_model merging
	int			      mergeModels;
	int			      mergeRadius;
	char		      mergeClass[ 1024 ];
	vec_t             mergeMaxSize;
	mergeAxis_t       mergeAxis;
	// entities
	decoreEnts_t      entities;
	// system
	ThreadMutex       mutex;
}decoreGroup_t;

decoreGroup_t *decoreGroups Q_ASSIGN( NULL );
int            numDecoreGroups Q_ASSIGN ( 0 );
int            numDecoreEntities Q_ASSIGN ( 0 );
int            numDecoreEntitiesPushedToWorldspawn Q_ASSIGN ( 0 );
int            numDecoreTestNodes Q_ASSIGN ( 0 );

/* rtlights import */
#define MAX_SKIPSTYLES	128
qboolean	   importRtlights = qfalse;
qboolean	   importRtlightsSkipCubemapped = qfalse;
float		   importRtlightsColorscale = 1;
float		   importRtlightsSaturate = 1;
float		   importRtlightsRadiusmod = 0.5;
int			   importRtlightsSpawnflags = 0;
float		   importRtlightsNoShadowDeviance = 0.1;
float		   importRtlightsNoShadowSamples = 0.1;
float		   importRtlightsNoShadowMinDist = 0.5;
float		   importRtlightsShadowDeviance = 0.0;
float		   importRtlightsShadowSamples = 0.0;
float		   importRtlightsShadowMinDist = 0.1;
int			   importRtlightsSkipStyles[MAX_SKIPSTYLES];
int			   importRtlightsSkipStylesNum = 0;

/*
=========================================================

 NODES

=========================================================
*/

void PrintNode_f(decoreNode_t *node)
{
	decoreNode_t *c;

	// node + children
	Sys_Printf("node");
	if (node->child != NULL)
	{
		Sys_Printf("{");
		PrintNode_f((decoreNode_t *)node->child);
		Sys_Printf("}");
	}
	// next nodes
	if (node->next != NULL)
	{
		for (c = (decoreNode_t *)node->next; c != NULL; c = (decoreNode_t *)c->next)
		{
			Sys_Printf(".next");
			if (c->child != NULL)
			{
				Sys_Printf("{");
				PrintNode_f((decoreNode_t *)c->child);
				Sys_Printf("}");
			}
		}
	}
}

/*
PrintNode
node debug print
*/
void PrintNode(decoreNode_t *node)
{
	if (!node)
		Sys_Printf("{null node}");
	else
		Sys_Printf(">>");
	PrintNode_f(node);
	Sys_Printf("\n");
}

/*
FreeNodesArray
allocate nodes array
*/
decoreNode_t *NewNodesArray(int size)
{
	decoreNode_t *nodes, *node;
	int i;

	nodes = (decoreNode_t *)safe_malloc( sizeof(decoreNode_t) * size );
	for (i = 0; i < size; i++)
	{
		node = &nodes [ i ];
		memset(node, 0, sizeof(decoreNode_t));
		node->ptr = NULL;
		node->child = NULL;
		node->next = NULL;
	}
	return nodes;
}

/*
FreeNodesArray
free single node
*/
void FreeNode(decoreNode_t *node)
{
	decoreNode_t *c, *next;

	if (!node)
		return;
	
	// free child node
	if (node->child != NULL)
		FreeNode((decoreNode_t *)node->child);
	// free all next items
	if (node->next != NULL)
	{
		for (c = (decoreNode_t *)node->next; c != NULL; c = next)
		{
			if (c->child != NULL)
				FreeNode((decoreNode_t *)c->child);
			next = (decoreNode_t *)c->next;
			free(c);
		}
	}
	// free this node
	free(node);
}

/*
FreeNodesArray
frees nodes array
*/
void FreeNodesArray(decoreNode_t *nodes, int size)
{
	decoreNode_t *node;
	int i;

	for (i = 0; i < size; i++)
	{
		node = &nodes [ i ];
		if ( node->next != NULL )
			FreeNode( (decoreNode_t *)node->next );
		if ( node->child != NULL )
			FreeNode( (decoreNode_t *)node->child );
	}
}

/*
NewNode
creates new node
*/
decoreNode_t *NewNode(void *ptr)
{	
	decoreNode_t *node;

	node = (decoreNode_t *)safe_malloc(sizeof(decoreNode_t));
	memset(node, 0, sizeof(decoreNode_t));
	node->ptr = ptr;
	node->child = NULL;
	node->next = NULL;

	return node;
}

/*
PushNode
push node in the end of chain of current node
*/
void PushNode(decoreNode_t *node, void *ptr)
{
	decoreNode_t *n = NewNode(ptr);
	n->next = node->next;
	node->next = n;
}

/*
PushNodeChild
push child node
*/
void PushNodeChild(decoreNode_t *node, void *ptr)
{
	decoreNode_t *n;

	n = NewNode(ptr);
	if (node->child != NULL)
	{
		n->next = node->child;
		node->child = n;
		return;
	}
	node->child = n;
}

/*
=========================================================

 ENTITIES ARRAY

=========================================================
*/

/*
EntityArrayAdd
add one entity to entities array
*/
void EntityArrayAdd(decoreEnts_t *groupents, entity_t *e)
{
	/* grow entities array */
	if (groupents->numEntities >= groupents->maxEntities)
	{
		if (!groupents->entities)
		{
			groupents->numEntities = 0;
			groupents->maxEntities = 1024;
			groupents->entities = (entity_t **)safe_malloc ( groupents->maxEntities * sizeof(entity_t *) );
		}
		else
		{
			entity_t **newEntities = (entity_t **)safe_malloc ( (groupents->numEntities + 4096) * sizeof(entity_t *) );
			memcpy(newEntities, groupents->entities, groupents->numEntities * sizeof(entity_t *));
			free(groupents->entities);
			groupents->entities = newEntities;
			groupents->maxEntities = groupents->numEntities + 4096;
		}
	}
	/* add entity */
	groupents->entities[groupents->numEntities++] = e;
}

/*
EntityArrayFlush
free / cleanup entities array
*/

void EntityArrayFlush(decoreEnts_t *groupents)
{
	//if( groupents->entities )
	//	free( groupents->entities );
	memset( groupents, 0, sizeof( decoreEnts_t ) );
}


/*
EntityArrayCopy
copies entity array
*/

void EntityArrayCopy(decoreEnts_t *src, decoreEnts_t *dst)
{
	/* flush array */
	EntityArrayFlush(dst);

	/* copy */
	dst->numEntities = src->numEntities;
	dst->maxEntities = src->maxEntities;
	dst->entities = (entity_t **)safe_malloc ( dst->maxEntities * sizeof(entity_t *) );
	memcpy(dst->entities, src->entities, dst->numEntities * sizeof(entity_t *) );
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

decoreGroup_t *NewDecoreGroup(char *name, decoreMatchParm_t match)
{
	decoreGroup_t *group;
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
	memset(group, 0, sizeof(decoreGroup_t));
	group->match = match;
	group->actions = NULL;
	group->mergeAxis = MERGE_XYZ;
	group->mergeRadius = 128;
	strcpy( group->name, name );
	strcpy( group->mergeClass, "" );
	ThreadMutexInit(&group->mutex);

	numDecoreGroups++;
	return group;
}

/*
PopulateDecoreGroup()
get a decoration group for entity
returns NULL if no group
*/

qboolean PopulateDecoreGroup(entity_t *e, decoreMatchParm_t match, char *keyname)
{
	const char *value;
	qboolean populated;
	decoreGroup_t *group;
	int	i;

	/* get key */
	value = ValueForKey( e, keyname );
	if( value[ 0 ] == '\0' )
		return qfalse;

	/* find group */
	populated = qfalse;
	for (i = 0; i < numDecoreGroups; i++)
	{
		group = &decoreGroups[i];
		if (group->match != match || Q_stricmp( value, group->name) )
			continue;
	
		/* add entity to a group */
		ThreadMutexLock(&group->mutex);
		{
			EntityArrayAdd(&group->entities, e);
		}
		ThreadMutexUnlock(&group->mutex);
		populated = qtrue;
		return populated;
	}
	return populated;
}

/*
FreeDecoreGroup()
free all allocated fiels on decoregroup struct
*/

void FreeDecoreGroup( decoreGroup_t *group )
{
	EntityArrayFlush( &group->entities );
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
void DecoreParseActions ( decoreActions_t *actions )
{
	decoreAction_t actiondata;

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
		memset(&actiondata, 0, sizeof(decoreAction_t));
		actiondata.code = DECOREACTION_NULL;

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
			memcpy(&actions->action[actions->num], &actiondata, sizeof(decoreAction_t));
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

/*
EntityPopulateDecorationGroups()
entity worker for decoration groups
*/


void EntityPopulateDecorationGroups(int entNum)
{
	if (PopulateDecoreGroup( &entities[ entNum ], GROUPMATCH_GROUP, "_decore" ) || 
		PopulateDecoreGroup( &entities[ entNum ], GROUPMATCH_CLASSNAME, "classname" ) ||
		PopulateDecoreGroup( &entities[ entNum ], GROUPMATCH_MODEL, "model" ) )
		numDecoreEntities++;
}

/*
LoadDecorationScript()
loads up decore groups
sets entities for decore groups
*/
void LoadDecorationScript( void )
{
	decoreGroup_t *group;

	/* file exists? */
	if( vfsGetFileCount( "scripts/decorations.txt" ) == 0 )
		return;

	/* note */
	Sys_FPrintf( SYS_VRB, "--- LoadDecorations ---\n" );

	/* create the array */
	decoreGroups = (decoreGroup_t *)safe_malloc(sizeof(decoreGroup_t) * MAX_DECORE_GROUPS);
	memset(decoreGroups, 0, sizeof(decoreGroup_t) * MAX_DECORE_GROUPS );
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

		/* new model group */
		if( !Q_stricmp( token, "model" ) )
		{
			GetToken( qfalse );
			group = NewDecoreGroup( token, GROUPMATCH_MODEL );	
			MatchToken( "{" );
		}

		/* rtlights import options */
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
				if( !Q_stricmp( token, "colorScale" ) )
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
				if( !Q_stricmp( token, "radiusMod" ) )
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
				if( !Q_stricmp( token, "noShadowDeviance" ) )
				{
					GetToken( qfalse );
					importRtlightsNoShadowDeviance = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "noShadowSamples" ) )
				{
					GetToken( qfalse );
					importRtlightsNoShadowSamples = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "noShadowMinDist" ) )
				{
					GetToken( qfalse );
					importRtlightsNoShadowMinDist = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "shadowDeviance" ) )
				{
					GetToken( qfalse );
					importRtlightsShadowDeviance = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "shadowSamples" ) )
				{
					GetToken( qfalse );
					importRtlightsShadowSamples = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "shadowMinDist" ) )
				{
					GetToken( qfalse );
					importRtlightsShadowMinDist = atof( token );
					continue;
				}
				if( !Q_stricmp( token, "skipCubemapped" ) )
				{
					GetToken( qfalse );
					if ( atoi( token ) )
						importRtlightsSkipCubemapped = qtrue;
					else
						importRtlightsSkipCubemapped = qfalse;
					continue;
				}
				if( !Q_stricmp( token, "skipStyle" ) )
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

		/* parse group-related parm */
		if (group == NULL)
			continue;

		if( !Q_stricmp( token, "mergeModels" ) )
		{
			group->mergeModels = 1;
			if ( GetToken( qfalse ) == qtrue )
				strcpy( group->mergeClass , token );
		}
		else if( !Q_stricmp( token, "mergeRadius" ) )
		{
			if ( GetToken( qfalse ) == qtrue )
				group->mergeRadius = atoi (token);
		}
		else if( !Q_stricmp( token, "mergeAxis" ) )
		{
			if ( GetToken( qfalse ) == qtrue )
			{
				if( !Q_stricmp( token, "xyz" ) )
					group->mergeAxis = MERGE_XYZ;
				else if( !Q_stricmp( token, "xy" ) )
					group->mergeAxis = MERGE_XY;
			}
		}
		else if( !Q_stricmp( token, "mergeMaxSize" ) )
		{
			if ( GetToken( qfalse ) == qtrue )
				group->mergeMaxSize = atof (token);
		}
		else if( !Q_stricmp( token, "entity" ) )
		{
			if ( group->actions != NULL )
				Error ( "LoadDecorationScript: double 'entity' definition at line %i", scriptline );
			group->actions = (decoreActions_t *)safe_malloc( sizeof(decoreActions_t) );
			memset( group->actions, 0, sizeof(decoreActions_t) );
			DecoreParseActions( group->actions );
		}
	}

	/* emit some stats */
	Sys_FPrintf( SYS_VRB, "%9i decoration groups\n", numDecoreGroups );
}

/*
EntityProcessActions()
processes actions on specific entity
*/

void EntityProcessActions(entity_t *e, decoreActions_t *actions)
{
	decoreAction_t *action;
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

#if 0
void DebugMergeModels(decoreGroup_t *group, decoreNode_t *dstnodes, int numSrcNodes)
{
	int numents, numgents, entsmin, entsmax, numgroups, totalents;
	decoreNode_t *testnode, *mergenode;
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
		for (mergenode = (decoreNode_t *)dstnodes[i].next; mergenode != NULL; mergenode = (decoreNode_t *)mergenode->next)
		{
			numgroups++;
			numgents = 1;
			for (testnode = (decoreNode_t *)mergenode->child; testnode != NULL; testnode = (decoreNode_t *)testnode->next)
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
#endif

/*
ImportFoliage()
import foliage models from _foliageX.reg files
*/

void ImportFoliage( char *source )
{
	char file[ MAX_OS_PATH ];
	int startEntities;
	int i;

	/* note it */
	Sys_FPrintf(SYS_VRB, "--- ImportFoliage ---\n" );

	/* import */
	startEntities = numEntities;
	for (i = 1; i < 10; i++)
	{
		if (i < 2)
			sprintf( file, "%s_foliage.reg", source );
		else
			sprintf( file, "%s_foliage%i.reg", source, i );
		if( FileExists( file ) == qtrue ) 
			LoadMapFile( file, qfalse, qtrue );
	}

	/* emit some stats */
	Sys_FPrintf(SYS_VRB, "%9d entities imported\n", numEntities - startEntities );
}

/*
ImportRtlights()
import lights from .rtlights file
*/

void ImportRtlights( void )
{
	int		i, n, a, style, shadow, flags, size, intensity, numimported;
	char	tempchar, *s, *t, cubemapname[MAX_OS_PATH], value[128];
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
	Sys_FPrintf(SYS_VRB, "%9d lights imported\n", numimported );
	
	// flags for rtlight rendering
	#undef DARKPLACES_LIGHTFLAG_NORMALMODE
	#undef DARKPLACES_LIGHTFLAG_REALTIMEMODE
}



/*
CompareRandom()
compare function for qsort()
*/
static int CompareRandom( const void *a, const void *b )
{
	return (rand() > 0.5) ? -1 : 0;
}

/*
ShuffleTestNode()
randomizes test node entities so there will be several groupings possible
*/
void ShuffleTestNode( decoreEnts_t *testnode )
{
	entity_t *temp;
	int index, j;

	/* fisher-yates shuffle */
	for( j = 0; j < testnode->numEntities; j++ )
	{
		index = rand() % testnode->numEntities;
		if( index != j )
		{
			temp = testnode->entities[ index ];
			testnode->entities[ index ] = testnode->entities[ j ];
			testnode->entities[ j ] = temp;
		}
	}
}

/*
ProcessDecorationGroup()
processes entities under decoration group
*/
picoModel_t *FindModel( const char *name, int frame );
void ProcessDecorationGroup(int groupNum)
{
	decoreGroup_t *group;
	entity_t *e;
	int i;
	
	/* get group */
	group = &decoreGroups[ groupNum ];
	if ( !group->entities.numEntities )
		return;

	/* process actions */
	for( i = 0; i < group->entities.numEntities; i++ ) 
	{
		/* get entity */
		e = group->entities.entities[ i ];

		/* process actions */
		EntityProcessActions( e, group->actions );
	}

	/* process merging */
	if( group->mergeModels  )
	{
		decoreNode_t *dst, *mergenode, *dstnodes, *testnode;
		decoreEnts_t *src, basenode, shufflednode;
		int j, num, avgmin, avgmax, best, *stats;
		const char *model, *value;
		vec3_t delta, size, scale;
		picoModel_t *m;
		vec_t temp;
		entity_t *e2;
		char str[512];

		/* allocate nodes */
		memset(&basenode, 0, sizeof(basenode));
		memset(&shufflednode, 0, sizeof(shufflednode));
		EntityArrayCopy(&group->entities, &basenode);
		dstnodes = NewNodesArray( numDecoreTestNodes );

		/* exclude models from merging */
		src = &basenode;
		for ( i = 0; i < src->numEntities; i++ )
		{
			e = src->entities[ i ];

			/* already excluded? */
			if (e == NULL)
				continue;

			/* exclude models which was already targeted */
			value = ValueForKey( e, "target" );	
			if ( value != NULL && value[0] != 0 )
			{
				src->entities[ i ] = NULL;
				continue;
			}

			/* exclude models which are too large */
			if( group->mergeMaxSize )
			{
				/* get model name */
				model = ValueForKey( e, "_model" );	
				if( model[ 0 ] == '\0' )
					model = ValueForKey( e, "model" );
				if( model[ 0 ] == '\0' )
					continue;

				/* find model */
				m = FindModel(model, IntForKey( e, "_frame" ));
				if (m == NULL)
					continue;

				/* get scale */
				scale[ 0 ] = scale[ 1 ] = scale[ 2 ] = 1.0f;
				temp = FloatForKey( e, "modelscale" );
				if( temp != 0.0f )
					scale[ 0 ] = scale[ 1 ] = scale[ 2 ] = temp;
				value = ValueForKey( e, "modelscale_vec" );
				if( value[ 0 ] != '\0' )
					sscanf( value, "%f %f %f", &scale[ 0 ], &scale[ 1 ], &scale[ 2 ] );

				/* measure dest size */
				VectorSubtract( m->maxs, m->mins, size );
				for ( j = 0; j < 3; j++ )
					size[ j ] = size[ j ] * scale[ j ];

				/* push to world? */
				if (size[ 0 ] > group->mergeMaxSize || size[ 1 ] > group->mergeMaxSize || size[ 2 ] > group->mergeMaxSize)
				{
					numDecoreEntitiesPushedToWorldspawn++;
					SetKeyValue( e, "_pv2", "-0.1" ); /* vortex: bias surfaces a bit so decore editor won't do zfighting */
					src->entities[ i ] = NULL;
				}
			}
		}

		/* create merge candidates */
		for ( i = 0; i < numDecoreTestNodes; i++ )
		{
			/* create test node */
			src = &shufflednode;
			EntityArrayCopy(&basenode, src);
			ShuffleTestNode(src);

			/* get dst node */
			dst = &dstnodes[ i ];

			/* walk all entities for node */
			for ( j = 0; j < src->numEntities; j++ )
			{
				e = src->entities[ j ];

				/* already grouped? */
				if (e == NULL)
					continue;

				/* test entity against existing merging groups */
				for (mergenode = (decoreNode_t *)dst->next; mergenode != NULL; mergenode = (decoreNode_t *)mergenode->next)
				{
					e2 = (entity_t *)mergenode->ptr;
					VectorSubtract (e->origin, e2->origin, delta);
					if ( group->mergeAxis == MERGE_XY )
						delta[ 3 ] = 0;
					if (VectorLength(delta) < group->mergeRadius)
					{
						PushNodeChild(mergenode, e );
						src->entities[ j ] = NULL;
						break;
					}	
				}

				/* failed to merge for now - create new group */
				if (src->entities[ j ] != NULL)
				{
					PushNode( dst, e );
					src->entities[ j ] = NULL;
				}
			}
		}

		/* calc badly-balanced groups which has less than (avg / 0.75) or greater than avg 1.25 entities count for each pass */
		stats = (int *)safe_malloc( sizeof(int) * numDecoreTestNodes * 2);
		memset( stats, 0, sizeof(int) * numDecoreTestNodes * 2 );
		for ( i = 0; i < numDecoreTestNodes; i++ )
		{
			if (dstnodes[i].next == NULL)
				continue;
			for (mergenode = (decoreNode_t *)dstnodes[i].next; mergenode != NULL; mergenode = (decoreNode_t *)mergenode->next)
				stats[i*2]++;
			avgmin = ( basenode.numEntities / stats[i*2] ) * 0.75;
			avgmax = ( basenode.numEntities / stats[i*2] ) * 1.25;

			/* cals unbalanced groups */
			for (mergenode = (decoreNode_t *)dstnodes[i].next; mergenode != NULL; mergenode = (decoreNode_t *)mergenode->next)
			{
				num = 1;
				for (testnode = (decoreNode_t *)mergenode->child; testnode != NULL; testnode = (decoreNode_t *)testnode->next)
					num++;
				if (num < avgmin || num > avgmax)
					stats[i*2 + 1]++;
			}
		}

		/* pick best result */
		best = 0;
		for ( i = 0; i < numDecoreTestNodes; i++ )
		{
			if (dstnodes[i].next == NULL)
				continue;
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
		for (mergenode = (decoreNode_t *)dstnodes[ best ].next; mergenode != NULL; mergenode = (decoreNode_t *)mergenode->next)
		{
			num++;

			/* make main entity be a target for others */
			sprintf(str, "__decgrp_%s_%i", group->name, num);
			e = CreateEntity( group->mergeClass );
			EntityProcessActions( e, group->actions );
			SetKeyValue( e, "targetname", str );
			SetKeyValue( e, "_noflood", "1" ); // don't leak
			e->forceSubmodel = qtrue;

			/* target head node entity */
			e2 = (entity_t *)mergenode->ptr;
			SetKeyValue( e2, "target", str );
			VectorSet( delta, e2->origin[ 0 ], e2->origin[ 1 ], e2->origin[ 2 ] );

			/* target all other ents */
			for ( testnode = (decoreNode_t *)mergenode->child; testnode != NULL; testnode = (decoreNode_t *)testnode->next )
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
		FreeNodesArray( dstnodes, numDecoreTestNodes );
		free(stats);

		/* store result for stats */
		group->mergeModels = num;
	}
}

/*
LoadDecorations()
load up decoratino scripts
*/

void LoadDecorations( void )
{
	decoreGroups = NULL;
	LoadDecorationScript();
}

/*
ImportDecorations()
import decoration entities in map
*/

void ImportDecorations( char *source )
{
	// import foliage
	if (nofoliage == qfalse)
		ImportFoliage( source );
	
	// import rtlights
	if (importRtlights)
		ImportRtlights();
}

/*
ProcessDecorations()
does all decoration job
*/

void ProcessDecorations( void )
{
	int	i, f, fOld, start;
	decoreGroup_t *group;

	/* early out */
	if (!numDecoreGroups)
		return;

	/* find all map entities */
	Sys_FPrintf (SYS_VRB, "--- FindDecorationGroups ---\n" );
	numDecoreEntities = 0;
	RunThreadsOnIndividual(numEntities, verbose ? qtrue : qfalse, EntityPopulateDecorationGroups);
	for (i = 0; i < numDecoreGroups; i++)
	{
		group = &decoreGroups[ i ];
		if ( group->entities.numEntities )
			Sys_FPrintf( SYS_VRB, "%9i entities in '%s'\n", group->entities.numEntities, group->name);
	}
	
	/* get number of test nodes */
	numDecoreTestNodes = IntForKey( &entities[ 0 ], "_mergetests" );
	if( numDecoreTestNodes <= 0 )
		numDecoreTestNodes = DEFAULT_DECORE_TESTNODES;
	if( numDecoreTestNodes < MIN_DECORE_TESTNODES )
		numDecoreTestNodes = MIN_DECORE_TESTNODES;
	if( numDecoreTestNodes > MAX_DECORE_TESTNODES )
		numDecoreTestNodes = MAX_DECORE_TESTNODES;

	/* walk all groups and entities */
	Sys_FPrintf (SYS_VRB, "--- ProcessDecorations ---\n" );
	start = I_FloatTime();
	fOld = -1;
	for ( i = 0; i < numDecoreGroups; i++ )
	{
		/* print pacifier */
		f = 10 * i / numDecoreGroups;
		if( f != fOld )
		{
			fOld = f;
			Sys_FPrintf(SYS_VRB, "%d...", f );
		}

		/* process */
		ProcessDecorationGroup( i );
	}
	Sys_FPrintf(SYS_VRB, " (%d)\n", (int) (I_FloatTime() - start) );
	Sys_FPrintf( SYS_VRB, "%9i tests performed\n", numDecoreTestNodes );

	/* emit stats */
	for ( i = 0; i < numDecoreGroups; i++ )
	{
		group = &decoreGroups[ i ];
		if ( group->mergeModels )
			Sys_FPrintf( SYS_VRB, "%9i models merged to %i in '%s'\n", group->entities.numEntities, group->mergeModels, group->name );

		// free group-allocated stuff
		FreeDecoreGroup ( group );
	}
	free( decoreGroups );
	numDecoreGroups = 0;
	Sys_FPrintf( SYS_VRB, "%9i large models pushed to worldspawn\n", numDecoreEntitiesPushedToWorldspawn );
}