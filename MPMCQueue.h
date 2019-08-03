#ifndef MPMCQUEUE_H_
#define MPMCQUEUE_H_

#include"TurnSequncer.h"
#include<stdexcept>
#include<memory>
#include<stdlib.h>
#include<iostream>
constexpr std::size_t kCacheLineSize=128;

template<typename>
class MPMCQueueBase;






template<typename T>
struct SingleElementQueue
{
    ~SingleElementQueue() noexcept
    {
        //ele in content_
        if((sequncer_.uncompleteTurnLSB() & 1) == 1)
        {
            destroyContents();
        }
    }
    bool mayEnqueue(const uint32_t turn) const noexcept
    {
        return sequncer_.isTurn(turn * 2);
    }
    bool mayDequeue(const uint32_t turn) const noexcept
    {
        return sequncer_.isTurn(turn * 2 + 1);
    }
    template<typename... Args>
    void enqueue(const uint32_t turn,
                    std::atomic<uint32_t>& spinCutoff,
                    bool update,
                    Args&&... args)
    {
        sequncer_.waitForTurn(turn*2,spinCutoff,update);
        new (&contents_) T(std::forward<Args>(args)...);
        sequncer_.completeTurn(turn*2);
    }


    void enqueue(const uint32_t turn,
                    std::atomic<uint32_t>& spinCutoff,
                    bool update,
                    T&& gonner) noexcept
    {
        sequncer_.waitForTurn(turn*2,spinCutoff,update);
        new (&contents_) T(std::move(gonner));
        sequncer_.completeTurn(turn*2);
    }


    void dequeue(uint32_t turn,
                    std::atomic<uint32_t>& spinCutoff,
                    bool update,
                    T& elem) noexcept
    {    
        sequncer_.waitForTurn(turn*2+1,spinCutoff,update);
        elem=std::move(*ptr());
        destroyContents();
        sequncer_.completeTurn(turn*2+1);
    }
    T* ptr() noexcept
    {
        return static_cast<T*>(static_cast<void*>(&contents_));
    }
    
    void destroyContents() noexcept
    {
        try
        {
            ptr()->~T();
        }
        catch(...)
        {

        }
        
    }
    
    //prevent false shareing in neighbouring SingleElementQueue    
    alignas(kCacheLineSize) TurnSequncer sequncer_;
    std::aligned_storage<sizeof(T),alignof(T)> contents_;

};

template<
    template<typename T, bool Dynamic> class Derived,
    typename T,
    bool Dynamic>
class MPMCQueueBase<Derived<T,Dynamic>>
{
  public:

    typedef T value_type;
    
    using Slot=SingleElementQueue<T>;

    explicit MPMCQueueBase(size_t cap)
        : capacity_(cap),
          dstate_(0),
          dcapacity_(0),
          pushTicket_(0),
          popTicket_(0),
          popSpinCutoff_(0),
          pushSpinCutoff_(0)
    {
        //throw if cap==0
        if(capacity_ == 0)
            throw std::invalid_argument("MPMCQueue with explicit capacity 0 is impossible");

    }   

    MPMCQueueBase() noexcept
        : capacity_(0),
          dstate_(0),
          dcapacity_(0),
          pushTicket_(0),
          popTicket_(0),
          popSpinCutoff_(0),
          pushSpinCutoff_(0)
    {

    }
    MPMCQueueBase(MPMCQueueBase<Derived<T,Dynamic>>&& rhs)
        : capacity_(rhs.capacity_),
          slots_(rhs.slots_),
          dstate_(rhs.dstate_.load(std::memory_order_relaxed)),
          dcapacity_(rhs.dcapacity_.load(std::memory_order_relaxed)),
          pushTicket_(rhs.pushTicket_.load(std::memory_order_relaxed)),
          popTicket_(rhs.popTicket_.load(std::memory_order_relaxed)),
          popSpinCutoff_(rhs.popSpinCutoff_.load(std::memory_order_relaxed)),
          pushSpinCutoff_(rhs.pushSpinCutoff_.load(std::memory_order_relaxed))
    {
        rhs.capacity_ = 0;
        rhs.slots_ = nullptr;
        rhs.dstate_.store(0, std::memory_order_relaxed);
        rhs.dcapacity_.store(0, std::memory_order_relaxed);
        rhs.pushTicket_.store(0, std::memory_order_relaxed);
        rhs.popTicket_.store(0, std::memory_order_relaxed);
        rhs.pushSpinCutoff_.store(0, std::memory_order_relaxed);
        rhs.popSpinCutoff_.store(0, std::memory_order_relaxed);
    }

