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
#define CONVERT_ASE_C



/* dependencies */
#include "q3map2.h"

int convertedSurfaces = 0;
int convertedVertexes = 0;
int convertedTriangles = 0;

/*
ConvertCollapsedSurface()
for collapsed mode converting
*/

int FindVertexInCollapsedMesh(int bspVertexIndex)
{
	int					i;
	bspDrawVert_t		*dv, *dv2;

	dv = &bspDrawVerts[ bspVertexIndex ];
	for (i = 0; i < collapseNumVerts; i++)
	{
		dv2 = &collapseDrawVerts[ i ];
		if (VectorCompare(dv->xyz, dv2->xyz) == qtrue)
			return i;
	}
	Sys_Printf( "Error finding collapsed vertex at '%f %f %f'\n", dv->xyz[ 0 ], dv->xyz[ 1 ], dv->xyz[ 2 ] );
	return bspVertexIndex;
}

static void ConvertCollapsedSurface( FILE *f, bspModel_t *model, int modelNum, int meshNum, int shaderNum, vec3_t origin )
{
	int					i, s, v, a, b, c;
	bspDrawSurface_t	*ds;
	bspDrawVert_t		*dv, *dv2;
	vec3_t				normal;
	int					countIndexes, countTexVerts, exportedFaces, exportedTexVerts, exportedTexFaces;
	qboolean            collapseVertices;
 
	/* print object header for each dsurf */
	if (modelNum == 0)
		sprintf( name, "shader%d", meshNum + 1);
	else
		sprintf( name, "model%dshader%d", modelNum, meshNum + 1);
	fprintf( f, "*GEOMOBJECT\t{\r\n" );
	fprintf( f, "\t*NODE_NAME\t\"%s\"\r\n", name );
	fprintf( f, "\t*NODE_TM\t{\r\n" );
	fprintf( f, "\t\t*NODE_NAME\t\"%s\"\r\n", name );
	fprintf( f, "\t\t*INHERIT_POS\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*INHERIT_ROT\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*INHERIT_SCL\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*TM_ROW0\t1.0\t0\t0\r\n" );
	fprintf( f, "\t\t*TM_ROW1\t0\t1.0\t0\r\n" );
	fprintf( f, "\t\t*TM_ROW2\t0\t0\t1.0\r\n" );
	fprintf( f, "\t\t*TM_ROW3\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*TM_POS\t%f\t%f\t%f\r\n", origin[ 0 ], origin[ 1 ], origin[ 2 ] );
	fprintf( f, "\t}\r\n" );
	convertedSurfaces++;

	/* reset vertex collapse array and other counters */
	collapseNumVerts = 0;
	numMarkDrawSurfaces = 0;
	countIndexes = 0;
	countTexVerts = 0;

	collapseVertices = qfalse;

	/* basic counts, mark drawsurfaces that needs to be processed, collapse identical vertices */
	for( s = 0; s < model->numBSPSurfaces; s++ )
	{
		ds = &bspDrawSurfaces[model->firstBSPSurface + s];
		if( ds->surfaceType != MST_PLANAR && ds->surfaceType != MST_TRIANGLE_SOUP )
			continue;
		if( ds->shaderNum != shaderNum)
			continue;
		countIndexes += ds->numIndexes;
		countTexVerts += ds->numVerts;
		// to speed up next loops
		markDrawSurfaces[numMarkDrawSurfaces] = model->firstBSPSurface + s;
		numMarkDrawSurfaces++;
		// collapse vertices into mesh
		for (i = 0; i < ds->numVerts; i++)
		{
			v = i + ds->firstVert;
			dv = &bspDrawVerts[ v ];
			if (!collapseVertices)
				v = collapseNumVerts;
			else
			{
				for (v = 0; v < collapseNumVerts; v++)
				{
					dv2 = &collapseDrawVerts[ v ];
					if (VectorCompare(dv2->xyz, dv->xyz) == qtrue) // already exists, merge normals
					{
						VectorAdd( dv2->normal, dv->normal, dv2->normal);
						VectorNormalize( dv2->normal, dv2->normal);
						break;
					}
				}
			}
			if (v == collapseNumVerts) // not found, add vertex and normal
			{
				VectorCopy( dv->xyz, collapseDrawVerts[collapseNumVerts].xyz );
				VectorCopy( dv->normal, collapseDrawVerts[collapseNumVerts].normal );
				collapseNumVerts++;
			}
		}
	}

	/* print mesh header */
	fprintf( f, "\t*MESH\t{\r\n" );
	fprintf( f, "\t\t*TIMEVALUE\t0\r\n" );
	fprintf( f, "\t\t*MESH_NUMVERTEX\t%d\r\n", collapseNumVerts );
	fprintf( f, "\t\t*MESH_NUMFACES\t%d\r\n", countIndexes / 3 );
	fprintf( f, "\t\t*COMMENT\t\"COLLAPSED BY SHADER\"\r\n" );

	/* export vertex xyz */
	fprintf( f, "\t\t*MESH_VERTEX_LIST\t{\r\n" );
	for (i = 0; i < collapseNumVerts; i++)
	{
		dv = &collapseDrawVerts[ i ];
		fprintf( f, "\t\t\t*MESH_VERTEX\t%d\t%f\t%f\t%f\r\n", i, dv->xyz[ 0 ], dv->xyz[ 1 ], dv->xyz[ 2 ] );
		convertedVertexes++;
	}
	fprintf( f, "\t\t}\r\n" );

	/* export face normals */
	fprintf( f, "\t\t*MESH_NORMALS\t{\r\n" );
	exportedFaces = 0;
	for( s = 0; s < numMarkDrawSurfaces; s++ )
	{
		i = markDrawSurfaces[s];
		ds = &bspDrawSurfaces[i];
		for( i = 0; i < ds->numIndexes; i += 3, exportedFaces++ )
		{
			a = bspDrawIndexes[ i + ds->firstIndex ];
			c = bspDrawIndexes[ i + ds->firstIndex + 1 ];
			b = bspDrawIndexes[ i + ds->firstIndex + 2 ];
			a = FindVertexInCollapsedMesh(ds->firstVert + a);
			b = FindVertexInCollapsedMesh(ds->firstVert + b);
			c = FindVertexInCollapsedMesh(ds->firstVert + c);
			VectorCopy( collapseDrawVerts[ a ].normal, normal );
			VectorAdd( normal, collapseDrawVerts[ b ].normal, normal );
			VectorAdd( normal, collapseDrawVerts[ c ].normal, normal );
			if( VectorNormalize( normal, normal ) )
				fprintf( f, "\t\t\t*MESH_FACENORMAL\t%d\t%f\t%f\t%f\r\n", exportedFaces, normal[ 0 ], normal[ 1 ], normal[ 2 ] );
		}
	}

	/* export vertex normals */
	for (i = 0; i < collapseNumVerts; i++)
	{
		dv = &collapseDrawVerts[ i ];
		fprintf( f, "\t\t\t*MESH_VERTEXNORMAL\t%d\t%f\t%f\t%f\r\n", i, dv->normal[ 0 ], dv->normal[ 1 ], dv->normal[ 2 ] );
	}
	fprintf( f, "\t\t}\r\n" );

	/* export faces */
	fprintf( f, "\t\t*MESH_FACE_LIST\t{\r\n" );
	exportedFaces = 0;
	for( s = 0; s < numMarkDrawSurfaces; s++ )
	{
		i = markDrawSurfaces[s];
		ds = &bspDrawSurfaces[i];
		for( i = 0; i < ds->numIndexes; i += 3, exportedFaces++)
		{
			a = bspDrawIndexes[ i + ds->firstIndex ];
			c = bspDrawIndexes[ i + ds->firstIndex + 1 ];
			b = bspDrawIndexes[ i + ds->firstIndex + 2 ];
			a = FindVertexInCollapsedMesh(ds->firstVert + a);
			b = FindVertexInCollapsedMesh(ds->firstVert + b);
			c = FindVertexInCollapsedMesh(ds->firstVert + c);
			fprintf( f, "\t\t\t*MESH_FACE\t%d\tA:\t%d\tB:\t%d\tC:\t%d\tAB:\t1\tBC:\t1\tCA:\t1\t*MESH_SMOOTHING\t0\t*MESH_MTLID\t0\r\n", exportedFaces, a, b, c );
			convertedTriangles++;
		}
	}
	fprintf( f, "\t\t}\r\n" );

	/* export vertex st */
	fprintf( f, "\t\t*MESH_NUMTVERTEX\t%d\r\n", countTexVerts );
	fprintf( f, "\t\t*MESH_TVERTLIST\t{\r\n" );
	exportedTexVerts = 0;
	for( s = 0; s < numMarkDrawSurfaces; s++ )
	{
		i = markDrawSurfaces[s];
		ds = &bspDrawSurfaces[i];
		for( i = 0; i < ds->numVerts; i++ )
		{
			v = i + ds->firstVert;
			dv = &bspDrawVerts[ v ];
			fprintf( f, "\t\t\t*MESH_TVERT\t%d\t%f\t%f\t%f\r\n", exportedTexVerts + i, dv->st[ 0 ], (1.0 - dv->st[ 1 ]), 1.0f );
		}
		exportedTexVerts += ds->numVerts;
	}
	fprintf( f, "\t\t}\r\n" );
	
	/* export texture faces */
	fprintf( f, "\t\t*MESH_NUMTVFACES\t%d\r\n", countIndexes / 3 );
	fprintf( f, "\t\t*MESH_TFACELIST\t{\r\n" );
	exportedTexFaces = 0;
	exportedTexVerts = 0;
	for( s = 0; s < numMarkDrawSurfaces; s++ )
	{
		i = markDrawSurfaces[s];
		ds = &bspDrawSurfaces[i];
		for( i = 0; i < ds->numIndexes; i += 3, exportedTexFaces++ )
		{
			a = exportedTexVerts + bspDrawIndexes[ i + ds->firstIndex ];
			c = exportedTexVerts + bspDrawIndexes[ i + ds->firstIndex + 1 ];
			b = exportedTexVerts + bspDrawIndexes[ i + ds->firstIndex + 2 ];
			fprintf( f, "\t\t\t*MESH_TFACE\t%d\t%d\t%d\t%d\r\n", exportedTexFaces, a, b, c );
		}
		exportedTexVerts += ds->numVerts;
	}
	fprintf( f, "\t\t}\r\n" );

	/* print mesh footer */
	fprintf( f, "\t}\r\n" );

	/* print object footer */
	fprintf( f, "\t*PROP_MOTIONBLUR\t0\r\n" );
	fprintf( f, "\t*PROP_CASTSHADOW\t1\r\n" );
	fprintf( f, "\t*PROP_RECVSHADOW\t1\r\n" );
	fprintf( f, "\t*MATERIAL_REF\t%d\r\n", ds->shaderNum );
	fprintf( f, "}\r\n" );
}


