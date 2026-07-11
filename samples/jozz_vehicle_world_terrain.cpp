// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_world_terrain.h"

#include "gfx/debug_adapter.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace JozzWorldLayout;

namespace
{

// --- Deterministic value noise (iq-style integer hash + quintic blend) -----
// Not b3CreateWave: that generator is a single sine product (one frequency,
// see PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md §3) and cannot produce "three
// different noise scales" layered together. This is a small from-scratch
// generator instead, seeded so the same seed always reproduces the same
// terrain (P7 - "Przebuduj teren" is a deliberate re-roll, not randomness).

uint32_t HashLattice( uint32_t seed, int32_t ix, int32_t iz )
{
	uint32_t h = seed;
	h ^= (uint32_t)ix * 0x27d4eb2du;
	h ^= (uint32_t)iz * 0x165667b1u;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;
	return h;
}

float HashToUnitFloat( uint32_t h )
{
	return (float)( h & 0x00FFFFFFu ) / (float)0x00FFFFFFu; // [0, 1]
}

float Quintic( float t )
{
	return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
}

float Lerp( float a, float b, float t )
{
	return a + ( b - a ) * t;
}

float Clamp01( float t )
{
	return t < 0.0f ? 0.0f : ( t > 1.0f ? 1.0f : t );
}

float Clamp( float t, float lo, float hi )
{
	return t < lo ? lo : ( t > hi ? hi : t );
}

// Smoothstep on an arbitrary [edge0, edge1] range.
float Smoothstep( float edge0, float edge1, float x )
{
	float t = Clamp01( ( x - edge0 ) / ( edge1 - edge0 ) );
	return t * t * ( 3.0f - 2.0f * t );
}

// Value noise in [0, 1], sampled at world-ish (x, z) with the given seed
// layer and wavelength (== lattice spacing in meters).
float ValueNoise2D( uint32_t seed, float x, float z, float wavelength )
{
	float gx = x / wavelength;
	float gz = z / wavelength;
	int32_t ix = (int32_t)std::floor( gx );
	int32_t iz = (int32_t)std::floor( gz );
	float fx = gx - (float)ix;
	float fz = gz - (float)iz;

	float ux = Quintic( fx );
	float uz = Quintic( fz );

	float v00 = HashToUnitFloat( HashLattice( seed, ix, iz ) );
	float v10 = HashToUnitFloat( HashLattice( seed, ix + 1, iz ) );
	float v01 = HashToUnitFloat( HashLattice( seed, ix, iz + 1 ) );
	float v11 = HashToUnitFloat( HashLattice( seed, ix + 1, iz + 1 ) );

	float vx0 = Lerp( v00, v10, ux );
	float vx1 = Lerp( v01, v11, ux );
	return Lerp( vx0, vx1, uz );
}

// Same as ValueNoise2D but re-mapped to [-1, 1], for the signed height layers.
float SignedNoise2D( uint32_t seed, float x, float z, float wavelength )
{
	return ValueNoise2D( seed, x, z, wavelength ) * 2.0f - 1.0f;
}

// Distinct seed offsets per layer so the octaves are decorrelated even
// though they share the same base seed.
constexpr uint32_t kMacroSeedOffset = 0x1u;
constexpr uint32_t kMesoSeedOffset = 0x2u;
constexpr uint32_t kMicroSeedOffset = 0x3u;
constexpr uint32_t kMacroOctave2SeedOffset = 0x4u;
constexpr uint32_t kWarpSeedOffsetX = 0x5u;
constexpr uint32_t kWarpSeedOffsetZ = 0x6u;
constexpr uint32_t kRoughnessSeedOffset = 0x7u;
// Central mountain layers (E1 final polish).
constexpr uint32_t kMountainCenterSeedX = 0x8u;
constexpr uint32_t kMountainCenterSeedZ = 0x9u;
constexpr uint32_t kMountainWarpSeedX = 0xAu;
constexpr uint32_t kMountainWarpSeedZ = 0xBu;
constexpr uint32_t kMountainSpurSeed = 0xCu;
constexpr uint32_t kMountainSurfaceSeed = 0xDu;

// Classic ridged-noise remap: 1-|noise| turns smooth symmetric bumps into a
// connected network of ridges (near the noise's zero crossings) separated by
// broad valley floors. Squaring sharpens the crests further. Result in [0,1].
float Ridged( float signedNoiseValue )
{
	float r = 1.0f - std::fabs( signedNoiseValue );
	return r * r;
}

// One deterministic [0,1] draw from a seed layer (no spatial component) - used
// to jitter the mountain center so "Przebuduj teren" re-rolls where it grows.
float SeedUnitFloat( uint32_t seed )
{
	return HashToUnitFloat( HashLattice( seed, 0, 0 ) );
}

// Ridged multifractal: several ridged octaves (each half the wavelength, half
// the weight of the last) summed and normalized to [0,1]. This is the "new
// realistic noise" for the mountain - large ridges + medium + fine crags in
// one field (Jozz: "wieksze i mniejsze nierownosci"). Build-time only.
float RidgedFbm( uint32_t seed, float x, float z, float baseWavelength, int octaves )
{
	float sum = 0.0f;
	float amp = 1.0f;
	float norm = 0.0f;
	float wavelength = baseWavelength;
	uint32_t layer = seed;
	for ( int i = 0; i < octaves; ++i )
	{
		sum += Ridged( SignedNoise2D( layer, x, z, wavelength ) ) * amp;
		norm += amp;
		amp *= 0.5f;
		wavelength *= 0.5f;
		layer += 0x100u * (uint32_t)( i + 1 ); // decorrelate octaves
	}
	return sum / norm;
}

// The central mountain contribution. Returns its added height in meters and
// (via outMass) the [0,1] radial mass used by the caller to suppress the base
// terrain underneath, so the mountain replaces the local rolling ground rather
// than stacking on top of a random bump. See world_layout.h for the mechanism
// list; all sampling here is terrain-build/teleport time only, never the step.
float ComputeMountain( uint32_t seed, float localX, float localZ, float* outMass )
{
	// (1) Seed-jittered center, kept near the middle of the chunk.
	float centerX = kMountainCenterLocal + ( SeedUnitFloat( seed + kMountainCenterSeedX ) * 2.0f - 1.0f ) * kMountainJitter;
	float centerZ = kMountainCenterLocal + ( SeedUnitFloat( seed + kMountainCenterSeedZ ) * 2.0f - 1.0f ) * kMountainJitter;

	// (3) Domain-warp the sample point so the footprint outline is not a circle.
	float warpX = SignedNoise2D( seed + kMountainWarpSeedX, localX, localZ, kMountainWarpWavelength ) * kMountainWarpStrength;
	float warpZ = SignedNoise2D( seed + kMountainWarpSeedZ, localX, localZ, kMountainWarpWavelength ) * kMountainWarpStrength;
	float dx = ( localX + warpX ) - centerX;
	float dz = ( localZ + warpZ ) - centerZ;
	float dist = std::sqrt( dx * dx + dz * dz );

	// (4) Angular spur modulation: vary the effective radius by compass angle so
	// ridges/gullies radiate from the summit (mountain, not a volcano cone).
	float angle = std::atan2( dz, dx );
	float spur = SignedNoise2D( seed + kMountainSpurSeed, std::cos( angle ) * kMountainSpurRingRadius,
								std::sin( angle ) * kMountainSpurRingRadius, kMountainSpurWavelength );
	float effectiveRadius = kMountainRadius * ( 1.0f + spur * kMountainSpurAmount );

	// (2) Smoothstep radial mass: 1 at the summit, easing to 0 at the foot (flat
	// tangents at both ends -> no needle tip, no hard crease into the terrain).
	float t = Clamp01( 1.0f - dist / effectiveRadius );
	float mass = t * t * ( 3.0f - 2.0f * t );
	*outMass = mass;
	if ( mass <= 0.0f )
	{
		return 0.0f;
	}

	// (5) Craggy summit relief from the dedicated ridged FBM, biased near its
	// mean so it both raises crests and carves gullies, and faded by the mass.
	float surface = ( RidgedFbm( seed + kMountainSurfaceSeed, localX, localZ, kMountainSurfaceWavelength,
								 kMountainSurfaceOctaves ) -
					  kMountainSurfaceBias ) *
					kMountainSurfaceAmp;

	return mass * kMountainPeakHeight + surface * mass;
}

// height(x, z) = zakladka(x) + trudnosc(x) * [ makro(warp, ridged x2) + mezo*roughness + mikro*roughness ]
// (x, z) here are LOCAL to the offroad chunk: x in [0, kOffroadSize], x=0 is
// the seam against the plate. Continuous in both x and z, so it can be
// sampled at arbitrary points (teleport height), not just grid vertices.
float ComputeOffroadHeightLocal( uint32_t seed, float localX, float localZ )
{
	float difficulty = Smoothstep( 0.0f, kDifficultyGradientDistance, localX );

	// Domain warp: displace the sample point fed into the macro layer so
	// ridgelines curve and branch instead of following the noise lattice's
	// axes (Jozz: "blizej prawdziwych gor"). Cost is two extra noise taps at
	// terrain-build time only - never touches the physics step.
	float warpX = SignedNoise2D( seed + kWarpSeedOffsetX, localX, localZ, kWarpWavelength ) * kWarpStrength;
	float warpZ = SignedNoise2D( seed + kWarpSeedOffsetZ, localX, localZ, kWarpWavelength ) * kWarpStrength;
	float warpedX = localX + warpX;
	float warpedZ = localZ + warpZ;

	// Ridged macro, 2 octaves. elevationShape in [0,1] is both "the mountain
	// shape" and (reused below) the "how exposed is this point" signal.
	float ridge1 = Ridged( SignedNoise2D( seed + kMacroSeedOffset, warpedX, warpedZ, kMacroWavelength ) );
	float ridge2 = Ridged( SignedNoise2D( seed + kMacroOctave2SeedOffset, warpedX, warpedZ,
										   kMacroWavelength * kMacroOctave2WavelengthScale ) );
	float elevationShape = Lerp( ridge1, ridge2, kMacroOctave2Weight );
	float macro = ( elevationShape * 2.0f - 1.0f ) * kMacroAmplitude;

	// Roughness: elevation-driven (higher == rockier) but gated by its own
	// noise layer so it is a trend, not a mechanical rule (Jozz: "z
	// odpowiednim szumem na wystepowanie tego szumu") - some low ground gets
	// exposed rock, some high ground stays smooth.
	float roughnessNoise = ValueNoise2D( seed + kRoughnessSeedOffset, localX, localZ, kRoughnessWavelength );
	float roughness = Clamp01( elevationShape * Lerp( kRoughnessNoiseLerpLow, kRoughnessNoiseLerpHigh, roughnessNoise ) );

	float meso = SignedNoise2D( seed + kMesoSeedOffset, localX, localZ, kMesoWavelength ) * kMesoAmplitude *
				 Lerp( kRoughnessMesoFloor, 1.0f, roughness );
	float micro = SignedNoise2D( seed + kMicroSeedOffset, localX, localZ, kMicroWavelength ) * kMicroAmplitude *
				  Lerp( kRoughnessMicroFloor, 1.0f, roughness );

	// Central mountain: its radial mass suppresses the base terrain underneath
	// (so the mountain REPLACES local rolling ground, not doubles on top of it)
	// and adds its own summit mass + craggy relief.
	float mountainMass = 0.0f;
	float mountain = ComputeMountain( seed, localX, localZ, &mountainMass );
	float baseScale = 1.0f - mountainMass * kMountainBaseSuppress;

	float noiseCombined = ( macro + meso + micro ) * baseScale + mountain;

	// Seam overlap strip: ramps from kSeamOverlapDepth (below the plate top)
	// up to ~0 over the first ~4 columns. Noise itself is already ~0 there
	// because difficulty is 0 at localX=0, so this ramp is what actually
	// carries the surface from "under the plate" to "into the terrain".
	float seamRamp = kSeamOverlapDepth * ( 1.0f - Smoothstep( 0.0f, kSeamOverlapRunLocalX, localX ) );

	float height = seamRamp + difficulty * noiseCombined;
	return Clamp( height, kOffroadGlobalMinHeight + 0.5f, kOffroadGlobalMaxHeight - 0.5f );
}

// Builds one static body carrying the 3x3 tile plate. Tiles share the same Y
// (top at kPlateTopY, half-height 1.0) and are offset only in X/Z, spaced so
// there is zero gap and zero overlap (kPlateTileSize * 3 == kPlateExtent
// exactly). Only the center tile becomes the debug-renderer's "ground" shape
// (R10); the rest get a matching neutral color so the seams don't read as a
// texture change.
void BuildPlate( b3WorldId worldId, uint64_t terrainCategoryBits, JozzVehicleWorldGround* ground )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.name = "world_plate";
	bodyDef.position = { 0.0f, kPlateBodyY, 0.0f };
	ground->plateBodyId = b3CreateBody( worldId, &bodyDef );

