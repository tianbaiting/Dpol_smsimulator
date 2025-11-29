#ifndef SMLOGGER_HH
#define SMLOGGER_HH

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>

namespace SMLogger {

// 日志级别
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

// 日志配置
struct LogConfig {
    LogLevel level = LogLevel::INFO;
    bool async = true;              // 异步日志（推荐）
    bool console = true;            // 输出到控制台
    bool file = false;              // 输出到文件
    std::string filename = "smsim.log";
    size_t max_file_size = 10 * 1024 * 1024;  // 10MB
    size_t max_files = 3;           // 滚动文件数
    bool color = true;              // 彩色输出
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v";
};

// 日志管理器（单例）
class Logger {
public:
    // 获取全局日志实例
    static Logger& Instance();
    
    // 初始化日志系统
    void Initialize(const LogConfig& config = LogConfig());
    
    // 关闭日志系统
    void Shutdown();
    
    // 设置日志级别
    void SetLevel(LogLevel level);
    
    // 获取底层 spdlog logger
    std::shared_ptr<spdlog::logger> GetLogger(const std::string& name = "SMSimulator");
    
    // 刷新日志缓冲
    void Flush();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    bool m_initialized = false;
    LogConfig m_config;
    std::shared_ptr<spdlog::logger> m_default_logger;
};

// 便捷宏定义
#define SM_TRACE(...)    SMLogger::Logger::Instance().GetLogger()->trace(__VA_ARGS__)
#define SM_DEBUG(...)    SMLogger::Logger::Instance().GetLogger()->debug(__VA_ARGS__)
#define SM_INFO(...)     SMLogger::Logger::Instance().GetLogger()->info(__VA_ARGS__)
#define SM_WARN(...)     SMLogger::Logger::Instance().GetLogger()->warn(__VA_ARGS__)
#define SM_ERROR(...)    SMLogger::Logger::Instance().GetLogger()->error(__VA_ARGS__)
#define SM_CRITICAL(...) SMLogger::Logger::Instance().GetLogger()->critical(__VA_ARGS__)

// 带名字的日志宏
#define SM_LOGGER(name) SMLogger::Logger::Instance().GetLogger(name)

// 条件日志
#define SM_INFO_IF(cond, ...) if(cond) SM_INFO(__VA_ARGS__)
#define SM_WARN_IF(cond, ...) if(cond) SM_WARN(__VA_ARGS__)
#define SM_ERROR_IF(cond, ...) if(cond) SM_ERROR(__VA_ARGS__)

// 定时日志（避免日志轰炸）
#define SM_INFO_EVERY_N(n, ...) \
    do { \
        static int _log_count = 0; \
        if (++_log_count % (n) == 0) SM_INFO(__VA_ARGS__); \
    } while(0)

} // namespace SMLogger

#endif // SMLOGGER_HH
