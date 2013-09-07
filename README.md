Blood Map is a fork of Q3Map2 compiler used for RazorWind Games projects

Screenshots
------
![terrain alpha](/images/terrainalpha.jpg)
![Ambient occlusion](/images/ambientocclusion.jpg)
![misc_model grouping](/images/misc_model_grouping.jpg)

Known bugs
------
- MAX_LIGHTMAPS is 2 (was 4)
- floodlight code is turned off

--------------------------------------------------------------------------------
 Version History + Changelog
--------------------------------------------------------------------------------

1.1.4
------
- Get rid of MHASH dependency
- New key -entityid 'keyname' for -patch stage. Enables patching of submodel
  entity fields by making map/bsp entity pairs with same unique key index.
- New key -triggers for -patch stage. Enables replacing "trigger_" entities
  from .MAP. This requires gamecode support - trigger entities should be able
  to initialize themself with no submodel, but only mins/maxs keys.

 
1.1.3 (Blood Omnicide internal release)
------
- New -patch stage which will patch .BSP with .MAP contents.
  Patching will replace .BSP point entities with .MAP point entities, 
  effectively updating monsters, items spawn points ans so on, but not doors, 
  trains or any bmodel entities.
- Removed -onlyents compile


1.1.2 (Blood Omnicide internal release)
------
- New -optimize stage, which does various optimizations on BSP (see below)
- New: q3map_engineShader 'shadername' used by tidyshaders stage
- TidyShaders: replaces shaders with their engineShader
  this will automatically merge surfaces which got same engineShader
- TidyEntities: remove compiler-related entity keys such as _cs,
  _rs, _lightmapscale, _np and so on.
- MergeDrawSurfaces: optimizes BSP size by merging near drawsurfaces
  that has exactly same appearance. Surfaces are merged within {96 96 1024} block.
  -mergeblock X Y Z can force a custom merging block size. 
- MergeDrawVerts: optimizes BSP by merging coincident vertices
- -onlyshaders will force TidyShaders-only optimization.
- Other parms: -tidyshaders, -notidyshaders, -mergesurfaces, -nomergesurfaces,
  -mergeverts, -nomergeverts, -tidyents, -notidyents, -custinfoparms
- Experimental: -skylightao. Tries to use Ambient Occlusion technique to render
  sky light.
- Merged additional empty lightmap fix from NetRadiant
- Merged InsertModel() clipping fixes from NetRadiant

 
1.1.1 (Blood Omnicide internal release)
------
- Spherical-based ambient occlusion technique (dirtmapping mode 2 and 3),
  old dirtmapping 0 and 1 was cone-based. Dirtmap parameters for new mode works
  differently: gain sets additive brighten effect on sharp corners, scale
  darkens enclosed areas. New worldspawn keys: _ao, _aoMode, _aoDepth,
  _aoDepthExponent, _aoGain, _aoGainMask, _aoScale, _aoScaleMask.
  New: -dirtscalemask, -dirtgainmask, -dirtdepthexponent
- New: q3map_aoScale controls ambient occlusion intensity (both gain and scale)
- New: q3map_aoGainScale controls ambient occlusion gain intensity
- New: "_ao" key (same as _dirty), _aoGain, _aoScale, _aoMode, _aoDepth 
  
  
1.1.0 (Blood Omnicide internal release)
------
- _dirty key on worldspawn, works just like _floodlight.
  Values are "mode depth gain scale". Default values are "0 128 1 1"
