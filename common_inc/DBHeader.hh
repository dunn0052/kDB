#include <pthread.h>
#include <OFRI.hh>

/*
 * NOTE: DBHeader values should be accessed before fields
 *       to remain cache friendly
 */
struct DBHeader
{
    pthread_mutex_t m_DBLock;
    OBJECT m_ObjectName;
    RECORD m_NumRecords;
    RECORD m_LastWritten;
};