    MPMCQueueBase(const MPMCQueueBase&)=delete;
    MPMCQueueBase& operator=(const MPMCQueueBase&)=delete;
    ~MPMCQueueBase() noexcept
    {
        // delete [] slots_;
    }
    //elememt in queue + pending(calls to enqueue) - pending(calls to dequeue)
    ssize_t size() const noexcept
    {
        uint64_t pushes=pushTicket_.load(std::memory_order_acquire);//A
        uint64_t pops=popTicket_.load(std::memory_order_acquire);//B
        while (true)
        {
            uint64_t nextpushes=pushTicket_.load(std::memory_order_acquire);//C
            if(nextpushes == pushes)
                return ssize_t(pushes - pops);
            pushes=nextpushes;
            uint64_t nextpops=popTicket_.load(std::memory_order_acquire);//D
            if(nextpops == pops)
                return ssize_t(pushes - pops);
            pops=nextpops;
        }
        
    }

    uint64_t sizeGuess() const noexcept
    {
        return writeCount() - readCount();
    }

    uint64_t writeCount() const noexcept
    {
        return pushTicket_.load(std::memory_order_acquire);
    }

    uint64_t readCount() const noexcept
    {
        return popTicket_.load(std::memory_order_acquire);
    }

    bool isempty() const noexcept
    {
        return size() <= 0;
    }

    bool isFull() const noexcept
    {
        return size() >= static_cast<ssize_t>(capacity_);
    }

    size_t capacity() const noexcept
    {
        return capacity_;
    }
    // non-dynamic
    size_t allocatedCapacity() const noexcept
    {
        return capacity_;
    }

    template<typename... Args>
    void blockingWrite(Args&&... args) noexcept
    {
        enqueueWithTicketBase(pushTicket_++,slots_,capacity_,std::forward<Args>(args)...);
    }

    template<typename... Args>
    bool write(Args&&... args) noexcept
    {
        uint64_t ticket;
        Slot* slot;
        size_t cap;   
        if(static_cast<Derived<T,Dynamic>*>(this)->tryObtainReadyPushTicket(ticket,slot,cap))
        {
            enqueueWithTicketBase(ticket,slot,cap,std::forward<Args>(args)...);
            return true;
        }
        else
            return false;    
    }

    void blockingRead(T& elem) noexcept
    {
        uint64_t ticket;
        static_cast<Derived<T,Dynamic>*>(this)->blockingReadWithTicket(ticket,elem);
    }
    
    void blockingReadWithTicket(uint64_t& ticket,T& elem) noexcept
    {
        assert(capacity_ != 0);
        ticket = popTicket_++;
        dequeueWithTicketBase(ticket,slots_,capacity_,elem);

    }

    bool read(T& elem)
    {
        uint64_t ticket;
        return readAndGetTicket(ticket,elem);
    }
    bool readAndGetTicket(uint64_t& ticket,T& elem)
    {
        Slot* slot;
        size_t cap;
        if(static_cast<Derived<T,Dynamic>*>(this)->tryObtainReadyPopTicket(ticket,slot,cap))
        {
            dequeueWithTicketBase(ticket,slot,cap,elem);
            return true;
        }
        else
        {
            return false;
        }
    }
  protected:

    enum
    {
        kAdaptationFreq = 128,
    };

    alignas(kCacheLineSize) size_t capacity_;

    union 
    {
        Slot* slots_;
        //for dynamic queue
        std::atomic<Slot*> dslots_;
    };
    
    // for dynamic queue
    std::atomic<uint64_t> dstate_;

    std::atomic<size_t> dcapacity_;

    alignas(kCacheLineSize) std::atomic<uint64_t> pushTicket_;

    alignas(kCacheLineSize) std::atomic<uint64_t> popTicket_;

    alignas(kCacheLineSize) std::atomic<uint32_t> pushSpinCutoff_;

    alignas(kCacheLineSize) std::atomic<uint32_t> popSpinCutoff_;

    char pad[kCacheLineSize - sizeof(std::atomic<uint32_t>)];

