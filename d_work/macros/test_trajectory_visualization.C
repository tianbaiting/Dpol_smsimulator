// 测试轨迹可视化，提供更好的视角和缩放
void test_trajectory_visualization() {
    // 直接测试轨迹可视化，不依赖事件显示
    gSystem->Load("sources/build/libPDCAnalysisTools.so");
    
    // 1. 设置磁场
    MagneticField* magField = new MagneticField();
    magField->LoadFromROOTFile("magnetic_field_test.root");
    magField->SetRotationAngle(30.0);
    
    // 2. 创建轨迹计算器
    ParticleTrajectory trajectory(magField);
    trajectory.SetStepSize(10.0);       // 10mm步长
    trajectory.SetMaxTime(50.0);        // 50ns
    trajectory.SetMaxDistance(4000.0);  // 4m
    
    // 3. 模拟质子
    TVector3 position(0, 0, 0);  // 原点
    TLorentzVector momentum(15.3452, -4.0905, 628.752, 1129.57);  // 实际数据
    double charge = 1.0;  // 质子
    double mass = 938.272;  // 质子质量
    
    cout << "=== 计算质子轨迹 ===" << endl;
    cout << "起始动量: (" << momentum.Px() << ", " << momentum.Py() << ", " << momentum.Pz() << ") MeV/c" << endl;
    cout << "动量大小: " << momentum.P() << " MeV/c" << endl;
    
    // 计算轨迹
    auto traj = trajectory.CalculateTrajectory(position, momentum, charge, mass);
    
    if (traj.size() < 2) {
        cout << "轨迹计算失败!" << endl;
        return;
    }
    
    cout << "轨迹点数: " << traj.size() << endl;
    cout << "轨迹时间: " << traj.back().time << " ns" << endl;
    
    // 分析轨迹偏转
    cout << "\n=== 轨迹分析 ===" << endl;
    
    // 计算几个关键点的位置和偏转
    vector<int> checkPoints = {0, static_cast<int>(traj.size()/4), static_cast<int>(traj.size()/2), 
                              static_cast<int>(3*traj.size()/4), static_cast<int>(traj.size()-1)};
    
    for (int i : checkPoints) {
        if (i >= static_cast<int>(traj.size())) continue;
        
        const auto& point = traj[i];
        double progress = (double)i / (traj.size()-1) * 100.0;
        
        cout << "点 " << i << " (" << progress << "%):" << endl;
        cout << "  位置: (" << point.position.X() << ", " << point.position.Y() 
             << ", " << point.position.Z() << ") mm" << endl;
        cout << "  时间: " << point.time << " ns" << endl;
        
        if (i > 0) {
            TVector3 displacement = point.position - traj[0].position;
            cout << "  相对起点偏移: (" << displacement.X() << ", " 
                 << displacement.Y() << ", " << displacement.Z() << ") mm" << endl;
            cout << "  横向偏转角: " << TMath::ATan2(displacement.X(), displacement.Z()) * 180.0/TMath::Pi() << " 度" << endl;
        }
        cout << endl;
    }
    
    // 计算偏转半径和回旋频率（在均匀磁场中的近似）
    TVector3 B_center = magField->GetField(0, 0, 1500);  // 磁铁中心的磁场
    double B_magnitude = B_center.Mag();
    
    cout << "=== 物理分析 ===" << endl;
    cout << "磁铁中心磁场强度: " << B_magnitude << " T" << endl;
    
    if (B_magnitude > 0) {
        // 计算回旋半径 r = p / (qB)
        double p_perp = sqrt(momentum.Px()*momentum.Px() + momentum.Py()*momentum.Py());  // 垂直分量
        double cyclotron_radius = p_perp * 1e-3 / (charge * B_magnitude * 2.998e8 * 1.602e-19) * 1000.0;  // 转换为mm
        
        cout << "垂直动量分量: " << p_perp << " MeV/c" << endl;
        cout << "理论回旋半径: " << cyclotron_radius << " mm" << endl;
        
        // 实际测量的偏转半径
        TVector3 final_pos = traj.back().position;
        double actual_deflection = final_pos.X();  // X方向的偏转
        double path_length = final_pos.Z();        // Z方向的路径长度
        
        if (path_length > 0) {
            double deflection_angle = TMath::ATan2(actual_deflection, path_length);
            cout << "实际偏转角: " << deflection_angle * 180.0/TMath::Pi() << " 度" << endl;
            cout << "偏转角比例: " << actual_deflection/path_length * 100.0 << " %" << endl;
        }
    }
    
    // 建议可视化参数
    cout << "\n=== 可视化建议 ===" << endl;
    cout << "由于Z方向位移(" << traj.back().position.Z() << " mm)远大于X方向偏转(" 
         << traj.back().position.X() << " mm)，" << endl;
    cout << "轨迹在EVE中看起来接近直线是正常的。" << endl;
    cout << "偏转角度: " << TMath::ATan2(traj.back().position.X(), traj.back().position.Z()) * 180.0/TMath::Pi() << " 度" << endl;
    cout << "这是一个很小但真实的偏转！" << endl;
    
    delete magField;
}