#include "EventDisplay.hh"
#include "TSimData.hh"
#include "../../../sources/smg4lib/include/TBeamSimData.hh"
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

// 新增：显示事件与粒子轨迹
void EventDisplay::DisplayEventWithTrajectories(EventDataReader& reader, const RecoEvent& event,
                                               MagneticField* magField) {
    // 首先显示普通事件
    DisplayEvent(event);
    
    // 然后添加粒子轨迹
    if (magField) {
        const std::vector<TBeamSimData>* beamData = reader.GetBeamData();
        if (beamData && !beamData->empty()) {
            DrawParticleTrajectories(beamData, magField);
        } else {
            std::cout << "EventDisplay: No beam data available for trajectory calculation" << std::endl;
        }
    } else {
        std::cout << "EventDisplay: No magnetic field provided for trajectory calculation" << std::endl;
    }
    
    Redraw();
}

void EventDisplay::DrawParticleTrajectories(const std::vector<TBeamSimData>* beamData, MagneticField* magField) {
    if (!beamData || !magField) return;
    
    std::cout << "Drawing trajectories for " << beamData->size() << " particles" << std::endl;
    
    // 创建轨迹计算对象
    ParticleTrajectory trajectory(magField);
    trajectory.SetStepSize(5.0);      // 5 mm steps
    trajectory.SetMaxTime(50.0);      // 50 ns max time
    trajectory.SetMaxDistance(4000.0); // 4 m max distance
    trajectory.SetMinMomentum(10.0);   // 10 MeV/c min momentum
    
    for (size_t i = 0; i < beamData->size(); i++) {
        const TBeamSimData& particle = (*beamData)[i];
        
        // 从TBeamSimData对象中提取信息
        int pdgCode = particle.fPDGCode;
        double charge = particle.fCharge;
        double mass = particle.fMass;
        const char* particleName = particle.fParticleName.Data();
        TLorentzVector momentum = particle.fMomentum;
        TVector3 position = particle.fPosition;
        
        if (TMath::Abs(charge) < 1e-6) {
            std::cout << "Skipping neutral particle: " << particleName << std::endl;
            continue; // Skip neutral particles
        }
        
        // 计算轨迹
        std::vector<ParticleTrajectory::TrajectoryPoint> traj = 
            trajectory.CalculateTrajectory(position, momentum, charge, mass);
        
        if (traj.size() < 2) {
            std::cout << "Trajectory too short for particle " << particleName << std::endl;
            continue;
        }
        
        // 转换为绘图数据
        std::vector<double> x, y, z;
        trajectory.GetTrajectoryPoints(traj, x, y, z);
        
        // 绘制轨迹
        int color = (charge > 0) ? kRed : kBlue;
        DrawTrajectoryLine(x, y, z, particleName, color);
        
        // 打印轨迹信息
        trajectory.PrintTrajectoryInfo(traj);
    }
}

void EventDisplay::DrawTrajectoryLine(const std::vector<double>& x, 
                                    const std::vector<double>& y,
                                    const std::vector<double>& z,
                                    const char* particleName, 
                                    int color) {
    if (x.size() < 2) return;
    
    // 创建TEveLine对象来绘制轨迹
    TEveLine* trajLine = new TEveLine(Form("Trajectory_%s", particleName));
    trajLine->SetLineColor(color);
    trajLine->SetLineWidth(2);
    
    // 添加轨迹点
    for (size_t i = 0; i < x.size(); i++) {
        trajLine->SetPoint(i, x[i], y[i], z[i]);
    }
    
    // 添加到事件显示
    if (m_currentEventElements) {
        m_currentEventElements->AddElement(trajLine);
    }
    
    // 添加起始点标记
    TEvePointSet* startPoint = new TEvePointSet(Form("Start_%s", particleName));
    startPoint->SetMarkerColor(color);
    startPoint->SetMarkerStyle(29); // Star marker
    startPoint->SetMarkerSize(2.0);
    startPoint->SetNextPoint(x[0], y[0], z[0]);
    
    if (m_currentEventElements) {
        m_currentEventElements->AddElement(startPoint);
    }
    
    std::cout << "Drew trajectory for " << particleName << " with " 
              << x.size() << " points" << std::endl;
}

void EventDisplay::GetParticleInfo(int pdgCode, double& charge, double& mass, const char*& name) {
    // 根据PDG代码设置粒子信息
    switch (pdgCode) {
        case 2212:  // proton
            charge = 1.0;
            mass = 938.272; // MeV/c²
            name = "proton";
            break;
        case -2212: // antiproton
            charge = -1.0;
            mass = 938.272;
            name = "antiproton";
            break;
        case 211:   // pi+
            charge = 1.0;
            mass = 139.57;
            name = "pi+";
            break;
        case -211:  // pi-
            charge = -1.0;
            mass = 139.57;
            name = "pi-";
            break;
        case 11:    // electron
            charge = -1.0;
            mass = 0.511;
            name = "electron";
            break;
        case -11:   // positron
            charge = 1.0;
            mass = 0.511;
            name = "positron";
            break;
        case 13:    // muon-
            charge = -1.0;
            mass = 105.66;
            name = "muon-";
            break;
        case -13:   // muon+
            charge = 1.0;
            mass = 105.66;
            name = "muon+";
            break;
        case 2112:  // neutron
            charge = 0.0;
            mass = 939.565;
            name = "neutron";
            break;
        case 22:    // photon
            charge = 0.0;
            mass = 0.0;
            name = "photon";
            break;
        default:
            charge = 0.0;
            mass = 1.0;
            name = "unknown";
            std::cout << "Unknown PDG code: " << pdgCode << std::endl;
            break;
    }
}

// Other control methods from header...
void EventDisplay::ResetView() {
    SetupCamera();
    Redraw();
}

// ... Implementations for SetComponentVisibility and PrintComponentPositions if needed ...