    size_t idx(uint64_t ticket,size_t cap) noexcept
    {
        return ticket % cap;
    }

    uint32_t turn(uint64_t ticket,size_t cap)
    {
        return ticket / cap;
    }

    // try to get pushticket for singelementqueue won't bolck
    bool tryObtainReadyPushTicket(uint64_t& ticket,
                                 Slot*& slots,
                                 size_t& cap) noexcept
    {
        ticket=pushTicket_.load(std::memory_order_acquire);
        cap=capacity_;
        slots=slots_;
        while(true)
        {
            if(!slots[idx(ticket,cap)].mayEnqueue(turn(ticket,cap)))
            {
                uint64_t prev=ticket;
                ticket=pushTicket_.load(std::memory_order_acquire);
                if(prev == ticket)
                    return false; 
            }
            else
            {
                if(pushTicket_.compare_exchange_strong(ticket,ticket+1))
                    return true;
            }
        }
        
    }

    bool tryObtainPromisedPushTicket(uint64_t& ticket,
                                    Slot*& slots,
                                    size_t& cap) noexcept
    {
        auto numPushes = pushTicket_.load(std::memory_order_acquire);
        slots=slots_;
        cap=capacity_;
        while (true)
        {
            ticket=pushTicket_.load(std::memory_order_acquire);
            auto numPops = popTicket_.load(std::memory_order_acquire);
            numPushes=ticket;
            if( int64_t(numPushes - numPops) >= capacity_)        
                // Queue is full return false
                return false;
        
            else
            {
                if(pushTicket_.compare_exchange_strong(ticket,ticket+1))
                    return true;
            }
        }
        
    }

    bool tryObtainReadyPopTicket(uint64_t& ticket,
                                Slot*& slots,
                                size_t& cap) noexcept
    {
        ticket=pushTicket_.load(std::memory_order_acquire);
        slots=slots_;
        cap=capacity_;
        while (true)
        {   
            if(!slots[idx(ticket,cap)].mayDequeue(turn(ticket,cap)))
            {
                auto prev=ticket;
                ticket=pushTicket_.load(std::memory_order_acquire);
                if(prev == ticket)
                    return false;
            }
            else
            {
                if(pushTicket_.compare_exchange_strong(ticket,ticket + 1))
                    return true;
            }
            
        }
        
    }

    bool tryObatinPromistedPopTicket(uint64_t& ticket,
                                  Slot*& slots,
                                  size_t& cap) noexcept
    {
        auto numPops =popTicket_.load(std::memory_order_acquire);
        slots=slots_;
        cap=capacity_;
        while (true)
        {
            ticket=popTicket_.load(std::memory_order_acquire);
            auto numPushes=pushTicket_.load(std::memory_order_acquire);
            if( ticket >= numPushes )
            {
                //Queue is empty
                return false;
            }
            else
            {
                if(popTicket_.compare_exchange_strong(ticket,ticket+1))
                    return true;
            } 
        }
    }

    template<typename... Args>
    void enqueueWithTicketBase(uint64_t ticket,
                    Slot*& slots,
                    size_t& cap,
                    Args&&... args) noexcept
    {
        slots[idx(ticket,cap)].enqueue(turn(ticket,cap),
                                       pushSpinCutoff_,
                                       (ticket % kAdaptationFreq) == 0,
                                       std::forward<Args>(args)...);
    }

    void dequeueWithTicketBase(uint64_t ticket,
                    Slot*& slots,
                    size_t& cap,
                    T& elem) noexcept
    {
        slots[idx(ticket,cap)].dequeue(turn(ticket,cap),
                                       popSpinCutoff_,
                                       (ticket % kAdaptationFreq) == 0,
                                       elem);   
    }
};


template<typename T,bool Dynamic = false>
class MPMCQueue : public MPMCQueueBase<MPMCQueue<T,Dynamic>>
{
public:
    using Slot=SingleElementQueue<T>; 
    MPMCQueue(size_t capaticy) : MPMCQueueBase<MPMCQueue<T,Dynamic>>(capaticy)
    {
        size_t allocCap=capaticy * sizeof(Slot) + kCacheLineSize - 1;
        void* buf=std::malloc(allocCap);
        if(buf == nullptr)
            throw std::bad_alloc();
        buf_=buf;
        this->slots_=static_cast<Slot*>(std::align(kCacheLineSize,sizeof(Slot) * capaticy,buf,allocCap));
        if(this->slots_ == nullptr)
        {
            free(buf);
            throw std::bad_alloc();
        }
        for (size_t i = 0; i < capaticy; i++)
        {
            new (&this->slots_[i]) Slot();
        }
            
    }
    ~MPMCQueue() noexcept
    {
        free(buf_);
    }
private:
    void* buf_;
};


