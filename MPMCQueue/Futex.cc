#include<unistd.h>
#include<sys/syscall.h>
#include<errno.h>
#include<assert.h>
#include"./Futex.h"

int nativeFutexWake(const void* addr,int count,uint32_t wakeMask)
{
    int rv=syscall(__NR_futex,
                   addr,
                   FUTEX_WAKE_BITSET|FUTEX_PRIVATE_FLAG,
                   count,
                   nullptr,
                   nullptr,
                   wakeMask);
    if(rv<0)
        return 0;
    return rv;
}
//add time in the future
FutexResult nativeFutexWait(const void* addr,uint32_t expected,uint32_t waitMask)
{
    int op=FUTEX_WAIT_BITSET|FUTEX_PRIVATE_FLAG;
    int rv=syscall(__NR_futex,
                  addr,
                  op,
                  expected,
                  nullptr,
                  nullptr,
                  waitMask);
    if(rv==0)
    {
        return FutexResult::AWOKEN;
    }
    else
    {
        switch (errno)
        {
        case EINTR:
            return FutexResult::INTERRUPTED;
        case EWOULDBLOCK:
            return FutexResult::VALUE_CHANGED;        
        default:
            assert(false);
        }
    }
    
}

FutexResult
futexWait(const Futex* futex,uint32_t expected,uint32_t waitmask)
{
    return nativeFutexWait(futex,expected,waitmask);
}

int futexWake(const Futex* futex,int count,uint32_t wakeMask)
{
    return nativeFutexWake(futex,count,wakeMask);
}


