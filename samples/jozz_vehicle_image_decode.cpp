// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_vehicle_image_decode.h"

#include <cstdio>
#include <limits>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#endif

namespace
{

#if defined( _WIN32 )
std::string HResultStatus( const char* step, HRESULT hr )
{
	char buffer[160];
	std::snprintf( buffer, sizeof( buffer ), "texture decode failed at %s (HRESULT 0x%08X)", step, (unsigned int)hr );
	return buffer;
}

template <typename T>
void SafeRelease( T*& ptr )
{
	if ( ptr )
	{
		ptr->Release();
		ptr = nullptr;
	}
}
#endif

} // namespace

bool DecodeJozzVehiclePngRgba8( const uint8_t* data, size_t byteCount, JozzVehicleDecodedImage* out )
{
	if ( out == nullptr )
	{
		return false;
	}

	*out = {};
	if ( data == nullptr || byteCount == 0 )
	{
		out->status = "texture decode failed: empty image data";
		return false;
	}

#if defined( _WIN32 )
	if ( byteCount > (size_t)std::numeric_limits<DWORD>::max() )
	{
		out->status = "texture decode failed: image data exceeds WIC memory stream limit";
		return false;
	}

	HRESULT hrInit = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
	const bool shouldUninitialize = SUCCEEDED( hrInit );
	if ( FAILED( hrInit ) && hrInit != RPC_E_CHANGED_MODE )
	{
		out->status = HResultStatus( "CoInitializeEx", hrInit );
		return false;
	}

	IWICImagingFactory* factory = nullptr;
	IWICStream* stream = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICFormatConverter* converter = nullptr;

	HRESULT hr = CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &factory ) );
	if ( SUCCEEDED( hr ) )
	{
		hr = factory->CreateStream( &stream );
	}
	if ( SUCCEEDED( hr ) )
	{
		hr = stream->InitializeFromMemory( const_cast<BYTE*>( reinterpret_cast<const BYTE*>( data ) ), (DWORD)byteCount );
	}
	if ( SUCCEEDED( hr ) )
	{
		hr = factory->CreateDecoderFromStream( stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder );
	}
	if ( SUCCEEDED( hr ) )
	{
		hr = decoder->GetFrame( 0, &frame );
	}

	UINT width = 0;
	UINT height = 0;
	if ( SUCCEEDED( hr ) )
	{
		hr = frame->GetSize( &width, &height );
	}
	if ( SUCCEEDED( hr ) )
	{
		hr = factory->CreateFormatConverter( &converter );
	}
	if ( SUCCEEDED( hr ) )
	{
		hr = converter->Initialize( frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
									WICBitmapPaletteTypeCustom );
	}

	if ( SUCCEEDED( hr ) && ( width == 0 || height == 0 || width > (UINT)std::numeric_limits<int>::max() ||
							 height > (UINT)std::numeric_limits<int>::max() ) )
	{
		hr = E_INVALIDARG;
	}

	if ( SUCCEEDED( hr ) )
	{
		const size_t stride = (size_t)width * 4u;
		const size_t size = stride * (size_t)height;
		if ( size > (size_t)std::numeric_limits<UINT>::max() )
		{
			hr = E_OUTOFMEMORY;
		}
		else
		{
			out->rgba8.resize( size );
			hr = converter->CopyPixels( nullptr, (UINT)stride, (UINT)size, out->rgba8.data() );
		}
	}

	SafeRelease( converter );
	SafeRelease( frame );
	SafeRelease( decoder );
	SafeRelease( stream );
	SafeRelease( factory );

	if ( shouldUninitialize )
	{
		CoUninitialize();
	}

	if ( FAILED( hr ) )
	{
		*out = {};
		out->status = HResultStatus( "WIC PNG decode", hr );
		return false;
	}

	out->width = (int)width;
	out->height = (int)height;
	out->status = "texture decode: ok";
	return true;
#else
	( void )data;
	( void )byteCount;
	out->status = "texture decode unsupported on this platform";
	return false;
#endif
}