template<typename T>
class MPMCQueue<T,true> : public MPMCQueueBase<MPMCQueue<T,true>>
{
    // friend class MPMCQueueBase<MPMCQueue<T,true>>;
    using Slot=SingleElementQueue<T>;
    struct ClosedArray
    {
        uint64_t offset_{0};
        Slot* slots_{nullptr};
        void* buf_{nullptr};
        size_t capacity_{0};
    }; 
public:
    MPMCQueue(size_t queueCapacity) : MPMCQueueBase<MPMCQueue<T,true>>(queueCapacity),dbuf_(nullptr),closed_(nullptr)
    {
        assert(this->capacity_ == queueCapacity);
        assert(this->dstate_ == 0);
        size_t cap = std::min<size_t>(kDefaultMinDynamicCapacity,queueCapacity);
        initQueue(cap,kDefaultExpansionMultiplier);
    }

    ~MPMCQueue()
    {
        if(closed_ != nullptr)
        {
            // std::cout<<getNumClosed(this->dstate_.load())-1<<std::endl;
            for (int i = getNumClosed(this->dstate_.load()) - 1; i >= 0; i--)
            {
                for (size_t j = 0; j < closed_[i].capacity_; j++)
                {
                    closed_[i].slots_[j].~Slot();
                }
                free(closed_[i].buf_);
            }
            delete[] closed_;
        }
        for (size_t i = 0; i < this->dcapacity_; i++)
        {
            this->dslots_[i].~Slot();
        }
        free(this->dbuf_);
        // using Atomslots = std::atomic<Slot*>;
        // this->dslots_.~Atomslots();
    }
    // non-copyable
    MPMCQueue(const MPMCQueue&)=delete;
    MPMCQueue& operator=(const MPMCQueue&)=delete;

    size_t allocatedCapacity() const noexcept
    {
        return this->dcapacity_(std::memory_order_acquire);
    }
    template<typename... Args>
    void blockingWrite(Args&&... args)
    {
        uint64_t ticket=this->pushTicket_++;
        uint64_t state;
        uint64_t offset;
        Slot* slots;
        size_t cap;
        do
        {
            if(!trySeqlockReadSection(state,slots,cap))
            {
                ::_mm_pause();
                continue;
            }
            if(mayUpdateFromClosed(state,ticket,offset,slots,cap))
                break;
            if(slots[this->idx(ticket - offset,cap)].mayEnqueue(this->turn(ticket - offset,cap)))
                break;
            else if (this->popTicket_.load(std::memory_order_acquire) + cap > ticket)
            {
                continue;
            }
            else
            {
                if(tryExpand(state,cap))
                    continue;
                else
                    break;
            }
        } while (true);
        this->enqueueWithTicketBase(ticket - offset,slots,cap,std::forward<Args>(args)...);
    }
    void blockingReadWithTicket(uint64_t& ticket,T& elem) noexcept
    {
        ticket = this->popTicket_++;
        uint64_t state;
        Slot* slots;
        size_t cap;
        uint64_t offset;
        while (!trySeqlockReadSection(state,slots,cap))
        {
            ::_mm_pause();
        }
        mayUpdateFromClosed(state,ticket,offset,slots,cap);
        this->dequeueWithTicketBase(ticket - offset,slots,cap,elem);
    }
private:

    enum
    {
        kSeqlockBits = 6,
        kDefaultMinDynamicCapacity = 10,
        kDefaultExpansionMultiplier = 10,
    };

    size_t dmult_;

    ClosedArray* closed_;

    void* dbuf_;


