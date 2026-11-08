#ifndef __LOG_HPP__
#define __LOG_HPP__

#include <iostream>
#include <cstdio>
#include <string>
#include <filesystem> //C++17
#include <sstream>
#include <fstream>
#include <memory>
#include <ctime>
#include <unistd.h>
#include "Mutex.hpp"
namespace LogMoudle
{
    const std::string gsep = "\r\n";
    using MutexModule::LockGuard;  // 只导入 LockGuard
    class LogStrategy
    {
    public:
        virtual ~LogStrategy() = default;
        virtual void SyncLog(const std::string &message) = 0;
    };
    class ConsoleLogStrategy : public LogStrategy
    {
    public:
        ConsoleLogStrategy()
        {
        }
        void SyncLog(const std::string &message) override
        {
            {
                LockGuard lock(_mutex);
                // 必须 flush：否则重定向到文件/管道时是全缓冲，
                // fork 会把未刷出的缓冲区连同内容一起复制给子进程，导致日志重复
                std::cout << message << gsep << std::flush;
            }
        }
        ~ConsoleLogStrategy()
        {
        }

    private:
        MutexModule::Mutex _mutex;
    };
    const std::string defaultpath = "./log";
    const std::string defaultname = "mylog.txt";
    class FileLogStrategy : public LogStrategy
    {
    public:
        FileLogStrategy(const std::string path = defaultpath, const std::string filename = defaultname)
            : _path(path), _name(filename)
        {
            LockGuard lock(_mutex);
            if (std::filesystem::exists(_path))
            {
                return;
            }
            else
            {
                try
                {
                    std::filesystem::create_directories(_path);
                }
                catch (const std::filesystem::filesystem_error &e)
                {
                    std::cerr << e.what() << '\n';
                }
            }
        }
        void SyncLog(const std::string &message) override
        {
            LockGuard lockguard(_mutex);
            std::string filename = _path + (_path.back() == '/' ? "" : "/") + _name;
            std::ofstream out(filename, std::ios::app);
            if (!out.is_open())
            {
                return;
            }
            else
            {
                out << message << gsep;
                out.close();
            }
        }
        ~FileLogStrategy()
        {
        }

    private:
        std::string _path;
        std::string _name;
        MutexModule::Mutex _mutex;
    };
    enum class LogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };
    std::string Level2Str(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }
    std::string GetTimeStamp()
    {
        time_t curr = time(nullptr);
        struct tm curr_tm;
        localtime_r(&curr, &curr_tm);
        char timebuffer[128];
        snprintf(timebuffer, sizeof(timebuffer), "%4d-%02d-%02d %02d:%02d:%02d",
                 curr_tm.tm_year + 1900,
                 curr_tm.tm_mon + 1,
                 curr_tm.tm_mday,
                 curr_tm.tm_hour,
                 curr_tm.tm_min,
                 curr_tm.tm_sec);
        return timebuffer;
    }
    class Logger
    {
    public:
        Logger()
        {
            EnableConsoleLogStrategy();
        }
        void EnableConsoleLogStrategy()
        {
            _fflush_strategy = std::make_unique<ConsoleLogStrategy>();
        }
        void EnableFileLogStrategy()
        {
            _fflush_strategy = std::make_unique<FileLogStrategy>();
        }
        class LogMessage
        {
        public:
            LogMessage(LogLevel &level, std::string &src_name, int line_number, Logger &logger)
                : _curr_time(GetTimeStamp()),
                  _level(level),
                  _pid(getpid()),
                  _src_name(src_name),
                  _line_number(line_number),
                  _logger(logger)
            {
                std::stringstream ss;
                ss << "[" << _curr_time << "]"
                   << "[" << Level2Str(_level) << "]"
                   << "[" << _pid << "]"
                   << "[" << _src_name << "]"
                   << "[" << _line_number << "]"
                   << " - ";
                _loginfo = ss.str();
            }
            ~LogMessage()
            {
                _logger._fflush_strategy->SyncLog(_loginfo);
            }
            template <class T>
            LogMessage &operator<<(const T &info)
            {
                std::stringstream ss;
                ss << info;
                _loginfo += ss.str();
                return *this;
            }

        private:
            std::string _curr_time;
            LogLevel _level;
            pid_t _pid;
            std::string _src_name;
            int _line_number;
            std::string _loginfo; // 合并之后，一条完整的信息
            Logger &_logger;
        };
        ~Logger()
        {
        }
        LogMessage operator()(LogLevel level, std::string src_name, int line_number)
        {
            return LogMessage(level, src_name, line_number, *this);
        }

    private:
        std::unique_ptr<LogStrategy> _fflush_strategy;
    };
    Logger logger;
#define LOG(level) logger(level, __FILE__, __LINE__)
#define Enable_Console_Log_Strategy() logger.EnableConsoleLogStrategy()
#define Enable_File_Log_Strategy() logger.EnableFileLogStrategy()
}
#endif
