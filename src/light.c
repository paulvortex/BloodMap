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
#define LIGHT_C

/* dependencies */
#include "q3map2.h"

/*
================================================================================

 LIGHT CREATING

================================================================================
*/

/*
CreateSunLight() - ydnar
this creates a sun light
*/

static void CreateSunLight( sun_t *sun )
{
	int			i;
	float		photons, d, angle, elevation, da, de;
	vec3_t		direction;
	light_t		*light;
	
	
	/* dummy check */
	if( sun == NULL )
		return;
	
	/* fixup */
	if( sun->numSamples < 1 )
		sun->numSamples = 1;
	
	/* set photons */
	photons = sun->photons / sun->numSamples;
	
	/* create the right number of suns */
	for( i = 0; i < sun->numSamples; i++ )
	{
		/* calculate sun direction */
		if( i == 0 )
			VectorCopy( sun->direction, direction );
		else
		{
			/*
				sun->direction[ 0 ] = cos( angle ) * cos( elevation );
				sun->direction[ 1 ] = sin( angle ) * cos( elevation );
				sun->direction[ 2 ] = sin( elevation );
				
				xz_dist   = sqrt( x*x + z*z )
				latitude  = atan2( xz_dist, y ) * RADIANS
				longitude = atan2( x,       z ) * RADIANS
			*/
			
			d = sqrt( sun->direction[ 0 ] * sun->direction[ 0 ] + sun->direction[ 1 ] * sun->direction[ 1 ] );
			angle = atan2( sun->direction[ 1 ], sun->direction[ 0 ] );
			elevation = atan2( sun->direction[ 2 ], d );
			
			/* jitter the angles (loop to keep random sample within sun->deviance steridians) */
			do
			{
				da = (Random() * 2.0f - 1.0f) * sun->deviance;
				de = (Random() * 2.0f - 1.0f) * sun->deviance;
			}
			while( (da * da + de * de) > (sun->deviance * sun->deviance) );
			angle += da;
			elevation += de;
			
			/* debug code */
			//%	Sys_Printf( "%d: Angle: %3.4f Elevation: %3.3f\n", sun->numSamples, (angle / Q_PI * 180.0f), (elevation / Q_PI * 180.0f) );
			
			/* create new vector */
			direction[ 0 ] = cos( angle ) * cos( elevation );
			direction[ 1 ] = sin( angle ) * cos( elevation );
			direction[ 2 ] = sin( elevation );
		}
		
		/* create a light */
		numSunLights++;
		light = (light_t *)safe_malloc( sizeof( *light ) );
		memset( light, 0, sizeof( *light ) );
		light->next = lights;
		lights = light;
		
		/* initialize the light */
		light->flags = LIGHT_SUN_DEFAULT;
		light->type = EMIT_SUN;
		light->fade = 1.0f;
		light->falloffTolerance = falloffTolerance;
		light->filterRadius = sun->filterRadius / sun->numSamples;
		light->style = noStyles ? LS_NORMAL : sun->style;
		
		/* set the light's position out to infinity */
		VectorMA( vec3_origin, (MAX_WORLD_COORD * 8.0f), direction, light->origin );	/* MAX_WORLD_COORD * 2.0f */
		
		/* set the facing to be the inverse of the sun direction */
		VectorScale( direction, -1.0, light->normal );
		light->dist = DotProduct( light->origin, light->normal );
		
		/* set color and photons */
		VectorCopy( sun->color, light->color );
		light->photons = photons * skyScale;
	}

	/* another sun? */
	if( sun->next != NULL )
		CreateSunLight( sun->next );
}



/*
CreateSkyLights() - ydnar
simulates sky light with multiple suns
*/

static void CreateSkyLights( vec3_t color, float value, int iterations, float filterRadius, int style )
{
	int			i, j, numSuns;
	int			angleSteps, elevationSteps;
	float		angle, elevation;
	float		angleStep, elevationStep;
	float		step, start;
	sun_t		sun;
	
	
	/* dummy check */
	if( value <= 0.0f || iterations < 2 )
		return;
	
	/* calculate some stuff */
	step = 2.0f / (iterations - 1);
	start = -1.0f;
	
	/* basic sun setup */
	VectorCopy( color, sun.color );
	sun.deviance = 0.0f;
	sun.filterRadius = filterRadius;
	sun.numSamples = 1;
	sun.style = noStyles ? LS_NORMAL : style;
	sun.next = NULL;
	
	/* setup */
	elevationSteps = iterations - 1;
	angleSteps = elevationSteps * 4;
	angle = 0.0f;
	elevationStep = DEG2RAD( 90.0f / iterations );	/* skip elevation 0 */
	angleStep = DEG2RAD( 360.0f / angleSteps );
	
	/* calc individual sun brightness */
	numSuns = angleSteps * elevationSteps + 1;
	sun.photons = value / numSuns;
	
	/* iterate elevation */
	elevation = elevationStep * 0.5f;
	angle = 0.0f;
	for( i = 0, elevation = elevationStep * 0.5f; i < elevationSteps; i++ )
	{
		/* iterate angle */
		for( j = 0; j < angleSteps; j++ )
		{
			/* create sun */
			sun.direction[ 0 ] = cos( angle ) * cos( elevation );
			sun.direction[ 1 ] = sin( angle ) * cos( elevation );
			sun.direction[ 2 ] = sin( elevation );
			CreateSunLight( &sun );
			
			/* move */
			angle += angleStep;
		}
			
		/* move */
		elevation += elevationStep;
		angle += angleStep / elevationSteps;
	}
	
	/* create vertical sun */
	VectorSet( sun.direction, 0.0f, 0.0f, 1.0f );
	CreateSunLight( &sun );
	
	/* short circuit */
	return;
}



/*
CreateEntityLights()
creates lights from light entities
*/

void CreateEntityLights( void )
{
	int				i, j;
	light_t			*light, *light2;
	entity_t		*e, *e2;
	const char		*name;
	const char		*target;
	vec3_t			dest;
	const char		*_color;
	float			intensity, scale, deviance, filterRadius;
	int				spawnflags, flags, numSamples, mindist;
	qboolean		junior;

	
	/* go throught entity list and find lights */
	for( i = 0; i < numEntities; i++ )
	{
		/* get entity */
		e = &entities[ i ];
		name = ValueForKey( e, "classname" );
		
		/* ydnar: check for lightJunior */
		if( Q_strncasecmp( name, "lightJunior", 11 ) == 0 )
			junior = qtrue;
		else if( Q_strncasecmp( name, "light", 5 ) == 0 )
			junior = qfalse;
		else
			continue;
		
		/* lights with target names (and therefore styles) are only parsed from BSP */
		target = ValueForKey( e, "targetname" );
		if( target[ 0 ] != '\0' && i >= numBSPEntities )
			continue;
		
		/* create a light */
		numPointLights++;
		light = (light_t *)safe_malloc( sizeof( *light ) );
		memset( light, 0, sizeof( *light ) );
		light->next = lights;
		lights = light;
		
		/* handle spawnflags */
		spawnflags = IntForKey( e, "spawnflags" );
		
		/* ydnar: quake 3+ light behavior */
		if( wolfLight == qfalse )
		{
			/* set default flags */
			flags = LIGHT_Q3A_DEFAULT;
			
			/* linear attenuation? */
			if( spawnflags & 1 )
			{
				flags |= LIGHT_ATTEN_LINEAR;
				flags &= ~LIGHT_ATTEN_ANGLE;
			}
			
			/* no angle attenuate? */
			if( spawnflags & 2 )
				flags &= ~LIGHT_ATTEN_ANGLE;
		}
		
		/* ydnar: wolf light behavior */
		else
		{
			/* set default flags */
			flags = LIGHT_WOLF_DEFAULT;
			
			/* inverse distance squared attenuation? */
			if( spawnflags & 1 )
			{
				flags &= ~LIGHT_ATTEN_LINEAR;
				flags |= LIGHT_ATTEN_ANGLE;
			}
			
			/* angle attenuate? */
			if( spawnflags & 2 )
				flags |= LIGHT_ATTEN_ANGLE;
		}
		
		/* other flags (borrowed from wolf) */
		
		/* wolf dark light? */
		if( (spawnflags & 4) || (spawnflags & 8) )
			flags |= LIGHT_DARK;
		
		/* nogrid? */
		if( spawnflags & 16 )
			flags &= ~LIGHT_GRID;
		
		/* junior? */
		if( junior )
		{
			flags |= LIGHT_GRID;
			flags &= ~LIGHT_SURFACES;
		}

		/* vortex: unnormalized? */
		if (spawnflags & 32 || colorNormalize == qfalse)
			flags |= LIGHT_UNNORMALIZED;

		/* vortex: distance atten? */
		if (spawnflags & 64)
			flags |= LIGHT_ATTEN_DISTANCE;

		/* store the flags */
		light->flags = flags;

		/* VorteX: mindist, was constant 16, now variable */
		mindist = IntForKey( e, "_mindist" );
		if ( mindist )
			light->mindist = mindist;
		else
			light->mindist = 16;
		
		/* ydnar: set fade key (from wolf) */
		light->fade = 1.0f;
		if( light->flags & LIGHT_ATTEN_LINEAR )
		{
			light->fade = FloatForKey( e, "fade" );
			if( light->fade == 0.0f )
				light->fade = 1.0f;
		}
		
		/* ydnar: set angle scaling (from vlight) */
		light->angleScale = FloatForKey( e, "_anglescale" );
		if( light->angleScale != 0.0f )
			light->flags |= LIGHT_ATTEN_ANGLE;
		
		/* set origin */
		GetVectorForKey( e, "origin", light->origin);
		light->style = IntForKey( e, "_style" );
		if( light->style == LS_NORMAL )
			light->style = IntForKey( e, "style" );
		if( light->style < LS_NORMAL || light->style >= LS_NONE )
			Error( "Invalid lightstyle (%d) on entity %d", light->style, i );
		
		/* override */
		if( noStyles )
			light->style = LS_NORMAL;
		
		/* set light intensity */
		intensity = FloatForKey( e, "_light" );
		if( intensity == 0.0f )
			intensity = FloatForKey( e, "light" );
		if( intensity == 0.0f)
			intensity = 300.0f;
		
		/* ydnar: set light scale (sof2) */
		scale = FloatForKey( e, "scale" );
		if( scale == 0.0f )
			scale = 1.0f;
		intensity *= scale;
		
		/* ydnar: get deviance and samples */
		/* vortex: -deviance, -devianceSamples and -novediance keys */
		if (noDeviance)
		{
			deviance = 0.0f;
			numSamples = 1;
		}
		else
		{
			deviance = FloatForKey( e, "_deviance" );
			if( deviance == 0.0f )
				deviance = FloatForKey( e, "_deviation" );
			if( deviance == 0.0f )
				deviance = FloatForKey( e, "_jitter" );
			numSamples = IntForKey( e, "_samples" );
			if (numSamples <= 0.0f)
				numSamples = devianceSamples;
			if (deviance < 0.0f || numSamples < 1 )
			{
				deviance = 0.0f;
				numSamples = 1;
			}
			if (deviance <= 0.0f)
				deviance = devianceJitter;
			if (deviance > 0.0f)
				if (numSamples <= 0.0f)
					numSamples = 8; // default samples if no deviance is set
			numSamples *= devianceSamplesScale;
			if (numSamples < 1)
				numSamples = 1;
		}
		intensity /= numSamples;
		
		/* ydnar: get filter radius */
		filterRadius = FloatForKey( e, "_filterradius" );
		if( filterRadius == 0.0f )
			filterRadius = FloatForKey( e, "_filteradius" );
		if( filterRadius == 0.0f )
			filterRadius = FloatForKey( e, "_filter" );
		if( filterRadius < 0.0f )
			filterRadius = 0.0f;
		light->filterRadius = filterRadius;
		
		/* set light color */
		_color = ValueForKey( e, "_color" );
		if( _color && _color[ 0 ] )
		{
			sscanf( _color, "%f %f %f", &light->color[ 0 ], &light->color[ 1 ], &light->color[ 2 ] );
			if( colorsRGB ) 
			{
				light->color[0] = srgb_to_linear( light->color[0] );
				light->color[1] = srgb_to_linear( light->color[1] );
				light->color[2] = srgb_to_linear( light->color[2] );
			}
			if (!(light->flags & LIGHT_UNNORMALIZED))
				ColorNormalize( light->color, light->color );
		}
		else
			light->color[ 0 ] = light->color[ 1 ] = light->color[ 2 ] = 1.0f;

		intensity = intensity * pointScale;
		light->photons = intensity;
		light->type = EMIT_POINT;
		
		/* set falloff threshold */
		light->falloffTolerance = falloffTolerance/ numSamples;
		
		/* lights with a target will be spotlights */
		target = ValueForKey( e, "target" );
		if( target[ 0 ] )
		{
			float		radius;
			float		dist;
			sun_t		sun;
			const char	*_sun;
			
			
			/* get target */
			e2 = FindTargetEntity( target );
			if( e2 == NULL )
				Sys_Warning( e->mapEntityNum, "Spotlight at (%f %f %f) has missing target", light->origin[ 0 ], light->origin[ 1 ], light->origin[ 2 ] );
			else
			{
				/* not a point light */
				numPointLights--;
				numSpotLights++;
				
				/* make a spotlight */
				GetVectorForKey( e2, "origin", dest );
				VectorSubtract( dest, light->origin, light->normal );
				dist = VectorNormalize( light->normal, light->normal );
				radius = FloatForKey( e, "radius" );
				if( !radius )
					radius = 64;
				if( !dist )
					dist = 64;
				light->radiusByDist = (radius + 16) / dist;
				light->type = EMIT_SPOT;
				
				/* ydnar: wolf mods: spotlights always use nonlinear + angle attenuation */
				light->flags &= ~LIGHT_ATTEN_LINEAR;
				light->flags |= LIGHT_ATTEN_ANGLE;
				light->fade = 1.0f;
				
				/* ydnar: is this a sun? */
				_sun = ValueForKey( e, "_sun" );
				if( _sun[ 0 ] == '1' )
				{
					/* not a spot light */
					numSpotLights--;
					
					/* unlink this light */
					lights = light->next;
					
					/* make a sun */
					VectorScale( light->normal, -1.0f, sun.direction );
					VectorCopy( light->color, sun.color );
					sun.photons = (intensity / pointScale);
					sun.deviance = deviance / 180.0f * Q_PI;
					sun.numSamples = numSamples;
					sun.style = noStyles ? LS_NORMAL : light->style;
					sun.next = NULL;
					
					/* make a sun light */
					CreateSunLight( &sun );
					
					/* free original light */
					free( light );
					light = NULL;
					
					/* skip the rest of this love story */
					continue;
				}
			}
		}
		
		/* jitter the light */
		for( j = 1; j < numSamples; j++ )
		{
			/* create a light */
			light2 = (light_t *)safe_malloc( sizeof( *light ) );
			memcpy( light2, light, sizeof( *light ) );
			light2->next = lights;
			lights = light2;
			
			/* add to counts */
			if( light->type == EMIT_SPOT )
				numSpotLights++;
			else
				numPointLights++;

			/* apply deviance */
			if (devianceForm == 1)
			{
				/* spherical jitter */
				scale = Random();
				dest[0] = Random() * 2.0f - 1.0;
				dest[1] = Random() * 2.0f - 1.0;

				dest[2] = Random() * 2.0f - 1.0;
				VectorNormalize(dest, dest);
				VectorScale(dest, scale, dest);
			}
			else
			{
				/* box jitter */
				dest[0] = Random() * 2.0f - 1.0;
				dest[1] = Random() * 2.0f - 1.0;
				dest[2] = Random() * 2.0f - 1.0;
				scale = min(1.0, VectorLength(dest) / 1.43f);
				scale = sqrt(deviance * deviance);
			}

			/* soft light emitting zone falloff */
			if (devianceAtten)
			{
				scale = sqrt(1.0f - scale);
				VectorScale(light2->color, scale, light2->color);
			}

			/* jitter it */
			light2->origin[ 0 ] = light->origin[ 0 ] + dest[ 0 ] * deviance;
			light2->origin[ 1 ] = light->origin[ 1 ] + dest[ 1 ] * deviance;
			light2->origin[ 2 ] = light->origin[ 2 ] + dest[ 2 ] * deviance;
		}
	}
}

