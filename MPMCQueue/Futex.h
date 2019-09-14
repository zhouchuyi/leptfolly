#include<linux/futex.h>
#include<atomic>
#include<limits>

enum class FutexResult{
    VALUE_CHANGED,
    AWOKEN,
    INTERRUPTED,
};

using Futex=std::atomic<uint32_t>;

//sleep if futex==expected
FutexResult
futexWait(const Futex* futex,uint32_t expected,uint32_t waitmask=-1);

//wake up waiters if (waitmask&wakemask)!=0,return the number of awoken threads
int futexWake(const Futex* futex,int count=std::numeric_limits<int>::max(),uint32_t wakeMask=-1);



class spinlock
{
public:
    spinlock() : lock_(0){}
    void lock()
    {
        uint32_t count=0;
        while(!trylock())
        {
            if(++count>1000)
                futexWait(&lock_,LOCKED);
        }
        
    }
    void unlock()
    {
        lock_.store(UNLOCK,std::memory_order_release);
    }
private:
    bool trylock()
    {
        uint32_t expect=UNLOCK;
        return lock_.compare_exchange_strong(expect,LOCKED,std::memory_order_acq_rel);
    }
    ~spinlock()=default;
    Futex lock_;
    // uint32_t LOCKED=1;
    // uint32_t UNLOCK=0;
    enum : uint32_t{
        LOCKED=1,
        UNLOCK=0,
    };
};




