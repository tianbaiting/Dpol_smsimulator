#include "EventDisplay.hh"
#include "TSimData.hh"
#include "TROOT.h"
#include "TEnv.h"
#include "TGeoNode.h"
#include "TGeoVolume.h"
#include "TGeoManager.h"
#include "TGLViewer.h"
#include "TEveLine.h"

// ===================================================================
//  Implementation: EventDisplay
// ===================================================================

EventDisplay::EventDisplay(const char* geom_file, const GeometryManager& geo)
    : m_geo(geo), m_currentEventElements(nullptr) 
{
    InitEve();
    LoadGeometry(geom_file);
    SetupCamera();
}

EventDisplay::~EventDisplay() {
    ClearCurrentEvent();
}

void EventDisplay::InitEve() {
    if (gEve) return;
    TEveManager::Create();
    gEnv->SetValue("OpenGL.Highlight.Select", 0);
    gEnv->SetValue("OpenGL.EventHandler.EnableMouseOver", 0);
    std::cout << "EventDisplay: EVE manager initialized with stability settings." << std::endl;
}

void EventDisplay::LoadGeometry(const char* geom_file) {
    if (!gGeoManager) {
        // 按照 Geant4 的建议，使用 mm-based 单位（G4Units）导入几何
        // 首先允许设置默认单位，然后选择 G4 单位系统（mm-based）
        TGeoManager::LockDefaultUnits(kFALSE);
        TGeoManager::SetDefaultUnits(TGeoManager::EDefaultUnits::kG4Units);

        TGeoManager::Import(geom_file);
        std::cout << "EventDisplay: Geometry loaded from " << geom_file << std::endl;
    }
    if (gEve && gGeoManager) {
        gEve->AddGlobalElement(new TEveGeoTopNode(gGeoManager, gGeoManager->GetTopNode()));
        gEve->Redraw3D();
    }
}

void EventDisplay::SetupCamera() {
    if (!gEve) return;
    TGLViewer* viewer = gEve->GetDefaultGLViewer();
    if (viewer) {
        viewer->CurrentCamera().RotateRad(-1.2, 0.5);
        viewer->CurrentCamera().Dolly(5000, kFALSE, kFALSE);
    }
}

void EventDisplay::ClearCurrentEvent() {
    if (m_currentEventElements) {
        m_currentEventElements->Destroy(); // Destroys children and self
        m_currentEventElements = nullptr;
    }
}

// New primary interface: consume a ready RecoEvent and draw it
void EventDisplay::DisplayEvent(const RecoEvent& event) {
    ClearCurrentEvent();

    m_currentEventElements = new TEveElementList(Form("Event_%lld", event.eventID));
    gEve->AddElement(m_currentEventElements);

    // 1. Raw Hits
    TEvePointSet* raw_hits = new TEvePointSet("Raw Hits");
    raw_hits->SetMarkerColor(kRed);
    raw_hits->SetMarkerStyle(20);
    raw_hits->SetMarkerSize(1.0);
    for (const auto& hit : event.rawHits) {
        raw_hits->SetNextPoint(hit.position.X(), hit.position.Y(), hit.position.Z());
    }
    m_currentEventElements->AddElement(raw_hits);

    // 2. Smeared Hits
    TEvePointSet* smeared_hits = new TEvePointSet("Smeared Hits");
    smeared_hits->SetMarkerColor(kBlue);
    smeared_hits->SetMarkerStyle(20);
    smeared_hits->SetMarkerSize(1.0);
    for (const auto& hit : event.smearedHits) {
        smeared_hits->SetNextPoint(hit.position.X(), hit.position.Y(), hit.position.Z());
    }
    m_currentEventElements->AddElement(smeared_hits);

    // 3. Reconstructed Tracks
    for (const auto& track : event.tracks) {
        TEvePointSet* reco_pts = new TEvePointSet("Reco Points");
        reco_pts->SetMarkerColor(kGreen+2);
        reco_pts->SetMarkerStyle(4);
        reco_pts->SetMarkerSize(2.5);
        reco_pts->SetNextPoint(track.start.X(), track.start.Y(), track.start.Z());
        reco_pts->SetNextPoint(track.end.X(), track.end.Y(), track.end.Z());
        m_currentEventElements->AddElement(reco_pts);

        TEveLine* track_line = new TEveLine("Track");
        track_line->SetPoint(0, track.start.X(), track.start.Y(), track.start.Z());
        track_line->SetPoint(1, track.end.X(), track.end.Y(), track.end.Z());
        track_line->SetLineColor(kGreen+2);
        track_line->SetLineWidth(3);
        m_currentEventElements->AddElement(track_line);
    }

    Redraw();
    std::cout << "Displayed Event: " << event.eventID << std::endl;
}

// Backward-compatible helper: accept reader and event (reader unused here)
void EventDisplay::DisplayEvent(EventDataReader& /*reader*/, const RecoEvent& event) {
    DisplayEvent(event);
}

// Other control methods from header...
void EventDisplay::ResetView() {
    SetupCamera();
    Redraw();
}

// ... Implementations for SetComponentVisibility and PrintComponentPositions if needed ...