/*
ConvertSurface()
converts a bsp drawsurface to an ase chunk
*/

static void ConvertSurface( FILE *f, bspModel_t *model, int modelNum, bspDrawSurface_t *ds, int surfaceNum, vec3_t origin )
{
	int				i, v, face, a, b, c;
	bspDrawVert_t	*dv;
	vec3_t			normal;
	char			name[ MAX_OS_PATH ];
	
	/* ignore patches for now */
	if( ds->surfaceType != MST_PLANAR && ds->surfaceType != MST_TRIANGLE_SOUP )
		return;
	
	/* print object header for each dsurf */
	sprintf( name, "mat%dmodel%dsurf%d", ds->shaderNum, modelNum, surfaceNum );
	fprintf( f, "*GEOMOBJECT\t{\r\n" );
	fprintf( f, "\t*NODE_NAME\t\"%s\"\r\n", name );
	fprintf( f, "\t*NODE_TM\t{\r\n" );
	fprintf( f, "\t\t*NODE_NAME\t\"%s\"\r\n", name );
	fprintf( f, "\t\t*INHERIT_POS\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*INHERIT_ROT\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*INHERIT_SCL\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*TM_ROW0\t1.0\t0\t0\r\n" );
	fprintf( f, "\t\t*TM_ROW1\t0\t1.0\t0\r\n" );
	fprintf( f, "\t\t*TM_ROW2\t0\t0\t1.0\r\n" );
	fprintf( f, "\t\t*TM_ROW3\t0\t0\t0\r\n" );
	fprintf( f, "\t\t*TM_POS\t%f\t%f\t%f\r\n", origin[ 0 ], origin[ 1 ], origin[ 2 ] );
	fprintf( f, "\t}\r\n" );
	convertedSurfaces++;
	
	/* print mesh header */
	fprintf( f, "\t*MESH\t{\r\n" );
	fprintf( f, "\t\t*TIMEVALUE\t0\r\n" );
	fprintf( f, "\t\t*MESH_NUMVERTEX\t%d\r\n", ds->numVerts );
	fprintf( f, "\t\t*MESH_NUMFACES\t%d\r\n", ds->numIndexes / 3 );
	switch( ds->surfaceType )
	{
		case MST_PLANAR:
			fprintf( f, "\t\t*COMMENT\t\"SURFACETYPE\tMST_PLANAR\"\r\n" );
			break;
		case MST_TRIANGLE_SOUP:
			fprintf( f, "\t\t*COMMENT\t\"SURFACETYPE\tMST_TRIANGLE_SOUP\"\r\n" );
			break;
	}
	
	/* export vertex xyz */
	fprintf( f, "\t\t*MESH_VERTEX_LIST\t{\r\n" );
	for( i = 0; i < ds->numVerts; i++ )
	{
		v = i + ds->firstVert;
		dv = &bspDrawVerts[ v ];
		fprintf( f, "\t\t\t*MESH_VERTEX\t%d\t%f\t%f\t%f\r\n", i, dv->xyz[ 0 ], dv->xyz[ 1 ], dv->xyz[ 2 ] );
		convertedVertexes++;
	}
	fprintf( f, "\t\t}\r\n" );

	/* export vertex normals */
	fprintf( f, "\t\t*MESH_NORMALS\t{\r\n" );
	for( i = 0; i < ds->numIndexes; i += 3 )
	{
		face = (i / 3);
		a = bspDrawIndexes[ i + ds->firstIndex ];
		b = bspDrawIndexes[ i + ds->firstIndex + 1 ];
		c = bspDrawIndexes[ i + ds->firstIndex + 2 ];
		VectorCopy( bspDrawVerts[ a ].normal, normal );
		VectorAdd( normal, bspDrawVerts[ b ].normal, normal );
		VectorAdd( normal, bspDrawVerts[ c ].normal, normal );
		if( VectorNormalize( normal, normal ) )
			fprintf( f, "\t\t\t*MESH_FACENORMAL\t%d\t%f\t%f\t%f\r\n", face, normal[ 0 ], normal[ 1 ], normal[ 2 ] );
	}
	for( i = 0; i < ds->numVerts; i++ )
	{
		v = i + ds->firstVert;
		dv = &bspDrawVerts[ v ];
		fprintf( f, "\t\t\t*MESH_VERTEXNORMAL\t%d\t%f\t%f\t%f\r\n", i, dv->normal[ 0 ], dv->normal[ 1 ], dv->normal[ 2 ] );
	}
	fprintf( f, "\t\t}\r\n" );
	
	/* export faces */
	fprintf( f, "\t\t*MESH_FACE_LIST\t{\r\n" );
	for( i = 0; i < ds->numIndexes; i += 3 )
	{
		face = (i / 3);
		a = bspDrawIndexes[ i + ds->firstIndex ];
		c = bspDrawIndexes[ i + ds->firstIndex + 1 ];
		b = bspDrawIndexes[ i + ds->firstIndex + 2 ];
		fprintf( f, "\t\t\t*MESH_FACE\t%d\tA:\t%d\tB:\t%d\tC:\t%d\tAB:\t1\tBC:\t1\tCA:\t1\t*MESH_SMOOTHING\t0\t*MESH_MTLID\t0\r\n", face, a, b, c );
		convertedTriangles++;
	}
	fprintf( f, "\t\t}\r\n" );
	
	/* export vertex st */
	fprintf( f, "\t\t*MESH_NUMTVERTEX\t%d\r\n", ds->numVerts );
	fprintf( f, "\t\t*MESH_TVERTLIST\t{\r\n" );
	for( i = 0; i < ds->numVerts; i++ )
	{
		v = i + ds->firstVert;
		dv = &bspDrawVerts[ v ];
		fprintf( f, "\t\t\t*MESH_TVERT\t%d\t%f\t%f\t%f\r\n", i, dv->st[ 0 ], (1.0 - dv->st[ 1 ]), 1.0f );
	}
	fprintf( f, "\t\t}\r\n" );
	
	/* export texture faces */
	fprintf( f, "\t\t*MESH_NUMTVFACES\t%d\r\n", ds->numIndexes / 3 );
	fprintf( f, "\t\t*MESH_TFACELIST\t{\r\n" );
	for( i = 0; i < ds->numIndexes; i += 3 )
	{
		face = (i / 3);
		a = bspDrawIndexes[ i + ds->firstIndex ];
		c = bspDrawIndexes[ i + ds->firstIndex + 1 ];
		b = bspDrawIndexes[ i + ds->firstIndex + 2 ];
		fprintf( f, "\t\t\t*MESH_TFACE\t%d\t%d\t%d\t%d\r\n", face, a, b, c );
	}
	fprintf( f, "\t\t}\r\n" );
	
	/* print mesh footer */
	fprintf( f, "\t}\r\n" );

	/* print object footer */
	fprintf( f, "\t*PROP_MOTIONBLUR\t0\r\n" );
	fprintf( f, "\t*PROP_CASTSHADOW\t1\r\n" );
	fprintf( f, "\t*PROP_RECVSHADOW\t1\r\n" );
	fprintf( f, "\t*MATERIAL_REF\t%d\r\n", ds->shaderNum );
	fprintf( f, "}\r\n" );
}