	const Vec4 neutralTileColor = MakeColor( b3_colorSlateGray );

	int tileIndex = 0;
	for ( int row = 0; row < kPlateTileCount; ++row )
	{
		for ( int col = 0; col < kPlateTileCount; ++col )
		{
			float offsetX = ( (float)col - 1.0f ) * kPlateTileSize;
			float offsetZ = ( (float)row - 1.0f ) * kPlateTileSize;

			b3ShapeDef shapeDef = b3DefaultShapeDef();
			shapeDef.filter.categoryBits = terrainCategoryBits;
			b3BoxHull hull = b3MakeOffsetBoxHull( kPlateTileHalf, 1.0f, kPlateTileHalf, { offsetX, 0.0f, offsetZ } );
			b3ShapeId shapeId = b3CreateHullShape( ground->plateBodyId, &shapeDef, &hull.base );
			ground->plateTileShapeIds[tileIndex] = shapeId;

			bool isCenterTile = ( row == 1 && col == 1 );
			if ( isCenterTile )
			{
				SetGroundShape( shapeId );
			}
			else
			{
				SetShapeMaterial( shapeId, neutralTileColor, 0.0f, 0.9f );
			}

			tileIndex += 1;
		}
	}
}

// Builds the offroad heightfield data + shape and attaches it to the given
// body (caller creates/keeps the body across regenerations).
void BuildOffroadShape( b3BodyId offroadBodyId, uint64_t terrainCategoryBits, uint32_t seed,
						 b3HeightFieldData** outField, b3ShapeId* outShapeId )
{
	std::vector<float> heights( (size_t)kOffroadGridPoints * (size_t)kOffroadGridPoints );
	float peakHeight = -1e9f;
	float peakLocalX = 0.0f;
	float peakLocalZ = 0.0f;
	for ( int row = 0; row < kOffroadGridPoints; ++row )
	{
		float localZ = (float)row * kOffroadCellSize;
		for ( int col = 0; col < kOffroadGridPoints; ++col )
		{
			float localX = (float)col * kOffroadCellSize;
			float h = ComputeOffroadHeightLocal( seed, localX, localZ );
			heights[(size_t)row * (size_t)kOffroadGridPoints + (size_t)col] = h;
			if ( h > peakHeight )
			{
				peakHeight = h;
				peakLocalX = localX;
				peakLocalZ = localZ;
			}
		}
	}

	// Headless framing aid: print the highest grid vertex (the mountain summit)
	// in world coordinates so a --screenshot run can aim the camera at it. Off
	// unless JOZZ_M6_TERRAIN_DUMP is set - see the env registry in the rig lab.
	if ( std::getenv( "JOZZ_M6_TERRAIN_DUMP" ) != nullptr )
	{
		std::printf( "[terrain] seed=%u summit world=(%.1f, %.1f) h=%.2f\n", seed,
					 kOffroadOriginX + peakLocalX, kOffroadOriginZ + peakLocalZ, peakHeight );
		std::fflush( stdout );
	}

	b3HeightFieldDef def = {};
	def.heights = heights.data();
	def.materialIndices = nullptr; // single baseMaterial for the whole chunk (per-cell materials: Etap 3)
	def.scale = { kOffroadCellSize, 1.0f, kOffroadCellSize };
	def.countX = kOffroadGridPoints;
	def.countZ = kOffroadGridPoints;
	def.globalMinimumHeight = kOffroadGlobalMinHeight;
	def.globalMaximumHeight = kOffroadGlobalMaxHeight;
	def.clockwiseWinding = false;

	*outField = b3CreateHeightField( &def );

	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.baseMaterial.friction = 0.85f;
	shapeDef.filter.categoryBits = terrainCategoryBits;
	*outShapeId = b3CreateHeightFieldShape( offroadBodyId, &shapeDef, *outField );
}

} // namespace

