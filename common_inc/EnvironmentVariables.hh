#ifndef __ENVIRONMENT_VARIABLES_HH
#define __ENVIRONMENT_VARIABLES_HH

#include <Logger.hh>
#include <unordered_map>
#include <fstream>

class ConfigValues
{

public:

    // Singleton instance
    static ConfigValues& Instance(void)
    {
        static ConfigValues instance;
        return instance;
    }


    // Check environment variables first for overrides and then check config
    std::string Get(const std::string& variableName)
    {
        std::unordered_map<std::string, std::string>::iterator variable = m_EnvironmentVariableMap.find(variableName);
        if(m_EnvironmentVariableMap.find(variableName) == m_EnvironmentVariableMap.end())
        {
            std::string value = std::getenv(variableName.c_str());

            if(value.empty())
            {
                value = GetFromFile(variableName);
            }

            if(value.empty())
            {
                LOG_WARN("Could not find environment variable: ", variableName);
                return value;
            }

            m_EnvironmentVariableMap[variableName] = value;
            return m_EnvironmentVariableMap[variableName];
        }

        return variable->second;
    }

    ~ConfigValues()
    {

    }

private:

    const std::string WHITESPACE = " \n\r\t\f\v";
 
    std::string ltrim(const std::string &s)
    {
        size_t start = s.find_first_not_of(WHITESPACE);
        return (start == std::string::npos) ? "" : s.substr(start);
    }
    
    std::string rtrim(const std::string &s)
    {
        size_t end = s.find_last_not_of(WHITESPACE);
        return (end == std::string::npos) ? "" : s.substr(0, end + 1);
    }
    
    std::string trim(const std::string &s) {
        return rtrim(ltrim(s));
    }
 
    // Variable format is VARIABLE_NAME=value
    std::string GetFromFile(const std::string& variableName)
    {
        static const std::string CONFIG_FILE_NAME = "./config/kDB_config.txt";
        std::ifstream configFile;
        configFile.open(CONFIG_FILE_NAME,
            std::fstream::in);
        
        std::string line;

        if( !configFile.is_open() )
        {
            LOG_WARN("Could not open up ", CONFIG_FILE_NAME, " for reading!");
            return std::string("");
        }

        while(std::getline(configFile, line))
        {
            std::string::size_type assignmentPosition = line.find('=');
            if (assignmentPosition != std::string::npos)
            {
                 if(variableName == trim(line.substr(0, assignmentPosition)));
                 {
                    configFile.close();
                    return trim(line.substr(assignmentPosition + 1, line.length()));
                 }
            }
        }

        configFile.close();
        return std::string("");
    }

    std::unordered_map<std::string, std::string> m_EnvironmentVariableMap;
    ConfigValues() {};
};

#endif