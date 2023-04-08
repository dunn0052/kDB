#ifndef __DATABASE_ACCESS_HH
#define __DATABASE_ACCESS_HH

#include <DBMap.hh>
#include <Constants.hh>
#include <ConfigValues.hh>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string>
#include <sstream>
#include <retcode.hh>

class DatabaseAccess
{

    public:
        DatabaseAccess(OBJECT& object)
            : m_DBAddress(nullptr), m_Size(0), m_ObjectName(object),
              m_Object(dbSizes[m_ObjectName]), m_IsOpen(false)
        {
            std::map<std::string, OBJECT_SCHEMA>::iterator it = dbSizes.find(m_ObjectName);
            if(it != dbSizes.end())
            {
                m_Object = it->second;
                Open();
            }
        }

        DatabaseAccess(DatabaseAccess&& other)
            : m_DBAddress(std::move(other.m_DBAddress)),
              m_Size(std::move(other.m_Size)),
              m_ObjectName(std::move(other.m_ObjectName)),
              m_Object(std::move(other.m_Object)),
              m_IsOpen(std::move(other.m_IsOpen))
        {
            Open();
        }

        DatabaseAccess& operator=(DatabaseAccess&& other)
        {
            m_DBAddress = std::move(other.m_DBAddress);
            m_Size = std::move(other.m_Size);
            m_ObjectName = std::move(other.m_ObjectName);
            m_Object = std::move(other.m_Object);
            m_IsOpen = std::move(other.m_IsOpen);

            Open();

            return *this;
        }

        ~DatabaseAccess()
        {
            Close();
        }

        char* Get(const RECORD record)
        {
            if(m_IsOpen && nullptr != m_DBAddress)
            {
                size_t byte_index = m_Object.objectSize * record;
                if( m_Size > byte_index )
                {
                    return m_DBAddress + byte_index;
                }
            }

            return nullptr;
        }

        char* Get(const OFRI& ofri)
        {
            if(m_IsOpen && nullptr != m_DBAddress)
            {
                size_t byte_index = (m_Object.objectSize * ofri.r) +
                    m_Object.fields[ofri.f].fieldOffset +
                    (m_Object.fields[ofri.f].fieldSize * ofri.i);
                if( m_Size > byte_index )
                {
                    return m_DBAddress + byte_index;
                }
            }

            return nullptr;
        }

        RETCODE WriteValue(const OFRI& ofri, const std::string& value)
        {
            RETCODE retcode = RTN_OK;
            void* p_value = Get(ofri);
            if(nullptr == p_value)
            {
                return RTN_NULL_OBJ;
            }

            if(ofri.i > m_Object.fields[ofri.f].numElements - 1)
            {
                return RTN_NULL_OBJ;
            }

            switch(m_Object.fields[ofri.f].fieldType)
            {
                case 'O': // Object
                {
                    if(value.size() > OBJECT_NAME_LEN)
                    {
                        return RTN_BAD_ARG;
                    }

                    memset(p_value, 0, OBJECT_NAME_LEN);
                    strncpy(static_cast<char*>(p_value),
                        value.c_str(), value.size());
                    break;
                }
                case 'S': // String
                {
                    if(value.size() > sizeof(char*) * m_Object.fields[ofri.f].numElements)
                    {
                        return RTN_BAD_ARG;
                    }

                    memset(p_value, 0, sizeof(char*) * m_Object.fields[ofri.f].numElements);
                    strncpy(static_cast<char*>(p_value),
                        value.c_str(), value.size());
                    break;
                }
                case 'C': // Char
                case 'Y': // Unsigned char (byte)
                {
                    if(value.size() > 1)
                    {
                        return RTN_BAD_ARG;
                    }

                    *static_cast<char*>(p_value) = value.c_str()[0];
                    break;
                }
                case 'N': // signed integer
                {
                    int int_val = atol(value.c_str());
                    if(0 == int_val and "0" != value.c_str())
                    {
                        return RTN_BAD_ARG;
                    }

                    *static_cast<int*>(p_value) = atol(value.c_str());
                    break;
                }
                case 'F': // Field
                case 'R': // Record
                case 'I': // Index
                case 'U': // Unsigned integer
                {
                    unsigned int int_val = static_cast<unsigned int>(atol(value.c_str()));
                    if(0 == int_val and "0" != value.c_str())
                    {
                        return RTN_BAD_ARG;
                    }
                    *static_cast<unsigned int*>(p_value) = int_val;
                    break;
                }
                case 'B': // Bool
                {
                    std::stringstream value_buffer;
                    value_buffer << std::uppercase << value;
                    bool bool_val = true;

                    if("FALSE" == value_buffer.str() or
                        "0" == value_buffer.str())
                    {
                        *static_cast<bool*>(p_value) = false;
                    }
                    else if("TRUE" != value_buffer.str() or
                        "1" != value_buffer.str())
                    {
                        *static_cast<bool*>(p_value) = true;
                    }
                    else
                    {
                        return RTN_BAD_ARG;
                    }
                    
                    break;
                }
                default:
                {
                    return RTN_BAD_ARG;
                }
            }

            return RTN_OK;
        }

