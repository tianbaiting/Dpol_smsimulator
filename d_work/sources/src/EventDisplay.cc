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

EventDisplay::EventDisplay(const char* geom_file, PDCSimAna& ana)
    : m_pdc_ana(ana), m_currentEventElements(nullptr)
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

void EventDisplay::DisplayEvent(EventDataReader& reader) {
    ClearCurrentEvent();

    m_pdc_ana.ProcessEvent(reader.GetHits());

    m_currentEventElements = new TEveElementList(Form("Event_%lld", reader.GetCurrentEventNumber()));
    gEve->AddElement(m_currentEventElements);

    // Create and add propagated tracks (TrackPropagator holds propagator instance)
    // TEveElementList* propagated = m_trackPropagator.CreateTracks(reader.GetHits(), /*vertex*/ m_pdc_ana.GetRecoPoint1());
    // if (propagated) {
    //     m_currentEventElements->AddElement(propagated);
    // }

    // 1. Raw Hits
    TEvePointSet* raw_hits = new TEvePointSet("Raw Hits");
    raw_hits->SetMarkerColor(kRed);
    raw_hits->SetMarkerStyle(20);
    raw_hits->SetMarkerSize(1.0);
    TClonesArray* hits_array = reader.GetHits();
    for (int i = 0; i < hits_array->GetEntries(); ++i) {
        TSimData* hit = (TSimData*)hits_array->At(i);
        if (hit && hit->fDetectorName.Contains("PDC")) {
            raw_hits->SetNextPoint(hit->fPrePosition.X(), hit->fPrePosition.Y(), hit->fPrePosition.Z());
        }
    }
    m_currentEventElements->AddElement(raw_hits);

    // 2. Smeared Hits
    TEvePointSet* smeared_hits = new TEvePointSet("Smeared Hits");
    smeared_hits->SetMarkerColor(kBlue);
    smeared_hits->SetMarkerStyle(20);
    smeared_hits->SetMarkerSize(1.0);
    auto smeared_positions = m_pdc_ana.GetSmearedGlobalPositions();
    for (const auto& pos : smeared_positions) {
        smeared_hits->SetNextPoint(pos.X(), pos.Y(), pos.Z());
    }
    m_currentEventElements->AddElement(smeared_hits);

    // 3. Reconstructed Track
    TVector3 p1 = m_pdc_ana.GetRecoPoint1();
    TVector3 p2 = m_pdc_ana.GetRecoPoint2();
    if (p1.Mag() > 0 && p2.Mag() > 0) {
        TEvePointSet* reco_pts = new TEvePointSet("Reco Points");
        reco_pts->SetMarkerColor(kGreen+2);
        reco_pts->SetMarkerStyle(4);
        reco_pts->SetMarkerSize(2.5);
        reco_pts->SetNextPoint(p1.X(), p1.Y(), p1.Z());
        reco_pts->SetNextPoint(p2.X(), p2.Y(), p2.Z());
        m_currentEventElements->AddElement(reco_pts);

        TEveLine* track = new TEveLine("Track");
        track->SetPoint(0, p1.X(), p1.Y(), p1.Z());
        track->SetPoint(1, p2.X(), p2.Y(), p2.Z());
        track->SetLineColor(kGreen+2);
        track->SetLineWidth(3);
        m_currentEventElements->AddElement(track);
    }

    Redraw();
    std::cout << "Displayed Event: " << reader.GetCurrentEventNumber() << std::endl;
}

// Other control methods from header...
void EventDisplay::ResetView() {
    SetupCamera();
    Redraw();
}

// ... Implementations for SetComponentVisibility and PrintComponentPositions if needed ...