/*
CreateSurfaceLights() - ydnar
this hijacks the radiosity code to generate surface lights for first pass
*/

#define APPROX_BOUNCE	1.0f

void CreateSurfaceLights( void )
{
	int					i;
	bspDrawSurface_t	*ds;
	surfaceInfo_t		*info;
	shaderInfo_t		*si;
	light_t				*light;
	float				subdivide;
	vec3_t				origin;
	clipWork_t			cw;
	const char			*nss;
	
	
	/* get sun shader supressor */
	nss = ValueForKey( &entities[ 0 ], "_noshadersun" );
	
	/* walk the list of surfaces */
	for( i = 0; i < numBSPDrawSurfaces; i++ )
	{
		/* get surface and other bits */
		ds = &bspDrawSurfaces[ i ];
		info = &surfaceInfos[ i ];
		si = info->si;
		
		/* sunlight? */
		if( si->sun != NULL && nss[ 0 ] != '1' )
		{
			Sys_FPrintf( SYS_VRB, "Sun: %s\n", si->shader );
			CreateSunLight( si->sun );
			si->sun = NULL; /* FIXME: leak! */
		}
		
		/* sky light? */
		if( si->skyLightValue > 0.0f )
		{
			Sys_FPrintf( SYS_VRB, "Sky: %s\n", si->shader );
			CreateSkyLights( si->color, si->skyLightValue, si->skyLightIterations, si->lightFilterRadius, si->lightStyle );
			skyLightSurfaces = qtrue;
			si->skyLightValue = 0.0f;	/* FIXME: hack! */
		}
		
		/* try to early out */
		if( si->value <= 0 )
			continue;
		
		/* autosprite shaders become point lights */
		if( si->autosprite )
		{
			/* create an average xyz */
			VectorAdd( info->mins, info->maxs, origin );
			VectorScale( origin, 0.5f, origin );
			
			/* create a light */
			light = (light_t *)safe_malloc( sizeof( *light ) );
			memset( light, 0, sizeof( *light ) );
			light->next = lights;
			lights = light;
			
			/* set it up */
			light->flags = LIGHT_Q3A_DEFAULT;
			light->type = EMIT_POINT;
			light->photons = si->value * pointScale;
			light->fade = 1.0f;
			light->si = si;
			VectorCopy( origin, light->origin );
			VectorCopy( si->color, light->color );
			light->falloffTolerance = falloffTolerance;
			light->style = si->lightStyle;
			
			/* add to point light count and continue */
			numPointLights++;
			continue;
		}
		
		/* get subdivision amount */
		if( si->lightSubdivide > 0 )
			subdivide = si->lightSubdivide;
		else
			subdivide = defaultLightSubdivide;
		
		/* switch on type */
		switch( ds->surfaceType )
		{
			case MST_PLANAR:
			case MST_TRIANGLE_SOUP:
				RadLightForTriangles( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
				break;
			
			case MST_PATCH:
				RadLightForPatch( i, 0, info->lm, si, APPROX_BOUNCE, subdivide, &cw );
				break;
			
			default:
				break;
		}
	}
}



/*
SetEntityOrigins()
find the offset values for inline models
*/

void SetEntityOrigins( void )
{
	int					i, j, k, f;
	entity_t			*e;
	vec3_t				origin;
	const char			*key;
	int					modelnum;
	bspModel_t			*dm;
	bspDrawSurface_t	*ds;
	
	
	/* ydnar: copy drawverts into private storage for nefarious purposes */
	yDrawVerts = (bspDrawVert_t *)safe_malloc( numBSPDrawVerts * sizeof( bspDrawVert_t ) );
	memcpy( yDrawVerts, bspDrawVerts, numBSPDrawVerts * sizeof( bspDrawVert_t ) );
	
	/* set the entity origins */
	for( i = 0; i < numEntities; i++ )
	{
		/* get entity and model */
		e = &entities[ i ];
		key = ValueForKey( e, "model" );
		if( key[ 0 ] != '*' )
			continue;
		modelnum = atoi( key + 1 );
		dm = &bspModels[ modelnum ];
		
		/* get entity origin */
		key = ValueForKey( e, "origin" );
		if( key[ 0 ] == '\0' )
			continue;
		GetVectorForKey( e, "origin", origin );
		
		/* set origin for all surfaces for this model */
		for( j = 0; j < dm->numBSPSurfaces; j++ )
		{
			/* get drawsurf */
			ds = &bspDrawSurfaces[ dm->firstBSPSurface + j ];
			
			/* set its verts */
			for( k = 0; k < ds->numVerts; k++ )
			{
				f = ds->firstVert + k;
				VectorAdd( origin, bspDrawVerts[ f ].xyz, yDrawVerts[ f ].xyz );
			}
		}
	}
}

/*
================================================================================

 SAMPLING

================================================================================
*/

/*
PointToPolygonFormFactor()
calculates the area over a point/normal hemisphere a winding covers
ydnar: fixme: there has to be a faster way to calculate this
without the expensive per-vert sqrts and transcendental functions
ydnar 2002-09-30: added -faster switch because only 19% deviance > 10%
between this and the approximation
*/

#define ONE_OVER_2PI	0.159154942f	//% (1.0f / (2.0f * 3.141592657f))

float PointToPolygonFormFactor( const vec3_t point, const vec3_t normal, const winding_t *w )
{
	vec3_t		triVector, triNormal;
	int			i, j;
	vec3_t		dirs[ MAX_POINTS_ON_WINDING ];
	float		total;
	float		dot, angle, facing;
	
	
	/* this is expensive */
	for( i = 0; i < w->numpoints; i++ )
	{
		VectorSubtract( w->p[ i ], point, dirs[ i ] );
		VectorNormalize( dirs[ i ], dirs[ i ] );
	}
	
	/* duplicate first vertex to avoid mod operation */
	VectorCopy( dirs[ 0 ], dirs[ i ] );
	
	/* calculcate relative area */
	total = 0.0f;
	for( i = 0; i < w->numpoints; i++ )
	{
		/* get a triangle */
		j = i + 1;
		dot = DotProduct( dirs[ i ], dirs[ j ] );
		
		/* roundoff can cause slight creep, which gives an IND from acos */
		if( dot > 1.0f )
			dot = 1.0f;
		else if( dot < -1.0f )
			dot = -1.0f;
		
		/* get the angle */
		angle = acos( dot );
		
		CrossProduct( dirs[ i ], dirs[ j ], triVector );
		if( VectorNormalize( triVector, triNormal ) < 0.0001f )
			continue;
		
		facing = DotProduct( normal, triNormal );
		total += facing * angle;
		
		/* ydnar: this was throwing too many errors with radiosity + crappy maps. ignoring it. */
		if( total > 6.3f || total < -6.3f )
			return 0.0f;
	}
	
	/* now in the range of 0 to 1 over the entire incoming hemisphere */
	total *= ONE_OVER_2PI;
	return total;
}

/*
LightContribution()
determines the amount of light reaching a sample (luxel or vertex) from a given light
*/

int LightContribution( trace_t *trace, int lightflags, qboolean point3d )
{
	light_t	*light;
	float angle;
	float add;
	float dist;
 
	/* get light */
	light = trace->light;
	
	/* clear color */
	VectorClear( trace->color );
	VectorClear( trace->colorNoShadow );
	
	/* ydnar: early out */
	if( (light->flags & lightflags) != lightflags || light->envelope <= 0.0f )
		return qfalse;
	
	/* do some culling checks */
	if( light->type != EMIT_SUN )
	{
		/* sun only? */
		if( sunOnly )
			return qfalse;

		/* MrE: if the light is behind the surface */
		if( !point3d && trace->twoSided == qfalse )
			if( DotProduct( light->origin, trace->normal ) - DotProduct( trace->origin, trace->normal ) < 0.0f )
				return 0;
		
		/* ydnar: test pvs */
		if( !ClusterVisible( trace->cluster, light->cluster ) )
			return 0;
	}
	
	/* exact point to polygon form factor */
	if( light->type == EMIT_AREA )
	{
		float		factor;
		float		d;
		vec3_t		pushedOrigin;
		
		/* project sample point into light plane */
		d = DotProduct( trace->origin, light->normal ) - light->dist;
		if( d < 3.0f )
		{
			/* sample point behind plane? */
			if( !(light->flags & LIGHT_TWOSIDED) && d < -1.0f )
				return 0;
			
			/* sample plane coincident? */
			if (!point3d)
				if( d > -3.0f && DotProduct( trace->normal, light->normal ) > 0.9f )
					return 0;
		}
		
		/* nudge the point so that it is clearly forward of the light */
		/* so that surfaces meeting a light emiter don't get black edges */
		if( d > -8.0f && d < 8.0f )
			VectorMA( trace->origin, (8.0f - d), light->normal, pushedOrigin );				
		else
			VectorCopy( trace->origin, pushedOrigin );
		
		/* get direction and distance */
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if( dist >= light->envelope )
			return 0;
		
		/* ptpff approximation */
		if( faster )
		{
			/* angle attenuation */
			angle = point3d ? 1.0f : DotProduct( trace->normal, trace->direction );
			
			/* twosided lighting */
			if( trace->twoSided )
				angle = fabs( angle );
			
			/* attenuate */
			angle *= -DotProduct( light->normal, trace->direction );
			if( angle == 0.0f )
				return 0;
			else if( angle < 0.0f && (trace->twoSided || (light->flags & LIGHT_TWOSIDED)) )
				angle = -angle;
			add = light->photons / (dist * dist) * angle;
		}
		else
		{
			/* calculate the contribution */
			factor = PointToPolygonFormFactor( pushedOrigin, (point3d ? trace->direction : trace->normal), light->w );
			if( factor == 0.0f )
				return 0;
			else if( factor < 0.0f )
			{
				/* twosided lighting */
				if( trace->twoSided || (light->flags & LIGHT_TWOSIDED) )
				{
					factor = -factor;

					/* push light origin to other side of the plane */
					VectorMA( light->origin, -2.0f, light->normal, trace->end );
					dist = SetupTrace( trace );
					if( dist >= light->envelope )
						return 0;
				}
				else
					return 0;
			}
			
			/* ydnar: moved to here */
			add = factor * light->add;
		}
	}
	
	/* point/spot lights */
	else if( light->type == EMIT_POINT || light->type == EMIT_SPOT )
	{
		/* get direction and distance */
		VectorCopy( light->origin, trace->end );
		dist = SetupTrace( trace );
		if( dist >= light->envelope )
			return 0;
		
		/* clamp the distance to prevent super hot spots */
		if( dist < light->mindist )
			dist = light->mindist;
		
		/* angle attenuation */
		angle = 1.0f;
		if (light->flags & LIGHT_ATTEN_ANGLE)
		{
			if( point3d )
				angle = 0.7f; // vortex: 0.6 is average angle atten when tracing against sphere, 0.7 fits most lightmapped surfaces
			else
			{
				/* standard Lambert attenuation */ 
				angle = DotProduct( trace->normal, trace->direction );

				/* twosided lighting */
				if( trace->twoSided )
					angle = fabs( angle );

				/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
				if( lightAngleHL )
				{
					if( angle > 0.001f ) 
					{
						// skip coplanar
						if( angle > 1.0f )
							angle = 1.0f;
						angle = ( angle * 0.5f ) + 0.5f;
						angle *= angle;
					}
					else
						angle = 0;
				}

				/* angle attenuation scale */
				if( light->angleScale != 0.0f)
				{
					angle /= light->angleScale;
					if( angle > 1.0f )
						angle = 1.0f;
				}
			}
		}

		/* attenuate */
		if( light->flags & LIGHT_ATTEN_LINEAR )
		{
			add = angle * light->photons * linearScale - (dist * light->fade);
			if( add < 0.0f )
				add = 0.0f;
		}
		else
			add = light->photons / ( dist * dist ) * angle;

		/* handle spotlights */
		if( light->type == EMIT_SPOT )
		{
			float	distByNormal, radiusAtDist, sampleRadius;
			vec3_t	pointAtDist, distToSample;
	
			/* do cone calculation */
			distByNormal = -DotProduct( trace->displacement, light->normal );
			if( distByNormal < 0.0f )
				return 0;
			VectorMA( light->origin, distByNormal, light->normal, pointAtDist );
			radiusAtDist = light->radiusByDist * distByNormal;
			VectorSubtract( trace->origin, pointAtDist, distToSample );
			sampleRadius = VectorLength( distToSample );
			
			/* outside the cone */
			if( sampleRadius >= radiusAtDist )
				return 0;
			
			/* attenuate */
			if( sampleRadius > (radiusAtDist - 32.0f) )
				add *= ((radiusAtDist - sampleRadius) / 32.0f);
		}
	}
	
	/* ydnar: sunlight */
	else if( light->type == EMIT_SUN )
	{
		/* get origin and direction */
		VectorAdd( trace->origin, light->origin, trace->end );
		dist = SetupTrace( trace );
		
		/* angle attenuation */
		angle = 1.0f;
		if (light->flags & LIGHT_ATTEN_ANGLE)
		{
			if( point3d )
				angle = 0.7f; // vortex: 0.6 is average angle atten when tracing against sphere, 0.7 fits most lightmapped surfaces
			else
			{
				/* standard Lambert attenuation */ 
				angle = DotProduct( trace->normal, trace->direction );

				/* twosided lighting */
				if( trace->twoSided )
					angle = fabs( angle );

				/* jal: optional half Lambert attenuation (http://developer.valvesoftware.com/wiki/Half_Lambert) */
				if( lightAngleHL )
				{
					if( angle > 0.001f ) 
					{
						// skip coplanar
						if( angle > 1.0f )
							angle = 1.0f;
						angle = ( angle * 0.5f ) + 0.5f;
						angle *= angle;
					}
					else
						angle = 0;
				}

				/* angle attenuation scale */
				if( light->angleScale != 0.0f)
				{
					angle /= light->angleScale;
					if( angle > 1.0f )
						angle = 1.0f;
				}
			}
		}
	
		/* attenuate */
		add = light->photons * angle;
		if( add <= 0.0f )
			return 0;

		/* VorteX: set noShadow color */
		VectorScale(light->color, add, trace->colorNoShadow);
		
		/* setup trace */
		trace->testAll = qtrue;
		VectorScale( light->color, add, trace->color );
		
		/* trace to point */
		if( trace->testOcclusion && !trace->forceSunlight )
		{
			/* raytrace */
			TraceLine( trace );
			if( !(trace->compileFlags & C_SKY) || trace->opaque )
			{
				VectorClear( trace->color );
				return -1;
			}
		}
		
		/* return to sender */
		return 1;
	}

	/* VorteX: set noShadow color */
	VectorScale(light->color, add, trace->colorNoShadow);
	
	/* ydnar: changed to a variable number */
	if( add <= 0.0f || (add <= light->falloffTolerance && (light->flags & LIGHT_FAST_ACTUAL)) )
		return 0;
	
	/* setup trace */
	trace->testAll = qfalse;
	VectorScale( light->color, add, trace->color );
	
	/* raytrace */
	TraceLine( trace );
	if( trace->passSolid || trace->opaque )
	{
		VectorClear( trace->color );
		return -1;
	}
	
	/* return to sender */
	return 1;
}

/*
LightContributionAllStyles()
determines the amount of light reaching a sample (luxel or vertex)
*/

void LightContributionAllStyles( trace_t *trace, byte styles[ MAX_LIGHTMAPS ], vec3_t colors[ MAX_LIGHTMAPS ], int lightflags, qboolean point3d )
{
	int i, lightmapNum;
	
	/* clear colors */
	for( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		VectorClear( colors[ lightmapNum ] );
	
	/* ydnar: normalmap */
	if( !point3d && normalmap )
	{
		colors[ 0 ][ 0 ] = (trace->normal[ 0 ] + 1.0f) * 127.5f;
		colors[ 0 ][ 1 ] = (trace->normal[ 1 ] + 1.0f) * 127.5f;
		colors[ 0 ][ 2 ] = (trace->normal[ 2 ] + 1.0f) * 127.5f;
		return;
	}
	
	/* ydnar: don't bounce ambient all the time */
	if( !bouncing )
		VectorCopy( ambientColor, colors[ 0 ] );
	
	/* ydnar: trace to all the list of lights pre-stored in tw */
	for( i = 0; i < trace->numLights && trace->lights[ i ] != NULL; i++ )
	{
		/* setup trace */
		trace->light = trace->lights[ i ];

		/* style check */
		for( lightmapNum = 0; lightmapNum < MAX_LIGHTMAPS; lightmapNum++ )
		{
			if( styles[ lightmapNum ] == trace->light->style ||
				styles[ lightmapNum ] == LS_NONE )
				break;
		}
		
		/* max of MAX_LIGHTMAPS (4) styles allowed to hit a sample */
		if( lightmapNum >= MAX_LIGHTMAPS )
			continue;
		
		/* sample light */
		LightContribution( trace, lightflags, point3d );
		if( trace->color[ 0 ] == 0.0f && trace->color[ 1 ] == 0.0f && trace->color[ 2 ] == 0.0f )
			continue;
		
		/* handle negative light */
		if( trace->light->flags & LIGHT_NEGATIVE )
			VectorNegate( trace->color, trace->color );
		
		/* set style */
		styles[ lightmapNum ] = trace->light->style;
		
		/* add it */
		VectorAdd( colors[ lightmapNum ], trace->color, colors[ lightmapNum ] );
	}
}

/*
LightContributionSuper()
LightContribution with recursive supersampling
*/

int LightContributionSuper(trace_t *trace, int lightflags, qboolean point3d, int samples, const vec3_t mv1, const vec3_t mv2, const vec3_t mv3, vec3_t sampleSize )
{
	vec3_t total, totalnoshadow, totaldir, baseOrigin, scaledSampleSize;
	int i, c, numSamples, maxContribution, oldCluster;
	float oldInhibitRadius, scaledInhibitRadius;

	static const vec3_t SuperSamples[ 8 ] = 
	{
		{  0.5,  0.5,  0.5 },
		{  0.5, -0.5,  0.5 },
		{ -0.5, -0.5,  0.5 },
		{ -0.5,  0.5,  0.5 },
		{  0.5,  0.5, -0.5 },
		{  0.5, -0.5, -0.5 },
		{ -0.5, -0.5, -0.5 },
		{ -0.5,  0.5, -0.5 }
	};
	
	/* init */
	VectorCopy(trace->origin, baseOrigin);
	VectorClear(total);
	VectorClear(totalnoshadow);
	VectorClear(totaldir);
	oldCluster = trace->cluster;
	maxContribution = -1;
	numSamples = 0;

	/* futher supersampling */
	if (samples > 2)
	{
		VectorScale( sampleSize, 0.5, scaledSampleSize );
		oldInhibitRadius = trace->inhibitRadius;
		scaledInhibitRadius = oldInhibitRadius * 0.5;
	}

	/* gather */
	if( point3d )
	{
		/* gather the 8 points */
		for( i = 0; i < 8; i++ )
		{	
			trace->origin[0] = baseOrigin[0] + mv1[0]*SuperSamples[i][0]*sampleSize[0] + mv2[0]*SuperSamples[i][1]*sampleSize[1] + mv3[0]*SuperSamples[i][2]*sampleSize[2];
			trace->origin[1] = baseOrigin[1] + mv1[1]*SuperSamples[i][0]*sampleSize[0] + mv2[1]*SuperSamples[i][1]*sampleSize[1] + mv3[1]*SuperSamples[i][2]*sampleSize[2];
			trace->origin[2] = baseOrigin[2] + mv1[2]*SuperSamples[i][0]*sampleSize[0] + mv2[2]*SuperSamples[i][1]*sampleSize[1] + mv3[2]*SuperSamples[i][2]*sampleSize[2];
			trace->cluster = ClusterForPoint( trace->origin );
			if( trace->cluster < 0 )
				continue;
			if( samples <= 2 )
				c = LightContribution( trace, lightflags, point3d );
			else
			{
				trace->inhibitRadius = scaledInhibitRadius;
				c = LightContributionSuper( trace, lightflags, point3d, samples - 1, mv1, mv2, mv3, scaledSampleSize );
				trace->inhibitRadius = oldInhibitRadius;
			}
			VectorAdd(total, trace->color, total);
			VectorAdd(totalnoshadow, trace->colorNoShadow, totalnoshadow);
			VectorMA(totaldir, VectorLength( trace->colorNoShadow ), trace->direction, totaldir);
			maxContribution = max( c, maxContribution );
			numSamples += (c <= 0) ? 2 : 1;
		}
	}
	else
	{
		/* gather the 4 points */
		for( i = 0; i < 4; i++ )
		{
			trace->origin[0] = baseOrigin[0] + mv1[0]*SuperSamples[i][0]*sampleSize[0] + mv2[0]*SuperSamples[i][1]*sampleSize[1];
			trace->origin[1] = baseOrigin[1] + mv1[1]*SuperSamples[i][0]*sampleSize[0] + mv2[1]*SuperSamples[i][1]*sampleSize[1];
			trace->origin[2] = baseOrigin[2] + mv1[2]*SuperSamples[i][0]*sampleSize[0] + mv2[2]*SuperSamples[i][1]*sampleSize[1];
			trace->cluster = ClusterForPoint( trace->origin );
			if( trace->cluster < 0 )
				continue;
			if( samples <= 2 )
				c = LightContribution( trace, lightflags, point3d );
			else
			{
				trace->inhibitRadius = scaledInhibitRadius;
				c = LightContributionSuper( trace, lightflags, point3d, samples - 1, mv1, mv2, mv3, scaledSampleSize );
				trace->inhibitRadius = oldInhibitRadius;
			}
			VectorAdd(total, trace->color, total);
			VectorAdd(totalnoshadow, trace->colorNoShadow, totalnoshadow);
			VectorMA(totaldir, VectorLength( trace->colorNoShadow ), trace->direction, totaldir);
			maxContribution = max( c, maxContribution );
			numSamples++;
		}
	}

	/* average */
	if( numSamples > 0 )
	{
		total[0] /= numSamples;
		total[1] /= numSamples;
		total[2] /= numSamples;
		totalnoshadow[0] /= numSamples;
		totalnoshadow[1] /= numSamples;
		totalnoshadow[2] /= numSamples;
	}
	VectorCopy(total, trace->color);
	VectorCopy(totalnoshadow, trace->colorNoShadow);
	VectorNormalize( totaldir, trace->direction );

	/* restore trace */
	VectorCopy( baseOrigin, trace->origin );
	trace->cluster = oldCluster;

	/* return to sender */
	return 1;
}

/*
================================================================================

 LIGHTGRID

================================================================================
*/

#define GRID_NUM_NORMALS		42
#define GRID_NUM_OFFSETS		14
#define	GRID_MAX_CONTRIBUTIONS	32768

vec3_t GridNormalsModel[GRID_NUM_NORMALS] = { {8.72255, 5.42647, -0.04020}, {10.25000, -0.04020, -0.04020}, {8.32059, 3.17549, 5.10490}, {5.42647, -0.04020, 8.72255}, {8.32059, -

3.17549, 5.10490}, {8.72255, -5.42647, -0.04020}, {5.10490, 8.32059, 3.17549}, {-0.04020, 8.72255, 5.42647}, {3.17549, 5.10490, 8.32059}, {-3.17549, 5.10490, 8.32059}, {-5.42647, -

0.04020, 8.72255}, {-0.04020, -0.04020, 10.25000}, {3.17549, -5.10490, 8.32059}, {-3.17549, -5.10490, 8.32059}, {-0.04020, -8.72255, 5.42647}, {5.10490, -8.32059, 3.17549}, {-0.04020, 

-10.25000, -0.04020}, {-0.04020, -8.72255, -5.42647}, {5.10490, -8.32059, -3.17549}, {-5.10490, -8.32059, 3.17549}, {-8.72255, -5.42647, -0.04020}, {-5.10490, -8.32059, -3.17549}, {-

8.32059, -3.17549, 5.10490}, {-8.32059, 3.17549, 5.10490}, {-8.72255, 5.42647, -0.04020}, {-10.25000, -0.04020, -0.04020}, {-8.32059, 3.17549, -5.10490}, {-5.42647, -0.04020, -8.72255}, 

{-8.32059, -3.17549, -5.10490}, {-3.17549, -5.10490, -8.32059}, {-0.04020, -0.04020, -10.25000}, {5.42647, -0.04020, -8.72255}, {3.17549, -5.10490, -8.32059}, {-3.17549, 5.10490, -

8.32059}, {-0.04020, 8.72255, -5.42647}, {3.17549, 5.10490, -8.32059}, {5.10490, 8.32059, -3.17549}, {8.32059, 3.17549, -5.10490}, {8.32059, -3.17549, -5.10490}, {-0.04020, 10.25000, -

0.04020}, {-5.10490, 8.32059, 3.17549}, {-5.10490, 8.32059, -3.17549} };

vec3_t GridNormals[GRID_NUM_NORMALS];

vec3_t GridOffsetsModel[ GRID_NUM_OFFSETS ] =
{
	{ 0,  0,  1 },  // up
	{ 0,  0, -2 },  // down
	{ 0, -1,  1 },  // left
	{ 0,  2,  0 },  // right
	{ 1, -1,  0 },  // forward
	{-2,  0,  0 },  // backward
	{ 2, -1,  1 },  // up forward left
	{ 0,  0, -2 },  // down forward left
	{ 0,  2,  2 },  // up forward right
	{ 0,  0, -2 },  // down forward right
	{-2, -2,  2 },  // up back left
	{ 0,  0, -2 },  // down back left
	{ 0,  2,  2 },  // up back right
	{ 0,  0, -2 }   // down back right
};

vec3_t GridOffsets[ GRID_NUM_OFFSETS ];

typedef struct
{
	vec3_t		dir;
	vec3_t		color;
	int			style;
}
gridContribution_t;

int gridPointsMapped = 0;
int gridPointsIlluminated = 0;
int gridPointsOccluded = 0;
int gridPointsFlooded = 0;
int gridSampleLightmap = 0;

/*
AllocateGridArea
adds a new lightgrid brush
*/

void AllocateGridArea(vec3_t mins, vec3_t maxs)
{
	if ( numGridAreas >= MAX_GRID_AREAS )
	{
		Error("MAX_LIGHTGRID_AREAS reached (%i)\n", MAX_GRID_AREAS );
		return;
	}

	VectorCopy( mins, gridAreas[ numGridAreas ].mins );
	VectorCopy( maxs, gridAreas[ numGridAreas ].maxs );
	numGridAreas++;

	//Sys_Printf("AllocateGridArea: { %f %f %f } { %f %f %f }\n", mins[ 0 ], mins[ 1 ], mins[ 2 ], maxs[ 0 ], maxs[ 1 ], maxs[ 2 ]);
}


/*
IlluminateGridPoint
new version of lightgrid tracer
- better detection of direction/ambient light
- fully match lightmap
todo: support minlight
*/

#define GRID_BLOCK_OPTIMIZATION
void IlluminateGridPoint(trace_t *trace, int num, rawLightmap_t **lightmaps, int numLightmaps)
{
	int						i, j, x, y, z, mod, numContributions = 0, *cluster;
	float					d, shade, ambient, ambientLevel, *luxel, *normal, *origin;
	vec3_t					baseOrigin, nudgedOrigin, color, mins, maxs;
	rawGridPoint_t			*gp;
	bspGridPoint_t			*bgp;
	rawLightmap_t           *lm;
	gridContribution_t		contributions[ GRID_MAX_CONTRIBUTIONS ];
	float					addSize, pointMap[GRID_NUM_NORMALS];
	static const vec3_t     forward = { 1.0f, 0.0f, 0.0f };
	static const vec3_t     right = { 0.0f, 1.0f, 0.0f };
	static const vec3_t     up = { 0.0f, 0.0f, 1.0f };
	qboolean                flood;
#ifdef GRID_BLOCK_OPTIMIZATION
	int						l;
#endif
	
	/* get grid points */
	gp = &rawGridPoints[ num ];
	bgp = &bspGridPoints[ num ];

	/* flood unmapped points */
	if( gp->mapped == qfalse )
	{
		if( gp->flooded == qfalse )
			gridPointsOccluded++;
		gp->flooded = qtrue;
		return;
	}

	/* get origin */
	mod = num; 
	z = mod / (gridBounds[ 0 ] * gridBounds[ 1 ]);
	mod -= z * (gridBounds[ 0 ] * gridBounds[ 1 ]); 
	y = mod / gridBounds[ 0 ];
	mod -= y * gridBounds[ 0 ];
	x = mod;
	baseOrigin[ 0 ] = gridMins[ 0 ] + x * gridSize[ 0 ];
	baseOrigin[ 1 ] = gridMins[ 1 ] + y * gridSize[ 1 ];
	baseOrigin[ 2 ] = gridMins[ 2 ] + z * gridSize[ 2 ];

	/* init sample */
	memset(pointMap, 0, sizeof(float) * GRID_NUM_NORMALS);
	numContributions = 0;

	/* trace the point type (illuminated, flooded) */
	flood = qtrue;
	trace->inhibitRadius = 0.125f;
	for( i = 0; i < GRID_NUM_OFFSETS; i++ )
	{
		VectorMA( baseOrigin, 2.0, GridOffsets[ i ], nudgedOrigin );
		VectorCopy( baseOrigin, trace->origin );
		VectorCopy( nudgedOrigin, trace->end );
		trace->cluster = ClusterForPoint( trace->origin );
		SetupTrace( trace );
		TraceLine( trace );
		if( trace->opaque == qfalse )
		{
			flood = qfalse;
			break;
		}
	}

	/* flood bad points later */
	if( flood == qtrue )
	{
		if( gp->flooded == qfalse )
			gridPointsOccluded++;
		gp->flooded = qtrue;
		return;
	}

	/* set up inhibit sphere */
	trace->inhibitRadius = (gridSize[ 0 ] + gridSize[ 1 ] + gridSize[ 2 ]) / 3.0f * 0.75f;

	/* find point cluster */
	trace->cluster = ClusterForPoint( baseOrigin );
	if( trace->cluster < 0 )
	{
		/* try nudge */
		for( i = 0; i < GRID_NUM_OFFSETS; i++ )
		{
			VectorAdd( baseOrigin, GridOffsets[ i ], nudgedOrigin );
			trace->cluster = ClusterForPoint( nudgedOrigin );
			if( trace->cluster >= 0 )
				break;
		}
		/* succesfully nudged */
		if( i < GRID_NUM_OFFSETS )
			VectorCopy( nudgedOrigin, baseOrigin );
	}
	VectorCopy( baseOrigin, trace->origin );

	/* flood bad clusters later */
	if( trace->cluster < 0 )
	{
		if( gp->flooded == qfalse )
			gridPointsOccluded++;
		gp->flooded = qtrue;
		return;
	}
	
	/* trace lighting for grid point */
	VectorSet( color, 0, 0, 0 );

	/* trace to all the lights, find the major light direction */
#ifndef GRID_BLOCK_OPTIMIZATION
	for( trace->light = lights; trace->light != NULL; trace->light = trace->light->next )
	{
#else
	for( l = 0; l < trace->numLights; l++ )
	{
		trace->light = trace->lights[ l ];
#endif
		/* sample light */
		if( gridSuperSample > 1 )
		{
			if (!LightContributionSuper( trace, LIGHT_GRID, qtrue, gridSuperSample, forward, right, up, gridSize ))
				continue;
		}
		else if( !LightContribution( trace, LIGHT_GRID, qtrue ) )
			continue;
		
		/* handle negative light */
		if( trace->light->flags & LIGHT_NEGATIVE )
			VectorNegate( trace->color, trace->color );

		/* add a contribution */
		addSize = VectorLength( trace->color );
		VectorAdd( color, trace->color, color );
		VectorCopy( trace->color, contributions[ numContributions ].color );
		VectorCopy( trace->direction, contributions[ numContributions ].dir );
		contributions[ numContributions ].style = trace->light->style;
		numContributions++;
		
		/* push average direction around */
		addSize = VectorLength( trace->color );
		VectorMA( gp->dir, addSize, trace->direction, gp->dir );

		/* shade the pointmap (used to calculate ambient lighting) */
		for( i = 0; i < GRID_NUM_NORMALS; i++)
		{
			shade = DotProduct( GridNormals[ i ], trace->direction );
			if( shade <= 0 )
				continue;
			pointMap[ i ] += shade * (addSize / 255.0f);
		}

		/* stop after a while */
		if( numContributions >= GRID_MAX_CONTRIBUTIONS )
			break;
	}

	/* store cointributions count */
	gp->contributions = numContributions;
	
	/* normalize to get primary light direction */
	VectorNormalize( gp->dir, gp->dir );

	/* average pointmap to get minimal ambient component */
	ambientLevel = 0;
	for( i = 0; i < GRID_NUM_NORMALS; i++)
		if( pointMap[ i ] > 0.1 )
			ambientLevel += 1;
	ambientLevel /= GRID_NUM_NORMALS; // 51% of the sphere lit up is 2% ambient (so single lightsource will never generate ambient)
	ambient = max(0, ambientLevel * ambientLevel - 0.1);

	/* now that we have identified the primary light direction, go back and separate all the light into directed and ambient */
	for( i = 0; i < numContributions; i++ )
	{
		/* find appropriate style */
		if( contributions[ i ].style != 0 )
			continue;

		/* get relative directed strength */
		d = DotProduct( contributions[ i ].dir, gp->dir ) * (1.0 - ambient);
		if( d < 0.0f )
			d = 0.0f;

		/* add the directed color */
		VectorMA( gp->directed[ 0 ], d, contributions[ i ].color, gp->directed[ 0 ] );

		/* add ambient color */
		VectorMA( gp->ambient[ 0 ], 1.0 - d, contributions[ i ].color, gp->ambient[ 0 ] );
	}

	/* fix color by nearest lightmap samples (if presented) */
	if (numLightmaps && lightmaps)
	{
		VectorClear( color );
		numContributions = 0;

		/* get bounds */
		VectorCopy( baseOrigin, mins );
		VectorMA( mins, -0.05, gridSize, mins );
		VectorMA( mins, 1.1, gridSize, maxs );
		
		/* walk lightmaps */
		for( j = 0; j < numLightmaps; j++ )
		{
			/* get lightmap */
			lm = lightmaps[ j ];

			/* walk luxels */
			for( y = 0; y < lm->sh; y++ )
			{
				for( x = 0; x < lm->sw; x++ )
				{
					/* check cluster */
					cluster = SUPER_CLUSTER( x, y );
					if( *cluster < 0 )
						continue;

					/* check origin */
					origin = SUPER_ORIGIN( x, y );
					if( origin[ 0 ] > maxs[ 0 ] || origin[ 0 ] < mins[ 0 ] ||
						origin[ 1 ] > maxs[ 1 ] || origin[ 1 ] < mins[ 1 ] ||
						origin[ 2 ] > maxs[ 2 ] || origin[ 2 ] < mins[ 2 ] )
						continue;

					/* sample luxel */
					luxel = SUPER_LUXEL( 0, x, y );
					normal = SUPER_TRINORMAL( x, y );
					if (luxel[ 4 ] <= 0.0f)
						continue;
					if (normal[ 2 ] > 0.5)
					{
						VectorMA( color, 2.0, luxel, color );
						numContributions += 2;
					}
					else
					{
						VectorAdd( color, luxel, color );
						numContributions++;
					}
				}
			}
		}

		/* any samples? */
		if( numContributions )
		{
			VectorScale( color, 1.0 / (float)numContributions, color );
			ambient = gp->ambient[ 0 ][ 0 ] * 0.299 + gp->ambient[ 0 ][ 1 ] * 0.587 + gp->ambient[ 0 ][ 2 ] * 0.114;
			shade = gp->directed[ 0 ][ 0 ] * 0.299 + gp->directed[ 0 ][ 1 ] * 0.587 + gp->directed[ 0 ][ 2 ] * 0.114;
			d = shade / (ambient + shade);
			if( d < 0.0f )
				d = 0.0f;

			/* set the directed color */
			gp->directed[ 0 ][ 0 ] = gp->directed[ 0 ][ 0 ] * 0.33 + color[ 0 ] * d * 0.67;
			gp->directed[ 0 ][ 1 ] = gp->directed[ 0 ][ 1 ] * 0.33 + color[ 1 ] * d * 0.67;
			gp->directed[ 0 ][ 2 ] = gp->directed[ 0 ][ 2 ] * 0.33 + color[ 2 ] * d * 0.67;

			/* set ambient color */
			gp->ambient[ 0 ][ 0 ] = gp->ambient[ 0 ][ 0 ] * 0.33 + color[ 0 ] * (1.0 - d) * 0.67;
			gp->ambient[ 0 ][ 1 ] = gp->ambient[ 0 ][ 1 ] * 0.33 + color[ 1 ] * (1.0 - d) * 0.67;
			gp->ambient[ 0 ][ 2 ] = gp->ambient[ 0 ][ 2 ] * 0.33 + color[ 2 ] * (1.0 - d) * 0.67;

		}
		else
		{
			/* flood unsampled points (only color, leave direction) */
			//if( gp->flooded == qfalse )
			//	gridPointsOccluded++;
			//gp->flooded = qtrue;
			//gp->floodOnlyColor = qtrue;
		}
	}

	/* set min color */
	VectorMA( gp->ambient[ 0 ], 0.5, gp->directed[ 0 ], color );
	for( i = 0; i < 3; i++ )
		if( color[ i ] < minGridLight[ i ] )
			gp->ambient[ 0 ][ i ] += minGridLight[ i ] - color[ i ];

	/* set color mod */
	if( !bouncing )
	{
		for( i = 0; i < 3; i++ )
		{
			gp->ambient[ 0 ][ i ] *= colorMod[ i ];
			gp->directed[ 0 ][ i ] *= colorMod[ i ];
		}
	}
		
	/* store off sample */
	ColorToBytes( gp->ambient[ 0 ], bgp->ambient[ 0 ], 1.0, qfalse );
	ColorToBytes( gp->directed[ 0 ], bgp->directed[ 0 ], 1.0, qfalse );
	if( !bouncing )
		NormalToLatLong( gp->dir, bgp->latLong );
	gridPointsIlluminated++;
}

#ifdef GRID_BLOCK_OPTIMIZATION

/*
IlluminateGridBlock()
illuminates lightgrid block
*/

void IlluminateGridBlock( int num )
{
	int mod, x, y, z, i, j, k, px, py, pz, cluster, *clusters, numClusters, numLightmaps;
	vec3_t mins, maxs, normal, origin;
	rawLightmap_t *lm, *lms[ 32768 ];
	trace_t trace;

	/* get block coordinates */
	mod = num; 
	z = mod / (gridBlocks[ 0 ] * gridBlocks[ 1 ]);
	mod -= z * (gridBlocks[ 0 ] * gridBlocks[ 1 ]); 
	y = mod / gridBlocks[ 0 ];
	mod -= y * gridBlocks[ 0 ];
	x = mod;

	/* get min/max with 1 point margin */
	mins[ 0 ] = gridMins[ 0 ] + (x * gridBlockSize[ 0 ] - 1) * gridSize[ 0 ];
	mins[ 1 ] = gridMins[ 1 ] + (y * gridBlockSize[ 1 ] - 1) * gridSize[ 1 ];
	mins[ 2 ] = gridMins[ 2 ] + (z * gridBlockSize[ 2 ] - 1) * gridSize[ 2 ];
	maxs[ 0 ] = mins[ 0 ] + (gridBlockSize[ 0 ] + 2) * gridSize[ 0 ];
	maxs[ 1 ] = mins[ 1 ] + (gridBlockSize[ 1 ] + 2) * gridSize[ 1 ];
	maxs[ 2 ] = mins[ 2 ] + (gridBlockSize[ 2 ] + 2) * gridSize[ 2 ];
	VectorSet(normal, 0, 0, 0);

	/* transform block indexes to point indexes */
	x = x * gridBlockSize[ 0 ];
	y = y * gridBlockSize[ 1 ];
	z = z * gridBlockSize[ 2 ];

	/* setup trace */
	trace.entityNum = -1;
	trace.testOcclusion = (!noTrace && !noTraceGrid) ? qtrue : qfalse;
	trace.inhibitRadius = 0.125f;
	trace.twoSided = qfalse;
	trace.occlusionBias = 0;
	trace.forceSunlight = qfalse;
	trace.forceSelfShadow = qfalse;
	trace.recvShadows = WORLDSPAWN_RECV_SHADOWS;
	trace.numSurfaces = 0;
	trace.surfaces = NULL;
	trace.numLights = 0;
	trace.lights = NULL;

	/* allocate clusters */
	numClusters = 0;
	clusters = (int *)safe_malloc(sizeof(int) * gridBlockSize[ 0 ] * gridBlockSize[ 1 ] * gridBlockSize[ 2 ]);
	for( i = 0; i < gridBlockSize[ 2 ]; i++ )
	{
		pz = z + i;
		if (pz >= gridBounds[ 2 ])
			break;
		for( j = 0; j < gridBlockSize[ 1 ]; j++ )
		{
			py = y + j;
			if (py >= gridBounds[ 1 ])
				break;
			for( k = 0; k < gridBlockSize[ 0 ]; k++ )
			{
				px = x + k;
				if (px >= gridBounds[ 0 ])
					break;
				// get cluster
				origin[ 0 ] = gridMins[ 0 ] + px * gridSize[ 0 ];
				origin[ 1 ] = gridMins[ 1 ] + py * gridSize[ 1 ];
				origin[ 2 ] = gridMins[ 2 ] + pz * gridSize[ 2 ];
				cluster = ClusterForPoint( origin );
				if (cluster < 0)
					continue;
				clusters[ numClusters++ ] = cluster;
			}
		}
	}

	/* create trace lights */
	CreateTraceLightsForBounds( qtrue, mins, maxs, normal, numClusters, clusters, LIGHT_GRID, &trace );

	/* find lightmap surfaces */
	numLightmaps = 0;
	if( gridSampleLightmap )
	{
		for( i = 0; i < numRawLightmaps && numLightmaps < 32768; i++)
		{
			lm = &rawLightmaps[ i ];
			if( lm->mins[ 0 ] > maxs[ 0 ] || lm->maxs[ 0 ] < mins[ 0 ] ||
				lm->mins[ 1 ] > maxs[ 1 ] || lm->maxs[ 1 ] < mins[ 1 ] ||
				lm->mins[ 2 ] > maxs[ 2 ] || lm->maxs[ 2 ] < mins[ 2 ] )
				continue;
			lms[ numLightmaps++ ] = lm;
		}
	}

	/* debug code */
	//Sys_Printf("Grid Block %i - %i lights\n", num, trace.numLights );

	/* illuminate grid points */
	for( i = 0; i < gridBlockSize[ 2 ]; i++ )
	{
		pz = z + i;
		if (pz >= gridBounds[ 2 ])
			break;
		for( j = 0; j < gridBlockSize[ 1 ]; j++ )
		{
			py = y + j;
			if (py >= gridBounds[ 1 ])
				break;
			for( k = 0; k < gridBlockSize[ 0 ]; k++ )
			{
				px = x + k;
				if (px >= gridBounds[ 0 ])
					break;
				IlluminateGridPoint( &trace, (pz * gridBounds[1] + py) * gridBounds[0] + px, lms, numLightmaps );
			}
		}
	}

	/* free stuff */
	FreeTraceLights( &trace );
	free( clusters );
}

#else 

void IlluminateGridPointOld( int num )
{
	trace_t trace;

	trace.entityNum = -1;
	trace.testOcclusion = (!noTrace && !noTraceGrid) ? qtrue : qfalse;
	trace.inhibitRadius = 0.125f;
	trace.twoSided = qfalse;
	trace.occlusionBias = 0;
	trace.forceSunlight = qfalse;
	trace.forceSelfShadow = qfalse;
	trace.recvShadows = WORLDSPAWN_RECV_SHADOWS;
	trace.numSurfaces = 0;
	trace.surfaces = NULL;
	trace.numLights = 0;
	trace.lights = NULL;

	IlluminateGridPoint( &trace, num, NULL, 0 );
}

#endif

/*
FloodGridPoint
applies post-illuminate function to a grid point
*/

void FloodGridPoint(int num)
{
	vec3_t avgambient, avgdirected, avgdirection;
	rawGridPoint_t *gp, *ar, *sr;
	bspGridPoint_t *bgp;
	int x, y , z, mod, xw, yw, zw, i, j, k, avgsamples;
	float f;

	/* get grid point */
	gp = &rawGridPoints[ num ];
	bgp = &bspGridPoints[ num ];

	/* early out */
	if( gp->flooded == qfalse || gp->contributions > 0 )
		return;

	/* get location */
	mod = num;
	z = mod / (gridBounds[ 0 ] * gridBounds[ 1 ]);
	mod -= z * (gridBounds[ 0 ] * gridBounds[ 1 ]);
	y = mod / gridBounds[ 0 ];
	mod -= y * gridBounds[ 0 ];
	x = mod;
	
	/* shift calculated position */
	xw = yw = zw = 2;

	/* clear samples */
	VectorClear(avgambient);
	VectorClear(avgdirected);
	VectorClear(avgdirection);
	avgsamples = 0;

	/* get average samples */
	ar = &rawGridPoints[(z * gridBounds[1] + y) * gridBounds[0] + x];
	for( k = 0; k < zw; k++ )
	{
		if( z + k >= gridBounds[2] )
			continue;
		for( j = 0;j < yw;j++ )
		{
			if( y + j >= gridBounds[1] )
				continue;
			for( i = 0; i < xw; i++ )
			{
				if (x + i >= gridBounds[0])
					continue;		
				sr = ar + (k * gridBounds[1] + j) * gridBounds[0] + i;

				/* sample from illuminated or filled flooded points */
				if( (sr->flooded == qfalse || sr->contributions > 0) && sr != gp )
				{
					VectorAdd( avgambient, sr->ambient[ 0 ], avgambient );
					VectorAdd( avgdirected, sr->directed[ 0 ], avgdirected );
					VectorAdd( avgdirection, sr->dir, avgdirection );
					avgsamples++;
				}
			}
		}
	}

	/* can't average? */
	if( !avgsamples )
		return;
	
	/* average */
	f = 1.0f / avgsamples;
	VectorScale( avgambient, f, gp->ambient[ 0 ] );
	VectorScale( avgdirected, f, gp->directed[ 0 ] );
	if (gp->floodOnlyColor == qfalse)
		VectorNormalize( avgdirection, gp->dir );

	/* store off averaged sample */
	ColorToBytes( gp->ambient[ 0 ], bgp->ambient[ 0 ], 1.0, qfalse );
	ColorToBytes( gp->directed[ 0 ], bgp->directed[ 0 ], 1.0, qfalse );
	if( !bouncing )
		NormalToLatLong( gp->dir, bgp->latLong );

	/* mark as flooded, add to stats */
	gp->contributions = 1;
	gridPointsFlooded++;
}

/*
FinishIlluminateGrid()
flood uncalculated grid points from neighbors and print stats
*/

void FinishIlluminateGrid( void )
{
	int i, numFloodPasses, totalFlooded;

	/* flood (this is a fast operation, so no pacifier) */
	totalFlooded = 0;
	numFloodPasses = max( max( gridBounds[ 0 ], gridBounds[ 1 ] ), gridBounds[ 2 ] ) + 1;
	for (i = 0; i < numFloodPasses; i++)
	{
		gridPointsFlooded = 0;
		RunThreadsOnIndividual( numRawGridPoints, qfalse, FloodGridPoint );
		totalFlooded += gridPointsFlooded;

		/* early out (all points was flooded) */
		if( gridPointsFlooded == 0 )
			break;
	}
	
	/* print stats */
	Sys_FPrintf( SYS_VRB, "%9d flood passes\n", i );
	Sys_FPrintf( SYS_VRB, "%9d points illuminated\n", gridPointsIlluminated );
	Sys_FPrintf( SYS_VRB, "%9d points occluded\n", gridPointsOccluded );
	Sys_FPrintf( SYS_VRB, "%9d points flooded\n", totalFlooded );
}

/*
IlluminateGrid()
performs lightgrid tracing
*/

void IlluminateGrid( void )
{
	/* set up light envelopes */
	SetupEnvelopes( qtrue, fastgrid );
	
	/* note it */
	Sys_Printf( "--- IlluminateGrid ---\n" );

	/* trace */
	gridPointsIlluminated = 0;
	gridPointsOccluded = 0;
	gridPointsFlooded = 0;
	gridSampleLightmap = qfalse;
#ifdef GRID_BLOCK_OPTIMIZATION
	RunThreadsOnIndividual( numGridBlocks, qtrue, IlluminateGridBlock );
#else
	RunThreadsOnIndividual( numRawGridPoints, qtrue, IlluminateGridPointOld );
#endif

	/* postprocess */
	FinishIlluminateGrid();
}

/*
IlluminateGridByLightmap()
performs lightgrid calculations based on lightmap
- grid points containing lightmap surfaces get light level sampled from them
- other grid points get traced
*/

void IlluminateGridByLightmap( void )
{
	/* set up light envelopes */
	SetupEnvelopes( qtrue, fastgrid );
	
	/* note it */
	Sys_Printf( "--- IlluminateGridByLightmap ---\n" );

	/* trace */
	gridPointsIlluminated = 0;
	gridPointsOccluded = 0;
	gridPointsFlooded = 0;
	gridSampleLightmap = true;
#ifdef GRID_BLOCK_OPTIMIZATION
	RunThreadsOnIndividual( numGridBlocks, qtrue, IlluminateGridBlock );
#else
	RunThreadsOnIndividual( numRawGridPoints, qtrue, IlluminateGridPointOld );
#endif

	/* postprocess */
	FinishIlluminateGrid();
}

/*
SetupGrid()
calculates the size of the lightgrid and allocates memory
*/

void SetupGrid( void )
{
	int			    i, j, x, y, z, mod;
	vec3_t		    maxs, newsize, origin;
	const char	    *value;
	char		    temp[ 64 ];

	/* don't do this if not grid lighting */
	if( noGridLighting )
		return;
	
	/* worldspawn-set grid size */
	if( KeyExists( &entities[ 0 ], "gridsize" ) == qtrue )
	{
		value = ValueForKey( &entities[ 0 ], "gridsize" );
		if( value[ 0 ] != '\0' )
			if (sscanf( value, "%f %f %f", &newsize[ 0 ], &newsize[ 1 ], &newsize[ 2 ] ))
				VectorCopy( newsize, gridSize );
	}

	/* quantize it */
	for( i = 0; i < 3; i++ )
		gridSize[ i ] = gridSize[ i ] >= 1.0f ? floor( gridSize[ i ] ) : 1.0f;
	
	/* ydnar: increase gridSize until grid count is smaller than max allowed */
	numRawGridPoints = MAX_MAP_LIGHTGRID + 1;
	j = 0;
	while( numRawGridPoints > MAX_MAP_LIGHTGRID )
	{
		/* get world bounds */
		for( i = 0; i < 3; i++ )
		{
			gridMins[ i ] = gridSize[ i ] * ceil( bspModels[ 0 ].mins[ i ] / gridSize[ i ] );
			maxs[ i ] = gridSize[ i ] * floor( bspModels[ 0 ].maxs[ i ] / gridSize[ i ] );
			gridBounds[ i ] = (maxs[ i ] - gridMins[ i ]) / gridSize[ i ] + 1;
		}
	
		/* set grid size */
		numRawGridPoints = gridBounds[ 0 ] * gridBounds[ 1 ] * gridBounds[ 2 ];
		
		/* increase grid size a bit */
		if( numRawGridPoints > MAX_MAP_LIGHTGRID )
			gridSize[ j++ % 3 ] += 16.0f;
	}

	/* count blocks used for batch tracing */
	gridBlockSize[ 0 ] = max( 4, 128 / gridSize[ 0 ] );
	gridBlockSize[ 1 ] = max( 4, 128 / gridSize[ 1 ] );
	gridBlockSize[ 2 ] = max( 4, 128 / gridSize[ 2 ] );
	gridBlocks[ 0 ] = ceil( (float)gridBounds[ 0 ] / gridBlockSize[ 0 ] );
	gridBlocks[ 1 ] = ceil( (float)gridBounds[ 1 ] / gridBlockSize[ 1 ] );
	gridBlocks[ 2 ] = ceil( (float)gridBounds[ 2 ] / gridBlockSize[ 2 ] );
	numGridBlocks = gridBlocks[ 0 ] * gridBlocks[ 1 ] * gridBlocks[ 2 ];
	
	/* print stats */
	Sys_Printf( "Grid point size = { %1.0f, %1.0f, %1.0f }\n", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );
	Sys_FPrintf( SYS_VRB, "Grid dimensions = %ix%ix%i\n", gridBounds[ 0 ], gridBounds[ 1 ], gridBounds[ 2 ] );
	Sys_FPrintf( SYS_VRB, "Grid block = %ix%ix%i (total %ix%ix%i, %i points per block)\n", gridBlockSize[ 0 ], gridBlockSize[ 1 ], gridBlockSize[ 2 ], gridBlocks[ 0 ], gridBlocks[ 1 

], gridBlocks[ 2 ], gridBlockSize[ 0 ] * gridBlockSize[ 1 ] * gridBlockSize[ 2 ] );

	/* build normals used to calculate ambient lighting in new TraceGrid path */
	for( i = 0; i < GRID_NUM_NORMALS; i++ )
		VectorNormalize( GridNormalsModel[ i ], GridNormals[ i ] );

	/* build grid offsets used for nudging code */
	for( i = 0; i < GRID_NUM_OFFSETS; i++ )
	{
		GridOffsets[ i ][ 0 ] = GridOffsetsModel[ i ][ 0 ] * gridSize[ 0 ];
		GridOffsets[ i ][ 1 ] = GridOffsetsModel[ i ][ 1 ] * gridSize[ 1 ];
		GridOffsets[ i ][ 2 ] = GridOffsetsModel[ i ][ 2 ] * gridSize[ 2 ];
	}
	
	/* different? */
	if( !VectorCompare( gridSize, gridDefaultSize ) )
	{
		sprintf( temp, "%.0f %.0f %.0f", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );
		SetKeyValue( &entities[ 0 ], "gridsize", (const char*) temp );
		Sys_FPrintf( SYS_VRB, "Storing adjusted grid size\n" );
	}
	
	/* 2nd variable. fixme: is this silly? */
	numBSPGridPoints = numRawGridPoints;
	
	/* allocate lightgrid */
	rawGridPoints = (rawGridPoint_t *)safe_malloc( numRawGridPoints * sizeof( rawGridPoint_t ) );
	memset( rawGridPoints, 0, numRawGridPoints * sizeof( rawGridPoint_t ) );
	if( bspGridPoints != NULL )
		free( bspGridPoints );
	bspGridPoints = (bspGridPoint_t *)safe_malloc( numBSPGridPoints * sizeof( bspGridPoint_t ) );
	memset( bspGridPoints, 0, numBSPGridPoints * sizeof( bspGridPoint_t ) );
	
	/* map lightgrid */
	gridPointsMapped = 0;
	for( i = 0; i < numRawGridPoints; i++ )
	{
		/* set ambient color */
		VectorCopy( ambientColor, rawGridPoints[ i ].ambient[ j ] );
		rawGridPoints[ i ].styles[ 0 ] = LS_NORMAL;
		bspGridPoints[ i ].styles[ 0 ] = LS_NORMAL;
		for( j = 1; j < MAX_LIGHTMAPS; j++ )
		{
			rawGridPoints[ i ].styles[ j ] = LS_NONE;
			bspGridPoints[ i ].styles[ j ] = LS_NONE;
		}

		/* get origin */
		mod = i;
		z = mod / (gridBounds[ 0 ] * gridBounds[ 1 ]);
		mod -= z * (gridBounds[ 0 ] * gridBounds[ 1 ]);
		y = mod / gridBounds[ 0 ];
		mod -= y * gridBounds[ 0 ];
		x = mod;
		origin[ 0 ] = gridMins[ 0 ] + x * gridSize[ 0 ];
		origin[ 1 ] = gridMins[ 1 ] + y * gridSize[ 1 ];
		origin[ 2 ] = gridMins[ 2 ] + z * gridSize[ 2 ];

		/* map grid point */
		rawGridPoints[ i ].mapped = numGridAreas ? qfalse : qtrue;
		rawGridPoints[ i ].contributions = 0;
		rawGridPoints[ i ].flooded = qfalse;
		rawGridPoints[ i ].floodOnlyColor = qfalse;
		for( j = 0; j < numGridAreas; j++ )
		{
			/* test bounds */
			if( gridAreas[ j ].mins[ 0 ] > (origin[ 0 ] + gridSize[ 0 ]) || gridAreas[ j ].maxs[ 0 ] < (origin[ 0 ] - gridSize[ 0 ]) ||
				gridAreas[ j ].mins[ 1 ] > (origin[ 1 ] + gridSize[ 1 ]) || gridAreas[ j ].maxs[ 1 ] < (origin[ 1 ] - gridSize[ 1 ]) ||
				gridAreas[ j ].mins[ 2 ] > (origin[ 2 ] + gridSize[ 2 ]) || gridAreas[ j ].maxs[ 2 ] < (origin[ 2 ] - gridSize[ 2 ]) )
				continue;

			/* mark as mapped */
			rawGridPoints[ i ].mapped = qtrue;
			break;
		}

		/* add to stats */
		if ( rawGridPoints[ i ].mapped )
			gridPointsMapped++;
	}
	
	/* note it */
	Sys_FPrintf( SYS_VRB, "%9d grid areas\n", numGridAreas );
	Sys_FPrintf( SYS_VRB, "%9d grid blocks\n", numGridBlocks );
	Sys_FPrintf( SYS_VRB, "%9d grid points\n", numRawGridPoints );
	Sys_Printf( "%9d grid points mapped (%.2f percent)\n", gridPointsMapped, ((float)gridPointsMapped / numRawGridPoints) * 100 );
	if( gridSuperSample >= 2 )
		Sys_Printf( "%9d grid points sampled\n", gridPointsMapped * gridSuperSample * gridSuperSample * gridSuperSample );
	else
		Sys_Printf( "%9d grid points sampled\n", gridPointsMapped );
}


/*
SampleGrid()
calculates the lighting at point based on illuminated LightGrid
*/

void SampleGrid(vec3_t origin, vec3_t outambient, vec3_t outdirected, vec3_t outdirection )
{
	int i, j, k, index[3];
	float trans[3], blend1, blend2, blend;
	rawGridPoint_t *ar, *sr;

	/* init */
	VectorClear(outambient);
	VectorClear(outdirected);
	VectorClear(outdirection);

	/* transform origin into lightGrid space */
	trans[0] = (origin[0] - gridMins[0]) / gridSize[0];
	trans[1] = (origin[1] - gridMins[1]) / gridSize[1];
	trans[2] = (origin[2] - gridMins[2]) / gridSize[2];
	trans[0] = max(0, min(trans[0], gridBounds[0] - 1));
	trans[1] = max(0, min(trans[1], gridBounds[1] - 1));
	trans[2] = max(0, min(trans[2], gridBounds[2] - 1));
	index[0] = (int)floor(trans[0]);
	index[1] = (int)floor(trans[1]);
	index[2] = (int)floor(trans[2]);

	/* now lerp the values */
	ar = &rawGridPoints[(index[2] * gridBounds[1] + index[1]) * gridBounds[0] + index[0]];
	for (k = 0;k < 2;k++)
	{
		blend1 = (k ? (trans[2] - index[2]) : (1 - (trans[2] - index[2])));
		if (blend1 < 0.001f || index[2] + k >= gridBounds[2])
			continue;
		for (j = 0;j < 2;j++)
		{
			blend2 = blend1 * (j ? (trans[1] - index[1]) : (1 - (trans[1] - index[1])));
			if (blend2 < 0.001f || index[1] + j >= gridBounds[1])
				continue;
			for (i = 0;i < 2;i++)
			{
				blend = blend2 * (i ? (trans[0] - index[0]) : (1 - (trans[0] - index[0])));
				if (blend < 0.001f || index[0] + i >= gridBounds[0])
					continue;			
				sr = ar + (k * gridBounds[1] + j) * gridBounds[0] + i;
				VectorMA(outambient, blend * (1.0f / 128.0f), sr->ambient[ 0 ], outambient);
				VectorMA(outdirected, blend * (1.0f / 128.0f), sr->directed[ 0 ], outdirected);
				VectorMA(outdirection, blend, sr->dir, outdirection);
			}
		}
	}

	/* renormalize light direction */
	VectorNormalize(outdirection, outdirection);
}

/*
LightWorld()
does what it says...
*/

void LightWorld( char *mapSource )
{
	qboolean useMinLight = qfalse, useMinVertexLight = qfalse, useMinGridLight = qfalse, useAmbient = qfalse, useColormod = qfalse;
	int	b, bt;
	vec3_t color;
	float f;

	/* note it */
	Sys_Printf( "--- LightWorld ---\n" );
	
	/* get color/_color (used for minlight/minvertexlight/ambient */
	GetEntityMinlightAmbientColor( &entities[ 0 ], color, NULL, NULL, NULL, NULL, qtrue );

	/* minlight/_minlight */
	if( KeyExists( &entities[ 0 ], "minlight" ) )
	{
		useMinLight = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "minlight"), "%f %f %f", &minLight[ 0 ], &minLight[ 1 ], &minLight[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "minlight" );
			VectorScale( color, f, minLight );
		}
	}
	if( KeyExists( &entities[ 0 ], "_minlight" ) )
	{
		useMinLight = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "_minlight"), "%f %f %f", &minLight[ 0 ], &minLight[ 1 ], &minLight[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "_minlight" );
			VectorScale( color, f, minLight );
		}
	}
	if( useMinLight )
	{
		VectorCopy( minLight, minVertexLight );
		VectorCopy( minLight, minGridLight );
	}

	/* minvertexlight/_minvertexlight */
	if( KeyExists( &entities[ 0 ], "minvertexlight" ) )
	{
		useMinVertexLight = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "minvertexlight"), "%f %f %f", &minVertexLight[ 0 ], &minLight[ 1 ], &minLight[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "minvertexlight" );
			VectorScale( color, f, minVertexLight );
		}
	}
	if( KeyExists( &entities[ 0 ], "_minvertexlight" ) )
	{
		useMinVertexLight = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "_minvertexlight"), "%f %f %f", &minVertexLight[ 0 ], &minVertexLight[ 1 ], &minVertexLight[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "_minvertexlight" );
			VectorScale( color, f, minVertexLight );
		}
	}

	/* mingridlight/_mingridlight */
	if( KeyExists( &entities[ 0 ], "mingridlight" ) )
	{
		useMinGridLight = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "mingridlight"), "%f %f %f", &minGridLight[ 0 ], &minGridLight[ 1 ], &minGridLight[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "mingridlight" );
			VectorScale( color, f, minGridLight );
		}
	}
	if( KeyExists( &entities[ 0 ], "_mingridlight" ) )
	{
		useMinGridLight = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "_mingridlight"), "%f %f %f", &minGridLight[ 0 ], &minGridLight[ 1 ], &minGridLight[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "_mingridlight" );
			VectorScale( color, f, minGridLight );
		}
	}

	/* ambient/_ambient */
	if( KeyExists( &entities[ 0 ], "ambient" ) )
	{
		useAmbient = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "ambient"), "%f %f %f", &ambientColor[ 0 ], &ambientColor[ 1 ], &ambientColor[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "ambient" );
			VectorScale( color, f, ambientColor );
		}
	}
	if( KeyExists( &entities[ 0 ], "_ambient" ) )
	{
		useAmbient = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "_ambient"), "%f %f %f", &ambientColor[ 0 ], &ambientColor[ 1 ], &ambientColor[ 2 ] ) != 3 )
		{
			f = FloatForKey( &entities[ 0 ], "_ambient" );
			VectorScale( color, f, ambientColor );
		}
	}

	/* colormod/_colormod */
	if( KeyExists( &entities[ 0 ], "colormod" ) )
	{
		useColormod = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "colormod"), "%f %f %f", &colorMod[ 0 ], &colorMod[ 1 ], &colorMod[ 2 ] ) != 3 )
			colorMod[ 0 ] = colorMod[ 1 ] = colorMod[ 2 ] = FloatForKey( &entities[ 0 ], "colormod" );
	}
	if( KeyExists( &entities[ 0 ], "_colormod" ) )
	{
		useColormod = qtrue;
		if( sscanf( ValueForKey( &entities[ 0 ], "_colormod"), "%f %f %f", &colorMod[ 0 ], &colorMod[ 1 ], &colorMod[ 2 ] ) != 3 )
			colorMod[ 0 ] = colorMod[ 1 ] = colorMod[ 2 ] = FloatForKey( &entities[ 0 ], "_colormod" );
	}

	/* show stats */
	if( useMinLight )
		Sys_Printf( "Min light override by worldspawn key\n" );
	if( minLight[ 0 ] || minLight[ 1 ] || minLight[ 2 ] )
		Sys_Printf( "Min light = ( %f %f %f )\n", minLight[ 0 ], minLight[ 1 ], minLight[ 2 ] );
	if( useMinVertexLight )
		Sys_Printf( "Min vertex light override by worldspawn key\n" );
	if( ( minVertexLight[ 0 ] || minVertexLight[ 1 ] || minVertexLight[ 2 ] ) && ( minLight[ 0 ] != minVertexLight[ 0 ] || minLight[ 1 ] != minVertexLight[ 1 ] || minLight[ 2 ] != 

minVertexLight[ 2 ] ) )
		Sys_Printf( "Min vertex light = ( %f %f %f )\n", minVertexLight[ 0 ], minVertexLight[ 1 ], minVertexLight[ 2 ] );
	if( useMinGridLight )
		Sys_Printf( "Min grid light override by worldspawn key\n" );
	if( ( minGridLight[ 0 ] || minGridLight[ 1 ] || minGridLight[ 2 ] ) && ( minLight[ 0 ] != minGridLight[ 0 ] || minLight[ 1 ] != minGridLight[ 1 ] || minLight[ 2 ] != 

minGridLight[ 2 ] ) )
		Sys_Printf( "Min grid light = ( %f %f %f )\n", minGridLight[ 0 ], minGridLight[ 1 ], minGridLight[ 2 ] );
	if( useAmbient )
		Sys_Printf( "Ambient light override by worldspawn key\n" );
	if( ambientColor[ 0 ] || ambientColor[ 1 ] || ambientColor[ 2 ] )
		Sys_Printf( "Ambient light = ( %f %f %f )\n", ambientColor[ 0 ], ambientColor[ 1 ], ambientColor[ 2 ] );
	if( useColormod )
		Sys_Printf( "Global colormod override by worldspawn key\n" );
	if( colorMod[ 0 ] != 1 || colorMod[ 1 ] != 1 || colorMod[ 2 ] != 1 )
		Sys_Printf( "Colormod = ( %f %f %f )\n", colorMod[ 0 ], colorMod[ 1 ], colorMod[ 2 ] );

	/* initialize filter effects  */
	SetupDirt();
	SetupFloodLight();

	/* initialize lightmaps */
	SetupSurfaceLightmaps();
	
	/* initialize the surface facet tracing */
	SetupTraceNodes();

	/* allocate raw lightmaps */
	AllocateSurfaceLightmaps();

	/* create world lights (note in verbose mode) */
	if( verbose )
		Sys_Printf( "--- CreateLights ---\n" );
	CreateEntityLights();
	CreateSurfaceLights();
	if( numPointLights || verbose )
		Sys_Printf( "%9d point lightsources created\n", numPointLights );
	if( numSpotLights || verbose )
		Sys_Printf( "%9d spot lightsources created\n", numSpotLights );
	if( numDiffuseLights || verbose )
		Sys_Printf( "%9d diffuse (area) lightsources created\n", numDiffuseLights );
	if( numSunLights || verbose )
		Sys_Printf( "%9d sun/sky lightsources created\n", numSunLights );

	/* smooth normals */
	if( shade )
	{
		Sys_Printf( "--- SmoothNormals ---\n" );
		SmoothNormals();
	}
	
	/* setup grid */
	Sys_Printf( "--- SetupGrid ---\n" );
	SetupGrid();
	
	/* illuminate lightgrid */
	if( !noGridLighting && !gridFromLightmap)
		IlluminateGrid( );
	
	/* slight optimization to remove a sqrt */
	subdivideThreshold *= subdivideThreshold;
	
	/* map the world luxels */
	Sys_Printf( "--- MapRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, qtrue, MapRawLightmap );
	Sys_Printf( "%9d luxels\n", numLuxels );
	Sys_Printf( "%9d luxels mapped\n", numLuxelsMapped );
	Sys_Printf( "%9d luxels remapped\n", numLuxelsRemapped );
	Sys_Printf( "%9d luxels nudged\n", numLuxelsNudged );
	Sys_Printf( "%9d luxels occluded\n", numLuxelsOccluded );

	/* dirty them up */
	if( !gridOnly && dirty )
	{
		Sys_Printf( "--- DirtyRawLightmap ---\n" );
		RunThreadsOnIndividual( numRawLightmaps, qtrue, DirtyRawLightmap );
	}

	/* floodlight pass */
	if( !gridOnly )
	{
		// VorteX: floodlight are off at the moment
		//FloodlightRawLightmaps();
	}

	/* ydnar: set up light envelopes */
	if( !gridOnly )
		SetupEnvelopes( qfalse, fast );
	
	/* light up my world */
	lightsPlaneCulled = 0;
	lightsEnvelopeCulled = 0;
	lightsBoundsCulled = 0;
	lightsClusterCulled = 0;

	/* illuminate lightmaps */
	Sys_Printf( "--- IlluminateRawLightmap ---\n" );
	RunThreadsOnIndividual( numRawLightmaps, qtrue, IlluminateRawLightmap );
	Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );
	
	/* filter lightmaps */
	Sys_Printf( "--- FilterRawLightmap ---\n" );
	ThreadMutexInit(&LightmapGrowStitchMutex);
	RunThreadsOnIndividual( numRawLightmaps, qtrue, FilterRawLightmap );
	if( numLuxelsStitched || verbose )
		Sys_Printf( "%9d luxels marked for stitching\n", numLuxelsStitched );
	ThreadMutexDelete(&LightmapGrowStitchMutex);

	/* stitch lightmaps */
	StitchRawLightmaps();

	/* generate debug surfaces for luxels */
	if( debugLightmap )
	{
		StripExtension( mapSource );
		DefaultExtension( mapSource, "_luxels.ase" );
		WriteRawLightmapsFile( mapSource );
	}

	/* illuminate vertexes */
	Sys_Printf( "--- IlluminateVertexes ---\n" );
	RunThreadsOnIndividual( numBSPDrawSurfaces, qtrue, IlluminateVertexes );
	Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

	/* ydnar: emit statistics on light culling */
	Sys_FPrintf( SYS_VRB, "%9d lights plane culled\n", lightsPlaneCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights envelope culled\n", lightsEnvelopeCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights bounds culled\n", lightsBoundsCulled );
	Sys_FPrintf( SYS_VRB, "%9d lights cluster culled\n", lightsClusterCulled );

	/* radiosity */
	b = 1;
	bt = bounce;
	while( bounce > 0 )
	{
		/* store off the bsp between bounces */
		StoreSurfaceLightmaps();
		Sys_Printf( "Writing %s\n", source );
		WriteBSPFile( source );
		
		/* note it */
		Sys_Printf( "\n--- Radiosity (bounce %d of %d) ---\n", b, bt );
		
		/* flag bouncing */
		bouncing = qtrue;
		VectorClear( ambientColor );
		VectorSet( colorMod, 1, 1, 1 );
		
		/* generate diffuse lights */
		RadFreeLights();
		RadCreateDiffuseLights();
		
		/* setup light envelopes */
		SetupEnvelopes( qfalse, fastbounce );
		if( numLights == 0 )
		{
			Sys_Printf( "No diffuse light to calculate, ending radiosity.\n" );
			break;
		}
		
		/* add to lightgrid */
		if( bouncegrid )
		{
			Sys_Printf( "--- BounceGrid ---\n" );
#ifdef GRID_BLOCK_OPTIMIZATION
	RunThreadsOnIndividual( numGridBlocks, qtrue, IlluminateGridBlock );
#else
	RunThreadsOnIndividual( numRawGridPoints, qtrue, IlluminateGridPointOld );
#endif
		}
		
		/* light up my world */
		lightsPlaneCulled = 0;
		lightsEnvelopeCulled = 0;
		lightsBoundsCulled = 0;
		lightsClusterCulled = 0;

		/* illuminate lightmaps */
		Sys_Printf( "--- IlluminateRawLightmap ---\n" );
		numLuxelsIlluminated = 0;
		RunThreadsOnIndividual( numRawLightmaps, qtrue, IlluminateRawLightmap );
		Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );

		/* filter lightmaps */
		Sys_Printf( "--- FilterRawLightmap ---\n" );
		ThreadMutexInit(&LightmapGrowStitchMutex);
		RunThreadsOnIndividual( numRawLightmaps, qtrue, FilterRawLightmap );
		if( numLuxelsStitched || verbose )
			Sys_Printf( "%9d luxels marked for stitching\n", numLuxelsStitched );
		ThreadMutexDelete(&LightmapGrowStitchMutex);
		
		/* stitch lightmaps */
		StitchRawLightmaps();

		/* illuminate vertexes */
		Sys_Printf( "--- IlluminateVertexes ---\n" );
		RunThreadsOnIndividual( numBSPDrawSurfaces, qtrue, IlluminateVertexes );
		Sys_Printf( "%9d vertexes illuminated\n", numVertsIlluminated );

		/* ydnar: emit statistics on light culling */
		Sys_FPrintf( SYS_VRB, "%9d lights plane culled\n", lightsPlaneCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights envelope culled\n", lightsEnvelopeCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights bounds culled\n", lightsBoundsCulled );
		Sys_FPrintf( SYS_VRB, "%9d lights cluster culled\n", lightsClusterCulled );
		
		/* interate */
		bounce--;
		b++;
	}

	/* sample lightgrid from lightmap */
	if( gridFromLightmap )
	{
		/* sample lightgrid from lightmap */
		if( !noGridLighting )
			IlluminateGridByLightmap();

		/* sample lightmap from lightgrid */
		if ( debugGrid )
		{
			Sys_Printf( "--- LightGridRawLightmap ---\n" );
			numLuxelsIlluminated = 0;
			RunThreadsOnIndividual( numRawLightmaps, qtrue, LightGridRawLightmap );
			Sys_Printf( "%9d luxels illuminated\n", numLuxelsIlluminated );
		}	
	}
}



