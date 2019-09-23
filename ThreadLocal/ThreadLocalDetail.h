#pragma once

#include<pthread.h>
#include<assert.h>

#include<atomic>
#include<mutex>
#include<functional>
#include<string>
#include<vector>
#include<limits>

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

constexpr uint32_t InvalidEntryId = std::numeric_limits<uint32_t>::max();

class ThreadEntry;

class ThreadEntryNode
{
public:

    ThreadEntry* parent_;    
    ThreadEntry* next_;
    ThreadEntry* prev_;
    uint32_t id_;
public:
    
    // invoked when reserve ThreadEntry and head
    //init node in threadentry
    void initZero(ThreadEntry* t,uint32_t id)
    {
        assert(t && id != InvalidEntryId);
        parent_ = t;
        id_ = id;
        prev_ = next_ = nullptr;
    }
    //invoked to init the node onm head
    void init(ThreadEntry* t,uint32_t id)
    {
        assert(t && id != InvalidEntryId);
        id_ = id;
        parent_ = prev_ = next_ = t;
    }

    void initIfZero();
   
    void eraseZero();

    ThreadEntryNode* getNext();
    
    ThreadEntryNode* getPrev();

    // push the node , head is &head_ in StaticMeta
    void push_back(ThreadEntry* head);

    ThreadEntry* getThreadEntry()
    {
        return parent_;
    }
    
    bool empty() const
    {
        return (next_ == prev_);
    }

};



//TODO add usr's own deleter
struct Element
{
    using DeleteType = void(void* ptr);
    //called after StaticMeta::get
    //TODO
    template<typename Ptr>
    void set(Ptr p)
    {
        if(p)
        {
            //add node to the double list
            node_.initIfZero();
            ptr_ = static_cast<void*>(p);
            deleter_ = [](void* ptr){ delete static_cast<Ptr>(ptr); };
        
        }
    }

    bool dispose()
    {
        if(ptr_ == nullptr)
            return false;
        (*deleter_)(ptr_);
        cleanup();
        return true;
    }

    void cleanup()
    {
        ptr_ = nullptr;
        deleter_ = nullptr;
    }


    ThreadEntryNode node_;
    void* ptr_;
    DeleteType* deleter_;
};

class StaticMetaBase;

struct ThreadEntry
{
    ThreadEntry* next_{nullptr};
    ThreadEntry* prev_{nullptr};
    Element* elements_{nullptr};
    std::atomic<size_t> capacity_{};
    StaticMetaBase* meta{nullptr};

    size_t getElementCapacity() const
    {
        return capacity_.load(std::memory_order_relaxed);
    }
    
    void setElementCapacity(size_t capacity)
    {
        capacity_.store(capacity,std::memory_order_relaxed);
    }

    

};




class StaticMetaBase
{
public:

    struct EntryID
    {
        std::atomic<uint32_t> value_;

        EntryID() : value_(InvalidEntryId) {}

        ~EntryID() = default;
        //XXX relax ok?
        uint32_t getID() const
        {
            return value_.load(std::memory_order_acquire);
        }

        uint32_t getOrAllocate(StaticMetaBase* meta)
        {
            uint32_t idval = getID();
            if(idval == InvalidEntryId)
            {
                return meta->allocate(this);
            }
            
            return idval;
        }
    
    };

    StaticMetaBase(const std::function<ThreadEntry*()>& threadEntry);
    ~StaticMetaBase()
    {
        free(head_.elements_);
    }

    // Element* reallocate(uint32_t id);  

    // void reserve(uint32_t id);

    //add t to the list
    void push_back(ThreadEntry* t)
    {
        t->next_ = &head_;
        t->prev_ = head_.prev_;

        head_.prev_->next_ = t;
        head_.prev_ = t;
    }

    void erase(ThreadEntry* t)
    {
        t->next_->prev_ = t->prev_;
        t->prev_->next_ = t->next_;
        t->next_ = t->prev_ = nullptr;
    }
    
    uint32_t allocate(EntryID* entry);

    static void onThreadExit(void*);

    void pushBack(ThreadEntry* threadEntry,uint32_t id);
    
    void reserveHead(uint32_t idval);

    void reserve(ThreadEntry* t,uint32_t id);

    void destroy(EntryID* ent);

    Element* reallocate(ThreadEntry* t,uint32_t idval,size_t& newcapacity);  
    
    std::mutex mutex_;
    std::atomic<uint32_t> nextId_;
    std::function<ThreadEntry*()> threadEntry_;
    ThreadEntry head_;
    pthread_key_t key_; //to manage the ThreadEntry per thread
};


class StaticMeta final : public StaticMetaBase
{
private:
 
public:
    StaticMeta() : StaticMetaBase(&StaticMeta::getThreadEntrySlow) {}
    ~StaticMeta() = default;

    static StaticMeta& instance()
    {
        static StaticMeta meta;
        return meta;
    }

    static Element& get(EntryID* ent)
    {
        StaticMeta& meta = StaticMeta::instance();
        static __thread ThreadEntry* threadEntry{};
        static __thread size_t capacity{};
        uint32_t idval = ent->getID();
        if(UNLIKELY(idval >= capacity))
        {
            meta.reserveAndCache(threadEntry,capacity,ent,idval);
        }
        idval = ent->getID();
        return threadEntry->elements_[idval];
    }

    void reserveAndCache(ThreadEntry*& threadEntry,size_t& capacity,EntryID* ent,uint32_t& id)
    {
        StaticMeta& meta = StaticMeta::instance();
        threadEntry = meta.threadEntry_();
        if(LIKELY(id == InvalidEntryId))
        {
            //get an id and reserve head_
            ent->getOrAllocate(&meta);
            //update id
            id = ent->getID();
        }
        assert(id != InvalidEntryId);
        capacity = threadEntry->getElementCapacity();
        //reserve enough space for threadEntry
        if(UNLIKELY(id >= capacity))
        {
            meta.reserve(threadEntry,id);
        }
    }

    static ThreadEntry* getThreadEntrySlow() // get ThreadEntry per thread , bind threadEntry_
    {
        StaticMeta& meta = StaticMeta::instance();
        ThreadEntry* entry = static_cast<ThreadEntry*>(pthread_getspecific(meta.key_));
        //set ThreadEntry and push it to the double list
        if(entry == nullptr)
        {
            static __thread ThreadEntry threadEntry;
            entry = &threadEntry;
            pthread_setspecific(meta.key_,entry);
            entry->meta = &meta;
            meta.push_back(entry);
            
        }
        return entry;
    }
};

