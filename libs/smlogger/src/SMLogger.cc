#include "SMLogger.hh"

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace SMLogger {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    Shutdown();
}

void Logger::Initialize(const LogConfig& config) {
    if (m_initialized) {
        spdlog::warn("Logger already initialized, reinitializing...");
        Shutdown();
    }
    
    m_config = config;
    
    try {
        std::vector<spdlog::sink_ptr> sinks;
        
        // 控制台输出
        if (config.console) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(config.pattern);
            sinks.push_back(console_sink);
        }
        
        // 文件输出
        if (config.file) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.filename, 
                config.max_file_size, 
                config.max_files
            );
            file_sink->set_pattern(config.pattern);
            sinks.push_back(file_sink);
        }
        
        // 创建 logger
        if (config.async) {
            // 异步模式：线程池大小1，队列大小8192
            spdlog::init_thread_pool(8192, 1);
            m_default_logger = std::make_shared<spdlog::async_logger>(
                "SMSimulator",
                sinks.begin(),
                sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block
            );
        } else {
            m_default_logger = std::make_shared<spdlog::logger>(
                "SMSimulator",
                sinks.begin(),
                sinks.end()
            );
        }
        
        // 设置日志级别
        switch (config.level) {
            case LogLevel::TRACE:    m_default_logger->set_level(spdlog::level::trace); break;
            case LogLevel::DEBUG:    m_default_logger->set_level(spdlog::level::debug); break;
            case LogLevel::INFO:     m_default_logger->set_level(spdlog::level::info); break;
            case LogLevel::WARN:     m_default_logger->set_level(spdlog::level::warn); break;
            case LogLevel::ERROR:    m_default_logger->set_level(spdlog::level::err); break;
            case LogLevel::CRITICAL: m_default_logger->set_level(spdlog::level::critical); break;
            case LogLevel::OFF:      m_default_logger->set_level(spdlog::level::off); break;
        }
        
        // 注册为默认 logger
        spdlog::set_default_logger(m_default_logger);
        
        // 设置刷新策略（警告级别及以上立即刷新）
        m_default_logger->flush_on(spdlog::level::warn);
        
        m_initialized = true;
        
        SM_INFO("SMSimulator Logger initialized");
        SM_INFO("  Mode: {}", config.async ? "Async" : "Sync");
        SM_INFO("  Level: {}", static_cast<int>(config.level));
        SM_INFO("  Console: {}", config.console);
        SM_INFO("  File: {}", config.file);
        if (config.file) {
            SM_INFO("  Log file: {}", config.filename);
        }
        
    } catch (const spdlog::spdlog_ex& ex) {
        fprintf(stderr, "Logger initialization failed: %s\n", ex.what());
        m_initialized = false;
    }
}

void Logger::Shutdown() {
    if (m_initialized) {
        // 避免在关闭 spdlog 线程池前再向异步 logger 提交日志（可能会抛出
        // "thread pool doesn't exist anymore" 异常或引起竞态）。q
        // 使用直接输出到 stderr 代替 SM_INFO，以保证不会调用 spdlog 的异步机制。
        fprintf(stderr, "SMSimulator: Shutting down logger...\n");
        Flush();
        spdlog::shutdown();
        m_initialized = false;
    }
}

void Logger::SetLevel(LogLevel level) {
    if (!m_initialized) {
        Initialize();
    }
    
    m_config.level = level;
    switch (level) {
        case LogLevel::TRACE:    m_default_logger->set_level(spdlog::level::trace); break;
        case LogLevel::DEBUG:    m_default_logger->set_level(spdlog::level::debug); break;
        case LogLevel::INFO:     m_default_logger->set_level(spdlog::level::info); break;
        case LogLevel::WARN:     m_default_logger->set_level(spdlog::level::warn); break;
        case LogLevel::ERROR:    m_default_logger->set_level(spdlog::level::err); break;
        case LogLevel::CRITICAL: m_default_logger->set_level(spdlog::level::critical); break;
        case LogLevel::OFF:      m_default_logger->set_level(spdlog::level::off); break;
    }
}

std::shared_ptr<spdlog::logger> Logger::GetLogger(const std::string& name) {
    if (!m_initialized) {
        Initialize();
    }
    
    if (name == "SMSimulator" || name.empty()) {
        return m_default_logger;
    }
    
    // 获取或创建命名 logger
    auto logger = spdlog::get(name);
    if (!logger) {
        logger = m_default_logger->clone(name);
        spdlog::register_logger(logger);
    }
    return logger;
}

void Logger::Flush() {
    if (m_initialized && m_default_logger) {
        m_default_logger->flush();
    }
}

} // namespace SMLogger
