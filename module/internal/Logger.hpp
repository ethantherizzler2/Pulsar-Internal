#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <ctime>
#include <cstdarg>
#include <cstdio>

namespace Logger
{
    enum class Level
    {
        Info,
        Warn,
        Error,
        Debug
    };

    struct Entry
    {
        Level level;
        std::string message;
        std::string timestamp;
    };

    inline std::string FormatString(const char* fmt, ...)
    {
        if (!fmt)
            return "";

        va_list args;
        va_start(args, fmt);

        va_list argsCopy;
        va_copy(argsCopy, args);
        int len = std::vsnprintf(nullptr, 0, fmt, argsCopy);
        va_end(argsCopy);

        if (len <= 0)
        {
            va_end(args);
            return "";
        }

        std::string result;
        result.resize(len);

        std::vsnprintf(result.data(), len + 1, fmt, args);
        va_end(args);

        return result;
    }

    class Core
    {
    public:
        static Core& Get()
        {
            static Core instance;
            return instance;
        }

        void Add(Level level, const std::string& msg)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            Entry entry;
            entry.level = level;
            entry.message = msg;
            entry.timestamp = GetTimestamp();

            m_logs.push_back(entry);

            std::ofstream file("debug.log", std::ios::app);
            if (file.is_open())
                file << "[" << entry.timestamp << "] [" << LevelToString(level) << "] " << msg << "\n";
        }

        void AddF(Level level, const char* fmt, ...)
        {
            if (!fmt)
                return;

            va_list args;
            va_start(args, fmt);

            va_list argsCopy;
            va_copy(argsCopy, args);
            int len = std::vsnprintf(nullptr, 0, fmt, argsCopy);
            va_end(argsCopy);

            if (len > 0)
            {
                std::string buffer;
                buffer.resize(len);
                std::vsnprintf(buffer.data(), len + 1, fmt, args);
                Add(level, buffer);
            }

            va_end(args);
        }

        const std::vector<Entry>& GetLogs() const { return m_logs; }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_logs.clear();
        }

    private:
        Core() = default;
        ~Core() = default;

        std::vector<Entry> m_logs;
        mutable std::mutex m_mutex;

        std::string GetTimestamp()
        {
            std::time_t now = std::time(nullptr);
            char buf[64];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&now));
            return buf;
        }

        std::string LevelToString(Level level)
        {
            switch (level)
            {
            case Level::Info:  return "INFO";
            case Level::Warn:  return "WARN";
            case Level::Error: return "ERROR";
            case Level::Debug: return "DEBUG";
            }
            return "UNKNOWN";
        }
    };

    inline void info(const std::string& msg) { Core::Get().Add(Level::Info, msg); }
    inline void warn(const std::string& msg) { Core::Get().Add(Level::Warn, msg); }
    inline void error(const std::string& msg) { Core::Get().Add(Level::Error, msg); }
    inline void debug(const std::string& msg) { Core::Get().Add(Level::Debug, msg); }

    inline void infof(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        Core::Get().AddF(Level::Info, fmt, args);
        va_end(args);
    }

    inline void warnf(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        Core::Get().AddF(Level::Warn, fmt, args);
        va_end(args);
    }

    inline void errorf(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        Core::Get().AddF(Level::Error, fmt, args);
        va_end(args);
    }

    inline void debugf(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        Core::Get().AddF(Level::Debug, fmt, args);
        va_end(args);
    }
}
