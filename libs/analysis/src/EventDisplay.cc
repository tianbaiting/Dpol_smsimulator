#include "EventDisplay.hh"
#include "TSimData.hh"
#include "TBeamSimData.hh"
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
    DrawCoordinateSystem();
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
        
        // 增强几何体的透明度以便更好地观察内部结构
        if (gGeoManager) {
            TObjArray* volumes = gGeoManager->GetListOfVolumes();
            if (volumes) {
                for (int i = 0; i < volumes->GetEntries(); i++) {
                    TGeoVolume* vol = (TGeoVolume*)volumes->At(i);
                    if (vol && vol->GetMedium()) {
                        // 设置透明度：80表示20%不透明度（80%透明）
                        vol->SetTransparency(80);
                        
                        // 对于特定体积名称，可以设置不同的透明度
                        TString volName = vol->GetName();
                        if (volName.Contains("yoke") || volName.Contains("Yoke")) {
                            vol->SetTransparency(60); // 磁轭更透明
                        } else if (volName.Contains("coil") || volName.Contains("Coil")) {
                            vol->SetTransparency(60); // 线圈稍微不透明一些
                        } else if (volName.Contains("PDC") || volName.Contains("pdc")) {
                            vol->SetTransparency(60); // PDC探测器保持一定可见性
                        }
                    }
                }
                std::cout << "EventDisplay: Applied transparency settings to " 
                         << volumes->GetEntries() << " volumes" << std::endl;
            }
        }
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

    // 1. Raw Hits - 每个hit使用单独的点集以便按能量调整大小/颜色
    for (size_t i = 0; i < event.rawHits.size(); ++i) {
        const auto& hit = event.rawHits[i];
        // marker size scale: base + log-energy mapping (protect against zero)
        double eng = hit.energy;
        double msize = 1.0 + (eng > 0 ? std::log10(1.0 + eng) : 0.0);
        TEvePointSet* hitMarker = new TEvePointSet(Form("RawHit_%zu", i));
        // Color map: low energy -> red, high energy -> orange
        Int_t color = (eng > 0) ? kRed + std::min(5, (int)std::floor(std::log10(1.0 + eng))) : kRed;
        hitMarker->SetMarkerColor(color);
        hitMarker->SetMarkerStyle(20);
        hitMarker->SetMarkerSize(msize);
        hitMarker->SetNextPoint(hit.position.X(), hit.position.Y(), hit.position.Z());
        m_currentEventElements->AddElement(hitMarker);
        // 同时在控制台打印能量（便于调试）
        std::cout << "RawHit " << i << ": pos=(" << hit.position.X() << ", " << hit.position.Y()
                  << ", " << hit.position.Z() << ") mm, E=" << eng << "" << std::endl;
    }

    // 2. Smeared Hits - 显示重建后的点，使用统一样式但更醒目
    TEvePointSet* smeared_hits = new TEvePointSet("Smeared Hits");
    smeared_hits->SetMarkerColor(kBlue);
    smeared_hits->SetMarkerStyle(4);
    smeared_hits->SetMarkerSize(2.0);
    for (const auto& hit : event.smearedHits) {
        smeared_hits->SetNextPoint(hit.position.X(), hit.position.Y(), hit.position.Z());
    }
    m_currentEventElements->AddElement(smeared_hits);

    // 3. Reconstructed Tracks
    for (size_t ti = 0; ti < event.tracks.size(); ++ti) {
        const auto& track = event.tracks[ti];
        TEvePointSet* reco_pts = new TEvePointSet(Form("RecoPoints_%zu", ti));
        reco_pts->SetMarkerColor(kGreen+2);
        reco_pts->SetMarkerStyle(4);
        reco_pts->SetMarkerSize(2.5);
        reco_pts->SetNextPoint(track.start.X(), track.start.Y(), track.start.Z());
        reco_pts->SetNextPoint(track.end.X(), track.end.Y(), track.end.Z());
        m_currentEventElements->AddElement(reco_pts);

        TEveLine* track_line = new TEveLine(Form("Track_%zu", ti));
        track_line->SetPoint(0, track.start.X(), track.start.Y(), track.start.Z());
        track_line->SetPoint(1, track.end.X(), track.end.Y(), track.end.Z());
        track_line->SetLineColor(kGreen+2);
        track_line->SetLineWidth(3);
        m_currentEventElements->AddElement(track_line);
    }
    
    // 4. Reconstructed Neutrons
    if (!event.neutrons.empty()) {
        TVector3 targetPos = m_geo.GetTargetPosition();
        DrawNeutrons(event.neutrons, targetPos);
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
    
    // 获取 Target 角度用于旋转入射粒子动量
    double targetAngleRad = m_geo.GetTargetAngleRad();
    TVector3 targetPos = m_geo.GetTargetPosition();
    
    std::cout << "Applying rotation of " << (targetAngleRad * TMath::RadToDeg()) 
              << " deg to incident particles" << std::endl;
    std::cout << "Using target position: (" << targetPos.X() << ", " 
              << targetPos.Y() << ", " << targetPos.Z() << ") mm" << std::endl;
    
    // 创建轨迹计算对象
    ParticleTrajectory trajectory(magField);
    trajectory.SetStepSize(10.0);      // 10 mm steps
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
        
        // 应用旋转到动量：RotateY(-targetAngle)
        TLorentzVector momentum = particle.fMomentum;
        TVector3 mom3 = momentum.Vect();
        mom3.RotateY(-targetAngleRad);  // 注意：负角度
        momentum.SetVect(mom3);
        
        // 使用 Target 位置作为起始位置
        TVector3 position = targetPos;



        // std::cout<<"zhengxiang chuanbo" << std::endl;
        
        // std::cout << "Particle " << i << " (" << particleName << "): "
        //           << "Original p=(" << particle.fMomentum.Px() << ", " 
        //           << particle.fMomentum.Py() << ", " << particle.fMomentum.Pz() << "), "
        //           << "Rotated p=(" << momentum.Px() << ", " 
        //           << momentum.Py() << ", " << momentum.Pz() << ") MeV/c" << std::endl;



        
        // 允许绘制中性粒子轨迹（ParticleTrajectory 对中性粒子将返回直线）
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
        int color;
        if (TMath::Abs(charge) < 1e-6) {
            color = kGray+1; // 中性粒子用灰色
        } else {
            color = (charge > 0) ? kRed : kBlue;
        }
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

// New methods for TargetReconstructor visualization
#include "TargetReconstructor.hh"  // Include here to access structs
#include "TEveLine.h"
#include "TColor.h"

void EventDisplay::DrawReconstructionResults(const TargetReconstructionResult& result, bool showTrials) {
    if (!m_currentEventElements) {
        std::cerr << "EventDisplay::DrawReconstructionResults: No current event elements!" << std::endl;
        return;
    }
    std::cout << "Drawing Target Reconstruction Results..." << std::endl;
    
    // Draw trial trajectories if requested
    if (showTrials) {
        for (size_t i = 0; i < result.trialTrajectories.size(); ++i) {
            TString trialName = Form("TrialTraj_%lu_p%.0f", i, result.trialMomenta[i]);
            int color = kBlue + i % 4; // Different shades of blue
            DrawTrajectory(result.trialTrajectories[i], trialName.Data(), color, 2); // dashed line
            
            std::cout << "Drew trial trajectory " << i << ": p=" << result.trialMomenta[i] 
                      << " MeV/c, dist=" << result.distances[i] << " mm" << std::endl;
        }
    }
    
    // Draw best trajectory
    if (!result.bestTrajectory.empty()) {
        DrawTrajectory(result.bestTrajectory, "BestBackpropTraj", kRed, 1); // solid red line
        
        std::cout << "Drew best backpropagation trajectory with " << result.bestTrajectory.size() 
                  << " points, p=" << result.bestMomentum.P() << " MeV/c" << std::endl;
    }
    
    // Redraw the display
    Redraw();
}

void EventDisplay::DrawTrajectory(const std::vector<TrajectoryPoint>& trajectory, 
                                 const char* name, int color, int style) {
    if (trajectory.empty() || !m_currentEventElements) {
        return;
    }
    
    // Create EVE line object
    TEveLine* trajLine = new TEveLine(name);
    trajLine->SetMainColor(color);
    trajLine->SetLineStyle(style);
    trajLine->SetLineWidth(2);
    
    // Add all trajectory points
    for (const auto& pt : trajectory) {
        trajLine->SetNextPoint(pt.position.X(), pt.position.Y(), pt.position.Z());
    }
    
    // Add to current event elements
    m_currentEventElements->AddElement(trajLine);
}

void EventDisplay::DrawCoordinateSystem() {
    if (!gEve) return;
    
    // 创建坐标系元素列表
    TEveElementList* coordSys = new TEveElementList("CoordinateSystem");
    
    // 坐标轴长度 (1000 mm = 1 m)
    double axisLength = 1000.0; // mm
    
    // X轴 (红色)
    TEveLine* xAxis = new TEveLine("X-Axis");
    xAxis->SetLineColor(kRed);
    xAxis->SetLineWidth(3);
    xAxis->SetPoint(0, 0, 0, 0);
    xAxis->SetPoint(1, axisLength, 0, 0);
    coordSys->AddElement(xAxis);
    
    // Y轴 (绿色)
    TEveLine* yAxis = new TEveLine("Y-Axis");
    yAxis->SetLineColor(kGreen);
    yAxis->SetLineWidth(3);
    yAxis->SetPoint(0, 0, 0, 0);
    yAxis->SetPoint(1, 0, axisLength, 0);
    coordSys->AddElement(yAxis);
    
    // Z轴 (蓝色)
    TEveLine* zAxis = new TEveLine("Z-Axis");
    zAxis->SetLineColor(kBlue);
    zAxis->SetLineWidth(3);
    zAxis->SetPoint(0, 0, 0, 0);
    zAxis->SetPoint(1, 0, 0, axisLength);
    coordSys->AddElement(zAxis);
    
    // 添加坐标轴标签点
    TEvePointSet* originPoint = new TEvePointSet("Origin");
    originPoint->SetMarkerColor(kBlack);
    originPoint->SetMarkerStyle(29); // Star
    originPoint->SetMarkerSize(3.0);
    originPoint->SetNextPoint(0, 0, 0);
    coordSys->AddElement(originPoint);
    
    // 添加轴端点标记
    TEvePointSet* xEnd = new TEvePointSet("X-End");
    xEnd->SetMarkerColor(kRed);
    xEnd->SetMarkerStyle(20);
    xEnd->SetMarkerSize(2.0);
    xEnd->SetNextPoint(axisLength, 0, 0);
    coordSys->AddElement(xEnd);
    
    TEvePointSet* yEnd = new TEvePointSet("Y-End");
    yEnd->SetMarkerColor(kGreen);
    yEnd->SetMarkerStyle(20);
    yEnd->SetMarkerSize(2.0);
    yEnd->SetNextPoint(0, axisLength, 0);
    coordSys->AddElement(yEnd);
    
    TEvePointSet* zEnd = new TEvePointSet("Z-End");
    zEnd->SetMarkerColor(kBlue);
    zEnd->SetMarkerStyle(20);
    zEnd->SetMarkerSize(2.0);
    zEnd->SetNextPoint(0, 0, axisLength);
    coordSys->AddElement(zEnd);
    
    // 添加长度标尺刻度 (每100mm一个刻度)
    TEveElementList* ruler = new TEveElementList("1000mm_Ruler");
    
    for (int i = 1; i <= 10; i++) {
        double pos = i * 100.0; // 每100mm一个刻度
        
        // X轴刻度
        TEveLine* xTick = new TEveLine(Form("X-Tick_%d00mm", i));
        xTick->SetLineColor(kRed);
        xTick->SetLineWidth(1);
        xTick->SetPoint(0, pos, -20, 0);
        xTick->SetPoint(1, pos, 20, 0);
        ruler->AddElement(xTick);
        
        // Y轴刻度
        TEveLine* yTick = new TEveLine(Form("Y-Tick_%d00mm", i));
        yTick->SetLineColor(kGreen);
        yTick->SetLineWidth(1);
        yTick->SetPoint(0, -20, pos, 0);
        yTick->SetPoint(1, 20, pos, 0);
        ruler->AddElement(yTick);
        
        // Z轴刻度
        TEveLine* zTick = new TEveLine(Form("Z-Tick_%d00mm", i));
        zTick->SetLineColor(kBlue);
        zTick->SetLineWidth(1);
        zTick->SetPoint(0, -20, 0, pos);
        zTick->SetPoint(1, 20, 0, pos);
        ruler->AddElement(zTick);
    }
    
    // 添加到EVE显示
    gEve->AddGlobalElement(coordSys);
    gEve->AddGlobalElement(ruler);
    
    std::cout << "EventDisplay: Added coordinate system with 1000mm axes and ruler marks" << std::endl;
}

void EventDisplay::DrawNeutrons(const std::vector<RecoNeutron>& neutrons, const TVector3& targetPos) {
    if (neutrons.empty()) return;
    
    // 创建中子显示元素组
    TEveElementList* neutronGroup = new TEveElementList("Neutrons");
    neutronGroup->SetMainColor(kOrange);
    
    for (size_t i = 0; i < neutrons.size(); ++i) {
        const RecoNeutron& neutron = neutrons[i];
        
        // 绘制中子飞行路径（直线）
        TEveLine* neutronPath = new TEveLine(Form("Neutron_%zu_Path", i));
        neutronPath->SetLineColor(kOrange);
        neutronPath->SetLineWidth(3);
        neutronPath->SetLineStyle(2); // 虚线，因为中子不带电
        
        // 从靶点到击中位置
        neutronPath->SetPoint(0, targetPos.X(), targetPos.Y(), targetPos.Z());
        neutronPath->SetPoint(1, neutron.position.X(), neutron.position.Y(), neutron.position.Z());
        neutronGroup->AddElement(neutronPath);
        
        // 在击中位置创建一个标记
        TEvePointSet* neutronHit = new TEvePointSet(Form("Neutron_%zu_Hit", i));
        neutronHit->SetMarkerColor(kOrange);
        neutronHit->SetMarkerStyle(20); // 实心圆
        neutronHit->SetMarkerSize(2.0);
        neutronHit->SetNextPoint(neutron.position.X(), neutron.position.Y(), neutron.position.Z());
        neutronGroup->AddElement(neutronHit);
        
        // 添加能量和速度信息标签
        TEveLine* infoLabel = new TEveLine(Form("Neutron_%zu_Info", i));
        infoLabel->SetTitle(Form("n: E=%.1f MeV, β=%.3f, ToF=%.1f ns", 
                                neutron.energy, neutron.beta, neutron.timeOfFlight));
        infoLabel->SetLineColor(kOrange);
        infoLabel->SetLineWidth(1);
        
        // 创建一个小的标记线来显示信息
        TVector3 labelPos = neutron.position + TVector3(50, 50, 0); // 偏移50mm
        infoLabel->SetPoint(0, neutron.position.X(), neutron.position.Y(), neutron.position.Z());
        infoLabel->SetPoint(1, labelPos.X(), labelPos.Y(), labelPos.Z());
        neutronGroup->AddElement(infoLabel);
        
        std::cout << "EventDisplay: Added neutron " << i << " -> "
                  << "pos=(" << neutron.position.X() << "," << neutron.position.Y() << "," << neutron.position.Z() << ") mm, "
                  << "E=" << neutron.energy << " MeV, β=" << neutron.beta << std::endl;
    }
    
    // 添加到当前事件
    if (m_currentEventElements) {
        m_currentEventElements->AddElement(neutronGroup);
    } else {
        gEve->AddElement(neutronGroup);
    }
    
    std::cout << "EventDisplay: Added " << neutrons.size() << " neutrons to display" << std::endl;
}

// ... Implementations for SetComponentVisibility and PrintComponentPositions if needed ...

