#ifndef INDEX_MEM_POOL_H_
#define INDEX_MEX_POOL_H_

#include<assert.h>
#include<stddef.h>
#include<type_traits>
#include<utility>
#include<atomic>
#include<functional>
#include<memory>
#include<set>
#include<mutex>

template<typename Pool>
struct IndexMemPoolRecycle;
template<typename T,
        bool EagerRecycleWhenTrival=false,
        bool EagerRecycleWhenNotTrival=true>
struct IndexMemPoolTraits
{
    static constexpr bool eagerRecycle()
    {
        return std::is_trivial<T>::value ? EagerRecycleWhenTrival : EagerRecycleWhenNotTrival;
    }

    static void initial(T* ptr)
    {
        new (ptr) T(); 
    }

    static void cleanup(T* ptr)
    {
        ptr->~T();
    }

    template<typename... Args>
    static void onAllocate(T* ptr,Args&&... args)
    {
        if(eagerRecycle())
        new (ptr) T(std::forward<Args>(args)...);
    }

    static void onRecycle(T* ptr)
    {
        if(eagerRecycle())
        ptr->~T();
    }
};

template<typename T>
using IndexMemPoolTraitsLazyRecycle=IndexMemPoolTraits<T,false,false>;
template<typename T>
using IndexMemPoolTraitsEagerRecycle=IndexMemPoolTraits<T,true,true>;

template<typename T,typename traits=IndexMemPoolTraits<T>>
class IndexMemPool
{
 private:
    typedef std::shared_ptr<std::set<uint32_t>> SetPtr;
    typedef std::unique_ptr<T,IndexMemPoolRecycle<IndexMemPool>> UniquePtr;   
 public:
    //noncopyable
    typedef T value_type;

    IndexMemPool(IndexMemPool&)=delete;
    IndexMemPool& operator=(const IndexMemPool&)=delete;

    explicit IndexMemPool(uint32_t capacity)
    :size_(0),
    capacity_(capacity),
    recycleIndexSet_(new std::set<uint32_t>()),
    mutex_()
    {
        
    }

    template<typename... Args>
    uint32_t allocIndex(Args&&... args)
    {
        uint32_t idx=idxpop();
        // fail if return 0
        if(idx!=0)
        {
            Slot& slot=slot_[idx];
            traits::onAllocate(&slot.elem,std::forward<Args>(args)...);
        }
        return idx;
    }
    void recycleIndex(uint32_t idx)
    {
        idxpush(idx);
    }

    uint32_t locatElem(const T* elem)
    {
        const Slot* slot=reinterpret_cast<const Slot*>(reinterpret_cast<const char*>(elem)-offsetof(Slot,elem));
        uint32_t rv=slot-slot_;
        assert(&(*this)[rv]==elem);
        return rv;
    }

    T& operator[](uint32_t idx)
    {
        return slot(idx).elem;   
    }
 
    const T& operator[](uint32_t idx)const
    {
        return slot(idx).elem;
    }
 
    
 
 private:
    struct Slot
    {
        T elem;
    };

    size_t mmapLength_;

    uint32_t capacity_;

    std::atomic<uint32_t> size_;

    SetPtr recycleIndexSet_;

    std::mutex mutex_; //protect ptr

    alignas(64) Slot* slot_;
    //use when recycle idx 
    void idxpush(uint32_t idx)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        // if(!recycleIndexSet_.unique())
        // {
        //     recycleIndexSet_.reset(new std::set<uint32_t>(*recycleIndexSet_));
        // }
        auto res=recycleIndexSet_->insert(idx);
        assert(res.second==true);
        traits::onRecycle(&slot(idx).elem);
    }
    //use when alloc
    uint32_t idxpop()
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if(recycleIndexSet_->empty())
            return ++size_;
        uint32_t idx=*recycleIndexSet_[0];
        size_t res=recycleIndexSet_->erase(*recycleIndexSet_->begin());
        assert(res==1);
        return idx;
    }

    Slot& slot(uint32_t idx)
    {
        assert(idx>0 && idx<=capacity_ && idx<=size_.load(std::memory_order_acquire));
        return slot_[idx];
    }
    const Slot& slot(uint32_t idx)const
    {
        assert(idx>0 && idx<=capacity_ && idx<=size_.load(std::memory_order_acquire));
        return slot_[idx];
    }
};

template<typename Pool>
struct IndexMemPoolRecycle
{
    Pool* pool_;
    IndexMemPoolRecycle(Pool* p);
    ~IndexMemPoolRecycle()=default;
    IndexMemPoolRecycle& operator=(const IndexMemPoolRecycle&)=default;
    void operator()(typename Pool::value_type* ele)const
    {
        pool_->recycleIndex(pool_->locateElem(ele));
    }

};












#endif