// 示例：如何在 main.cc 中初始化日志系统

#include "SMLogger.hh"
#include <cstdlib>  // for getenv

int main(int argc, char** argv)
{
    // ========== 初始化日志系统 ==========
    SMLogger::LogConfig logConfig;
    
    // 从环境变量读取日志级别
    const char* logLevel = std::getenv("SM_LOG_LEVEL");
    if (logLevel) {
        std::string level(logLevel);
        if (level == "TRACE") logConfig.level = SMLogger::LogLevel::TRACE;
        else if (level == "DEBUG") logConfig.level = SMLogger::LogLevel::DEBUG;
        else if (level == "INFO") logConfig.level = SMLogger::LogLevel::INFO;
        else if (level == "WARN") logConfig.level = SMLogger::LogLevel::WARN;
        else if (level == "ERROR") logConfig.level = SMLogger::LogLevel::ERROR;
        else if (level == "OFF") logConfig.level = SMLogger::LogLevel::OFF;
    }
    
    // 从环境变量决定是否写日志文件
    const char* logFile = std::getenv("SM_LOG_FILE");
    if (logFile) {
        logConfig.file = true;
        logConfig.filename = logFile;
    }
    
    // 批量运行模式：减少控制台输出，增加文件输出
    const char* batchMode = std::getenv("SM_BATCH_MODE");
    if (batchMode && std::string(batchMode) == "1") {
        logConfig.level = SMLogger::LogLevel::WARN;  // 只输出警告和错误
        logConfig.file = true;
        logConfig.console = false;  // 批量运行时不输出到控制台
    }
    
    // 初始化
    SMLogger::Logger::Instance().Initialize(logConfig);
    
    SM_INFO("===============================================");
    SM_INFO("SMSimulator v5.5 Starting");
    SM_INFO("===============================================");
    
    // ========== 你的原始代码 ==========
    
    // 设置随机数种子
    CLHEP::HepRandom::setTheEngine(new CLHEP::RanecuEngine);
    G4long seed = time(NULL);
    CLHEP::HepRandom::setTheSeed(seed);
    SM_DEBUG("Random seed set to: {}", seed);
    
    // 创建 RunManager
    G4RunManager* runManager = new G4RunManager;
    SM_INFO("G4RunManager created");
    
    // 初始化探测器构建
    DeutDetectorConstruction* detector = new DeutDetectorConstruction();
    runManager->SetUserInitialization(detector);
    SM_INFO("Detector construction initialized");
    
    // ... 其他初始化代码 ...
    
    // 运行模拟
    if (argc == 1) {
        SM_INFO("Interactive mode");
        // UI session
    } else {
        SM_INFO("Batch mode: processing macro file {}", argv[1]);
        UImanager->ApplyCommand(command + argv[1]);
    }
    
    // ========== 清理 ==========
    SM_INFO("Simulation completed, cleaning up...");
    
    delete runManager;
    
    // 关闭日志系统（会自动刷新缓冲）
    SMLogger::Logger::Instance().Shutdown();
    
    return 0;
}
