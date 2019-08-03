#ifndef RWSPINGLOCK_H_
#define RWSPINGLOCK_H_

#include<atomic>
#include<thread>
/* A high performence RWSpinglock taking facebooks's folly as reference
*/

class RWSpingLock
{
private:
    //we set upgarde state to prevent starving the writer.It indicate 
    //that the data will be write int the future 
    enum : int32_t {READER = 4,UPGRADE = 2, WRITER = 1};
public:
    RWSpingLock() : bits_(0) {}
    ~RWSpingLock()=default;
    //lock is noncopyable
    RWSpingLock(const RWSpingLock&)=delete;
    RWSpingLock& operator=(const RWSpingLock&)=delete;

    void lock()
    {
        uint64_t count=0;
        while(!try_lock())
        {
            if(++count>1000)
                std::this_thread::yield();
        }
        
    }

    void unlock()
    {
        //need to reset upgrade
        bits_.fetch_and(~(WRITER|UPGRADE),std::memory_order_release);
    }

    void lock_shared()
    {
        uint64_t count=0;
        while(!try_lock_shared())
        {
            if(++count>1000)
                std::this_thread::yield();
        }
        
    }

    void unlock_shared()
    {
        bits_.fetch_add(-READER,std::memory_order_acq_rel);
    }

    void unlock_and_lock_shared()
    {
        bits_.fetch_add(READER,std::memory_order_acq_rel);
        unlock();
    }

    void lock_upgrade()
    {
        uint64_t count=0;
        while(!try_lock_upgrade())
        {
            ++count;
            if(count>1000)
                std::this_thread::yield();
        }
        
    }
    void unlock_upgrade()
    {
        bits_.fetch_add(-UPGRADE,std::memory_order_acq_rel);
    }
    void unlock_upgrade_and_lock()
    {
        uint64_t count=0;
        while(!try_unlock_upgarde_and_lock())
        {
            ++count;
            if(count>1000)
                std::this_thread::yield();
        }
        
    }

    void unlock_upgrade_and_lock_shared()
    {
        bits_.fetch_add(READER-UPGRADE,std::memory_order_acq_rel);
    }

    void unlock_and_lock_upgrade()
    {
        //directly cas or two step --- set upgrade first then reset writer 

        // int32_t expect=WRITER;
        // bits_.compare_exchange_strong(expect,UPGRADE,std::memory_order_acq_rel);
        bits_.fetch_or(UPGRADE,std::memory_order_acq_rel);
        bits_.fetch_sub(WRITER,std::memory_order_release);
    }


public:
  class ReadHolder;
  class UpgradeHolder;
  class WriterHolder;

  class ReadHolder
  {
    private:
        friend class UpgradeHolder;
        friend class WriterHolder;
        RWSpingLock *lock_;
    public:
        explicit ReadHolder(RWSpingLock* lock):lock_(lock)
        {
            if(lock_)
                lock_->lock_shared();
        }
        explicit ReadHolder(RWSpingLock& lock):lock_(&lock)
        {
            if(lock_)
                lock_->lock_shared();
        }
        ReadHolder(ReadHolder&& other):lock_(other.lock_)
        {
            other.lock_=nullptr;
        }
        ReadHolder(UpgradeHolder&& other):lock_(other.lock_)
        {
            other.lock_=nullptr;
            if(lock_)
                lock_->unlock_upgrade_and_lock_shared();
        }
        ReadHolder(WriterHolder&& other):lock_(other.lock_)
        {
            other.lock_=nullptr;
            if(lock_)
                lock_->unlock_and_lock_shared();
        }
        ReadHolder& operator=(ReadHolder&& other)
        {
            std::swap(lock_,other.lock_);
            return *this;
        }
        void rest(RWSpingLock* lock = nullptr)
        {
            if(lock==lock_)
                return;
            if(lock_)
                lock_->unlock_shared();
            lock_=lock;
            if(lock_)
                lock_->lock_shared();
        }
        ~ReadHolder()
        {
            if(lock_)
                lock_->unlock_shared();
        }
  };
  
  class UpgradeHolder
  {
    public:
        explicit UpgradeHolder(RWSpingLock* lock):lock_(lock)
        {
            if(lock_)
                lock->lock_upgrade();
        }
        explicit UpgradeHolder(RWSpingLock& lock):lock_(&lock)
        {
            if(lock_)
                lock_->lock_upgrade();
        }
        UpgradeHolder(WriterHolder&& other):lock_(other.lock_)
        {
            other.lock_=nullptr;
            if(lock_)
                lock_->unlock_and_lock_upgrade();
        }
        ~UpgradeHolder()
        {
            if(lock_)
                lock_->unlock_upgrade();
        }
    private:
        RWSpingLock* lock_;
        friend class ReadHolder;
        friend class WriterHolder;
  };
  
  
  class WriterHolder
  {
    public:
        WriterHolder(RWSpingLock* lock):lock_(lock)
        {
            if(lock_)
                lock_->lock();
        }
        WriterHolder(RWSpingLock& lock):lock_(&lock)
        {
            if(lock_)
                lock_->lock();
        }
        WriterHolder(UpgradeHolder&& other):lock_(other.lock_)
        {
            other.lock_=nullptr;
            if(lock_)
                lock_->unlock_upgrade_and_lock();
        }
        ~WriterHolder()
        {
            if(lock_)
                lock_->unlock();
        }
    private:
        RWSpingLock* lock_;
        friend class ReadHolder;
        friend class UpgradeHolder;
  };
  
  
  
 
   
private:
    //try to get write permission
    bool try_lock()
    {
        int32_t expect=0;
        return bits_.compare_exchange_strong(expect,WRITER,std::memory_order_acq_rel);
    }
    // try to get reader permission 
    bool try_lock_shared()
    {
        int32_t value=bits_.fetch_add(READER,std::memory_order_acq_rel);
        if(value&(READER|WRITER))
        {
            bits_.fetch_add(-READER,std::memory_order_acq_rel);
            return false;
        }
        return true;
    }
    // try to unlock upgrade and lock write
    bool try_unlock_upgarde_and_lock()
    {
        int32_t expect=UPGRADE;
        return bits_.compare_exchange_strong(expect,WRITER,std::memory_order_acq_rel);
    } 
    // try to lock upgrade
    bool try_lock_upgrade()
    {
        int32_t value=bits_.fetch_or(UPGRADE,std::memory_order_acq_rel);
        //if the upgrade bit has already  been setted,don't worry. return false
        return (value&(UPGRADE|WRITER))==0;
    }

private:
    std::atomic<int32_t> bits_;
};
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 



#endif