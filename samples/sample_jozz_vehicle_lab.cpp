// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#include "imgui.h"
#include "sample.h"

#include "box3d/box3d.h"

class JozzVehicleLabM1 : public Sample
{
public:
	explicit JozzVehicleLabM1( SampleContext* context )
		: Sample( context )
	{
		if ( context->restart == false )
		{
			m_camera->SetView( 35.0f, 25.0f, 12.0f, { 0.0f, 1.5f, 0.0f } );
		}

		AddGroundBox( 20.0f );

		b3BodyDef bodyDef = b3DefaultBodyDef();
		bodyDef.type = b3_dynamicBody;
		bodyDef.position = { 0.0f, 4.0f, 0.0f };
		bodyDef.name = "jozz_m1_smoke_box";
		m_smokeBoxId = b3CreateBody( m_worldId, &bodyDef );

		b3ShapeDef shapeDef = b3DefaultShapeDef();
		shapeDef.density = 1.0f;
		shapeDef.baseMaterial.friction = 0.7f;
		shapeDef.baseMaterial.restitution = 0.1f;

		b3BoxHull box = b3MakeBoxHull( 0.5f, 0.5f, 0.5f );
		b3CreateHullShape( m_smokeBoxId, &shapeDef, &box.base );
	}

	bool DrawControls() override
	{
		ImGui::TextUnformatted( "Jozz Vehicle Lab M1" );
		ImGui::Separator();
		ImGui::BulletText( "Native Box3D sample host is alive." );
		ImGui::BulletText( "Camera, ImGui, debug draw, and physics are wired." );
		ImGui::BulletText( "This smoke test intentionally uses primitives only." );
		ImGui::Spacing();
		ImGui::TextWrapped( "Next target: replace this box-only scene with a primitive one-corner wheel-joint lab before touching glTF rendering." );
		return true;
	}

	void Render() override
	{
		Sample::Render();

		DrawTextLine( "Jozz Vehicle Lab M1 Smoke" );
		DrawTextLine( "Expected: one dynamic cube falls onto the ground." );
		DrawTextLine( "No glTF rendering yet; assets are validated offline through tools/asset_audit.py." );
	}

	static Sample* Create( SampleContext* context )
	{
		return new JozzVehicleLabM1( context );
	}

	b3BodyId m_smokeBoxId;
};

static int sampleJozzVehicleLabM1 = RegisterSample( "Jozz Vehicle", "Lab M1 Smoke", JozzVehicleLabM1::Create );
