#ifndef _CLI_HH
#define _CLI_HH

#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <Logger.hh>
#include <retcode.hh>
#include <OFRI.hh>

namespace CLI
{

/* USAGE:
 *
 */

/*  CREATE CUSTOM CLI ARG TYPE:
* class CLI_CustomArgs : public CLI::CLI_Argument<CUSTOM_TYPE, 1, 2>
* {
*       using CLI_Argument::CLI_Argument;
*
*        bool TryConversion(const std::string& conversion, CUSTOM_TYPE& value)
*        {
*            if( is_valid(conversion) )
*            {
*              value = conversion
*              return true;
*            }
*
*            return false;
*        }
* };
*
*/

    class CLI_Argument_Interface
    {
        public:
            virtual bool AddValue(std::string& cli_argument) = 0;
            virtual std::string Option() = 0;
            virtual void AddOption(const std::string& option) = 0;
            virtual void InUse() = 0;
            virtual bool IsInUse() = 0;
            virtual bool IsRequired() = 0;
            virtual ~CLI_Argument_Interface() {}

            friend std::ostream& operator<< (std::ostream& os, const CLI_Argument_Interface& p)
            {
                p.print(os);
                return os;
            }

        protected:
            virtual void print(std::ostream& os) const = 0;
    };

    template <class ArgType>
    struct ArgContainer
    {
        ArgType value;
    };

    template <class ArgType, size_t MinRequiredValues, size_t MaxRequiredValues>
    class CLI_Argument : public CLI_Argument_Interface
    {
        public:
            CLI_Argument(const std::string& option, const std::string& desc = "", bool required = false)
                : m_Option(option), m_Options(), m_Description(desc), m_InUse(false), m_IsRequired(required),
                  m_MinRequiredValues(MinRequiredValues), m_MaxRequiredValues(MaxRequiredValues)
                {
                };

        public:
            size_t m_MinRequiredValues;
            size_t m_MaxRequiredValues;

        public:
            virtual std::string Option() { return m_Option; }
            virtual void InUse() { m_InUse = m_MinRequiredValues <= m_Values.size() && m_Values.size() <= m_MaxRequiredValues;}
            virtual bool IsInUse() { return m_InUse; }
            virtual bool IsRequired() {return m_IsRequired; }
            void Required() { m_IsRequired = true; }

            virtual bool AddValue(std::string& cli_argument)
            {
                ArgContainer<ArgType> container;
                if(TryConversion(cli_argument, container.value))
                {
                    m_Values.push_back(container);
                    InUse();
                    return true;
                }
                else
                {
                    m_InUse = false;
                    return false;
                }
            }

            virtual void AddOption(const std::string& option)
            {
                m_Options.push_back(option);
            }

            virtual void print(std::ostream& os) const
            {
                os << "[" << m_Option;

                if(m_IsRequired)
                {
                    os << TextMod::Modifier(TextMod::FG_RED) << " (Required)"
                       << TextMod::Modifier(TextMod::FG_DEFAULT);
                }

                os << "]: ";

                os << m_Description;
            }

            ArgType& GetValue(size_t index = 0)
            {
                return m_Values.at(index).value;
            }

        protected:
            virtual bool TryConversion(const std::string& conversion, ArgType& value) = 0;

        protected:
            std::string m_Option;
            std::vector<std::string> m_Options;
            std::string m_Description;
            bool m_InUse;
            bool m_IsRequired;
            std::vector<ArgContainer<ArgType>> m_Values;
    };


    class CLI_IntArgument : public CLI_Argument<int, 1, 1>
    {

        using CLI_Argument::CLI_Argument; // Use base constructor

        bool TryConversion(const std::string& conversion, int& value)
        {
            std::size_t pos;
            try
            {
                value = std::stoi(conversion, &pos);
                return true;
            }
            catch(std::invalid_argument const& except)
            {
                LOG_WARN("Could not convert ", m_Option, " with argument ", conversion, " because it could not be converted to an integer!");
                m_InUse = false;
                return false;

            }
            catch(std::out_of_range const& except)
            {
                LOG_WARN("Could not convert ", m_Option, " with argument ", conversion, " because the number is too large!");
                m_InUse = false;
                return false;
            }

        }
    };

