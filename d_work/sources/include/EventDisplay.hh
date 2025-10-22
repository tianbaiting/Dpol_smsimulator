#ifndef EVENT_DISPLAY_H
#define EVENT_DISPLAY_H

// 包含所有必要的 ROOT 头文件
#include "TEveManager.h"
#include "TGeoManager.h"
#include "TEveGeoNode.h"
#include "TGLViewer.h"
#include "TGeoVolume.h"
#include "TEveElement.h"
#include "TEvePointSet.h"
#include "TEveTrack.h"
#include "TEveTrackPropagator.h"
#include "TFile.h"
#include "TTree.h"
#include "TClonesArray.h"
#include "TString.h"
#include "TLorentzVector.h"
#include <string>
#include <iostream>
#include <vector>

#include "EventDataReader.hh"
#include "RecoEvent.hh"
#include "GeometryManager.hh"
#include "ParticleTrajectory.hh"
#include "MagneticField.hh"

// Forward declaration
class TBeamSimData;


// ===================================================================
//  Class: EventDisplay
//  职责: 管理EVE可视化环境、几何体和事件显示
// ===================================================================
class EventDisplay {
public:
    // 构造函数：初始化EVE环境并加载几何体
    EventDisplay(const char* geom_file, const GeometryManager& geo);
    // 析构函数
    ~EventDisplay();

    // 显示重建后的事件（新的主要接口）
    void DisplayEvent(const RecoEvent& event);
    
    // 为了向后兼容的接口（已不推荐使用）
    void DisplayEvent(EventDataReader& reader, const RecoEvent& event);

    // 新增：显示事件与粒子轨迹
    void DisplayEventWithTrajectories(EventDataReader& reader, const RecoEvent& event,
                                     MagneticField* magField);

    // 清除当前显示的事件
    void ClearCurrentEvent();

    // 视图和几何体控制
    void SetComponentVisibility(const std::string& volName, bool isVisible, int transparency = 80);
    void HideYoke() { SetComponentVisibility("yoke", false); }
    void ShowYoke() { SetComponentVisibility("yoke", true, 95); }
    void ResetView();
    void PrintComponentPositions();

    // 强制重绘
    void Redraw() { if(gEve) gEve->Redraw3D(kTRUE); }
    
private:
    // 初始化函数
    void InitEve();
    void LoadGeometry(const char* geom_file);
    void SetupCamera();
    
    // 轨迹相关私有方法
    void DrawParticleTrajectories(const std::vector<TBeamSimData>* beamData, MagneticField* magField);
    void DrawTrajectoryLine(const std::vector<double>& x, 
                           const std::vector<double>& y,
                           const std::vector<double>& z,
                           const char* particleName, 
                           int color = kRed);
    
    // 从PDG代码获取粒子信息
    void GetParticleInfo(int pdgCode, double& charge, double& mass, const char*& name);
    
    // 私有成员变量
    const GeometryManager& m_geo; // 几何管理器引用
    TEveElementList* m_currentEventElements; // 指向当前显示的事件元素列表
};

#endif // EVENT_DISPLAY_H