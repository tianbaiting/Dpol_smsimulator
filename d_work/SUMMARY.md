# Dpol_smsimulator 模拟分析库

## 1. 概述

这是一个用于处理、分析和可视化粒子物理模拟数据的C++库。该库基于ROOT框架，旨在从模拟输出（可能是Geant4）中重建粒子径迹，并在磁场中进行动量分析。

其核心工作流程包括：
1.  **读取** 原始模拟数据。
2.  **重建** 探测器（如PDC）的击中点。
3.  利用磁场和重建的径迹 **反向传播**，以确定粒子在靶点的动量。
4.  **可视化** 几何、原始数据和重建结果。

## 2. 库组件分析

该库由几个关键的类组成，每个类都有明确的职责。

### 2.1. 数据读取与管理

-   **`EventDataReader`**:
    -   **职责**: 从ROOT文件中读取模拟事件数据。
    -   **功能**:
        -   打开包含`TTree`的`.root`文件。
        -   提供逐个事件的访问接口 (`NextEvent`, `GoToEvent`)。
        -   提供对原始模拟数据（如`TClonesArray`格式的击中信息和`TBeamSimData`格式的束流粒子信息）的访问。

-   **`GeometryManager`**:
    -   **职责**: 加载和管理实验几何布局。
    -   **功能**:
        -   从文本文件（如Geant4宏文件）中解析几何参数。
        -   提供探测器位置（如 `PDC1_Position`, `PDC2_Position`）和角度等关键几何信息。

### 2.2. 事件重建

-   **`PDCSimAna`** (PDC Simulation Analysis):
    -   **职责**: 重建来自漂移室（PDC）的径迹点。
    -   **功能**:
        -   接收原始的模拟击中数据 (`TClonesArray`)。
        -   根据探测器的几何信息，将分散的击中重建为两个空间点（`RecoPoint1`, `RecoPoint2`），代表粒子穿过两个PDC的位置。
        -   可以将位置分辨率（smearing）应用于重建过程。
        -   输出一个包含重建信息的 `RecoEvent` 对象。

-   **`RecoEvent`**:
    -   **职责**: 存储单个事件的完整重建信息。
    -   **功能**: 这是一个数据容器，包含：
        -   `rawHits`: 原始击中点。
        -   `smearedHits`: 考虑了探测器分辨率的击中点。
        -   `tracks`: 重建后的径迹（`RecoTrack`），通常包含由`PDCSimAna`确定的起点和终点。

### 2.3. 物理计算

-   **`MagneticField`**:
    -   **职责**: 管理和提供三维磁场数据。
    -   **功能**:
        -   从标准格式的文本文件加载磁场图。
        -   使用三线性插值法提供空间中任意点的磁场矢量 `B(x, y, z)`。
        -   支持磁场的旋转，以匹配实验布局。
        -   可以将加载后的磁场对象保存为ROOT文件，以加快后续读取速度。

-   **`ParticleTrajectory`**:
    -   **职责**: 计算带电粒子在磁场中的运动轨迹。
    -   **功能**:
        -   使用四阶龙格-库塔（Runge-Kutta）方法进行积分。
        -   输入粒子的初始位置、动量、电荷和质量，计算出一系列轨迹点。
        -   是进行动量重建的基础。

-   **`TargetReconstructor`**:
    -   **职责**: 核心物理分析模块，用于重建粒子在靶位置的初始动量。
    -   **功能**:
        -   接收一个重建径迹（`RecoTrack`）和目标靶点的位置。
        -   通过将粒子从探测器位置 **反向传播** 到靶点，迭代寻找最佳的初始动量。
        -   返回一个包含最佳动量、最终距离和轨迹点的结果结构体 (`TargetReconstructionResult`)。

### 2.4. 可视化

-   **`EventDisplay`**:
    -   **职责**: 使用ROOT的EVE（Event Visualization Environment）系统进行三维事件显示。
    -   **功能**:
        -   加载并显示探测器的几何模型。
        -   以三维形式展示原始击中、重建径迹以及通过`ParticleTrajectory`计算出的粒子轨迹。
        -   可以方便地控制不同几何组件的可见性。

## 3. 工作流程示例

一个典型的分析流程如下：

1.  创建一个 `GeometryManager` 实例并加载几何文件。
2.  创建一个 `MagneticField` 实例并加载磁场图。
3.  创建一个 `EventDataReader` 实例并打开包含模拟数据的ROOT文件。
4.  创建 `PDCSimAna`, `TargetReconstructor`, 和 `EventDisplay` 的实例，并将 `GeometryManager` 和 `MagneticField` 传递给它们。
5.  循环遍历每个事件：
    a.  使用 `EventDataReader::NextEvent()` 获取新事件。
    b.  使用 `PDCSimAna::ProcessEvent()` 从原始数据重建出 `RecoEvent`。
    c.  对于 `RecoEvent` 中的每条径迹，使用 `TargetReconstructor::ReconstructAtTargetWithDetails()` 计算其在靶点的动量。
    d.  （可选）使用 `EventDisplay::DisplayEventWithTrajectories()` 将几何、原始数据、重建径迹和计算出的轨迹进行三维可视化。
