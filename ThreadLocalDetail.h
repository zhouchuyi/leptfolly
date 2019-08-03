#pragma once

#include<pthread.h>
#include<assert.h>

#include<atomic>
#include<mutex>
#include<functional>
#include<string>
#include<vector>
#include<limits>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)


constexpr uint32_t kEntryIDINvalid = std::numeric_limits<uint32_t>::max();

enum class DestructionMode {THIS_THREAD, ALL_THREAD};

struct AccessModeStrict {};

struct ThreadEntry;

struct ThreadEntryNode
{
    uint32_t id;
    ThreadEntry* parent;
    ThreadEntry* prev;
    ThreadEntry* next;

    void initIfZero(bool locked);

    void init(ThreadEntry* entry,uint32_t newId)
    {
        next = prev = parent = entry;
        id = newId;
    }

    void initZero(ThreadEntry* entry,uint32_t newId)
    {
        parent = entry;
        prev = next = nullptr;
        id = newId;
    }

    bool empty() const
    {
        return (next == parent);
    }

    ThreadEntry* getParentEntry() 
    {
        return parent;
    
    }

    inline __attribute__((__always_inline__)) ThreadEntryNode* getPrev();

    inline __attribute__((__always_inline__)) ThreadEntryNode* getNext();

    // add *this between head and head->prev
    void push_back(ThreadEntry* head);
    
    void eraseZero();

};

struct ElementWrapper
{
    using DeleteFunctype = void(void*,DestructionMode);
    
    bool dispose(DestructionMode mode) 
    {
        if(ptr == nullptr)
            return false;
        ownsDeleter ?  (*delete2)(ptr,mode) : (*delete1)(ptr,mode) ;
    }

    void* release()
    {
        void* resptr = ptr;
        if(ptr != nullptr)
            cleanup();
        return resptr;
    }   

    template<typename Ptr>
    void set(Ptr p)
    {
        if(p)
        {
            ptr = p;
            node.initIfZero(true);
            delete1 = [](void* p_,DestructionMode){
                    delete static_cast<Ptr>(p_);
                                                    };
        
            ownsDeleter = false;
        }
    }

    template<typename Ptr>
    void set(Ptr p,const std::function<DeleteFunctype>& d)
    {
        if(p)
        {
            ptr = p;
            node.initIfZero(true);
            delete2 = new std::function<DeleteFunctype>(d);
            ownsDeleter = true;
        }
    }

    void cleanup()
    {
        if(ownsDeleter)
            delete delete2;
        ptr = nullptr;
        delete1 = nullptr;
        ownsDeleter = false;
    }

    void* ptr;
    ThreadEntryNode node;
    bool ownsDeleter;
    union 
    {
        DeleteFunctype* delete1;
        std::function<DeleteFunctype>* delete2;
    };
    
};


struct StaticMetaBase;
struct ThreadEntryList;


struct ThreadEntry
{
    ElementWrapper* element{nullptr};
    std::atomic<size_t> elementsCapacity{0};
    ThreadEntry* next{nullptr};
    ThreadEntry* prev{nullptr};
    ThreadEntryList* list{nullptr};
    ThreadEntry* listNext{nullptr};
    StaticMetaBase* meta{nullptr};
    bool removed_{false};

    size_t getElementsCapacity() const noexcept
    {
        return elementsCapacity.load(std::memory_order_relaxed);        
    }

    void setElementCapacity(size_t capacity) noexcept
    {
        elementsCapacity.store(capacity,std::memory_order_acquire);
    }
};


struct ThreadEntryList
{
    ThreadEntry* head{nullptr};
    size_t count{0};
};

inline __attribute__((__always_inline__)) ThreadEntryNode* ThreadEntryNode::getPrev()
{
    return &prev->element[id].node;
}

inline __attribute__((__always_inline__)) ThreadEntryNode* ThreadEntryNode::getNext()
{
    return &next->element[id].node;
}

class PthreadKeyUnregister
{
private:
    constexpr static size_t kMaxSize = 1UL<<16;
    std::mutex mutex_;
    size_t size_;
    pthread_key_t keys_[kMaxSize];

    static PthreadKeyUnregister instance_;

    void registerKeyImpl(pthread_key_t key)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        keys_[size_++] = key;
    }

    constexpr PthreadKeyUnregister() :mutex_(),size_(0),keys_() {}

public:
    
    ~PthreadKeyUnregister()
    {
        std::lock_guard<std::mutex> lk(mutex_);
        while (size_)
        {
            pthread_key_delete(keys_[--size_]);
        }
    }

    static void registerKey(pthread_key_t key)
    {
        if(key >= kMaxSize)
            throw std::logic_error("pthread_key limit has already been reached");
        instance_.registerKeyImpl(key);
    }


};





