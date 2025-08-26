// 自动生成的 Geant4 PDC 漂移室阳极丝阵列代码示例
// 假设已解析 XML 文件，获得 wirepos, wirez, wireid 等信息
// 可直接嵌入你的 DetectorConstruction 类

#include <vector>
#include <G4Tubs.hh>
#include <G4LogicalVolume.hh>
#include <G4PVPlacement.hh>
#include <G4Material.hh>
#include <G4SystemOfUnits.hh>

struct WireInfo {
    int wireid;
    double wirepos; // X 方向，单位 mm
    double wirez;   // Z 方向，单位 mm
};

// 示例：从 XML 解析得到的丝参数
std::vector<WireInfo> wires = {
    {0, -822, 40}, {1, -810, 40}, {2, -798, 40}, /* ...后续省略... */
};

void ConstructPDCWires(G4LogicalVolume* motherLV, G4Material* wireMat) {
    double wireRadius = 0.025 * mm; // 50um 直径
    double wireLength = 400 * mm;   // 丝长
    double wireY = 0 * mm;          // Y 方向中心
    for (const auto& wire : wires) {
        G4Tubs* wireSolid = new G4Tubs("AnodeWire", 0, wireRadius, wireLength/2, 0, 360*deg);
        G4LogicalVolume* wireLV = new G4LogicalVolume(wireSolid, wireMat, "AnodeWireLV");
        // 设置为 sensitive detector 可在后续添加
        new G4PVPlacement(0, G4ThreeVector(wire.wirepos*mm, wireY, wire.wirez*mm),
                          wireLV, "AnodeWire", motherLV, false, wire.wireid);
    }
}

// 在 DetectorConstruction::Construct() 中调用
// ConstructPDCWires(gasLogicalVolume, wireMaterial);

// 你只需将 wires 数组替换为完整 XML 解析结果即可。
