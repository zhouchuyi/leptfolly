#pragma once

#include<iterator>
#include<type_traits>
#include<utility>

#include"ThreadLocalDetail.h"


template<typename T,class Tag,class AccessMode>
class ThreadLocalPtr;

template<typename T,class Tag = void,class AccessMode = void>
class ThreadLocal
{
public:
    constexpr ThreadLocal(/* args */);
    ~ThreadLocal();
    //non-copybale
    ThreadLocal(const ThreadLocal&) = delete;
    ThreadLocal& operator=(const ThreadLocal&) = delete;

    T* operator->() const
    {
    
    }

    T& operator*() const
    {

    }

    void reset(T* newPtr = nullptr)
    {

    } 

private:

    T* makeTlp() const
    {
    auto ptr = construct_();
    tlp_.reset(ptr);
    return ptr;
    }

    mutable ThreadLocalPtr<T,Tag,AccessMode> tlp_;
    std::function<T*()> construct_;
};

template<typename T,class Tag,class AccessMode>
class ThreadLocalPtr
{
public:
    typedef StaticMeta<Tag,AccessMode> StaticMeta;

    constexpr ThreadLocalPtr() : id_() {}

    ThreadLocalPtr(ThreadLocalPtr&& gonner) noexcept : id_(std::move(gonner.id_)) {}

    ~ThreadLocalPtr()
    {
        destroy();
    }

    T* get() const
    {
        ElementWrapper& w = StaticMeta::get(&id_);
        return static_cast<T*>(w.ptr);
    }

    T* release() const
    {
        ElementWrapper* w = StaticMeta::get(&id_);
        return static_cast<T*>(w->release());    
    }
    void reset(T* newPtr = nullptr) const
    {
        ElementWrapper& w = StaticMeta::get(&id_);
        w.dispose(DestructionMode::THIS_THREAD);
        w.cleanup();
        w.set(nullptr);
    }
    class Accessor
    {
        StaticMetaBase& meta_;
        std::mutex mutex_;
        uint32_t id_;
        public:
            class Iterator;
            friend class Iterator;
            struct Iterator
            {
                const Accessor* accesor_;
                ThreadEntryNode* e_;
                friend class Accessor;
                void increment();
                void decrement();
                const T& dereference() const;
                T& deference();
                explicit Iterator(const Accessor* accesor)
                    : accesor_(accesor),e_(accesor_->meta_.head_.elements[accesor_->id].node)
                {
                    
                }

            };
            
            
            


            Accessor(/* args */);
            ~Accessor();
        
    
    };
    
    
    

private:
    void destroy()
    {

    }

    mutable typename StaticMetaBase::EntryID id_;
};