- New: -vertexscale scales vertex lighting for map
- New: -brightness scales lightmaps intensity for map
  (just like q3map_lightmapBrightness_
- q3map_vertexPointSample forces a LightGrid point sampling for vertex lighting
- q3map_noVertexLight breaks any vertex light calculations
- deluxemap luxels now initialized to surface normal
- New: global deviance parms: -deviance, -deviancesamples, -nodeviance
- New: -notracegrid disables shadow occlusion for lightgrid
- New: _mindist key for lights (overide default 'hot distance' of 16)
- Renormalize SUPER_NORMAL when converting to tangentspace deluxemaps.
  Clamp Z component to 75.5 degrees to prevent exaggerations and light blooms.
  (this is what Darkplaces does for modelspace deluxemaps)/
- -broadmerge - merges together same-shader vertexlit-only surfaces
- submodel sizes now printed with -info 
  
 
1.0.9 (Blood Omnicide internal release)
------
- New: "Decorate" function which will run some automations in very beginning of
  BSP stage. Automations include: force entity keys based on classname or 
  _decore key, importing .rtlights, assign target/targetname keys on misc_models
  so they will produce a "bulk-bmodel" (useful with trees and grass)
  Options for function is stored inside 'basedir'/scripts/decorations.txt.
  
Example of decorations.txt:

options
{
	importrtlights
	{
		active            true
		colorscale        1.5
		saturate          0.8
		radiusmod         0.5
		spawnflags        32    // angle attenuation
		// no shadow lights (area lights)
		noShadowDeviance  5     // 5 degrees deviance to simulate penumbra
		noShadowSamples   24    // sample with 24 lights
		noShadowMinDist   1.5
		// shadow lights 
		shadowDeviance    2
		shadowSamples     12
		shadowMinDist     0.15
		skipCubemapped    1     // skip cubemap-filtered lights
		skipStyle         1     // skip light styled with #1
	}
}
// sun lights
group sunlight
{
	entity
	{
		default "_anglescale" 0.5
		default "_color" "0.6 0.6 0.6"
		default "light" 2500
		default "spawnflags" 32
	}
}
// generic misc_models
group generic
{
	// static bmodel ents
	entity
	{
		default "_cs" 1
		default "_rs" 1
		flagset "spawnflags" 5 // cast shadows + lightmapped
	}
}
// func_walls
class func_wall
{
	entity
	{
		default "_cs" 1
		default "_rs" 1
	}
}


1.0.8 (Blood Omnicide internal release)
------
- New: misc_decal compiler entity, idential to _decal. Now supports _ls
  (lightmapscale) and _np (smooth normals) keys
- New: -noclipmodel entirely disables misc_model "autoclip" spawnflag
- Removed: entity _smooth key (only _smoothnormals and _np are valid keys)
- _smoothnormals now works on func_group and (perharps) misc_model
- New: _nonsolid 1 entity key breaks generation of BSP brushes
- New: _vp 'dist' key forces a vertical texture projection on
  misc_model (effect is same as for q3map_tcMod ivector)
- New: _uvs 'scale' scales UV on misc_model
- q3map_alphamod random 'min' 'max' 'power' - set random vertex alpha of
  vertices inside brush. Randomizing using precalculated random grid.
- q3map_alphamod randomjitter 'min' 'max' 'power' - adds random value
- q3map_alphamod randomscale 'min' 'max' 'power' - scale by random value
- q3map_textureImage 'image' mimics qer_editorImage but not seen by Radiant
  Allows use of different sized texture when qer_editorimage is set


1.0.7b (ArcadeQuake internal release)  
------
- -custinfoparms now works in convert stage
- New: -collapsebytexture" for ASE convertsion. Enables mesh collapsing by
  shader name.


1.0.7a (ArcadeQuake internal release)  
------
- MAX_MAP_MODELS 4096 (was 1024)
- MAX_MAP_BRUSHES 65536 (was 32768)
- MAX_MAP_PLANES 2097152 (was 1048576)
- MAX_CUST_SURFACEPARMS 1024 (was 64)


1.0.7 (ArcadeQuake internal release)
------
- fixed crash with -super 2 with tangentspace deluxemaps
- splotchFix now fixes normal as well as luxel and deluxel, this removes a
  glitch with bad conversion of CLUSTER_FLOODED deluxels to tangentspace
  (because of null normal).

  
1.0.6 (ArcadeQuake internal release)
------
- new -nocolornorm switch and -colornorm (default for all games except dq,
  prophecy, darkplaces) for light stage to set lights color normalization.
  This is useful to mimic hmap2 behavior since it do not normalize light colors
- light stage now write "deluxeMaps" worldspawn key which is "0" for no
  deluxemapping, "1" for modelspace deluxemaps and "2" for tangentspace  
- shadowless deluxemaps calculation are now optional since Darkplaces
  now fixes deluxemaps shadow edge bug. Use -nodeluxeshadow / -deluxeshadow
  compile switch to change it
- fixed nasty bug where -samplescale will be read as integer, resulting
  to no support of -samplescale 0.5 and so on. 
- added experimental -nodetailcollision feature which disables creation
  of collision planes for detail brushes. This got used on Arcade Quake levels.
- removed -deluxemode key, use -tangentspace/-modelspace instead


1.0.5 (DeluxeQuake internal release)
------
- new commandline keys "-gridscale X" and "-gridambientscale X" to scale grid
  lightning, note that ambient grid receive both "-gridscale"
  and "-gridambientscale". For -darkplaces, -dq and -prophecy game mode added 
  default game-specific values: -gridscale 0.3 -gridambientscale 0.4
- modified game-specific options prints at the begin of light stage


1.0.4 (DeluxeQuake internal release)
------
- "light" got spawnflag 32 "unnormalized" - it means that light color will
  not be normalized on parse
- for "light" added spawnflag 64 "distance_falloff" to gain access to light
  distance attenuation used for sun and area lights
- to make things easy, behavior of _deviance/_samples on lights is changed, 
  now when "_deviance" detected, "_samples" get same start value.
  So in most cases we need to set 1 key instead of 2.


1.0.3 (DeluxeQuake internal release)
------
- fixed tangentspace deluxemaps with radiocity (was broken)
- fixed bug with overbrights on shadow edges in deluxemapping. This bug was here
  because shadowed luxels does not receive light directions leading to very
  contrast light direction vectors in light/shadow zones, then texture filtering
  introduces a bug when interpolating this values. Now deluxels calculated with
  no shadows (shadowing work is done in lightmap already)
- added "-keeplights" switch into light phase, this works like "_keeplights 1"
  world key. World key has greater priority (so if you set -keeplights and
  _keeplights 0 light entites won't be keeped). Also Per-game "keepLights"
  setting added, it defaults to True in "dq", "darkplaces" and "prophecy"
  games.
- Global floodlight code reverted back to have no effect on deluxemap
  (it don't add anything and deluxemaps looks blurry). q3map_floodlight shader
  keyword is changed to have 'light_direction_power' parameter in the end
  unless "low_quality", so now it is
  q3map_floodlight 'r' 'g' 'b' 'dist' 'intentity' 'light_direction_power'. 
  Use 0 in to have no effect on deluxemap, 1 to have standart effect like
  all lights do and 200 and more to make floodlight override deluxemap on
  this surface (it would be useful for floodlighted water since if some
  lightsource will be too close it will make dark zone on deluxemap).
- added printing "entity has vertex normal smoothing with..." when
  _smoothnormals/_sn/_smooth keys is found, just like _lightmapscale did


1.0.2 (DeluxeQuake internal release)
------
- added "_smoothnormals" or "_sn" or "_smooth" to set nonplanar normal
  smoothing on entities. This works exactly as -shadeangle and overrides
  shader-set normal smoothing (q3map_shadeangle).
- reorganized floodlighting code to be more clean
- fixed bug with floodlight not adding a light direction to deluxemaps
- removed "q3map_minlight" test code that was added in previous version
- added "-deluxemode 1" switch to generate tangentspace deluxemaps instead
  of modelspace. Actually deluxemaps being converted to tangentspace in
  StoreLigtmaps phase. "-deluxemode 0" will switch back to modelspace
  deluxemaps
- added game-specific setting of deluxemode. "darkplaces", "dq" and "prophecy"
  games still have 0 because darkplaces engine can't detect tangentspace
  deluxemaps on Q3BSP yet (probably detection can be done by setting of
  custom world field?)

  
1.0.1 (DeluxeQuake internal release)
------
- added shader deprecation keyword q3map_deprecateShader 'shader', a global
  variant of q3map_baseShader/q3map_remapShader. Replacing is done in early
  load stage, so all q3map2 keyworlds are supported (instead of
  q3map_remapShader which only remaps rendering part of shader so
  surfaceparms won't work). Maximum of 16 chained deprecations are allowed.
  In other worlds if you deprecate the shader by this keyword, it will be
  shown in map with another name
- added "_patchMeta 1" entity key (or "patchMeta") entity keyword to force
  entity patch surfaces to be converted to planar brush at compile. This works
  exactly as "-patchmeta" bsp switch, but only for user customized entities.
  Additional 2 new keys: "_patchQuality" ("patchQuality") and "_patchSubdivide"
  ("patchSubdivide") are added. _PatchQuality divides the default patch
  subdivisions by it's value (2 means 2x more detailed patch).
  _patchSubdivide overrides patch subdivisions for that entity (use 4 to
  match Quake3) and ignores _patchQuality. Note: using "_patchMeta" on world
  makes all world patches to be triangulated, but other entities will remain
  same
- added "EmitMetaStats" printing in the end of BSP stage to show full meta
  statistics, not only for world. So all "_patchMeta 1" surfaces will be in it.
- added gametype-controlled structure fields for "-deluxe", "-subdivisions",
  "-nostyles", "-patchshadows". New "dq" (deluxequake), "prophecy" and
  "darkplaces" games uses them. Additionaly added "-nodeluxe",
  "-nopatchshadows", "-styles" to negate positive defaults.
- Floodligting code is changed to handle custom surfaces.
  And "q3map_floodlight 'r' 'g' 'b' 'diste' 'bright' 'low_quality' shader
  keyword was added. Per-surfaces floodlight code does not intersect with
  global floodlight done by -floodlight of _floodlight worldspawn key,
  their effects get summarized. This is good way to light up surfacelight
  surfaces, such as water
- added printing of game-specific options (default light gamma, exposure and
  such) to light phase
- "func_wall" entities now have _castShadows default to 1. This works only for
  "dq" and "prophecy" games
- added "-samplesize" switch to light phase, this scales samplesizes for all
  lightmaps, useful to compile map with different lightmap quality
- added "_ls" key which duplicates "_lightmapscale" but have short name
  (order of checking is lightmapscale-'_lightmapscale-'_ls)

  
1.0.0 (DeluxeQuake internal release)
------
- based on https://zerowing.idsoftware.com/svn/radiant/GtkRadiant/trunk
  rev 158 (GtkRadiant 1.5.0)
- applied FS3-FS20g patches by TwentySeven