#pragma once

#include<atomic>

#include"ThreadLocal.h"

template<typename inT>
class leptVar
{
    struct VarCache;
public:
    
    explicit leptVar(inT initial = 0,uint32_t cachesize = 1000)
     : target_(initial),
       cacheSize_(cachesize)
    {

    } 
    ~leptVar() = default;

    //non-copyable
    const leptVar& operator=(const leptVar&) = delete;
    leptVar(const leptVar&) = delete;

    void increment(inT inc)
    {
        auto cache = cache_.get();
        if(UNLIKELY(cache == nullptr))
        {
            cache = new VarCache(*this);
            cache_.reset(cache);
        }

        cache->increment(inc);        
    }

    inT readFast() const
    {
        return target_.load(std::memory_order_relaxed);
    }

    inT readFull() const
    {
        inT res = readFast();
        auto accessor = cache_.accessAllThreads();
        for (auto it = accessor.begin(); it != accessor.end() ; it++)
        {
            if(!it->reset_.load(std::memory_order_acquire))
                res += it->cacheVar_.load(std::memory_order_acquire);
        }
        
        return res;
    }

    inT readFastAndReset()
    {
        return target_.exchange(0,std::memory_order_relaxed);
    }

    inT readFullAndReset()
    {
        inT res = readFastAndReset();
        auto accessor = cache_.accessAllThreads();
        for (auto it = accessor.begin(); it != cache_.end() ; it++)
        {
            if(!it->reset_.load(std::memory_order_acquire))
            {
                res += it->cacheVar_.load(std::memory_order_acquire);
                it->reset_.store(true,std::memory_order_release);
            }
        }
        
        return res;

    }

    void set(inT newval)
    {
        auto accessor = cache_.accessAllThreads();
        for (auto it = accessor.begin(); it != cache_.end() ; it++)
        {
            it->reset_.store(true,std::memory_order_release);
        }
        
        target_.store(newval,std::memory_order_release);
    }

    void setCacheSize(uint32_t newsize)
    {
        cacheSize_.store(newsize,std::memory_order_release);
    }

    uint32_t getCacheSize() const
    {
        return cacheSize_.load(std::memory_order_acquire);
    }

    leptVar& operator++()
    {
        increment(1);
        return *this;
    }

    leptVar& operator--()
    {
        increment(-1);
        return *this;
    }


    leptVar& operator+=(inT inc)
    {
        increment(inc);
        return *this;
    }

    leptVar& operator-=(inT inc)
    {
        increment(-inc);
        return *this;
    }


private:

    struct VarCache
    {
        VarCache(leptVar& var)
         : reset_(false),
           numUpdate_(0),
           cacheVar_(0),
           parent_(&var)
        {

        }

        ~VarCache()
        {
            flush();
        }  
    
        void flush()
        {
            parent_->target_.fetch_add(cacheVar_);
            numUpdate_ = 0;
            cacheVar_.store(0);
        }

        void increment(inT inc)
        {
            if(LIKELY(!reset_.load(std::memory_order_acquire)))
            {
                cacheVar_.store(cacheVar_.load(std::memory_order_relaxed) + inc,std::memory_order_release);
            }
            else
            {
                reset_.store(false,std::memory_order_release);
                cacheVar_.store(inc,std::memory_order_release);
            }
            ++numUpdate_;
            if(numUpdate_ >= parent_->cacheSize_)
            {
                flush();
            }
        }

        std::atomic<bool> reset_;
        std::atomic<uint32_t> numUpdate_;
        std::atomic<inT> cacheVar_;
        leptVar* parent_;
    };

    std::atomic<uint32_t> target_;
    std::atomic<uint32_t> cacheSize_;
    ThreadLocalPtr<VarCache> cache_; 

};