JozzVehicleWorldGround CreateJozzWorldGround( b3WorldId worldId, uint64_t terrainCategoryBits, uint32_t seed )
{
	JozzVehicleWorldGround ground;
	ground.seed = seed;

	BuildPlate( worldId, terrainCategoryBits, &ground );

	b3BodyDef offroadBodyDef = b3DefaultBodyDef();
	offroadBodyDef.name = "world_offroad";
	offroadBodyDef.position = { kOffroadOriginX, 0.0f, kOffroadOriginZ };
	ground.offroadBodyId = b3CreateBody( worldId, &offroadBodyDef );

	BuildOffroadShape( ground.offroadBodyId, terrainCategoryBits, seed, &ground.offroadField, &ground.offroadShapeId );

	return ground;
}

void DestroyJozzWorldGround( JozzVehicleWorldGround* ground )
{
	if ( ground->offroadField != nullptr )
	{
		b3DestroyHeightField( ground->offroadField );
		ground->offroadField = nullptr;
	}
}

void RegenerateJozzWorldOffroad( JozzVehicleWorldGround* ground, uint64_t terrainCategoryBits, uint32_t seed )
{
	b3DestroyShape( ground->offroadShapeId, false );
	b3DestroyHeightField( ground->offroadField );
	ground->offroadField = nullptr;

	ground->seed = seed;
	BuildOffroadShape( ground->offroadBodyId, terrainCategoryBits, seed, &ground->offroadField, &ground->offroadShapeId );
}

float SampleJozzWorldGroundHeight( const JozzVehicleWorldGround& ground, float worldX, float worldZ )
{
	if ( worldX < kOffroadOriginX )
	{
		return kPlateTopY;
	}

	float localX = worldX - kOffroadOriginX;
	float localZ = worldZ - kOffroadOriginZ;
	return ComputeOffroadHeightLocal( ground.seed, localX, localZ );
}
