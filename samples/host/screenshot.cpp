// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "screenshot.h"

#if defined( _WIN32 )

#include <cstdio>
#include <cstring>
#include <vector>

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <wincodec.h>

// This sokol version only exposes the swap chain (the older per-view getters in
// the header doc block do not exist). The device, context and backbuffer are
// derived from it. Declared extern "C" so this helper does not pull in the
// whole windowed app header.
extern "C" const void* sapp_d3d11_get_swap_chain( void );

namespace
{

bool WriteBgraPng( const char* path, const uint8_t* bgra, UINT width, UINT height )
{
	HRESULT initHr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
	bool shouldUninit = SUCCEEDED( initHr ); // S_FALSE / RPC_E_CHANGED_MODE mean COM is already up

	bool ok = false;
	IWICImagingFactory* factory = nullptr;
	if ( SUCCEEDED( CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &factory ) ) ) )
	{
		wchar_t widePath[1024] = {};
		MultiByteToWideChar( CP_UTF8, 0, path, -1, widePath, 1024 );

		IWICStream* stream = nullptr;
		IWICBitmapEncoder* encoder = nullptr;
		IWICBitmapFrameEncode* frame = nullptr;
		IPropertyBag2* props = nullptr;
		if ( SUCCEEDED( factory->CreateStream( &stream ) ) &&
			 SUCCEEDED( stream->InitializeFromFilename( widePath, GENERIC_WRITE ) ) &&
			 SUCCEEDED( factory->CreateEncoder( GUID_ContainerFormatPng, nullptr, &encoder ) ) &&
			 SUCCEEDED( encoder->Initialize( stream, WICBitmapEncoderNoCache ) ) &&
			 SUCCEEDED( encoder->CreateNewFrame( &frame, &props ) ) && SUCCEEDED( frame->Initialize( props ) ) &&
			 SUCCEEDED( frame->SetSize( width, height ) ) )
		{
			WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
			frame->SetPixelFormat( &format );
			if ( SUCCEEDED( frame->WritePixels( height, width * 4, width * height * 4, const_cast<BYTE*>( bgra ) ) ) &&
				 SUCCEEDED( frame->Commit() ) && SUCCEEDED( encoder->Commit() ) )
			{
				ok = true;
			}
		}
		if ( props )
		{
			props->Release();
		}
		if ( frame )
		{
			frame->Release();
		}
		if ( encoder )
		{
			encoder->Release();
		}
		if ( stream )
		{
			stream->Release();
		}
		factory->Release();
	}

	if ( shouldUninit )
	{
		CoUninitialize();
	}
	return ok;
}

} // namespace

bool CaptureFrameToPng( const char* path )
{
	IDXGISwapChain* swapChain = (IDXGISwapChain*)sapp_d3d11_get_swap_chain();
	if ( swapChain == nullptr )
	{
		std::fprintf( stderr, "screenshot: no D3D11 swap chain\n" );
		return false;
	}

	ID3D11Texture2D* backbuffer = nullptr;
	swapChain->GetBuffer( 0, IID_PPV_ARGS( &backbuffer ) );
	if ( backbuffer == nullptr )
	{
		std::fprintf( stderr, "screenshot: could not get swap chain backbuffer\n" );
		return false;
	}

	ID3D11Device* device = nullptr;
	backbuffer->GetDevice( &device );
	ID3D11DeviceContext* context = nullptr;
	if ( device != nullptr )
	{
		device->GetImmediateContext( &context );
	}
	if ( device == nullptr || context == nullptr )
	{
		std::fprintf( stderr, "screenshot: no D3D11 device/context\n" );
		if ( device != nullptr )
		{
			device->Release();
		}
		backbuffer->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC desc = {};
	backbuffer->GetDesc( &desc );

	// The swapchain is single-sample (sokol desc.sample_count = 1), so the
	// backbuffer copies straight into a staging texture with no resolve.
	D3D11_TEXTURE2D_DESC stagingDesc = desc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	ID3D11Texture2D* staging = nullptr;
	bool ok = false;
	if ( SUCCEEDED( device->CreateTexture2D( &stagingDesc, nullptr, &staging ) ) )
	{
		context->CopyResource( staging, backbuffer );

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if ( SUCCEEDED( context->Map( staging, 0, D3D11_MAP_READ, 0, &mapped ) ) )
		{
			// Pack into a tight BGRA buffer; the staging row pitch is usually
			// padded past width*4.
			std::vector<uint8_t> tight( (size_t)desc.Width * desc.Height * 4 );
			const uint8_t* src = (const uint8_t*)mapped.pData;
			for ( UINT y = 0; y < desc.Height; ++y )
			{
				std::memcpy( &tight[(size_t)y * desc.Width * 4], src + (size_t)y * mapped.RowPitch, (size_t)desc.Width * 4 );
			}
			context->Unmap( staging, 0 );

			ok = WriteBgraPng( path, tight.data(), desc.Width, desc.Height );
		}
		staging->Release();
	}

	context->Release();
	device->Release();
	backbuffer->Release();
	if ( ok )
	{
		std::fprintf( stderr, "screenshot: wrote %s (%ux%u)\n", path, desc.Width, desc.Height );
	}
	else
	{
		std::fprintf( stderr, "screenshot: FAILED to write %s\n", path );
	}
	return ok;
}

#else // !_WIN32

#include <cstdio>

bool CaptureFrameToPng( const char* path )
{
	std::fprintf( stderr, "screenshot: only implemented on the Windows/D3D11 backend (%s)\n", path );
	return false;
}

#endif
