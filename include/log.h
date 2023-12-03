#ifndef __FANSHUTOU_LOG_H__
#define __FANSHUTOU_LOG_H__

#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <stdarg.h>
#include <map>
#include "util.h"
#include "singleton.h"
#include "thread.h"

#define FANSHUTOU_LOG_LEVEL(logger, level) \
    if(logger->getLevel() <= level) \
        fst::LogEventWrap(fst::LogEvent::ptr(new fst::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, fst::GetThreadId(),\
                fst::GetFiberId(), time(0), fst::Thread::GetName()))).getSS()


#define FANSHUTOU_LOG_DEBUG(logger) FANSHUTOU_LOG_LEVEL(logger, fst::LogLevel::DEBUG)
#define FANSHUTOU_LOG_INFO(logger) FANSHUTOU_LOG_LEVEL(logger, fst::LogLevel::INFO)
#define FANSHUTOU_LOG_WARN(logger) FANSHUTOU_LOG_LEVEL(logger, fst::LogLevel::WARN)
#define FANSHUTOU_LOG_ERROR(logger) FANSHUTOU_LOG_LEVEL(logger, fst::LogLevel::ERROR)


#define FANSHUTOU_LOG_FATAL(logger) FANSHUTOU_LOG_LEVEL(logger, fst::LogLevel::FATAL)
#define FANSHUTOU_LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(logger->getLevel() <= level) \
        fst::LogEventWrap(fst::LogEvent::ptr(new fst::LogEvent(logger, level, \
                        __FILE__, __LINE__, 0, fst::GetThreadId(),\
                fst::GetFiberId(), time(0), fst::Thread::GetName()))).getEvent()->format(fmt, __VA_ARGS__)
#define FANSHUTOU_LOG_FMT_DEBUG(logger, fmt, ...) FANSHUTOU_LOG_FMT_LEVEL(logger, fst::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define FANSHUTOU_LOG_FMT_INFO(logger, fmt, ...)  FANSHUTOU_LOG_FMT_LEVEL(logger, fst::LogLevel::INFO, fmt, __VA_ARGS__)
#define FANSHUTOU_LOG_FMT_WARN(logger, fmt, ...)  FANSHUTOU_LOG_FMT_LEVEL(logger, fst::LogLevel::WARN, fmt, __VA_ARGS__)
#define FANSHUTOU_LOG_FMT_ERROR(logger, fmt, ...) FANSHUTOU_LOG_FMT_LEVEL(logger, fst::LogLevel::ERROR, fmt, __VA_ARGS__)
#define FANSHUTOU_LOG_FMT_FATAL(logger, fmt, ...) FANSHUTOU_LOG_FMT_LEVEL(logger, fst::LogLevel::FATAL, fmt, __VA_ARGS__)

#define FANSHUTOU_LOG_ROOT() fst::LoggerMgr::GetInstance()->getRoot()
#define FANSHUTOU_LOG_NAME(name) fst::LoggerMgr::GetInstance()->getLogger(name)

namespace fst {

class Logger;
class LoggerManager;

class LogLevel {
public:
    enum Level {
        UNKNOW = 0,
        DEBUG = 1,
        INFO = 2,
        WARN = 3,
        ERROR = 4,
        FATAL = 5
    };
    static const char* ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str);
};

class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;
    LogEvent(std::shared_ptr<Logger> logger, 
            LogLevel::Level level,
            const char* file_name, 
            uint32_t line, 
            uint32_t elapse,
            uint32_t thread_id, 
            uint32_t fiber_id,
            uint64_t time,
            const std::string& thread_name)
            : m_file_name(file_name),
            m_line(line),
            m_elapse(elapse),
            m_thread_id(thread_id),
            m_fiber_id(fiber_id),
            m_time(time),
            m_thread_name(thread_name),
            m_logger(logger),
            m_level(level){}

    std::string getFile() const { return m_file_name;}
    uint32_t getLine() const { return m_line;}
    uint32_t getElapse() const { return m_elapse;}
    uint32_t getThreadId() const { return m_thread_id;}
    uint32_t getFiberId() const { return m_fiber_id;}
    uint64_t getTime() const { return m_time;}
    const std::string& getThreadName() const { return m_thread_name;}
    std::string getContent() const { return m_ss.str();}
    std::shared_ptr<Logger> getLogger() const { return m_logger;}
    LogLevel::Level getLevel() const { return m_level;}
    std::stringstream& getSS() { return m_ss;}
    void format(const char* fmt, ...);
    void format(const char* fmt, va_list al);
