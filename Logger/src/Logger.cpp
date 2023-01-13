#include <Logger.hh>
#include <stdio.h>
#include <cstdarg>
#include <iostream>

namespace Log
{
    const TextMod::Modifier Logger::TEXT_COLOR_BLUE(TextMod::ColorCode::FG_BLUE);
    const TextMod::Modifier Logger::TEXT_COLOR_GREEN(TextMod::ColorCode::FG_GREEN);
    const TextMod::Modifier Logger::TEXT_COLOR_YELLOW(TextMod::ColorCode::FG_YELLOW);
    const TextMod::Modifier Logger::TEXT_COLOR_RED(TextMod::ColorCode::FG_RED);
    const TextMod::Modifier Logger::TEXT_COLOR_DEFAULT(TextMod::ColorCode::FG_DEFAULT);

    Logger& Logger::Instance(void)
    {
        static Logger instance;
        return instance;
    }

    void Logger::Log(LogLevel level, const char* debugLevel, const char* fileName, int lineNum, const char* format, ...)
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        char LOG_PREFIX[1024] = "";

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

        std::cout << TEXT_COLOR << "[" << debugLevel << "]";
 #ifdef __LOG_SHOW_LINE
                std::cout << "[" << fileName << ":" << lineNum << "]";
 #endif
        std::cout << " ";
        char* s_Message = NULL;
        int nLength = 0;
        va_list args;
        va_start(args, format);
        //  Return the number of characters in the string referenced the list of arguments.
        // _vscprintf doesn't count terminating '\0' (that's why +1)
        nLength = std::vsprintf(LOG_PREFIX, format, args) + 1;
        s_Message = new char[nLength];
        vsnprintf(s_Message, nLength, format, args);
        std::cout << TEXT_COLOR << s_Message << "\n";
        va_end(args);

        delete[] s_Message;
    }

    Logger::~Logger()
    {
        std::cout << TEXT_COLOR_DEFAULT;
    }
}
