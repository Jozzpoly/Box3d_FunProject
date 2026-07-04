// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct JozzVehicleDecodedImage
{
	int width = 0;
	int height = 0;
	std::vector<uint8_t> rgba8;
	std::string status;
};

bool DecodeJozzVehiclePngRgba8( const uint8_t* data, size_t byteCount, JozzVehicleDecodedImage* out );