struct StaticMetaBase
{
    using getThreadEntry = std::function<ThreadEntry*()>;
    
    class EntryID
    {
      public:

        std::atomic<uint32_t> value_;
        EntryID() : value_(kEntryIDINvalid) {}
        ~EntryID() = default;

        EntryID(EntryID&& gonner) : value_(gonner.value_.load())
        {
            gonner.value_ = kEntryIDINvalid;
        }
        // noncopyable
        EntryID(const EntryID&) = delete;
        EntryID& operator=(const EntryID&) = delete;
    
        uint32_t getOrInvalid() 
        {
            return value_.load(std::memory_order_acquire);
        }

        uint32_t getOrAllocate(StaticMetaBase& meta)
        {
            uint32_t id = getOrInvalid();
            if(id != kEntryIDINvalid)
                return id;
            return meta.allocate(this);
        } 

    };

    StaticMetaBase(const getThreadEntry& threadEntry,bool strict);    
    ~StaticMetaBase()
    {
        std::terminate();
    }
    // add t between head_ and head_.prev
    void push_back(ThreadEntry* t)
    {
        t->next = &head_;
        t->prev = head_.prev;
        head_.prev->next = t;
        head_.prev = t;
    }
    
    void erase(ThreadEntry* t)
    {
        t->prev->next = t->next;
        t->next->prev = t->prev;
        t->next = t->prev = t;
    }
    
    static ThreadEntryList* getThreadEntryList();

    static bool dying();

    static void onThreadExit(void* ptr);

    uint32_t elementsCapacity() const;

    uint32_t allocate(EntryID* ent);

    void destroy(EntryID* id);

    void reserve(EntryID* id);

    ElementWrapper& getElement(EntryID* id);

    void reserveHeadUnlocked(uint32_t id);

    void pushBackLocked(ThreadEntry* t,uint32_t id);
    void pushBackUnLocked(ThreadEntry* t,uint32_t id);

    static ElementWrapper*
    reallocate(ThreadEntry* threadEntry, uint32_t idval,size_t& newCapacity);

    uint32_t nextId_;
    std::vector<uint32_t> freeIds_;
    std::mutex mutex_;
    pthread_key_t key_;
    ThreadEntry head_;
    getThreadEntry threadEntry_;
    bool strict_;

};


template<typename Tag,typename AccessMode>
class StaticMeta final : StaticMetaBase
{
    StaticMeta() : StaticMetaBase(&StaticMeta::getThreadEntrySlow,
                                std::is_same<AccessMode,AccessModeStrict>::value)
    {
        
    }

    static StaticMeta<Tag,AccessMode>& instance()
    {
        static StaticMeta<Tag,AccessMode> meta;
        return meta;
    }

    static ElementWrapper& get(EntryID* ent)
    {
        uint32_t id = ent->getOrInvalid();
        static __thread size_t capacity{};
        static __thread ThreadEntry* threadEntry{};
        if(UNLIKELY(capacity <= id))
        
            getSlowReserveAndCache(ent,id,threadEntry,capacity);
    
        return threadEntry->element[id];
    }

    static void getSlowReserveAndCache(EntryID* ent,
                                       uint32_t& id,
                                       ThreadEntry*& threadEntry,
                                       size_t& capacity)
                                
    {
        auto& inst = instance();
        threadEntry = inst.threadEntry_();
        if(UNLIKELY(threadEntry->getElementsCapacity() <= id))
        {
            inst.reserve(ent);
            id = ent->getOrInvalid();
        }   
        capacity = threadEntry->getElementsCapacity();
        
    }
    

    static ThreadEntry* getThreadEntrySlow()
    {
        auto& inst = instance();
        pthread_key_t key = inst.key_;
        ThreadEntry* threadEntry = static_cast<ThreadEntry*>(pthread_getspecific(key));
        if(threadEntry == nullptr)
        {
            ThreadEntryList *list = StaticMeta::getThreadEntryList();
            static __thread ThreadEntry ThreadEntrySingLeton;
            threadEntry = &ThreadEntrySingLeton;
            
            assert(threadEntry->list == nullptr);
            assert(threadEntry->listNext == nullptr);

            threadEntry->list = list;
            threadEntry->listNext = list->head;
            list->head = threadEntry;

            list->count++;
            threadEntry->meta = &inst;
            pthread_setspecific(key,threadEntry);

        }

        return threadEntry;
    }


};


