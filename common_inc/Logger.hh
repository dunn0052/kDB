#ifndef _LOGGER_H
#define _LOGGER_H

// #define options
// __LOG_ENABLE -- perform any logging at all
// __LOG_SHOW_LINE -- shows exactly which file.line the log is generated
// __LOG_SHOW_COLOR -- color logs based on level
// __LOG_SHOW_TIME --  show time log was generated

#include <ostream>
#include <sstream>
#include <iostream>
#include <string>
#include <TextModifiers.hh>
#include <mutex>

#if __LOG_SHOW_TIME
#include <chrono>
#include <ctime>
#endif

#if __LOG_ENABLE
    #define LOG_TEMPLATE( LEVEL, ... ) Log::Logger::Instance().Log(std::cerr, Log::LogLevel::LEVEL, #LEVEL, __FILE__, __LINE__, ##__VA_ARGS__ )
    #define LOG_ERROR_TEMPLATE( LEVEL, ... ) Log::Logger::Instance().Log(std::cerr, Log::LogLevel::LEVEL, #LEVEL, __FILE__, __LINE__, ##__VA_ARGS__ )
#else
    #define LOG_TEMPLATE( LEVEL, ... )
    #define LOG_ERROR_TEMPLATE( LEVEL, ...)
#endif


//#define LOG_DEBUG( MESSAGE, ... ) LOG_TEMPLATE( MESSAGE, DEBUG, ##__VA_ARGS__ )
#define LOG_DEBUG( ... ) LOG_TEMPLATE( DEBUG, ##__VA_ARGS__ )
#define LOG_INFO( ... ) LOG_TEMPLATE( INFO, ##__VA_ARGS__ )
#define LOG_WARN( ... ) LOG_ERROR_TEMPLATE( WARN, ##__VA_ARGS__ )
#define LOG_ERROR( ... ) LOG_ERROR_TEMPLATE( ERROR, ##__VA_ARGS__ )

namespace Log
{
    enum class LogLevel
    {
        DEBUG,
        INFO,
        WARN,
        ERROR,
        NONE
    };

    class Logger
    {

    public:

        template<typename Stream, typename... RestOfArgs>
        Stream & Log(Stream& stream, LogLevel level, const char* debugLevel, const char* fileName, int lineNum, const RestOfArgs& ... args)
        {
            /* Internal string stream used to ensure thread safety when printing.
             * It is passed through to collect the arguments into a single string,
             * which will do a single << to the input stream at the end
             */
            std::stringstream internalStream;
            return Log(stream, internalStream, level, debugLevel, fileName, lineNum, args...);
        }

        template<typename Stream, typename... RestOfArgs>
        Stream & Log(Stream & stream, std::stringstream& internalStream, LogLevel level, const char* debugLevel, const char* fileName, int lineNum, const RestOfArgs& ... args)
        {

#if __LOG_ENABLE
#if __LOG_SHOW_COLOR
            m_LogLevel = level;
            switch (m_LogLevel)
            {
                case LogLevel::DEBUG:
                {
                    TEXT_COLOR = TextMod::ColorCode::FG_BLUE;
                    break;
                }

                case LogLevel::INFO:
                {
                    TEXT_COLOR = TextMod::ColorCode::FG_GREEN;
                    break;
                }

                case LogLevel::WARN:
                {
                    TEXT_COLOR = TextMod::ColorCode::FG_YELLOW;
                    break;
                }

                case LogLevel::ERROR:
                {
                    TEXT_COLOR = TextMod::ColorCode::FG_RED;
                    break;
                }
                default:
                {
                    TEXT_COLOR = TextMod::ColorCode::FG_DEFAULT;
                }
            }

        internalStream << TEXT_COLOR;
#endif

#endif
            internalStream <<  "[" << debugLevel << "]";

#if __LOG_SHOW_LINE
            internalStream << "[" << fileName << ":" << lineNum << "]";
#endif

#if __LOG_SHOW_TIME
            time_t currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            internalStream
                << "["
                << strtok(std::ctime(&currentTime), "\n")
                << "]";
#endif

            internalStream << " "; // Space between decorator and user text
            return Log(stream, internalStream, args...);
        }

        template<typename Stream, typename ThisArg, typename... RestOfArgs>
        Stream & Log(Stream & stream, std::stringstream& internalStream, const ThisArg & arg1, const RestOfArgs&... args)
        {
            internalStream << arg1;
            return Log(stream, internalStream, args...);
        }

        template<typename Stream, typename ThisArg>
        Stream & Log(Stream & stream, std::stringstream& internalStream, const ThisArg & arg1)
        {
            internalStream << arg1 << "\n";

#if __LOG_SHOW_COLOR
            // Reset for non-logger messages
            TEXT_COLOR = TextMod::ColorCode::FG_DEFAULT;
            internalStream << TEXT_COLOR;
            m_LogLevel = LogLevel::NONE;
#endif

            return (stream << internalStream.str());
        }

    static Logger& Instance(void)
    {
        static Logger instance;
        return instance;
    }

    ~Logger()
    {
#if __LOG_SHOW_COLOR
        TEXT_COLOR = TextMod::ColorCode::FG_DEFAULT;
        std::cout << TEXT_COLOR;
#endif
    }

    Logger() {};

    private:

        Logger(Logger const&) = delete;
        void operator = (Logger const&) = delete;
        LogLevel m_LogLevel = LogLevel::NONE;
        TextMod::Modifier TEXT_COLOR = TextMod::ColorCode::FG_DEFAULT;
        std::stringstream m_InternalStream;


    };

}

#endif