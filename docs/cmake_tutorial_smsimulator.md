# CMake Tutorial for SMSimulator Project

## 目录
1. [项目结构分析](#项目结构分析)
2. [CMake基础概念](#cmake基础概念)
3. [根CMakeLists.txt详解](#根cmakeliststxt详解)
4. [子项目组织](#子项目组织)
5. [ROOT字典生成](#root字典生成)
6. [依赖管理](#依赖管理)
7. [构建顺序控制](#构建顺序控制)
8. [最佳实践](#最佳实践)

## 项目结构分析

SMSimulator项目是一个典型的C++科学计算项目，使用了以下技术栈：
- **Geant4**: 粒子物理模拟框架
- **ROOT**: 数据分析框架，需要字典生成
- **ANAROOT**: 自定义分析框架
- **Xerces-C**: XML解析库

项目结构：
```
smsimulator5.5/
├── CMakeLists.txt                 # 根CMake文件
├── sources/
│   ├── CMakeLists.txt            # sources的CMake文件
│   ├── smg4lib/                  # 共享库模块
│   │   ├── CMakeLists.txt
│   │   ├── data/                 # 数据类模块（需要ROOT字典）
│   │   ├── action/               # Geant4动作类
│   │   ├── construction/         # Geant4几何构造
│   │   └── physics/              # Geant4物理过程
│   └── sim_deuteron/             # 主程序模块
└── bin/                          # 输出目录
```

## CMake基础概念

### 1. 项目声明
```cmake
cmake_minimum_required(VERSION 3.16)
project(smsimulator CXX)
```
- `cmake_minimum_required`: 指定最低CMake版本
- `project`: 声明项目名称和使用的语言

### 2. 变量和选项
```cmake
set(CMAKE_CXX_STANDARD 17)          # 设置C++标准
set(VARIABLE_NAME value)            # 设置变量
option(OPTION_NAME "描述" OFF)       # 定义开关选项
```

### 3. 查找依赖包
```cmake
find_package(PackageName REQUIRED COMPONENTS comp1 comp2)
```

### 4. 包含目录和链接库
```cmake
include_directories(path)           # 添加头文件搜索路径
link_directories(path)              # 添加库文件搜索路径
target_include_directories(target PRIVATE/PUBLIC path)  # 更现代的方式
target_link_libraries(target library)  # 链接库
```

## 根CMakeLists.txt详解

让我们分析根目录的CMakeLists.txt：

```cmake
# 1. 基础设置
cmake_minimum_required(VERSION 3.16)
project(smsimulator CXX)
set(CMAKE_CXX_STANDARD 17)

# 2. 查找ROOT框架
find_package(ROOT REQUIRED COMPONENTS Core RIO Net Hist Graf Graf3d Gpad Tree Rint Postscript Matrix Physics MathCore Thread)
include(${ROOT_USE_FILE})

# 3. 查找Geant4框架
find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})
include_directories(${Geant4_INCLUDE_DIRS})

# 4. 查找ANAROOT（自定义框架）
if(DEFINED ENV{TARTSYS})
  message(STATUS "Found ANAROOT, TARTSYS = " $ENV{TARTSYS})
  include_directories($ENV{TARTSYS}/include)
  link_directories($ENV{TARTSYS}/lib)
  set(ANAROOT_LIBRARIES anasamurai anabrips anacore XMLParser)
else()
  message(FATAL_ERROR "$TARTSYS is not defined. please define the environment for ANAROOT.")
endif()

# 5. 查找Xerces-C
find_package(XercesC REQUIRED)
include_directories(${XERCESC_INCLUDE_DIR})

# 6. 添加子目录
add_subdirectory(sources)
```

### 关键点解释：

1. **环境变量检查**: `if(DEFINED ENV{TARTSYS})`检查环境变量是否存在
2. **条件编译**: 使用`message(FATAL_ERROR)`在关键依赖缺失时停止构建
3. **全局设置**: 在根目录设置所有子项目都需要的依赖

## 子项目组织

### sources/CMakeLists.txt
```cmake
# Build smg4lib first as sim_deuteron depends on it
add_subdirectory(smg4lib)
add_subdirectory(sim_deuteron)
```

这里的关键是**构建顺序**：smg4lib必须先构建，因为sim_deuteron依赖它。

### smg4lib/CMakeLists.txt
```cmake
project(smg4)

# Build data first as other modules depend on it
add_subdirectory(data)
add_subdirectory(action)
add_subdirectory(construction)
add_subdirectory(physics)
```

同样，data模块必须首先构建，因为其他模块都依赖它。

## ROOT字典生成

ROOT框架需要为C++类生成字典以支持反射和I/O。这是科学计算项目的特殊需求：

### data/CMakeLists.txt中的字典生成
```cmake
# 1. 收集需要生成字典的头文件
file(GLOB TINC_FILES "include/T*.hh")

# 2. 动态生成LinkDef文件
file(WRITE smdata_linkdef.hh "#if defined(__CINT__) || defined(__CLING__)\n")
file(APPEND smdata_linkdef.hh "#pragma link off all globals;\n")
file(APPEND smdata_linkdef.hh "#pragma link off all classes;\n")
file(APPEND smdata_linkdef.hh "#pragma link off all functions;\n")

# 3. 为每个T开头的类添加链接指令
foreach(loop_var IN LISTS TINC_FILES)
  get_filename_component(loop_var_filename "${loop_var}" NAME_WE)
  file(APPEND smdata_linkdef.hh "#pragma link C++ class ${loop_var_filename}+;\n")
endforeach()
file(APPEND smdata_linkdef.hh "#endif\n")

# 4. 生成ROOT字典
ROOT_GENERATE_DICTIONARY(G__smrootdata
  ${TINC_FILES}      
  LINKDEF smdata_linkdef.hh
)

# 5. 创建包含字典的库
add_library(smdata SHARED ${SRC_FILES} G__smrootdata.cxx)
```

### 关键概念：
- **LinkDef文件**: 告诉ROOT哪些类需要生成字典
- **字典源文件**: ROOT生成的C++源文件（G__smrootdata.cxx）
- **动态生成**: 使用CMake在配置时生成LinkDef文件

## 依赖管理

### 库之间的依赖关系
```cmake
# action模块依赖data模块
target_include_directories(smaction PUBLIC 
    include
    ../include           # smg4lib/include
    ../data/include      # data模块的头文件
)

target_link_libraries(smaction PUBLIC 
    ${Geant4_LIBRARIES}
    smdata              # 链接data库
)
```

### 可执行文件的依赖
```cmake
target_link_libraries(sim_deuteron 
  sim_deuteron-so        # 自己的库
  smaction               # smg4lib的各个模块
  smconstruction
  smdata
  smphysics
  ${ANAROOT_LIBRARIES}   # 外部依赖
  ${ROOT_LIBRARIES} 
  ${Geant4_LIBRARIES}
  ${XercesC_LIBRARIES}
)
```

## 构建顺序控制

CMake通过依赖关系自动确定构建顺序，但我们需要明确声明：

### 1. 通过add_subdirectory顺序
```cmake
# 正确的顺序
add_subdirectory(data)        # 最基础的模块
add_subdirectory(action)      # 依赖data
add_subdirectory(construction) # 依赖data
add_subdirectory(physics)     # 独立模块
```

### 2. 通过target_link_libraries
```cmake
target_link_libraries(smaction PUBLIC smdata)  # 自动建立依赖关系
```

### 3. 自定义命令的依赖
```cmake
add_custom_command(
    TARGET sim_deuteron POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:sim_deuteron>
        ${CMAKE_SOURCE_DIR}/bin/sim_deuteron
    DEPENDS sim_deuteron
)
```

## 最佳实践

### 1. 模块化设计
- 每个功能模块一个CMakeLists.txt
- 清晰的依赖关系
- 避免循环依赖

### 2. 路径管理
```cmake
# 推荐：使用相对路径
../include
../data/include

# 避免：硬编码绝对路径
/home/user/project/include
```

### 3. 变量使用
```cmake
# 好的做法
set(SOURCES src/file1.cc src/file2.cc)
add_library(mylib ${SOURCES})

# 更好的做法
file(GLOB SOURCES "src/*.cc")
add_library(mylib ${SOURCES})
```

### 4. 条件编译
```cmake
if(DEFINED ENV{VARIABLE_NAME})
    # 处理存在的情况
else()
    message(FATAL_ERROR "Required environment variable not set")
endif()
```

### 5. 输出控制
```cmake
# 设置输出目录
set_target_properties(target PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib
)
```

## 调试技巧

### 1. 查看变量值
```cmake
message(STATUS "Variable value: ${VARIABLE_NAME}")
```

### 2. 详细输出
```bash
make VERBOSE=1  # 查看详细编译命令
```

### 3. 依赖关系图
```bash
cmake --graphviz=deps.dot .
dot -Tpng deps.dot -o deps.png
```

## 总结

SMSimulator项目的CMake系统展示了以下重要概念：

1. **分层结构**: 根项目 → 模块组 → 具体模块
2. **依赖管理**: 明确的依赖关系和构建顺序
3. **外部集成**: 与ROOT、Geant4等框架的集成
4. **自动化**: 字典生成、文件复制等自动化任务
5. **可维护性**: 模块化设计便于维护和扩展

这种结构适用于大多数科学计算项目，特别是需要集成多个外部框架的复杂项目。

## 实战演练：从零开始构建CMake项目

### 步骤1：创建基础结构
```bash
mkdir my_simulator
cd my_simulator
mkdir -p sources/{mylib,myapp} bin lib
```

### 步骤2：根CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.16)
project(MySimulator CXX)

set(CMAKE_CXX_STANDARD 17)

# 查找依赖
find_package(ROOT REQUIRED)

# 添加子目录
add_subdirectory(sources)
```

### 步骤3：构建一个简单库
sources/mylib/CMakeLists.txt：
```cmake
project(mylib)

# 收集源文件
file(GLOB SRC_FILES "src/*.cc")
file(GLOB INC_FILES "include/*.hh")

# 创建库
add_library(mylib SHARED ${SRC_FILES})

# 设置包含路径
target_include_directories(mylib PUBLIC include)

# 链接ROOT库
target_link_libraries(mylib PUBLIC ${ROOT_LIBRARIES})
```

### 步骤4：构建可执行文件
sources/myapp/CMakeLists.txt：
```cmake
project(myapp)

# 创建可执行文件
add_executable(myapp main.cc)

# 设置包含路径
target_include_directories(myapp PRIVATE 
    ../mylib/include
)

# 链接库
target_link_libraries(myapp 
    mylib
    ${ROOT_LIBRARIES}
)

# 复制到bin目录
add_custom_command(
    TARGET myapp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:myapp>
        ${CMAKE_SOURCE_DIR}/bin/
)
```

## 常见问题和解决方案

### 问题1：找不到头文件
```
error: 'SomeHeader.hh' file not found
```

**解决方案**：
```cmake
# 检查包含路径
target_include_directories(target PRIVATE 
    include
    ../other_module/include
)
```

### 问题2：链接错误
```
undefined reference to `SomeFunction`
```

**解决方案**：
```cmake
# 检查库依赖
target_link_libraries(target 
    required_library
    ${EXTERNAL_LIBRARIES}
)
```

### 问题3：ROOT字典问题
```
Error in <TClass::TClass>: cannot find any dictionary
```

**解决方案**：
```cmake
# 确保正确生成字典
ROOT_GENERATE_DICTIONARY(G__mydict
    MyClass.hh
    LINKDEF MyLinkDef.hh
)

# 包含字典源文件
add_library(mylib ${SOURCES} G__mydict.cxx)
```

### 问题4：构建顺序错误
```
No rule to make target 'libdependency.so'
```

**解决方案**：
```cmake
# 确保正确的add_subdirectory顺序
add_subdirectory(dependency_module)  # 先构建依赖
add_subdirectory(main_module)        # 后构建主模块
```

## 进阶技巧

### 1. 使用Generator Expressions
```cmake
# 根据构建类型设置不同的输出目录
set_target_properties(myapp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY 
    ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>
)
```

### 2. 自定义函数
```cmake
function(add_root_dictionary target)
    set(options)
    set(oneValueArgs LINKDEF)
    set(multiValueArgs HEADERS)
    
    cmake_parse_arguments(ARD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    ROOT_GENERATE_DICTIONARY(G__${target}
        ${ARD_HEADERS}
        LINKDEF ${ARD_LINKDEF}
    )
endfunction()

# 使用自定义函数
add_root_dictionary(mylib
    HEADERS MyClass.hh AnotherClass.hh
    LINKDEF MyLinkDef.hh
)
```

### 3. 配置文件生成
```cmake
# 生成配置头文件
configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/config.h
)
```

config.h.in：
```cpp
#ifndef CONFIG_H
#define CONFIG_H

#define PROJECT_VERSION_MAJOR @PROJECT_VERSION_MAJOR@
#define PROJECT_VERSION_MINOR @PROJECT_VERSION_MINOR@

#cmakedefine ENABLE_FEATURE_X

#endif
```

## 性能优化

### 1. 并行编译
```bash
make -j$(nproc)  # 使用所有CPU核心
```

### 2. 缓存优化
```cmake
# 避免重复的file(GLOB)
if(NOT DEFINED SRC_FILES)
    file(GLOB SRC_FILES "src/*.cc")
endif()
```

### 3. 预编译头文件
```cmake
target_precompile_headers(mylib PRIVATE
    <iostream>
    <vector>
    "CommonHeader.hh"
)
```

## 总结

通过这个教程，您应该能够：

1. 理解SMSimulator项目的CMake结构
2. 掌握ROOT字典生成的方法
3. 正确管理复杂项目的依赖关系
4. 解决常见的构建问题
5. 应用CMake最佳实践

记住：CMake是一个强大的工具，但需要仔细规划项目结构。从简单开始，逐步增加复杂性，始终保持模块化和清晰的依赖关系。