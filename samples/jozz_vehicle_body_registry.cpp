// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_body_registry.h"

#include <cstring>

namespace
{

static const JozzVehicleBodyModelDef s_bodyModels[] = {
	{ "brak", "Brak (sama bryła fizyczna)", nullptr, 0.0f, { 0.0f, 0.0f, 0.0f } },
	// Base pose solved from measured geometry, not eyeballed (commit a275947):
	// yaw -90 maps the model's rear (+Z) onto the car's rear (-X); anchoring at
	// chassis-local (0, -0.60, 0) centres the frame on wheelbase/track and
	// drops its floor to axle height.
	{ "rama_rurowa", "Rama rurowa Jozza (Nadwozie)", "assets/source/Nadwozie.gltf", -90.0f, { 0.0f, -0.60f, 0.0f } },
};

} // namespace

const JozzVehicleBodyModelDef* GetJozzVehicleBodyModels( int* outCount )
{
	*outCount = (int)( sizeof( s_bodyModels ) / sizeof( s_bodyModels[0] ) );
	return s_bodyModels;
}

const JozzVehicleBodyModelDef* FindJozzVehicleBodyModelByKey( const char* key )
{
	int count = (int)( sizeof( s_bodyModels ) / sizeof( s_bodyModels[0] ) );
	for ( int i = 0; i < count; ++i )
	{
		if ( std::strcmp( s_bodyModels[i].key, key ) == 0 )
		{
			return &s_bodyModels[i];
		}
	}
	return nullptr;
}