private:
    const char* m_file_name;            //文件名
    int32_t m_line = 0;                 //行号
    uint32_t m_elapse = 0;              //程序开始运行时间
    uint32_t m_thread_id = 0;           //线程ID
    uint32_t m_fiber_id = 0;            //协程ID
    uint64_t m_time = 0;                //时间戳
    std::string m_thread_name;          //线程名
    std::stringstream m_ss;             //日志内容 -- 流
    std::shared_ptr<Logger> m_logger;   //日志器
    LogLevel::Level m_level;            //日志级别
};

class LogEventWrap {
public:
    LogEventWrap(LogEvent::ptr e) : m_event(e) {}
    ~LogEventWrap();

    LogEvent::ptr getEvent() const { return m_event;}
    std::stringstream& getSS() {return m_event->getSS();}
private:
    LogEvent::ptr m_event;
};

class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern) : m_pattern(pattern){ init(); }
    std::string format(LogLevel::Level level, LogEvent::ptr event);
    std::ostream& format_ss(std::ostream& ofs,LogLevel::Level level, LogEvent::ptr event);
public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format_ss(std::ostream& os, LogLevel::Level level, LogEvent::ptr event) = 0;
    };
    void init();
    bool isError() const { return m_error;}
    const std::string getPattern() const { return m_pattern;}
private:
    std::string m_pattern;
    std::vector<FormatItem::ptr> m_items;
    bool m_error = false;

};

class LogAppender {
friend class Logger;
public:
    typedef std::shared_ptr<LogAppender> ptr;
    typedef Spinlock MutexType;
    virtual ~LogAppender() {}
    virtual void log(LogLevel::Level level, LogEvent::ptr event) = 0;
    virtual std::string toYamlString() = 0;
    void setFormatter(LogFormatter::ptr val);
    LogFormatter::ptr getFormatter();
    LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}
protected:
    LogLevel::Level m_level = LogLevel::DEBUG;
    bool m_hasFormatter = false;
    MutexType m_mutex;
    LogFormatter::ptr m_formatter;
};

class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;
    void log(LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
};

class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;
    FileLogAppender(const std::string& filename);
    void log(LogLevel::Level level, LogEvent::ptr event) override;
    std::string toYamlString() override;
    bool reopen();
private:
    std::string m_filename;
    std::ofstream m_filestream;
    uint64_t m_lastTime = 0;
};

class Logger : public std::enable_shared_from_this<Logger> {
friend class LoggerManager;
public:
    typedef std::shared_ptr<Logger> ptr;
    typedef Spinlock MutexType;
    Logger(const std::string& name = "root");
    void log(LogLevel::Level level, LogEvent::ptr event);
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();
    LogLevel::Level getLevel() const { return m_level;}
    void setLevel(LogLevel::Level val) { m_level = val;}
    const std::string& getName() const { return m_name;}
    void setFormatter(LogFormatter::ptr val);
    void setFormatter(const std::string& val);
    LogFormatter::ptr getFormatter();
    std::string toYamlString();
private:
    std::string m_name;
    LogLevel::Level m_level;
    MutexType m_mutex;
    std::list<LogAppender::ptr> m_appenders;
    LogFormatter::ptr m_formatter;
    Logger::ptr m_root;
};

class LoggerManager {
public:
    typedef Spinlock MutexType;
    LoggerManager();
    Logger::ptr getLogger(const std::string& name);
    void init();
    Logger::ptr getRoot() const { return m_root;}
    std::string toYamlString();
private:
    MutexType m_mutex;
    std::map<std::string, Logger::ptr> m_loggers;
    Logger::ptr m_root;
};

typedef fst::Singleton<LoggerManager> LoggerMgr;

}

#endif
