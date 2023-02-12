#ifndef _LOGGER_H
#define _LOGGER_H

#include <ostream>
#include <sstream>
#include <iostream>
#include <string>
#include <TextModifiers.hh>
#include <compiler_defines.hh>
#include <mutex>

#if __ENABLE_LOGGING
    #define LOG_TEMPLATE( LEVEL,  ... ) Log::Logger::Instance().Log(std::cout, Log::LogLevel::LEVEL, #LEVEL, __FILE__, __LINE__, ##__VA_ARGS__ )
#else
    #define LOG_TEMPLATE( LEVEL, ... )
#endif


//#define LOG_DEBUG( MESSAGE, ... ) LOG_TEMPLATE( MESSAGE, DEBUG, ##__VA_ARGS__ )
#define LOG_DEBUG( ... ) LOG_TEMPLATE( DEBUG, ##__VA_ARGS__ )
#define LOG_INFO( ... ) LOG_TEMPLATE( INFO, ##__VA_ARGS__ )
#define LOG_WARN( ... ) LOG_TEMPLATE( WARN, ##__VA_ARGS__ )
#define LOG_FATAL( ... ) LOG_TEMPLATE( FATAL, ##__VA_ARGS__ )

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

        // printf() style log
        void Log(LogLevel level, const char* debugLevel, const char* fileName, int lineNum, const char* format, ...);
        
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
#ifdef __LOG_COLORS
            
            if (level != m_LogLevel)
            {
                m_LogLevel = level;
                // Change logging color
                // Could just set enums to logging color
                switch (m_LogLevel)
                {
                    case LogLevel::DEBUG:
                    {
                        TEXT_COLOR = TEXT_COLOR_BLUE;
                        break;
                    }

                    case LogLevel::INFO:
                    {
                        TEXT_COLOR = TEXT_COLOR_GREEN;
                        break;
                    }

                    case LogLevel::WARN:
                    {
                        TEXT_COLOR = TEXT_COLOR_YELLOW;
                        break;
                    }

                    case LogLevel::FATAL:
                    {
                        TEXT_COLOR = TEXT_COLOR_RED;
                        break;
                    }
                    default:
                    {
                        TEXT_COLOR = TEXT_COLOR_DEFAULT;
                    }
                }
                
            }

            internalStream << TEXT_COLOR;
#endif

            internalStream <<  "[" << debugLevel << "]";

#ifdef __LOG_SHOW_LINE
            internalStream << "[" << fileName << ":" << lineNum << "]";
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
#ifdef __LOG_COLOR
            // Reset for non-logger messages
            TEXT_COLOR = TEXT_COLOR_DEFAULT;
            internalStream << TEXT_COLOR;
#endif
            return (stream << internalStream.str());
        }

    
    private:
        Logger() {};
        ~Logger();
        Logger(Logger const&) = delete;
        void operator = (Logger const&) = delete;
        LogLevel m_LogLevel = LogLevel::NONE;
        TextMod::Modifier TEXT_COLOR = TEXT_COLOR_DEFAULT;
        std::stringstream m_InternalStream;

    private:
        // colors
        static const TextMod::Modifier TEXT_COLOR_BLUE;
        static const TextMod::Modifier TEXT_COLOR_GREEN;
        static const TextMod::Modifier TEXT_COLOR_YELLOW;
        static const TextMod::Modifier TEXT_COLOR_RED;
        static const TextMod::Modifier TEXT_COLOR_DEFAULT;

        // thread guard
        std::mutex mMutex;
    };

}

#endif