/*
ConvertModel()
exports a bsp model to an ase chunk
*/

static void ConvertModel( FILE *f, bspModel_t *model, int modelNum, vec3_t origin, int collapseByTexture )
{
	int					i, s, dsi, meshNum;
	bspDrawSurface_t	*ds;
	
	if (collapseByTexture)
	{
		/* create geometry object for each texture */
		meshNum = 0;
		for( i = 0; i < numBSPShaders; i++ )
		{
			// only do export if model has any drawsurf of specified shader
			for (s = 0; s < model->numBSPSurfaces; s++)
			{
				dsi = s + model->firstBSPSurface;
				ds = &bspDrawSurfaces[ dsi ];
				if (ds->shaderNum == i)
				{
					ConvertCollapsedSurface(f, model, modelNum, meshNum, i, origin);
					meshNum++;
					break;
				}
			}
		}
		return;
	}
	/* go through each drawsurf in the model */
	for( i = 0; i < model->numBSPSurfaces; i++ )
	{
		s = i + model->firstBSPSurface;
		ds = &bspDrawSurfaces[ s ];
		ConvertSurface( f, model, modelNum, ds, s, origin );
	}
}



/*
ConvertShader()
exports a bsp shader to an ase chunk
*/

/*
	*MATERIAL 0 {
		*MATERIAL_NAME "models/test/rock16l"
		*MATERIAL_CLASS "Standard"
		*MATERIAL_AMBIENT 0.5882	0.5882	0.5882
		*MATERIAL_DIFFUSE 0.5882	0.5882	0.5882
		*MATERIAL_SPECULAR 0.5882	0.5882	0.5882
		*MATERIAL_SHINE 0.0000
		*MATERIAL_SHINESTRENGTH 0.0000
		*MATERIAL_TRANSPARENCY 0.0000
		*MATERIAL_WIRESIZE 1.0000
		*MATERIAL_SHADING Phong
		*MATERIAL_XP_FALLOFF 0.0000
		*MATERIAL_SELFILLUM 0.0000
		*MATERIAL_FALLOFF In
		*MATERIAL_XP_TYPE Filter
		*MAP_DIFFUSE {
			*MAP_NAME "Map #2"
			*MAP_CLASS "Bitmap"
			*MAP_SUBNO 1
			*MAP_AMOUNT 1.0000
			*BITMAP "models/test/rock16l"
			*MAP_TYPE Screen
			*UVW_U_OFFSET 0.0000
			*UVW_V_OFFSET 0.0000
			*UVW_U_TILING 1.0000
			*UVW_V_TILING 1.0000
			*UVW_ANGLE 0.0000
			*UVW_BLUR 1.0000
			*UVW_BLUR_OFFSET 0.0000
			*UVW_NOUSE_AMT 1.0000
			*UVW_NOISE_SIZE 1.0000
			*UVW_NOISE_LEVEL 1
			*UVW_NOISE_PHASE 0.0000
			*BITMAP_FILTER Pyramidal
		}
	}
*/

