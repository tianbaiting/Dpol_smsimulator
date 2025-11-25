# 如何编译和使用分析库

本文档旨在说明如何编译自定义的分析代码库 (`libPDCAnalysisTools.so`) 以及如何在ROOT宏中调用它。

---

## 1. 环境准备

在进行任何操作之前，必须先设置好本项目的软件环境。这通过运行项目根目录下的 `setup.sh` 脚本来完成。

```bash
# 在项目根目录下执行
source setup.sh
```

**必须**在每次打开新的终端会话时都执行此命令，因为它会设置所有必需的环境变量，如 `ROOT_INCLUDE_PATH` 和 `LD_LIBRARY_PATH`。

---

## 2. 编译分析库 (使用CMake)

我们的分析代码位于 `d_work/sources` 目录下，并使用CMake进行管理。编译过程被设计为在 `d_work` 目录下一个名为 `build` 的独立目录中进行，这可以保持源码目录的整洁。

**编译步骤如下:**

```bash
# 1. 进入 d_work 目录
cd /home/tbt/workspace/dpol/smsimulator5.5/d_work

# 2. 创建一个用于编译的目录 (如果尚不存在)
mkdir -p build

# 3. 进入 build 目录
cd build

# 4. 运行 CMake 来配置项目
#    这里的 "../sources" 指向包含 CMakeLists.txt 的源码目录
cmake ../sources

# 5. 运行 make 来编译代码
make
```

**编译产物:**

*   编译成功后，会在 `d_work/build/` 目录下生成一个名为 `libPDCAnalysisTools.so` 的共享库文件。
*   这个 `.so` 文件包含了所有我们编写的分析类 (`PDCSimAna`, `GeometryManager`, `EventDataReader`, `EventDisplay`) 的已编译代码。

**注意**: 如果您在 `d_work/sources/src` 目录下添加了新的 `.cc` 文件，您只需重复上述第 3 和第 5 步 (`cd build && make`)，CMake会自动发现并编译新文件。

---

## 3. 在ROOT宏中调用库

一旦 `libPDCAnalysisTools.so` 编译完成，您就可以在任何ROOT宏中加载并使用它。

**核心步骤:**

1.  **加载库**: 在宏的开头使用 `gSystem->Load("path/to/your/library.so")`。
2.  **包含头文件**: 使用 `#include` 来包含您想使用的类的头文件。
3.  **创建对象并调用**: 像使用普通C++类一样创建对象和调用其方法。

**示例宏 (`d_work/macros/run_display.C`):**

```cpp
// 包含了我们所有自定义类的头文件
#include "GeometryManager.hh"
#include "PDCSimAna.hh"
#include "EventDataReader.hh"
#include "EventDisplay.hh"

// 全局指针，方便在ROOT命令行中交互
EventDataReader* gReader = nullptr;
EventDisplay*    gDisplay = nullptr;

void run_display(Long64_t event_id = 0) {
    // 1. 加载我们编译好的库
    gSystem->Load("/home/tbt/workspace/dpol/smsimulator5.5/d_work/build/libPDCAnalysisTools.so");

    // 2. 创建并设置所有分析工具
    GeometryManager* geo = new GeometryManager();
    geo->LoadGeometry("/path/to/geometry.mac");

    PDCSimAna* ana = new PDCSimAna(*geo);
    ana->SetSmearing(0.5, 0.5); // 例如，设置模糊参数

    gReader = new EventDataReader("/path/to/data.root");
    gDisplay = new EventDisplay("/path/to/geometry.gdml", *ana);

    // 3. 定位到指定事件并显示
    if (gReader->GoToEvent(event_id)) {
        gDisplay->DisplayEvent(*gReader);
    }
}

// --- 用于交互的辅助函数 ---
void next() {
    if (gReader && gDisplay) {
        if (gReader->NextEvent()) gDisplay->DisplayEvent(*gReader);
    }
}

void prev() {
    if (gReader && gDisplay) {
        if (gReader->PrevEvent()) gDisplay->DisplayEvent(*gReader);
    }
}
```

**如何运行示例宏:**

```bash
# 确保已 source setup.sh

# 使用ROOT运行宏 (保持GUI窗口打开)
root -l /home/tbt/workspace/dpol/smsimulator5.5/d_work/macros/run_display.C

# 在ROOT的命令行中，你还可以交互式地操作：
# root [0] next()  // 显示下一个事件
# root [1] prev()  // 显示上一个事件
```
