# SMLogger 使用指南

## 概述

SMLogger 是一个基于 spdlog 的高性能异步日志系统，专为 SMSimulator 设计。

## 特性

- ✅ **异步日志**：不阻塞主计算线程
- ✅ **多级别**：TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL
- ✅ **多输出**：支持控制台和文件
- ✅ **彩色输出**：控制台彩色日志
- ✅ **滚动文件**：自动管理日志文件大小
- ✅ **线程安全**：支持多线程环境
- ✅ **高性能**：零拷贝、内存池

## 快速开始

### 1. 初始化日志系统

```cpp
#include "SMLogger.hh"

int main() {
    // 使用默认配置初始化
    SMLogger::Logger::Instance().Initialize();
    
    // 或者自定义配置
    SMLogger::LogConfig config;
    config.level = SMLogger::LogLevel::DEBUG;
    config.async = true;
    config.console = true;
    config.file = true;
    config.filename = "simulation.log";
    SMLogger::Logger::Instance().Initialize(config);
    
    // 你的代码...
    
    // 程序结束前关闭
    SMLogger::Logger::Instance().Shutdown();
    return 0;
}
```

### 2. 使用日志宏

```cpp
#include "SMLogger.hh"

void MyFunction() {
    SM_INFO("Simulation started");
    SM_DEBUG("Loading geometry from {}", filename);
    SM_WARN("Memory usage: {} MB", mem_mb);
    SM_ERROR("Failed to open file: {}", filepath);
    
    // 带格式化
    SM_INFO("Event {}/{} processed", current, total);
    SM_INFO("Position: ({}, {}, {})", x, y, z);
    
    // 条件日志
    SM_INFO_IF(verbose, "Verbose information");
    SM_WARN_IF(threshold > 100, "Threshold exceeded: {}", threshold);
    
    // 定时日志（避免日志轰炸）
    for (int i = 0; i < 1000000; i++) {
        SM_INFO_EVERY_N(10000, "Processed {} events", i);
    }
}
```

### 3. 使用命名 Logger

```cpp
// 为不同模块创建独立的 logger
auto geo_logger = SM_LOGGER("Geometry");
auto phys_logger = SM_LOGGER("Physics");

geo_logger->info("Geometry initialized");
phys_logger->debug("Physics list: {}", list_name);
```

## 日志级别

| 级别 | 用途 | 示例 |
|------|------|------|
| TRACE | 详细追踪 | 函数调用、循环迭代 |
| DEBUG | 调试信息 | 变量值、中间结果 |
| INFO | 一般信息 | 程序状态、进度 |
| WARN | 警告信息 | 资源不足、可恢复错误 |
| ERROR | 错误信息 | 操作失败、异常 |
| CRITICAL | 严重错误 | 致命错误、程序崩溃 |

## 运行时设置日志级别

```cpp
// 通过环境变量
export SM_LOG_LEVEL=DEBUG

// 或在代码中
SMLogger::Logger::Instance().SetLevel(SMLogger::LogLevel::DEBUG);
```

## 批量运行配置

```bash
# 批量运行时减少日志输出
export SM_LOG_LEVEL=WARN
./bin/sim_deuteron batch.mac

# 测试时增加日志输出
export SM_LOG_LEVEL=DEBUG
./bin/sim_deuteron test.mac
```

## 性能对比

| 场景 | std::cout | SMLogger (sync) | SMLogger (async) |
|------|-----------|-----------------|------------------|
| 10k 日志 | ~500ms | ~300ms | ~50ms |
| I/O 阻塞 | 是 | 是 | 否 |
| 多线程安全 | 否 | 是 | 是 |

## 迁移指南

### 从 std::cout 迁移

```cpp
// 之前
std::cout << "Value: " << value << std::endl;

// 之后
SM_INFO("Value: {}", value);
```

### 从 G4cout 迁移

```cpp
// 之前
G4cout << "Geometry loaded" << G4endl;

// 之后
SM_INFO("Geometry loaded");
```

## API 参考

### LogLevel 枚举
```cpp
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};
```

### LogConfig 结构
```cpp
struct LogConfig {
    LogLevel level = LogLevel::INFO;
    bool async = true;
    bool console = true;
    bool file = false;
    std::string filename = "smsim.log";
    size_t max_file_size = 10 * 1024 * 1024;  // 10MB
    size_t max_files = 3;
    bool color = true;
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v";
};
```

### 主要宏

| 宏 | 说明 |
|----|------|
| `SM_TRACE(...)` | 追踪级别日志 |
| `SM_DEBUG(...)` | 调试级别日志 |
| `SM_INFO(...)` | 信息级别日志 |
| `SM_WARN(...)` | 警告级别日志 |
| `SM_ERROR(...)` | 错误级别日志 |
| `SM_CRITICAL(...)` | 严重错误日志 |
| `SM_LOGGER(name)` | 获取命名 logger |
| `SM_INFO_IF(cond, ...)` | 条件日志 |
| `SM_INFO_EVERY_N(n, ...)` | 每 N 次输出 |

## 最佳实践

1. **程序入口初始化**
   ```cpp
   int main() {
       SMLogger::Logger::Instance().Initialize();
       // ...
       SMLogger::Logger::Instance().Shutdown();
   }
   ```

2. **使用合适的日志级别**
   - 生产环境：INFO
   - 开发调试：DEBUG
   - 性能分析：TRACE
   - 批量运行：WARN

3. **避免日志轰炸**
   ```cpp
   // 不好
   for (int i = 0; i < 1000000; i++) {
       SM_INFO("Event {}", i);
   }
   
   // 好
   for (int i = 0; i < 1000000; i++) {
       SM_INFO_EVERY_N(10000, "Processed {} events", i);
   }
   ```

4. **及时刷新重要日志**
   ```cpp
   SM_ERROR("Critical error occurred");
   SMLogger::Logger::Instance().Flush();
   ```
