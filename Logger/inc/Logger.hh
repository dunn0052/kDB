#ifndef _LOGGER_H
#define _LOGGER_H

#include <ostream>
#include <PrintColor.hh>

#if _LOG_DEBUG
    #define LOG_TEMPLATE( MESSAGE, LEVEL,  ... ) Log::Logger::Instance().Log(Log::LogLevel::LEVEL, #LEVEL , __FILE__, __LINE__, MESSAGE, ##__VA_ARGS__ )
#else
    #define LOG_TEMPLATE( MESSAGE, LEVEL, ... )
#endif


#define LOG_DEBUG( MESSAGE, ... ) LOG_TEMPLATE( MESSAGE, DEBUG, ##__VA_ARGS__ )
#define LOG_INFO( MESSAGE,  ... ) LOG_TEMPLATE( MESSAGE, INFO, ##__VA_ARGS__ )
#define LOG_WARN( MESSAGE, ... ) LOG_TEMPLATE( MESSAGE, WARN, ##__VA_ARGS__ )
#define LOG_FATAL( MESSAGE, ... ) LOG_TEMPLATE( MESSAGE, FATAL, ##__VA_ARGS__ )

#if _ENABLE_ASSERTS
    #define GTD_ASSERT( PREDICATE, ... ) { if(!( PREDICATE )) { LOG_FATAL("Assertion Failed: %s", ##__VA_ARGS__); __debugbreak(); }}
#endif

namespace Log
{
    enum class LogLevel
    {
        DEBUG,
        INFO,
        WARN,
        FATAL,
        NONE
    };

    class Logger
    {

    public:
        static Logger& Instance(void);

        void Log(LogLevel level, const char* debugLevel, const char* fileName, int lineNum, const char* format, ...);

    private:
        Logger() {};
        ~Logger() {/* should clean up any file streams here*/ };
        Logger(Logger const&) = delete;
        void operator = (Logger const&) = delete;
        LogLevel m_LogLevel = LogLevel::NONE;
        Color::Modifier TEXT_COLOR = TEXT_COLOR_DEFAULT;

    private:
        // colors
        static const Color::Modifier TEXT_COLOR_BLUE;
        static const Color::Modifier TEXT_COLOR_GREEN;
        static const Color::Modifier TEXT_COLOR_YELLOW;
        static const Color::Modifier TEXT_COLOR_RED;
        static const Color::Modifier TEXT_COLOR_DEFAULT;

        // thread guard
        //std::mutex mMutex;
    };

}

#endif