static void ConvertShader( FILE *f, bspShader_t *shader, int shaderNum )
{
	shaderInfo_t	*si;
	char			*c, filename[ MAX_OS_PATH ];
	
	
	/* get shader */
	si = ShaderInfoForShader( shader->shader );
	if( si == NULL )
	{
		Sys_Warning( "NULL shader in BSP" );
		return;
	}
	
	/* set bitmap filename */
	if( si->shaderImage->filename[ 0 ] != '*' )
		strcpy( filename, si->shaderImage->filename );
	else
		sprintf( filename, "%s.tga", si->shader );
	for( c = filename; *c != '\0'; c++ )
		if( *c == '/' )
			*c = '\\';
	
	/* print shader info */
	fprintf( f, "\t*MATERIAL\t%d\t{\r\n", shaderNum );
	fprintf( f, "\t\t*MATERIAL_NAME\t\"%s\"\r\n", shader->shader );
	fprintf( f, "\t\t*MATERIAL_CLASS\t\"Standard\"\r\n" );
	fprintf( f, "\t\t*MATERIAL_DIFFUSE\t%f\t%f\t%f\r\n", si->color[ 0 ], si->color[ 1 ], si->color[ 2 ] );
	fprintf( f, "\t\t*MATERIAL_SHADING Phong\r\n" );
	
	/* print map info */
	fprintf( f, "\t\t*MAP_DIFFUSE\t{\r\n" );
	fprintf( f, "\t\t\t*MAP_NAME\t\"%s\"\r\n", shader->shader );
	fprintf( f, "\t\t\t*MAP_CLASS\t\"Bitmap\"\r\n");
	fprintf( f, "\t\t\t*MAP_SUBNO\t1\r\n" );
	fprintf( f, "\t\t\t*MAP_AMOUNT\t1.0\r\n" );
	fprintf( f, "\t\t\t*MAP_TYPE\tScreen\r\n" );
	fprintf( f, "\t\t\t*BITMAP\t\"..\\%s\"\r\n", filename );
	fprintf( f, "\t\t\t*BITMAP_FILTER\tPyramidal\r\n" );
	fprintf( f, "\t\t}\r\n" );
	
	fprintf( f, "\t}\r\n" );
}