    void initQueue(const size_t cap,const size_t mult)
    {
        size_t allocCap = cap * sizeof(Slot) + kCacheLineSize - 1;
        void* buf = malloc(allocCap);
        if(buf == nullptr)
            throw std::bad_alloc();
        dbuf_ = buf;
        Slot* slots = static_cast<Slot*>(std::align(kCacheLineSize,sizeof(Slot) * cap,buf,allocCap));
        if(slots == nullptr)
        {
            throw std::bad_alloc();
        }
        for (size_t i = 0; i < cap; i++)
        {
            new (&slots[i]) Slot();
        }
        this->dslots_.store(slots);
        // new (&this->dslots_) std::atomic<Slot*>(slots);
        this->dcapacity_.store(cap);
        dmult_ = mult;      
        size_t maxClosed = 0;
        for (size_t exp = cap; exp < this->capacity_; exp*=mult)
        {
            ++maxClosed;
        }
        closed_ = (maxClosed > 0) ? new ClosedArray [maxClosed] : nullptr;
    }

    // void tryObtainReadyPushTicket(uint64_t& ticket,
    //                               Slot*& slots,
    //                               size_t& cap) noexcept
    // {

    // }

    bool trySeqlockReadSection(uint64_t& state,
                               Slot*& slots,
                               size_t& cap) noexcept
    {
        state = this->dstate_.load(std::memory_order_acquire);
        if(state & 1)//has locked
            return false;
        //start read
        slots = this->dslots_.load(std::memory_order_relaxed);
        cap = this->dcapacity_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        return (state == this->dstate_.load(std::memory_order_relaxed));
    }

    uint64_t getOffset(const uint64_t state) const noexcept
    {
        return (state >> kSeqlockBits);
    }

    int getNumClosed(const uint64_t state) const noexcept
    {
        return (state & ((1 << kSeqlockBits) - 1)) >> 1;
    }

    // template<typename... Args>
    // void enqueueWithTicket(const uint64_t ticket, Args&&... args) noexcept
    // {

    // }

    bool tryExpand(const uint64_t state, const size_t cap) noexcept
    {
        if(cap == this->capacity_)
            return false;
        assert((state & 1) == 0);
        // lock
        uint64_t oldval=state;
        if(this->dstate_.compare_exchange_strong(oldval,state + 1))
        {
            assert(cap == this->dcapacity_.load());
            size_t newCapacity = std::min(this->capacity_,cap * this->dmult_);
            size_t newAllocsize = newCapacity * sizeof(Slot) + kCacheLineSize -1;
            void* buf = std::malloc(newAllocsize);
            if(buf == nullptr)
            {
                throw std::bad_alloc();
            }
            void* oldbuf = this->dbuf_;
            this->dbuf_ = buf;
            // after std::align,the buf would be invalid,so it should be saved before std::align 
            Slot* newSlots = static_cast<Slot*>(std::align(kCacheLineSize,sizeof(Slot) * newCapacity,buf,newAllocsize));
            if(newSlots == nullptr)
            {
                throw std::bad_alloc();
            }
            for (size_t i = 0; i < newCapacity; i++)
            {
                new (&newSlots[i]) Slot();
            }
            uint64_t ticket = 1 + std::max(this->pushTicket_.load(),this->popTicket_.load());
            uint64_t offset = getOffset(state);
            int index = getNumClosed(state);
            // assert((index << 1) < (1 << kSeqlockBits));
            // closed_[index].slots=static_cast<Slot*>(std::align(kCacheLineSize,sizeof(Slot)*newCapacity,buf,newAllocsize));
            closed_[index].buf_ = oldbuf;
            closed_[index].offset_ = offset;
            closed_[index].slots_ = this->dslots_.load();
            closed_[index].capacity_ = cap;
            this->dslots_.store(newSlots);
            this->dcapacity_.store(newCapacity);
            // this->dbuf_ = buf;
            this->dstate_.store((ticket << kSeqlockBits) + (2 * (index + 1)));
            return true;
        }
        else
        {
            // othre thread has locked 
            return true;
        }
        

    }

    bool mayUpdateFromClosed(const uint64_t state,
                            const uint64_t ticket,
                            uint64_t& offset,
                            Slot*& slots,
                            size_t& cap) noexcept
    {
        offset = getOffset(state);
        if(ticket >= offset)
            return false;
        for (int i = getNumClosed(state) - 1; i >= 0; i--)
        {
            offset = closed_[i].offset_;
            if(offset <= ticket)
            {
                slots = closed_[i].slots_;
                cap = closed_[i].capacity_;
                return true;
            }
        }
        assert(false);
    }






};









#endif