    class CLI_FlagArgument: public CLI_Argument<bool, 0, 0>
    {
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, bool& value)
        {
            return false;
        }
    };

    class CLI_StringArgument: public CLI_Argument<std::string, 1, 1>
    {
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, std::string& value)
        {
            value = conversion;
            m_InUse = true;
            return true;
        }
    };

    class CLI_OBJECTArgument : public CLI::CLI_Argument<OBJECT, 1, 1>
    {
        using CLI_Argument::CLI_Argument;

        bool TryConversion(const std::string& conversion, OBJECT& value)
        {
            strncpy(value, conversion.c_str(), sizeof(value));
            m_InUse = true;
            return true;
        }
    };

    class CLI_OFRIArgument : public CLI::CLI_Argument<OFRI, 1, 1>
    {
        using CLI_Argument::CLI_Argument;

        // OBJECT.0.0.0
        bool TryConversion(const std::string& conversion, OFRI& value)
        {
            std::stringstream stream(conversion);

            // Must get OBJECT seperately or FRI will be included in stream out
            std::string token; 
            std::getline(stream, token, '.');
            if(stream.good())
            {
                strncpy(value.o, token.c_str(), sizeof(value.o));
            }
     
            // Now can get the rest of 0.0.0
            char ignore; // '.'
            if (stream >> value.f >> ignore >> value.r >> ignore >> value.i)
            {
                m_InUse = true;
                return true;
            }

            LOG_WARN("Could not convert OFRI: ", conversion);
            return false;
        }
    };

    class CLI_ORArgument : public CLI::CLI_Argument<OR, 1, 1>
    {
        using CLI_Argument::CLI_Argument;

        // OBJECT.0.0.0
        bool TryConversion(const std::string& conversion, OR& value)
        {
            std::stringstream stream(conversion);

            // Must get OBJECT seperately or FRI will be included in stream out
            std::string token; 
            std::getline(stream, token, '.');
            if(stream.good())
            {
                strncpy(value.o, token.c_str(), sizeof(value.o));
            }
     
            // Now can get the rest of 0.0.0
            char ignore; // '.'
            if (stream >> value.r)
            {
                m_InUse = true;
                return true;
            }

            LOG_WARN("Could not convert OR: ", conversion);
            return false;
        }
    };

    class Parser
    {
        public:
            Parser(const std::string& name, const std::string& desc = "")
                : m_Name(name), m_Description(desc)
                { }

            ~Parser() { }

            void Usage()
            {
                std::cout << *this;
            }

            Parser& AddArg(CLI_Argument_Interface& arg)
            {
                m_Arguments.emplace(arg.Option(), &arg);
                return *this;
            }

            bool ValidateArgs(void)
            {

                CLI_Argument_Interface* arg = nullptr;
                std::map<std::string, CLI_Argument_Interface*>::const_iterator iter;
                for(iter = m_Arguments.begin(); iter != m_Arguments.end(); iter++)
                {
                    arg = iter->second;
                    if(!arg->IsInUse() && arg->IsRequired())
                    {
                        return false;
                    }
                }

                return true;
            }

            bool ParseArg(char* argument)
            {
                std::string parsedArgument(argument);
                bool isArgGood = true;
                if ( m_Arguments.find(parsedArgument) == m_Arguments.end() )
                {
                    // Couldn't find option - must be a value
                    if(nullptr != m_CurrentArgument)
                    {
                        isArgGood = m_CurrentArgument->AddValue(parsedArgument);
                        if(!isArgGood)
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    m_CurrentArgument = m_Arguments[parsedArgument];
                    m_CurrentArgument->InUse();
                }

                return true;
            }

            RETCODE ParseCommandLineArguments(int argc, char* argv[])
            {
                size_t argument_index = 0;
                bool isArgGood = true;
                for(; argument_index < argc; argument_index++)
                {
                    isArgGood = ParseArg(argv[argument_index]);
                    if( !isArgGood )
                    {
                        return RTN_FAIL;
                    }
                }

                if( !ValidateArgs() )
                {
                    return RTN_FAIL;
                }

                return RTN_OK;
            }

            friend std::ostream&
            operator << (std::ostream& os, const Parser& parser)
            {
                os << TextMod::Modifier(TextMod::FG_DEFAULT) << parser.m_Name <<": " << parser.m_Description << std::endl;

                std::map<std::string, CLI_Argument_Interface*>::const_iterator arg;
                for(arg = parser.m_Arguments.begin(); arg != parser.m_Arguments.end(); arg++)
                {
                    os << *(arg->second) << std::endl;
                }

                return os;
            }

        private:
            std::string m_Name;
            std::string m_Description;
            std::map<std::string ,CLI_Argument_Interface*> m_Arguments;
            CLI_Argument_Interface* m_CurrentArgument = nullptr;
    };
}

#endif