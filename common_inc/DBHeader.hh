#include <pthread.h>
#include <OFRI.hh>

struct DBHeader
{
    pthread_mutex_t m_DBLock;
    OBJECT m_ObjectName;
    RECORD m_NumRecords;
};