/*
ConvertBSPToASE()
exports an 3d studio ase file from the bsp
*/

int ConvertBSPToASE( char *bspName, int collapseByTexture )
{
	int				i, modelNum;
	FILE			*f;
	bspShader_t		*shader;
	bspModel_t		*model;
	entity_t		*e;
	vec3_t			origin;
	const char		*key;
	char			name[ MAX_OS_PATH ], base[ MAX_OS_PATH ];
	
	
	/* note it */
	Sys_Printf( "--- BSPtoASE ---\n" );

	/* collapsing by texture name */
	if (collapseByTexture)
	{
		Sys_Printf( "Enabling collapsing meshes by texture name\n" );
		/* allocate mark tables */
		collapseDrawVerts = (bspDrawVert_t *)safe_malloc( numBSPDrawVerts * sizeof( bspDrawVert_t ) );
		markDrawSurfaces = (int *)safe_malloc( numBSPDrawSurfaces * sizeof( int ) );
	}

	/* create the ase filename from the bsp name */
	strcpy( name, bspName );
	StripExtension( name );
	strcat( name, ".ase" );
	Sys_Printf( "writing %s\n", name );
	
	ExtractFileBase( bspName, base );
	strcat( base, ".bsp" );
	
	/* open it */
	f = fopen( name, "wb" );
	if( f == NULL )
		Error( "Open failed on %s\n", name );
	
	/* print header */
	fprintf( f, "*3DSMAX_ASCIIEXPORT\t200\r\n" );
	fprintf( f, "*COMMENT\t\"Generated by Q3Map2 (ydnar) -convert -format ase\"\r\n" );
	fprintf( f, "*SCENE\t{\r\n" );
	fprintf( f, "\t*SCENE_FILENAME\t\"%s\"\r\n", base );
	fprintf( f, "\t*SCENE_FIRSTFRAME\t0\r\n" );
	fprintf( f, "\t*SCENE_LASTFRAME\t100\r\n" );
	fprintf( f, "\t*SCENE_FRAMESPEED\t30\r\n" );
	fprintf( f, "\t*SCENE_TICKSPERFRAME\t160\r\n" );
	fprintf( f, "\t*SCENE_BACKGROUND_STATIC\t0.0000\t0.0000\t0.0000\r\n" );
	fprintf( f, "\t*SCENE_AMBIENT_STATIC\t0.0000\t0.0000\t0.0000\r\n" );
	fprintf( f, "}\r\n" );
	
	/* print materials */
	fprintf( f, "*MATERIAL_LIST\t{\r\n" );
	fprintf( f, "\t*MATERIAL_COUNT\t%d\r\n", numBSPShaders );
	for( i = 0; i < numBSPShaders; i++ )
	{
		shader = &bspShaders[ i ];
		ConvertShader( f, shader, i );
	}
	fprintf( f, "}\r\n" );
	
	/* walk entity list */
	for( i = 0; i < numEntities; i++ )
	{
		/* get entity and model */
		e = &entities[ i ];
		if( i == 0 )
			modelNum = 0;
		else
		{
			key = ValueForKey( e, "model" );
			if( key[ 0 ] != '*' )
				continue;
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];
		
		/* get entity origin */
		key = ValueForKey( e, "origin" );
		if( key[ 0 ] == '\0' )
			VectorClear( origin );
		else
			GetVectorForKey( e, "origin", origin );
		
		/* convert model */
		convertedSurfaces = 0;
		convertedVertexes = 0;
		convertedTriangles = 0;
		Sys_Printf( "exporting model %i...", modelNum );
		ConvertModel( f, model, modelNum, origin, collapseByTexture );
		Sys_Printf( "%i surfaces, %i vertexes, %i triangles\n", convertedSurfaces, convertedVertexes, convertedTriangles );
	}
	
	/* close the file and return */
	fclose( f );
	
	/* return to sender */
	return 0;
}



