// SPDX-FileCopyrightText: 2026 Jozz Vehicle contributors
// SPDX-License-Identifier: MIT

#pragma once

#include "jozz_vehicle_m6_rig_lab.h"

#include "gfx/debug_adapter.h"
#include "gfx/draw.h"
#include "gfx/keycodes.h"
#include "imgui.h"
#include "implot.h"
#include "jozz_vehicle_asset_contract.h"
#include "jozz_vehicle_asset_dimensions.h"
#include "jozz_vehicle_asset_metadata.h"
#include "jozz_vehicle_asset_paths.h"
#include "jozz_vehicle_m5_test_course.h"
#include "jozz_vehicle_m6_config_io.h"
#include "jozz_vehicle_m6_suspension_rig.h"
#include "jozz_vehicle_m7_suspension_import.h"
#include "jozz_vehicle_steering_suspension_contract.h"
#include "jozz_vehicle_visual_mesh.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "box3d/box3d.h"

#include <cmath>

// M6/M7 Suspension Rig Lab: the multi-body suspension vehicle on the M7 real-
// forces foundation. The physics rig lives in jozz_vehicle_m6_suspension_rig
// (shared with the headless validation smoke); this sample adds input, camera,
// the M5 test course, live tuning behind a tabbed panel, hardpoint/link debug
// drawing, the visual-only glTF wheels, and Jozz's One_Sided_wheel_mount model
// riding the LIVE trailing-arm bodies. The M5 First Drivable sample stays
// untouched as the strut baseline.
//
// Session/preset files (JozzVehicleM6Config serialized via
// jozz_vehicle_m6_config_io): kSessionFilePath is an auto-save the destructor
// writes and the constructor restores, so restarting the sample (or the whole
// app) resumes the last tuning. kPresetDirectory holds named, user-saved
// presets (assets/vehicle_presets/*.json, three ship built-in) for switching
// between whole setups - drift/offroad/street - in two clicks instead of
// re-dragging every slider. build/ is gitignored so the session file never
// shows up as repo noise; the preset directory is committed on purpose.
static const char* kSessionFilePath = "build/jozz_vehicle_m6_session.json";
static const char* kPresetDirectory = "assets/vehicle_presets";

// Debug/view toggles (Debug tab checkboxes) are deliberately NOT part of
// JozzVehicleM6Config: they are not vehicle tuning, so they must not leak
// into named presets or get wiped by "Przywroc wszystkie ustawienia
// domyslne". But they were also not being restored across the engine's
// global "R" restart, which destroys and reconstructs this whole sample -
// the constructor hardcoded them back to fixed defaults every time. That
// meant every "R" silently re-enabled the rig diagnostic lines even after
// Jozz had turned them off. Small standalone key=value file, same
// build/-is-gitignored auto-save idea as kSessionFilePath, just a separate
// file so it stays out of the config JSON format entirely.
static const char* kDebugSessionFilePath = "build/jozz_vehicle_m6_debug_session.txt";

class JozzVehicleM6RigLab : public Sample
{
public:
	explicit JozzVehicleM6RigLab( SampleContext* context );
	~JozzVehicleM6RigLab() override;
	float InfoPanelWidthEm() const override;
	bool CondenseDebugOverlay() const override;
	void SaveDebugViewState();
	void LoadDebugViewState();
	void SyncEditFromConfig();
	void ApplyPendingStructuralSetup();
	void RecomputeRackTravel();
	void RefreshPresetList();
	void LoadPresetByName( const std::string& name );
	void SaveCurrentAsPreset( const std::string& name );
	void LoadWheelVisual();
	void LoadMountVisual();
	// Rigid chassis-frame skin (Nadwozie.gltf) fixed in the chassis body's local
	// frame; drawn only when m_showBodyVisual is on. See docs/PLAN_EDYTOR_RIGU_*.
	void LoadBodyVisual();
	void DrawBodyVisual();
	static bool CornerIsLeft( int corner );
	static void ArmEnds( const JozzVehicleRiggedPart& part, bool wheelNegX, b3Vec3& chassisEnd, b3Vec3& wheelEnd );
	void SetupMountRig();
	// New one-sided steering-suspension rig (front only), G1 of the rig-editor
	// warm-up. Loads/bakes/draws OneSided_Steering_Suspension_Rig on the LIVE
	// front-corner bodies; behind m_useSteeringRig so the default render path
	// (old mount on all four) is untouched. See docs/PLAN_EDYTOR_RIGU_*.
	void LoadSteeringRig();
	void SetupSteeringRig();
	void DrawSteeringRig();
	float GetSpawnHeight() const;
	void CreateVehicle();
	void ResetWorld();
	void DestroyVehicle();
	void UpdateWheelShapeVisibility();
	void ApplySuspensionTuning();
	void ApplySteeringTuning();
	void ApplyWheelFriction();
	void ApplyContactTuning();
	void Keyboard( int key, int action, int modifiers ) override;
	static void HelpMarker( const char* text );
	static void SectionHeader( const char* title );
	void DrawDriveTab();
	void DrawSteeringTab();
	void DrawSuspensionTab();
	void DrawChassisTab();
	void DrawWorldTab();
	void DrawDebugTab();
	bool DrawControls() override;
	void SampleTelemetry();
	void Step() override;
	void DrawRigDiagnostics();
	void DumpCornerGeometry();
	void Render() override;
	static Sample* Create( SampleContext* context );

