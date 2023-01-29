#ifndef HOOK_HH
#define HOOK_HH
#include <vector>
#include <algorithm>

template <class DelegateType>
class Hook
{

public:

    Hook() {}
    ~Hook() {}

    template <typename ... EventArgs>
    void Invoke(EventArgs&& ... args)
    {
        for (DelegateType delegate : m_Delegates)
        {
            delegate(std::forward<EventArgs>(args)...);
        }
    }
    void Clear()
    {
        m_Delegates.clear();
    }

    void operator +=(DelegateType&& delegate)
    {
        m_Delegates.push_back(std::forward<DelegateType>(delegate));
    }

    void operator -=(DelegateType& delegate)
    {
        m_Delegates.erase(std::remove(m_Delegates.begin(), m_Delegates.end(), delegate),
            m_Delegates.end());
    }

private:
    std::vector<DelegateType> m_Delegates;
};

   /* EXAMPLE */

   /*
   struct EventArgsTest
   {
      int m_Param;
      int m_Other;
   };

   void ExampleDelegateFunctor(void* obj, const EventArgsTest&& e)
   {
      std::cout << e.m_Other << std::endl;
   }

   typedef void (*DelegateFunctor)(void*, const EventArgsTest&&);



   void TestScope()
   {
      Ref<Dispatcher<DelegateFunctor>> d = CreateRef<Dispatcher<DelegateFunctor>>();
      *d += &ExampleDelegateFunctor;
      d->Invoke(EventArgsTest());
   }
   */

#endif