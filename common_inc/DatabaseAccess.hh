#ifndef __DATABASE_ACCESS_HH
#define __DATABASE_ACCESS_HH

#include <DBMap.hh>
#include <Constants.hh>
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
        DatabaseAccess(const OBJECT& object)
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

        char* Get(const DOFRI& dofri)
        {
            if(m_IsOpen && nullptr != m_DBAddress)
            {
                size_t byte_index = (m_Object.objectSize * dofri.r) +
                    m_Object.fields[dofri.f].fieldOffset +
                    (m_Object.fields[dofri.f].fieldSize * dofri.i);
                if( m_Size > byte_index )
                {
                    return m_DBAddress + byte_index;
                }
            }

            return nullptr;
        }

        RETCODE WriteValue(const DOFRI& dofri, const std::string& value)
        {
            RETCODE retcode = RTN_OK;
            void* p_value = Get(dofri);
            if(nullptr == p_value)
            {
                return RTN_NULL_OBJ;
            }

            switch(m_Object.fields[dofri.f].fieldType)
            {
                case 'D': // Databse innacurate because its a string alias
                {
                    if(value.size() > m_Object.fields[dofri.f].numElements)
                    {
                        return RTN_BAD_ARG;
                    }

                    strncpy(static_cast<char*>(p_value),
                        value.c_str(), value.size());
                    break;
                }
                case 'O': // Object
                {
                    if(value.size() > m_Object.fields[dofri.f].numElements)
                    {
                        return RTN_BAD_ARG;
                    }

                    strncpy(static_cast<char*>(p_value),
                        value.c_str(), value.size());
                    break;
                }
                case 'C': // Char
                case 'Y': // Unsigned char (byte)
                {
                    if(value.size() > m_Object.fields[dofri.f].numElements)
                    {
                        return RTN_BAD_ARG;
                    }

                    memcpy(p_value, value.c_str(), value.size());
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
                    size_t int_val = static_cast<size_t>(atol(value.c_str()));
                    if(0 == int_val and "0" != value.c_str())
                    {
                        retcode |= RTN_BAD_ARG;
                    }
                    *static_cast<size_t*>(p_value) = int_val;
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
                        bool_val = false;
                    }
                    *static_cast<bool*>(p_value) = bool_val;
                    break;
                }
                default:
                {
                    return RTN_BAD_ARG;
                }
            }

            return RTN_OK;
        }

        RETCODE ReadValue(const DOFRI& dofri, std::string& value)
        {
            void* p_value = Get(dofri);
            if(nullptr == p_value)
            {
                return RTN_NULL_OBJ;
            }

            std::stringstream db_value;

            switch(m_Object.fields[dofri.f].fieldType)
            {
                case 'D': // Databse innacurate because its a string alias
                {
                    db_value << *static_cast<DATABASE*>(p_value);
                    break;
                }
                case 'O': // Object
                {
                    db_value << *static_cast<OBJECT*>(p_value);
                    break;
                }
                case 'C': // Char
                case 'Y': // Unsigned char (byte)
                {
                    db_value.rdbuf()->sputn(reinterpret_cast<char*>(p_value),
                        sizeof(char) * m_Object.fields[dofri.f].numElements);
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
                    db_value << *static_cast<size_t*>(p_value);
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

            value = db_value.str();
            return RTN_OK;
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