	b3BodyId m_groundId;
	JozzVehicleM5TestCourse m_testCourse;
	JozzVehicleM6 m_vehicle;
	JozzVehicleM6Config m_config;
	JozzVehicleM6Config m_factoryConfig; // constructor-composed baseline; see the stash site
	JozzVehicleAuditMetadata m_assetMetadata;
	JozzVehicleVisualMesh m_wheelVisual;
	b3Transform m_wheelVisualCorrection;
	JozzVehicleAssetContract m_mountContract;
	JozzVehicleRiggedMesh m_riggedMountL; // authored (left corners)
	JozzVehicleRiggedMesh m_riggedMountR; // mirrored copy (right corners)
	JozzVehicleRiggedMesh m_dumper;		  // telescoping shock, per corner
	bool m_showDumper = true;

	// New steering-suspension rig (front-only, rig-editor warm-up G1). Loaded
	// always; drawn only when m_useSteeringRig is on, replacing the old mount on
	// the FRONT corners while the rear keeps the old mount (D1a). Visual-only:
	// rides the same live bodies the physics already drives.
	JozzVehicleAssetContract m_steeringContract;
	JozzVehicleSteeringSuspensionSockets m_steeringSockets;
	JozzVehicleRiggedMesh m_riggedSteeringL; // authored (left corners)
	JozzVehicleRiggedMesh m_riggedSteeringR; // mirrored copy (right corners)
	bool m_useSteeringRig = false;			 // JOZZ_M6_STEERING_RIG + Debug checkbox

	// Rigid chassis-frame skin (Nadwozie.gltf), fixed in the chassis body's local
	// frame (m_bodyChassisLocal is constant - the frame does not articulate).
	// Drawn only when m_showBodyVisual is on (JOZZ_M6_BODY + Debug checkbox);
	// default off so the suspension stays visible in this rig lab.
	JozzVehicleVisualMesh m_bodyVisual;
	b3WorldTransform m_bodyChassisLocal;
	bool m_showBodyVisual = false;

	bool m_armTint = false; // debug: tint wishbone arms to compare with debug lines
	bool m_dumpGeometry = false;
	int m_dumpFrame = 0;
	int m_forceTabIndex = -1; // JOZZ_M6_TAB: force a tab open once, for screenshots

	std::vector<std::string> m_availablePresets;
	int m_selectedPresetIndex = -1;
	char m_presetNameBuffer[64] = "";
	std::string m_presetStatus;
	std::string m_steeringClampStatus;
	b3Vec3 m_mountWheelCenterAuthored = { -0.416f, 0.175f, 0.0f };
	b3Vec3 m_damperUpperLAuthored = { 0.0164f, 0.6453f, 0.2844f };
	b3Vec3 m_damperUpperRAuthored = { 0.0164f, 0.6453f, -0.2844f };
	b3Vec3 m_damperLowerLAuthored = { -0.2516f, 0.0109f, 0.2844f };
	b3Vec3 m_damperLowerRAuthored = { -0.2516f, 0.0109f, -0.2844f };
	b3Transform m_bracketLocal[JOZZ_M6_CORNER_COUNT];
	b3Transform m_hubLocal[JOZZ_M6_CORNER_COUNT];
	bool m_cornerHasMount[JOZZ_M6_CORNER_COUNT] = {};

	// Steering-rig bakes: three body-local frames of the SAME model placement,
	// one per parent, so parts split by kinematic role (O1 in the audit doc):
	// chassis = fixed brackets; arm (lowerArm = "carrier": travels, does NOT
	// steer) = ChassisMount_b + wishbone wheel-ends; knuckle (travels AND steers)
	// = WheelCenter. Front corners only; m_steeringCornerActive gates the rest.
	b3Transform m_steeringChassisLocal[JOZZ_M6_CORNER_COUNT];
	b3Transform m_steeringArmLocal[JOZZ_M6_CORNER_COUNT];
	b3Transform m_steeringKnuckleLocal[JOZZ_M6_CORNER_COUNT];
	bool m_steeringCornerActive[JOZZ_M6_CORNER_COUNT] = {};
	JozzVehicleM7TrailingArmImport m_trailingImport;
	JozzVehicleM6DriveInput m_lastInput = {};
	float m_metersPerBlockbenchUnit;
	bool m_savedEnableContinuous;
	bool m_showWheelVisuals;
	bool m_showMountVisuals;
	bool m_showPrimitiveWheelShapes;
	bool m_showRigDiagnostics;
	bool m_invertSteering;

	float m_contactHertz;
	float m_contactDampingRatio;
	float m_contactSpeed;

	// Pending structural edits; require "Apply rig rebuild".
	int m_editFrontRigType;
	int m_editRearRigType;
	JozzVehicleM6WishboneGeometry m_editWishbone;
	JozzVehicleM6TrailingArmGeometry m_editTrailingArm;
	float m_editKnuckleMass;
	float m_editArmMass;
	int m_editEnvelopeMode;
	int m_editEnvelopeLayers;
	float m_editStrutCasterDeg;
	float m_editMaxSteeringAngleDegrees;
	float m_editFrontToeDeg;
	float m_editRearToeDeg;
	b3Vec3 m_editChassisHalfExtents;
	float m_editChassisDensity;
	float m_editCgVerticalOffset;
	float m_editAxleHalfSpacing;
	float m_editTrackHalfWidth;
	float m_editRestDrop;
	float m_editWheelDensity;
	bool m_structuralSetupDirty;

	static constexpr int TELEMETRY_CAPACITY = 600;
	float m_telemetryTime[TELEMETRY_CAPACITY];
	float m_telemetryTravel[JOZZ_M6_CORNER_COUNT][TELEMETRY_CAPACITY];
	float m_telemetrySlip[JOZZ_M6_CORNER_COUNT][TELEMETRY_CAPACITY];
	int m_telemetryHead;
	int m_telemetryCount;
	float m_telemetryClock;

};
