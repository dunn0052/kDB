#ifndef __ENVIRONMENT_VARIABLES_HH
#define __ENVIRONMENT_VARIABLES_HH

#include <Logger.hh>
#include <unordered_map>

class EnvironmentVariable
{

public:

    static EnvironmentVariable& Instance(void)
    {
        static EnvironmentVariable instance;
        return instance;
    }

    std::string Get(const std::string& variableName)
    {
        std::unordered_map<std::string, std::string>::iterator variable = m_EnvironmentVariableMap.find(variableName);
        if(m_EnvironmentVariableMap.find(variableName) == m_EnvironmentVariableMap.end())
        {
            const char* environmentVariable = std::getenv(variableName.c_str());
            if(nullptr == environmentVariable)
            {
                LOG_WARN("Could not find environment variable: ", variableName);
                return "";
            }

            m_EnvironmentVariableMap[variableName] = std::string(environmentVariable);
            return m_EnvironmentVariableMap[variableName];
        }

        return variable->second;
    }

    ~EnvironmentVariable()
    {

    }

private:

    std::unordered_map<std::string, std::string> m_EnvironmentVariableMap;
    EnvironmentVariable() {};
};

#endif