// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "jozz_scan_preview_lab.h"

#include "gfx/draw.h"
#include "imgui.h"
#include "jozz_scan_preview_pack.h"
#include "sample.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace
{

class JozzScanSourcePreviewLab final : public Sample
{
public:
	explicit JozzScanSourcePreviewLab( SampleContext* context )
		: Sample( context )
	{
		if ( context->restart == false )
		{
			m_camera->SetView( 38.0f, 28.0f, 40.0f, b3Pos_zero );
		}

		std::filesystem::path active = FindJozzActiveScanPreviewPack();
		if ( active.empty() )
		{
			m_pack.status =
				"preview pack: none selected; build exactly one pack or set JOZZ_SCAN_PREVIEW_PACK";
		}
		else if ( m_pack.Load( active ) )
		{
			FramePreview();
		}
	}

	~JozzScanSourcePreviewLab() override
	{
		m_pack.Destroy();
	}

	bool HasSolverControls() const override
	{
		return false;
	}

	bool CondenseDebugOverlay() const override
	{
		return true;
	}

	bool FocusBounds( b3AABB* bounds ) override
	{
		if ( m_pack.loaded == false )
		{
			return false;
		}
		*bounds = m_pack.bounds;
		return true;
	}

	void FocusHome() override
	{
		FramePreview();
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "P2A Source Visual Preview" );
		ImGui::Separator();
		ImGui::TextColored( ImVec4( 1.0f, 0.62f, 0.18f, 1.0f ), "SOURCE EVIDENCE ONLY" );
		ImGui::BulletText( "No collision shapes." );
		ImGui::BulletText( "Not an accepted world patch." );
		ImGui::BulletText( "GLB/PLY correspondence is not asserted." );
		ImGui::BulletText( "Textures are intentionally absent in this proof." );
		ImGui::Spacing();

		ImGui::TextWrapped( "%s", m_pack.status.c_str() );
		if ( m_pack.sourcePath.empty() == false )
		{
			std::string packLabel = m_pack.sourcePath.filename().string();
			ImGui::TextWrapped( "Pack: %s", packLabel.c_str() );
		}
		if ( m_pack.loaded )
		{
			ImGui::Text( "Tiles: %d", (int)m_pack.tiles.size() );
			ImGui::Text( "Vertices: %d", m_pack.vertexCount );
			ImGui::Text( "Triangles: %d", m_pack.triangleCount );
			ImGui::Checkbox( "Show geometry", &m_showGeometry );
			ImGui::Checkbox( "Show tile bounds", &m_showBounds );
			ImGui::Checkbox( "Show metre grid", &m_showGrid );
			ImGui::Checkbox( "Show lab axes", &m_showAxes );
			if ( ImGui::Button( "Frame whole preview" ) )
			{
				FramePreview();
			}

			if ( ImGui::CollapsingHeader( "Tiles" ) )
			{
				for ( JozzScanPreviewTile& tile : m_pack.tiles )
				{
					ImGui::PushID( tile.tileId );
					char label[64];
					std::snprintf( label, sizeof( label ), "Tile %d", tile.tileId );
					ImGui::Checkbox( label, &tile.visible );
					ImGui::SameLine();
					ImGui::TextDisabled( "%d tris", tile.triangleCount );
					ImGui::PopID();
				}
			}
		}
		else
		{
			ImGui::Spacing();
			ImGui::TextWrapped(
				"Expected default location: build/scan_pipeline/previews/<content-addressed-pack>. "
				"If more than one pack exists, set JOZZ_SCAN_PREVIEW_PACK before launching samples.exe." );
		}

		return true;
	}

	void Render() override
	{
		Sample::Render();

		if ( m_pack.loaded )
		{
			if ( m_showGrid )
			{
				b3Vec3 gridCenter = {
					0.5f * ( m_pack.bounds.lowerBound.x + m_pack.bounds.upperBound.x ),
					0.0f,
					0.5f * ( m_pack.bounds.lowerBound.z + m_pack.bounds.upperBound.z ),
				};
				float extentX = m_pack.bounds.upperBound.x - m_pack.bounds.lowerBound.x;
				float extentZ = m_pack.bounds.upperBound.z - m_pack.bounds.lowerBound.z;
				float halfExtent = 0.6f * std::max( extentX, extentZ );
				halfExtent = std::max( halfExtent, 10.0f );
				int divisions = std::clamp( (int)std::ceil( 2.0f * halfExtent ), 10, 200 );
				DrawGrid( b3ToPos( gridCenter ), b3Vec3_axisY, halfExtent, divisions, { 0.32f, 0.34f, 0.38f, 0.55f } );
			}
			if ( m_showAxes )
			{
				DrawAxes( b3WorldTransform_identity, 5.0f );
			}
			if ( m_showGeometry )
			{
				m_pack.Draw( m_showBounds );
			}
		}

		DrawTextLine( "P2A SOURCE VISUAL PREVIEW ONLY" );
		DrawTextLine( "No collision | Not accepted world | Textures off" );
	}

private:
	void FramePreview()
	{
		if ( m_pack.loaded == false )
		{
			return;
		}

		float aspect =
			m_context->windowHeight > 0
				? (float)m_context->windowWidth / (float)m_context->windowHeight
				: 16.0f / 9.0f;
		m_camera->Frame( m_pack.bounds, aspect, 1.25f );
	}

	JozzScanPreviewPack m_pack;
	bool m_showGeometry = true;
	bool m_showBounds = true;
	bool m_showGrid = true;
	bool m_showAxes = true;
};

} // namespace

Sample* CreateJozzScanSourcePreviewLab( SampleContext* context )
{
	return new JozzScanSourcePreviewLab( context );
}
