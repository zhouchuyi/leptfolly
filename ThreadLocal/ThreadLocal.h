#pragma once

#include<iterator>
#include<type_traits>
#include<utility>

#include"ThreadLocalDetail.h"


template<typename T,typename AccessMode = void>
class ThreadLocalPtr;

template<typename T,typename AccessMode = void>
class ThreadLocal
{
private:
    ThreadLocalPtr<T,AccessMode> tlp_;
    std::function<T*()> construct_;

    T* makeTlp()
    {
        T* ptr = construct_();
        tlp_.reset(ptr);
        return ptr;
    }

public:

    ThreadLocal() : construct_([](){ return new T(); }) ,tlp_() {}
    
    ~ThreadLocal() = default;
    
    T* get()
    {
        T* ptr = tlp_.get();
        return (!!ptr) ? ptr : makeTlp(); 
    }

    typedef typename ThreadLocalPtr<T,AccessMode>::Accessor Accessor;

    Accessor accessAllThreads() const
    {
       return tlp_.accessAllThreads(); 
    }

};




template<typename T,typename AccessMode>
class ThreadLocalPtr
{
private:
    typedef StaticMeta Meta;
    mutable typename Meta::EntryID id_;

public:
    ThreadLocalPtr() : id_() {}
    ~ThreadLocalPtr()
    {
        destory();
    }

    ThreadLocalPtr& operator=(ThreadLocalPtr&& ptr);
    
    T* get() const
    {
        Element& element = Meta::get(&id_);
        return static_cast<T*>(element.ptr_);
    }

    void reset(T* newPtr = nullptr)
    {
        Element& element = Meta::get(&id_);
        element.dispose();
        element.cleanup();
        element.set(newPtr);
    }

    // T* release()
    // {

    // }

   
    

    struct Accessor
    {
        StaticMetaBase& meta;
        std::mutex& mutex;
        uint32_t id;

        struct Iterator
        {
            ThreadEntryNode* node_;
            StaticMetaBase& meta;
            uint32_t id_;
            
            void increment()
            {
                node_ = node_->getNext();
                increUntilValid();
            }
            
            void decrement()
            {
                node_ = node_->getPrev();
                decreUntilValid();
            }

            void increUntilValid()
            {
                for (; node_ != &meta.head_.elements_[id_].node_ && !valid() ; node_ = node_->getNext())
                {
                }
            }

            void decreUntilValid()
            {
                for (; node_ != &meta.head_.elements_[id_].node_ && !valid() ; node_ = node_->getPrev())
                {
                }        
            }

            bool valid() const
            {
                return (node_->getThreadEntry()->elements_[id_].ptr_ != nullptr);
            }

            bool equal(const Iterator& it) const
            {
                return (it.id_ == id_ && it.node_ == node_ );
            }

            T& dereference()
            {
                return *static_cast<T*>(node_->getThreadEntry()->elements_[id_].ptr_);
            }

            const T& dereference() const
            {
                return *static_cast<T*>(node_->getThreadEntry()->elements_[id_].ptr_);
            }

            explicit Iterator(const Accessor* accessor)
             : meta(accessor->meta),
               id_(accessor->id),
               node_(&accessor->meta.head_.elements_[accessor->id].node_)
            {

            }
            ~Iterator() = default;
        
            public:
                using value_type = T;    
                using difference_type = ssize_t;
                using reference = T&;
                using pointer = T*;
                using const_reference = const T&;
                using const_pointer = const T*;
        
                Iterator& operator++()
                {
                    increment();
                    return *this;
                }

                Iterator& operator++(int)
                {
                    Iterator copy(*this);
                    increment();
                    return *this;
                }


                Iterator& operator--()
                {
                    decrement();
                    return *this;
                }

                Iterator& operator--(int)
                {
                    Iterator copy(*this);
                    decrement();
                    return *this;
                }

                T& operator*()
                {
                    return dereference();
                }

                const T& operator*() const
                {
                    return dereference();
                }

                T* operator->()
                {
                    return &dereference();
                }

                const T* operator->() const
                {
                    return &dereference();
                }

                bool operator==(const Iterator& it) const
                {
                    return equal(it);
                }

                bool operator!=(const Iterator& it) const
                {
                    return !equal(it);
                }
        };

        
        explicit Accessor(uint32_t idval)
         : meta(StaticMeta::instance()),
           mutex(meta.mutex_),
           id(idval)
        {
            mutex.lock();
        }
        ~Accessor()
        {
            release();
        }
    
        void release()
        {
            mutex.unlock();
        }
    
        Iterator begin() const
        {
            return ++Iterator(this);
        }
    
        Iterator end() const
        {
            return Iterator(this);
        }
    };

    Accessor accessAllThreads() const
    {
        return Accessor(id_.getOrAllocate(&StaticMeta::instance()));
    }

private:
    void destory()
    {
        Meta::instance().destroy(&id_);
    }

};