        RETCODE ReadValue(const OFRI& ofri, std::string& value)
        {
            void* p_value = Get(ofri);
            if(nullptr == p_value)
            {
                return RTN_NULL_OBJ;
            }

            if(ofri.i > m_Object.fields[ofri.f].numElements - 1)
            {
                return RTN_NULL_OBJ;
            }

            std::stringstream db_value;
            switch(m_Object.fields[ofri.f].fieldType)
            {
                case 'O': // Object
                {
                    db_value << *static_cast<OBJECT*>(p_value);
                    break;
                }
                case 'Y': // Unsigned char (byte)
                {
                    db_value << *static_cast<unsigned char*>(p_value);
                    break;
                }
                case 'C': // Char
                {
                    db_value << *static_cast<char*>(p_value);
                    break;
                }
                case 'S': // String
                {
                    db_value.rdbuf()->sputn(reinterpret_cast<char*>(p_value),
                        sizeof(char) * m_Object.fields[ofri.f].numElements);
                    break;
                }
                case 'N': // signed integer
                {
                    db_value << *static_cast<int*>(p_value);
                    break;
                }
                case 'F': // Field
                case 'R': // Record
                case 'I': // Index
                case 'U': // Unsigned integer
                {
                    db_value << *static_cast<unsigned int*>(p_value);
                    break;
                }
                case 'B': // Bool
                {
                    db_value << *static_cast<bool*>(p_value);
                    break;
                }
                default:
                {
                    return RTN_BAD_ARG;
                }
            }

            if(!db_value.good())
            {
                LOG_WARN("Schema error for: ", ofri.o,
                         " could not convert field: ", ofri.f, " to: ",
                         m_Object.fields[ofri.f].fieldType);
                return RTN_NULL_OBJ;
            }

            value = db_value.str();
            return RTN_OK;
        }

    inline bool IsValid()
    {
        return m_IsOpen;
    }

    private:

        char* MapObject(int fd, off_t size)
        {
            char *p_return = static_cast<char*>( mmap(nullptr, size,
                    PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0) );

            if( p_return == MAP_FAILED )
            {
                return nullptr;
            }

            return p_return;
        }

        RETCODE Open()
        {
            int fd = OpenDatabase();
            RETCODE retcode = RTN_OK;
            if( 0 > fd )
            {
                std::cout << "Failed to open: " << m_ObjectName << "\n";
                return RTN_NOT_FOUND;
            }

            struct stat statbuf;
            int error = fstat(fd, &statbuf);
            if( 0 > error )
            {
                return RTN_NOT_FOUND;
            }

            m_Size = statbuf.st_size;
            m_DBAddress = MapObject(fd, m_Size);
            if( nullptr == m_DBAddress )
            {
                std::cout << "Failed to map: " <<  m_ObjectName << "\n";
                retcode |= RTN_FAIL;
            }

            m_IsOpen = true;

            return retcode;
        }

        RETCODE Close()
        {
            int error = 0;

            error = munmap(m_DBAddress, m_Size);
            if( 0 != error )
            {
                return RTN_FAIL;
            }

            m_IsOpen = false;
            return RTN_OK;
        }

        int OpenDatabase()
        {
            std::stringstream filepath;
            std::string INSTALL_DIR =
                ConfigValues::Instance().Get(KDB_INSTALL_DIR);
            if("" == INSTALL_DIR)
            {
                return -1;
            }
            filepath << INSTALL_DIR << DB_DB_DIR << m_ObjectName << DB_EXT;
            const std::string path = filepath.str();
            int fd = open(path.c_str(), O_RDWR);
            return fd;
        }

        char* m_DBAddress;
        size_t m_Size;
        std::string m_ObjectName;
        OBJECT_SCHEMA m_Object;
        bool m_IsOpen;
};

#endif