/*
LightMain()
main routine for light processing
*/

int LightMain( int argc, char **argv )
{
	int			i;
	float		f;
	char		mapSource[ MAX_OS_PATH ];

	/* note it */
	Sys_Printf( "--- Light ---\n" );
	Sys_Printf( "--- GameSpecific ---\n" );

	/* set standard game flags */
	wolfLight = game->wolfLight;
	lmCustomSize = game->lightmapSize;
	lightmapGamma = game->lightmapGamma;
	lightmapCompensate = game->lightmapCompensate;
	lightmapExposure = game->lightmapExposure;
	noStyles = game->noStyles;
	keepLights = game->keepLights;
	colorNormalize = game->colorNormalize;
	lightmapsRGB = game->lightmapsRGB;
	colorsRGB = game->colorsRGB;
	texturesRGB = game->texturesRGB;
	deluxemap = game->deluxeMap;
	deluxemode = game->deluxeMode;
	lightAngleHL = game->lightAngleHL;
	Sys_Printf( " lightning model: %s\n", wolfLight == qtrue ? "wolf" : "quake3" );
	Sys_Printf( " lightmap size: %d x %d pixels\n", lmCustomSize, lmCustomSize );
	Sys_Printf( " lightning gamma: %f\n", lightmapGamma );
	Sys_Printf( " lightning compensation: %f\n", lightmapCompensate );
	Sys_Printf( " lightning exposure: %f\n", lightmapExposure );
	Sys_Printf( " shader lightstyles hack: %s\n", noStyles == qtrue ? "disabled" : "enabled");
	Sys_Printf( " keep lights: %s\n", keepLights == qtrue ? "enabled" : "disabled" );
	Sys_Printf( " light color normalization: %s\n", colorNormalize == qtrue ? "enabled" : "disabled" );
	Sys_Printf( " lightmap colorspace: %s\n", lightmapsRGB == qtrue ? "enabled" : "disabled" );
	Sys_Printf( " entity _color keys colorspace: %s\n", colorsRGB == qtrue ? "sRGB" : "linear"  );
	Sys_Printf( " texture default colorspace: %s\n", texturesRGB == qtrue ? "sRGB" : "linear" );
	Sys_Printf( " patch shadows: enabled\n", patchShadows == qtrue ? "enabled" : "disabled" );
	Sys_Printf( " deluxemapping: %s\n", deluxemap == qtrue ? (deluxemode > 0 ? "tangentspace deluxemaps" : "modelspace deluxemaps") : "disabled" );
	if( lightAngleHL )
		Sys_Printf( " half lambert light angle attenuation enabled \n" );

	Sys_Printf( "--- CommandLine ---\n" );
	
	/* process commandline arguments */
	strcpy(externalLightmapsPath, "");
	for( i = 1; i < (argc - 1) && argv[ i ]; i++ )
	{
		/* point lightsource scaling */
		if( !strcmp( argv[ i ], "-point" ) || !strcmp( argv[ i ], "-pointscale" ) )
		{
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			Sys_Printf( " Point (entity) light scaled by %f to %f\n", f, pointScale );
			i++;
		}
		
		/* area lightsource scaling */
		else if( !strcmp( argv[ i ], "-area" ) || !strcmp( argv[ i ], "-areascale" ) )
		{
			f = atof( argv[ i + 1 ] );
			areaScale *= f;
			Sys_Printf( " Area (shader) light scaled by %f to %f\n", f, areaScale );
			i++;
		}
		
		/* sky lightsource scaling */
		else if( !strcmp( argv[ i ], "-sky" ) || !strcmp( argv[ i ], "-skyscale" ) )
		{
			f = atof( argv[ i + 1 ] );
			skyScale *= f;
			Sys_Printf( " Sky/sun light scaled by %f to %f\n", f, skyScale );
			i++;
		}
		
		/* bound light scaling */
		else if( !strcmp( argv[ i ], "-bouncescale" ) )
		{
			f = atof( argv[ i + 1 ] );
			bounceScale *= f;
			Sys_Printf( " Bounce (radiosity) light scaled by %f to %f\n", f, bounceScale );
			i++;
		}
	
		/* all lights scaling */
		else if( !strcmp( argv[ i ], "-scale" ) )
		{
			f = atof( argv[ i + 1 ] );
			pointScale *= f;
			areaScale *= f;
			skyScale *= f;
			bounceScale *= f;
			Sys_Printf( " All light scaled by %f\n", f );
			i++;
		}

		/* vertex lighting scaling */
		else if( !strcmp( argv[ i ], "-vertexscale" ) )
		{
			vertexScale = atof( argv[ i + 1 ] );
			Sys_Printf( " Applying intensity modifier %f for vertex light\n", vertexScale );
			i++;
		}
		
		/* lighting gamma */
		else if( !strcmp( argv[ i ], "-gamma" ) )
		{
			f = atof( argv[ i + 1 ] );
			lightmapGamma = f;
			Sys_Printf( " Lighting gamma set to %f\n", lightmapGamma );
			i++;
		}
		
		/* lighting exposure */
		else if( !strcmp( argv[ i ], "-exposure" ) )
		{
			f = atof( argv[ i + 1 ] );
			lightmapExposure = f;
			if (lightmapExposure <= 0)
				lightmapExposure = 1;
			Sys_Printf( " Lighting exposure set to %f\n", lightmapExposure );
			i++;
		}
		
		/* lighting substraction */
		else if( !strcmp( argv[ i ], "-compensate" ) )
		{
			f = atof( argv[ i + 1 ] );
			if( f <= 0.0f )
				f = 1.0f;
			lightmapCompensate = f;
			Sys_Printf( " Lighting compensation set to 1/%f\n", lightmapCompensate );
			i++;
		}

		/* lighting brightness */
		else if( !strcmp( argv[ i ], "-brightness" ) )
		{
			f = atof( argv[ i + 1 ] );
			lightmapBrightness = f;
			Sys_Printf( " Lighting brightness set to %f\n", lightmapBrightness );
			i++;
		}
		
		/* global illumination */
		else if( !strcmp( argv[ i ], "-bounce" ) )
		{
			bounce = atoi( argv[ i + 1 ] );
			if( bounce < 0 )
				bounce = 0;
			else if( bounce > 0 )
				Sys_Printf( " Radiosity enabled with %d bounce(s)\n", bounce );
			i++;
		}
		
		/* lightmap/lightgrid supersampling */
		else if( !strcmp( argv[ i ], "-supersample" ) || !strcmp( argv[ i ], "-super" ) )
		{
			superSample = atoi( argv[ i + 1 ] );
			if( superSample < 1 )
				superSample = 1;
			else if( superSample > 1 )
				Sys_Printf( " Ordered-grid supersampling enabled with %d sample(s) per lightmap texel\n", (superSample * superSample) );
			i++;
		}
		
		/* lightmap adaptive supersampling */
		else if( !strcmp( argv[ i ], "-samples" ) )
		{
			lightSamples = atoi( argv[ i + 1 ] );
			if( lightSamples < 1 )
				lightSamples = 1;
			else if( lightSamples > 1 )
				Sys_Printf( " Adaptive supersampling enabled with %d sample(s) per lightmap texel\n", lightSamples );
			i++;
		}

		/* lightmap filter (blur) */
		else if( !strcmp( argv[ i ], "-filter" ) )
		{
			filter = qtrue;
			Sys_Printf( " Lightmap filtering enabled\n" );
		}
		
		/* phong shading */
		else if( !strcmp( argv[ i ], "-shadeangle" ) )
		{
			shadeAngleDegrees = atof( argv[ i + 1 ] );
			if( shadeAngleDegrees < 0.0f )
				shadeAngleDegrees = 0.0f;
			else if( shadeAngleDegrees > 0.0f )
			{
				shade = qtrue;
				Sys_Printf( " Phong shading enabled with a breaking angle of %f degrees\n", shadeAngleDegrees );
			}
			i++;
		}
		
		/* subdivision threshold */
		else if( !strcmp( argv[ i ], "-thresh" ) )
		{
			subdivideThreshold = atof( argv[ i + 1 ] );
			if( subdivideThreshold < 0 )
				subdivideThreshold = DEFAULT_SUBDIVIDE_THRESHOLD;
			else
				Sys_Printf( " Subdivision threshold set at %.3f\n", subdivideThreshold );
			i++;
		}
		
		/* lightmap approximation with vertex lighting */
		else if( !strcmp( argv[ i ], "-approx" ) )
		{
			approximateTolerance = atoi( argv[ i + 1 ] );
			if( approximateTolerance < 0 )
				approximateTolerance = 0;
			else if( approximateTolerance > 0 )
				Sys_Printf( " Approximating lightmaps within a byte tolerance of %d\n", approximateTolerance );
			i++;
		}

		/* ambient light */
		else if( !strcmp( argv[ i ], "-ambient" ) )
		{
			i++;
			if( strstr(argv[ i ], " ") )
				sscanf(argv[ i ], "%f %f %f", &ambientColor[ 0 ], &ambientColor[ 1 ], &ambientColor[ 2 ]); 
			else
				ambientColor[ 0 ] = ambientColor[ 1 ] = ambientColor[ 2 ] = atof(argv[ i ]);
			Sys_Printf( " Ambient lighting set to ( %f %f %f )\n", ambientColor[ 0 ], ambientColor[ 1 ], ambientColor[ 2 ] );
		}

		/* min light */
		else if( !strcmp( argv[ i ], "-minlight" ) )
		{
			i++;
			if( sscanf(argv[ i ], "%f %f %f", &minLight[ 0 ], &minLight[ 1 ], &minLight[ 2 ]) != 3 )
				minLight[ 0 ] = minLight[ 1 ] = minLight[ 2 ] = atof(argv[ i ]);
			Sys_Printf( " Min lighting set to ( %f %f %f )\n", minLight[ 0 ], minLight[ 1 ], minLight[ 2 ] );
			VectorCopy( minLight, minVertexLight );
			VectorCopy( minLight, minGridLight );
		}

		/* min vertex light */
		else if( !strcmp( argv[ i ], "-minvertex" ) )
		{
			i++;
			if( sscanf(argv[ i ], "%f %f %f", &minVertexLight[ 0 ], &minVertexLight[ 1 ], &minVertexLight[ 2 ]) != 3 )
				minVertexLight[ 0 ] = minVertexLight[ 1 ] = minVertexLight[ 2 ] = atof(argv[ i ]);
			Sys_Printf( " Min vertex lighting set to ( %f %f %f )\n", minVertexLight[ 0 ], minVertexLight[ 1 ], minVertexLight[ 2 ] );
		}

		/* min grid light */
		else if( !strcmp( argv[ i ], "-mingrid" ) )
		{
			i++;
			if( sscanf(argv[ i ], "%f %f %f", &minGridLight[ 0 ], &minGridLight[ 1 ], &minGridLight[ 2 ]) != 3 ) 
				minGridLight[ 0 ] = minGridLight[ 1 ] = minGridLight[ 2 ] = atof(argv[ i ]);
			Sys_Printf( " Min grid lighting set to ( %f %f %f )\n", minGridLight[ 0 ], minGridLight[ 1 ], minGridLight[ 2 ] );
		}

		/* global colormod */
		else if( !strcmp( argv[ i ], "-colormod" ) )
		{
			i++;
			if( sscanf(argv[ i ], "%f %f %f", &colorMod[ 0 ], &colorMod[ 1 ], &colorMod[ 2 ]) != 3 ) 
				colorMod[ 0 ] = colorMod[ 1 ] = colorMod[ 2 ] = atof(argv[ i ]);
			Sys_Printf( " Global colormod set to ( %f %f %f )\n", colorMod[ 0 ], colorMod[ 1 ], colorMod[ 2 ] );
		}

		/* lightgrid */
		else if( !strcmp( argv[ i ], "-gridsize" ) )
		{
			i++;
			sscanf(argv[ i ], "%f %f %f", &gridSize[ 0 ], &gridSize[ 1 ], &gridSize[ 2 ]); 
			Sys_Printf( " Lightgrid size set to { %f %f %f }\n", gridSize[ 0 ], gridSize[ 1 ], gridSize[ 2 ] );
		}
		else if( !strcmp( argv[ i ], "-gridsupersample" ) || !strcmp( argv[ i ], "-gridsuper" ) )
		{
			i++;
			gridSuperSample = atoi( argv[ i ] );
			if( gridSuperSample < 1 )
				gridSuperSample = 1;
			else if( gridSuperSample > 1 )
				Sys_Printf( " Lightgrid supersampling enabled with %d sample(s) per grid point\n", (gridSuperSample * gridSuperSample * gridSuperSample) );
		}
		else if( !strcmp( argv[ i ], "-gridfromlightmap" ) )
		{
			gridFromLightmap = qtrue;
			Sys_Printf( " Sample lightgrid from lightmap\n" );
		}
		
		else if( !strcmp( argv[ i ], "-debuggrid" ) )
		{
			debugGrid = qtrue;
			Sys_Printf( " Lightgrid debugging enabled (sample lightmap from lightgrid)\n" );
		}

		else if( !strcmp( argv[ i ], "-gridonly" ) )
		{
			gridOnly = qtrue;
			Sys_Printf( " Sample lightmap and vertex color from lightgrid (fast lighting mode)\n" );
		}

		/* deluxemapping */
		else if( !strcmp( argv[ i ], "-deluxe" ) || !strcmp( argv[ i ], "-deluxemap" ) )
		{
			deluxemap = qtrue;
			Sys_Printf( " Generating deluxemaps for average light direction\n" );
		}
		else if( !strcmp( argv[ i ], "-tangentspace" ))
		{
			deluxemode = 1;
			Sys_Printf( " Deluxemaps using tangentspace coords\n" );
		}
		else if( !strcmp( argv[ i ], "-modelspace" ))
		{
			deluxemode = 0;
			Sys_Printf( " Deluxemaps using modelspace coords\n" );
		}
		else if( !strcmp( argv[ i ], "-nodeluxe" ) || !strcmp( argv[ i ], "-nodeluxemap" ) )
		{
			deluxemap = qfalse;
			Sys_Printf( " Disabling generating of deluxemaps for average light direction\n" );
		}

		/* external lightmaps */
		else if( !strcmp( argv[ i ], "-external" ) )
		{
			externalLightmaps = qtrue;
			Sys_Printf( " Storing all lightmaps externally\n" );
		}
		else if( !strcmp( argv[ i ], "-externalpath" ) )
		{
			i++;
			if (i < argc)
			{
				SanitizePath(argv[ i ], externalLightmapsPath);
				Sys_Printf( " Storing all external lightmaps under %s\n" , externalLightmapsPath );
			}
		}
		else if( !strcmp( argv[ i ], "-lightmapsize" ) )
		{
			lmCustomSize = atoi( argv[ i + 1 ] );
			
			/* must be a power of 2 and greater than 2 */
			if( ((lmCustomSize - 1) & lmCustomSize) || lmCustomSize < 2 )
			{
				Sys_Warning( "Given lightmap size (%i) must be a power of 2, greater or equal to 2 pixels.", lmCustomSize );
				lmCustomSize = game->lightmapSize;
			}
			i++;
			Sys_Printf( " Default lightmap size set to %d x %d pixels\n", lmCustomSize, lmCustomSize );
			
			/* enable external lightmaps */
			if( lmCustomSize != game->lightmapSize )
			{
				externalLightmaps = qtrue;
				Sys_Printf( " Storing all lightmaps externally\n" );
			}
		}
		else if( !strcmp( argv[ i ], "-maxsurfacelightmapsize" ) )
		{
			lmMaxSurfaceSize = atoi( argv[ i + 1 ] );
			i++;
			Sys_Printf( " Max surface lightmap size set to %d x %d pixels\n", lmMaxSurfaceSize, lmMaxSurfaceSize );
		}

		/* sRGB flags */

		/* generate lightmap in sRGB colorspace */
		else if( !strcmp( argv[ i ], "-lightmapsrgb" ) )
		{
			lightmapsRGB = qtrue;
			Sys_Printf( " Generating lightmaps in sRGB colorspace\n" );
		}
		/* disable sRGB (force lightmaps to linear color) */
		else if( !strcmp( argv[ i ], "-lightmaprgb" ) )
		{
			lightmapsRGB = qfalse;
			Sys_Printf( " Generating lightmaps in linear RGB colorspace\n" );
		}
		/* set default texture colorspace to sRGB */
		else if( !strcmp( argv[ i ], "-sRGBtex" ) ) 
		{
			texturesRGB = qtrue;
			Sys_Printf( "Default texture colorspace: sRGB\n" );
		}
		/* set default texture colorspace to linear */
		else if( !strcmp( argv[ i ], "-nosRGBtex" ) ) 
		{
			texturesRGB = qfalse;
			Sys_Printf( "Default texture colorspace: linear\n" );
		}
		/* set entity _color keys colorspace to sRGB */
		else if( !strcmp( argv[ i ], "-sRGBcolor" ) ) 
		{
			colorsRGB = qtrue;
			Sys_Printf( "Entity _color keys colorspace: sRGB\n" );
		}
		/* set entity _color keys colorspace to linear */
		else if( !strcmp( argv[ i ], "-nosRGBcolor" ) ) 
		{
			colorsRGB = qfalse;
			Sys_Printf( "Entity _color keys colorspace: linear\n" );
		}
		/* disable sRGB for textures and _color */
		else if ( !strcmp( argv[ i ], "-nosRGB" ) ) 
		{
			texturesRGB = qfalse;
			Sys_Printf( "Default texture colorspace: linear\n" );
			colorsRGB = qfalse;
			Sys_Printf( "Entity _color keys colorspace: linear\n" );
		}
		
		/* ydnar: add this to suppress warnings */
		
		else if( !strcmp( argv[ i ],  "-entitysaveid") )
		{
			Sys_Printf( " Entity unique savegame identifiers enabled\n" );
			useEntitySaveId = qtrue;
		}

		else if( !strcmp( argv[ i ], "-wolf" ) )
		{
			/* -game should already be set */
			wolfLight = qtrue;
			Sys_Printf( " Enabling Wolf lighting model (linear default)\n" );
		}

		else if( !strcmp( argv[ i ], "-colornorm" ) )
		{
			colorNormalize = qtrue;
			Sys_Printf( " Enabling light color normalization\n" );
		}

		else if( !strcmp( argv[ i ], "-nocolornorm" ) )
		{
			colorNormalize = qtrue;
			Sys_Printf( " Disabling light color normalization\n" );
		}
		
		else if( !strcmp( argv[ i ], "-q3" ) )
		{
			/* -game should already be set */
			wolfLight = qfalse;
			Sys_Printf( " Enabling Quake 3 lighting model (nonlinear default)\n" );
		}
		
		else if( !strcmp( argv[ i ], "-sunonly" ) )
		{
			sunOnly = qtrue;
			Sys_Printf( " Only computing sunlight\n" );
		}

		else if( !strcmp( argv[ i ], "-bounceonly" ) )
		{
			bounceOnly = qtrue;
			Sys_Printf( " Storing bounced light (radiosity) only\n" );
		}
		
		else if( !strcmp( argv[ i ], "-nocollapse" ) )
		{
			noCollapse = qtrue;
			Sys_Printf( " Identical lightmap collapsing disabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-shade" ) )
		{
			shade = qtrue;
			Sys_Printf( " Phong shading enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-bouncegrid") )
		{
			bouncegrid = qtrue;
			if( bounce > 0 )
				Sys_Printf( " Grid lighting with radiosity enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-smooth" ) )
		{
			lightSamples = EXTRA_SCALE;
			Sys_Printf( " The -smooth argument is deprecated, use \"-samples 2\" instead\n" );
		}
		
		else if( !strcmp( argv[ i ], "-fast" ) )
		{
			fast = qtrue;
			fastgrid = qtrue;
			fastbounce = qtrue;
			Sys_Printf( " Fast mode enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-faster" ) )
		{
			faster = qtrue;
			fast = qtrue;
			fastgrid = qtrue;
			fastbounce = qtrue;
			Sys_Printf( " Faster mode enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-fastgrid" ) )
		{
			fastgrid = qtrue;
			Sys_Printf( " Fast grid lighting enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-fastbounce" ) )
		{
			fastbounce = qtrue;
			Sys_Printf( " Fast bounce mode enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-normalmap" ) )
		{
			normalmap = qtrue;
			Sys_Printf( " Storing normal map instead of lightmap\n" );
		}
		
		else if( !strcmp( argv[ i ], "-trisoup" ) )
		{
			trisoup = qtrue;
			Sys_Printf( " Converting brush faces to triangle soup\n" );
		}

		else if( !strcmp( argv[ i ], "-debug" ) )
		{
			debug = qtrue;
			Sys_Printf( " Lightmap debugging enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-debugsurfaces" ) || !strcmp( argv[ i ], "-debugsurface" ) )
		{
			debugSurfaces = qtrue;
			Sys_Printf( " Lightmap surface debugging enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-debugunused" ) )
		{
			debugUnused = qtrue;
			Sys_Printf( " Unused luxel debugging enabled\n" );
		}

		else if( !strcmp( argv[ i ], "-debugaxis" ) )
		{
			debugAxis = qtrue;
			Sys_Printf( " Lightmap axis debugging enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-debugcluster" ) )
		{
			debugCluster = qtrue;
			Sys_Printf( " Luxel cluster debugging enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-debugorigin" ) )
		{
			debugOrigin = qtrue;
			Sys_Printf( " Luxel origin debugging enabled\n" );
		}
		
		else if( !strcmp( argv[ i ], "-debugstitch" ) )
		{
			debugStitch = qtrue;
			Sys_Printf( " Luxel stitching debugging enabled\n" );
		}

		else if( !strcmp( argv[ i ], "-debugrawlightmap" ) )
		{
			debugRawLightmap = qtrue;
			Sys_Printf( " Raw lightmaps debugging enabled\n" );
		}

		else if( !strcmp( argv[ i ], "-debugdeluxe" ) )
		{
			deluxemap = qtrue;
			debugDeluxemap = qtrue;
			Sys_Printf( " Deluxemap debugging enabled\n" );
		}

		else if( !strcmp( argv[ i ], "-debuglightmap" ) || !strcmp( argv[ i ], "-debuglightmaps" ) )
		{
			debugLightmap = qtrue;
			Sys_Printf( " Lightmap debugging enabled, generating %mapname%_luxels.ase\n" );
		}

		else if( !strcmp( argv[ i ], "-export" ) )
		{
			exportLightmaps = qtrue;
			Sys_Printf( " Exporting lightmaps\n" );
		}
		
		else if( !strcmp(argv[ i ], "-notrace" )) 
		{
			noTrace = qtrue;
			Sys_Printf( " Shadow occlusion disabled\n" );
		}
		// VorteX: -notracegrid
		else if( !strcmp(argv[ i ], "-notracegrid" )) 
		{
			noTraceGrid = qtrue;
			Sys_Printf( " Shadow occlusion for lightgrid disabled\n" );
		}
		else if( !strcmp(argv[ i ], "-patchshadows" ) )
		{
			patchShadows = qtrue;
			Sys_Printf( " Patch shadow casting enabled\n" );
		}
		else if( !strcmp( argv[ i ], "-extra" ) )
		{
			superSample = EXTRA_SCALE;		/* ydnar */
			Sys_Printf( " The -extra argument is deprecated, use \"-super 2\" instead\n" );
		}
		else if( !strcmp( argv[ i ], "-extrawide" ) )
		{
			superSample = EXTRAWIDE_SCALE;	/* ydnar */
			filter = qtrue;					/* ydnar */
			Sys_Printf( " The -extrawide argument is deprecated, use \"-filter [-super 2]\" instead\n");
		}
		else if( !strcmp( argv[ i ], "-samplesize" ) )
		{
			sampleSize = atof( argv[ i + 1 ] );
			if( sampleSize < MIN_LIGHTMAP_SAMPLE_SIZE )
				sampleSize = MIN_LIGHTMAP_SAMPLE_SIZE;
			i++;
			Sys_Printf( " Default lightmap sample size set to %fx%f units\n", sampleSize, sampleSize );
		}
		else if( !strcmp( argv[ i ],  "-samplescale" ) )
 		{
			sampleScale = atof( argv[ i + 1 ] );
 			i++;
			Sys_Printf( " Lightmaps sample scale set to %.3f\n", sampleScale);
 		}
		else if( !strcmp( argv[ i ], "-novertex" ) )
		{
			noVertexLighting = qtrue;
			Sys_Printf( " Disabling vertex lighting\n" );
		}
		else if( !strcmp( argv[ i ], "-nogrid" ) )
		{
			noGridLighting = qtrue;
			Sys_Printf( " Disabling grid lighting\n" );
		}
		else if( !strcmp( argv[ i ], "-border" ) )
		{
			lightmapBorder = qtrue;
			Sys_Printf( " Adding debug border to lightmaps\n" );
		}
		else if( !strcmp( argv[ i ], "-nosurf" ) )
		{
			noSurfaces = qtrue;
			Sys_Printf( " Not tracing against surfaces\n" );
		}
		else if( !strcmp( argv[ i ], "-stitch" ) )
		{
			stitch = qtrue;
			Sys_Printf( " Enable stitching on all lightmaps\n" );
		}
		else if( !strcmp( argv[ i ], "-nostitch" ) )
		{
			noStitch = qtrue;
			Sys_Printf( " Disable lightmap stitching\n" );
		}
		else if( !strcmp( argv[ i ], "-dump" ) )
		{
			dump = qtrue;
			Sys_Printf( " Dumping radiosity lights into numbered prefabs\n" );
		}
		else if( !strcmp( argv[ i ], "-lomem" ) )
		{
			loMem = qtrue;
			Sys_Printf( " Enabling low-memory (slower) lighting mode\n" );
		}
		else if( !strcmp( argv[ i ], "-lomemsky" ) )
		{
			loMemSky = qtrue;
			Sys_Printf( " Enabling low-memory (slower) lighting mode for sky light\n" );
		}
		else if ( !strcmp( argv[ i ], "-lightanglehl" ) )
		{
			qboolean newLightAngleHL = atoi( argv[ i + 1 ] ) != 0 ? qtrue : qfalse;
			if ( newLightAngleHL != lightAngleHL ) 
			{
				lightAngleHL = newLightAngleHL;
				if( lightAngleHL )
					Sys_Printf( " Enabling half lambert light angle attenuation\n" );
				else
					Sys_Printf( " Disabling half lambert light angle attenuation\n" );
			}
			i++;
		}
		else if( !strcmp( argv[ i ], "-nostyle" ) || !strcmp( argv[ i ], "-nostyles" ) )
		{
			noStyles = qtrue;
			Sys_Printf( " Disabling lightstyles\n" );
		}
		else if( !strcmp( argv[ i ], "-style" ) || !strcmp( argv[ i ], "-styles" ) )
		{
			noStyles = qfalse;
			Sys_Printf( " Enabling lightstyles\n" );
		}
		else if( !strcmp( argv[ i ], "-keeplights" ))
		{
			keepLights = qtrue;
			Sys_Printf( " Leaving light entities on map after compile\n" );
		}
		else if( !strcmp( argv[ i ], "-cpma" ) )
		{
			cpmaHack = qtrue;
			Sys_Printf( " Enabling Challenge Pro Mode Asstacular Vertex Lighting Mode (tm)\n" );
		}
		else if( !strcmp( argv[ i ], "-floodlight" ) )
		{
			floodlighty = qtrue;
			Sys_Printf( " FloodLighting enabled\n" );
		}
		else if( !strcmp( argv[ i ], "-debugnormals" ) )
		{
			debugnormals = qtrue;
			Sys_Printf( " DebugNormals enabled\n" );
		}
		else if( !strcmp( argv[ i ], "-lowquality" ) )
		{
			floodlight_lowquality = qtrue;
			Sys_Printf( " Low Quality FloodLighting enabled\n" );
		}
		
		/* dirtmapping / ambient occlusion */
		else if( !strcmp( argv[ i ], "-dirty" ) || !strcmp( argv[ i ], "-ao" ) )
		{
			dirty = qtrue;
			Sys_Printf( " Dirtmapping enabled\n" );
		}
		else if( !strcmp( argv[ i ], "-nodirt" ) || !strcmp( argv[ i ], "-noao" ) )
		{
			nodirt = qtrue;
			Sys_Printf( " Dirtmapping disabled\n" );
		}
		else if( !strcmp( argv[ i ], "-dirtdebug" ) || !strcmp( argv[ i ], "-debugdirt" ) || !strcmp( argv[ i ], "-debugao" ) || !strcmp( argv[ i ], "-aodebug" ) )
		{
			dirtDebug = qtrue;
			Sys_Printf( " Dirtmap debugging enabled\n" );
		}
		else if( !strcmp( argv[ i ], "-dirtmode" ) || !strcmp( argv[ i ], "-aomode" ) )
		{
			dirtMode = (dirtMode_t)atoi( argv[ i + 1 ] );
			Sys_Printf( " Dirtmap mode set to '%s'\n", DirtModeName(&dirtMode) );
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtdepth" ) || !strcmp( argv[ i ], "-aodepth" ) )
		{
			dirtDepth = atof( argv[ i + 1 ] );
			if( dirtDepth <= 0.0f )
				dirtDepth = 128.0f;
			Sys_Printf( " Dirtmapping depth set to %.1f\n", dirtDepth );
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtdepthexponent" ) || !strcmp( argv[ i ], "-aodepthexponent" ) )
		{
			dirtDepthExponent = atof( argv[ i + 1 ] );
			if( dirtDepthExponent <= 0.0f )
				dirtDepthExponent = 2.0f;
			Sys_Printf( " Dirtmapping depth exponent set to %.1f\n", dirtDepthExponent );
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtscale" ) || !strcmp( argv[ i ], "-aoscale" ) )
		{
			dirtScale = atof( argv[ i + 1 ] );
			if( dirtScale <= 0.0f )
				dirtScale = 1.0f;
			Sys_Printf( " Dirtmapping scale set to %.1f\n", dirtScale );
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtscalemask" ) || !strcmp( argv[ i ], "-aoscalemask" ) )
		{
			sscanf(argv[ i + 1], "%f %f %f", &dirtScaleMask[0], &dirtScaleMask[1], &dirtScaleMask[2]); 
			Sys_Printf( " Dirtmapping scale mask set to %1.1f %1.1f %1.1f\n", dirtScaleMask[0], dirtScaleMask[1], dirtScaleMask[2]);
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtgain" ) || !strcmp( argv[ i ], "-aogain" ))
		{
			dirtGain = atof( argv[ i + 1 ] );
			if( dirtGain <= 0.0f )
				dirtGain = 1.0f;
			Sys_Printf( " Dirtmapping gain set to %.1f\n", dirtGain );
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtgainmask" ) || !strcmp( argv[ i ], "-aogainmask" ) )
		{
			sscanf(argv[ i + 1], "%f %f %f", &dirtGainMask[0], &dirtGainMask[1], &dirtGainMask[2]); 
			Sys_Printf( " Dirtmapping gain mask set to %1.1f %1.1f %1.1f\n", dirtGainMask[0], dirtGainMask[1], dirtGainMask[2]);
			i++;
		}
		else if( !strcmp( argv[ i ], "-dirtfilter" ) || !strcmp( argv[ i ], "-aofilter" ) )
		{
			dirtFilter = (dirtFilter_t)atoi( argv[ i + 1 ] );
			Sys_Printf( " Dirtmapping filter set to '%s'\n", DirtFilterName(&dirtFilter) );
			i++;
		}
		/* vortex: global deviance */
		else if( !strcmp( argv[ i ], "-nodeviance" ) )
		{
			noDeviance = qtrue;
			Sys_Printf( " Disable light's deviance\n" );
		}
		else if( !strcmp( argv[ i ], "-deviance" ) )
		{
			devianceJitter = atof( argv[ i + 1 ] );
			if( devianceJitter <= 0.0f )
				devianceJitter = 0.0f;
			Sys_Printf( " Default deviance jitter set to %.1f\n", devianceJitter );
			i++;
		}
		else if( !strcmp( argv[ i ], "-deviancesamples" ) )
		{
			devianceSamples = atoi( argv[ i + 1 ] );
			if( devianceSamples <= 0 )
				devianceSamples = 0;
			Sys_Printf( " Default deviance samples set to %i\n", devianceSamples );
			i++;
		}
		else if( !strcmp( argv[ i ], "-deviancesamplesscale" ) )
		{
			devianceSamplesScale = atof( argv[ i + 1 ] );
			if( devianceSamplesScale <= 0 )
				devianceSamplesScale = 0;
			Sys_Printf( " Deviance samples scale set to %i\n", devianceSamplesScale );
			i++;
		}
		else if( !strcmp( argv[ i ], "-devianceform" ) )
		{
			devianceForm = atoi( argv[ i + 1 ] );
			if (devianceForm == 0)
				Sys_Printf( " Deviance form set to box\n" );
			else if (devianceForm == 1)
				Sys_Printf( " Deviance form set to sphere\n" );
			else
			{
				devianceForm = 0;
				Sys_Printf( " Deviance form set to box\n" );
			}
			i++;
		}
		else if( !strcmp( argv[ i ], "-devianceatten" ) )
		{
			devianceAtten = atoi( argv[ i + 1 ] );
			if (devianceAtten == 0)
				Sys_Printf( " Deviance using no attenuation for light emitting zone\n" );
			else if (devianceAtten == 1)
				Sys_Printf( " Deviance using inverse square attenuation for light emitting zone\n" );
			else
			{
				devianceAtten = 0;
				Sys_Printf( " Deviance using no attenuation for light emitting zone\n" );
			}
			i++;
		}

		/* unhandled args */
		else
			Sys_Warning( "Unknown argument \"%s\"", argv[ i ] );

	}

	/* disable lightstyles is build do not support it */
#if MAX_LIGHTMAPS == 1
	noStyles = qtrue;
#endif

	/* set up lightmap debug state */
	if (debugSurfaces || debugAxis || debugCluster || debugOrigin || dirtDebug || normalmap || (debugGrid && !gridFromLightmap) || debugRawLightmap)
		lightmapDebugState = qtrue;
	else
		lightmapDebugState = qfalse;

	/* set up lmMaxSurfaceSize */
	if (lmMaxSurfaceSize == 0)
		lmMaxSurfaceSize = lmCustomSize;

	/* calculate static parms gamma/exposure/compensate */
	lightmapInvGamma = 1.0f / lightmapGamma;
	lightmapInvExposure = 1.0f / lightmapExposure;
	lightmapInvCompensate = 1.0f / lightmapCompensate;

	/* select optimal */
	ColorToBytes = ColorToBytesUnified;
	if ( lightmapInvGamma == 1 )
	{
		if ( lightmapInvExposure == 1 )
		{
			if ( lightmapInvCompensate == 1 )
				ColorToBytes = ColorToBytesLinear;
			else
				ColorToBytes = ColorToBytesLinearCompensate;
		}
		else
		{
			if ( lightmapInvCompensate == 1 )
				ColorToBytes = ColorToBytesLinearExposureCompensate;
			else
				ColorToBytes = ColorToBytesLinearExposure;
		}
	}
	else
	{
		if ( lightmapInvExposure == 1 )
		{
			if ( lightmapInvCompensate == 1 )
				ColorToBytes = ColorToBytesGamma;
			else
				ColorToBytes = ColorToBytesGammaCompensate;
		}
		else
		{
			if ( lightmapInvCompensate == 1 )
				ColorToBytes = ColorToBytesGammaExposure;
			else
				ColorToBytes = ColorToBytesGammaExposureCompensate;
		}
	}

	/* clean up map name */
	strcpy( source, ExpandArg( argv[ i ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );
	strcpy( mapSource, ExpandArg( argv[ i ] ) );
	StripExtension( mapSource );
	DefaultExtension( mapSource, ".map" );
	
	/* ydnar: set default sample size */
	SetDefaultSampleSize( sampleSize );
	
	/* ydnar: handle shaders */
	BeginMapShaderFile( source );
	LoadShaderInfo();
	
	/* note loading */
	Sys_Printf( "--- LoadBSPFile ---\n" );
	Sys_Printf( "loading %s\n", source );
	
	/* ydnar: load surface file */
	LoadSurfaceExtraFile( source );
	
	/* load bsp file */
	LoadBSPFile( source );
	
	/* parse bsp entities */
	ParseEntities();
	
	/* load map file */
	LoadMapFile( mapSource, (IntForKey( &entities[ 0 ], "_keepLights" ) > 0) ? qfalse : qtrue, qtrue, qfalse, qfalse );
	
	/* set the entity/model origins and init yDrawVerts */
	SetEntityOrigins();

	/* ydnar: set up optimization */
	SetupBrushes();

	/* light the world */
	LightWorld( mapSource );

	/* ydnar: store off lightmaps */
	StoreSurfaceLightmaps();

	/* vortex: set deluxeMaps key */
	if (deluxemap)
	{
		if (deluxemode)
			SetKeyValue(&entities[0], "deluxeMaps", "2");
		else
			SetKeyValue(&entities[0], "deluxeMaps", "1");
	}
	else
		SetKeyValue(&entities[0], "deluxeMaps", "0");

	/* vortex: set lightmapsRGB key */
	if (lightmapsRGB)
		SetKeyValue(&entities[0], "lightmapsRGB", "1");
	else
		SetKeyValue(&entities[0], "lightmapsRGB", "0");

	/* write out the bsp */
	Sys_Printf( "--- WriteBSPFile ---\n" );
	UnparseEntities(qfalse);
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );
	
	/* ydnar: export lightmaps */
	if( exportLightmaps && !externalLightmaps )
		ExportLightmaps();
	
	/* return to sender */
	return 0;
}