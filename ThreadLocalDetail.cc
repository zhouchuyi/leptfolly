#include"ThreadLocalDetail.h"
#include<cstring>

constexpr static double kSmallGrowthFactor = 1.1;
constexpr static double kBigGrowthFactor = 1.7;

void ThreadEntryNode::initIfZero(bool locked)
{
    if(UNLIKELY(!next))
    {
        if(locked)
            parent->meta->pushBackLocked(parent,id);
        else
            parent->meta->pushBackUnLocked(parent,id);
    }

}


void ThreadEntryNode::push_back(ThreadEntry* head)
{
    ThreadEntryNode* hnode = &head->element[id].node;
    ThreadEntryNode* pnode = &hnode->prev->element[id].node;
    //current
    next = head;
    prev = pnode->parent;
    
    hnode->prev = parent;
    pnode->next = parent;
}

void ThreadEntryNode::eraseZero()
{
    ThreadEntryNode* nnode = &next->element[id].node;
    ThreadEntryNode* pnode = &prev->element[id].node;

    nnode->prev = prev;
    pnode->next = next;

    prev = next =nullptr;
}

StaticMetaBase::StaticMetaBase(const getThreadEntry& threadEntry,bool strict)
    : strict_(strict),
      mutex_(),
      nextId_(1),
      freeIds_(),
      threadEntry_(threadEntry)
{
    head_.next = head_.prev = &head_;
    pthread_key_create(&key_,&onThreadExit);
    
}

ThreadEntryList* StaticMetaBase::getThreadEntryList()
{
    static __thread ThreadEntryList threadEntrylist;
    return &threadEntrylist;
}

bool StaticMetaBase::dying()
{
    for (auto i = getThreadEntryList()->head; i; i = i->listNext)
    {
        if(i->removed_)
            return true;
    }
    return false;    
}

uint32_t StaticMetaBase::elementsCapacity() const
{
    ThreadEntry* threadEntry = threadEntry_();
    return threadEntry->getElementsCapacity();
}

uint32_t StaticMetaBase::allocate(EntryID* ent)
{
    uint32_t id;
    std::lock_guard<std::mutex> lk(mutex_);
    
    id = ent->value_.load();
    if(id != kEntryIDINvalid)
        return id;

    if(!this->freeIds_.empty())
    {
        id = freeIds_.back();
        freeIds_.pop_back();
    }
    else
    {
        id = nextId_++;
    }
    uint32_t old = ent->value_.exchange(id);
    reserveHeadUnlocked(id);
    return id;
}

ElementWrapper* StaticMetaBase::reallocate(ThreadEntry* threadEntry, uint32_t idval,size_t& newCapacity)
{
    size_t smallCapacity = static_cast<size_t>((idval +5) * kSmallGrowthFactor);
    size_t bigCapacity = static_cast<size_t>((idval + 5) * kBigGrowthFactor);
    newCapacity = (threadEntry->meta && (bigCapacity <= threadEntry->meta->head_.getElementsCapacity())) ? bigCapacity : smallCapacity;
    ElementWrapper* reallocated = static_cast<ElementWrapper*>(calloc(newCapacity,sizeof(ElementWrapper)));
    return reallocated;
}

//reserve enough space for head_
void StaticMetaBase::reserveHeadUnlocked(uint32_t id)
{
    size_t prevCapacity = head_.getElementsCapacity();
    if(prevCapacity <= id)
    {
        size_t newCapacity;
        ElementWrapper* newElement = reallocate(&head_,id,newCapacity);
        if(newElement == nullptr)
            throw std::bad_alloc();
        if(prevCapacity)
            std::memcpy(newElement,head_.element,sizeof(ElementWrapper) * prevCapacity);
        std::swap(head_.element,newElement);

        for (size_t i = id; i < newCapacity; i++)
        {
            head_.element[id].node.init(&head_,i);
        }
        free(newElement);
    }
}

//reserve enough space for ThreadEntry::element
void StaticMetaBase::reserve(EntryID* id)
{
    uint32_t idval = id->getOrAllocate(*this);
    ThreadEntry* threadEntry = threadEntry_();
    size_t prevCapacity = threadEntry->getElementsCapacity();
    if(prevCapacity > idval)
        return;
    size_t newCapacity;
    ElementWrapper* newElement = reallocate(threadEntry,idval,newCapacity);
    {
        std::lock_guard<std::mutex> lk(mutex_);

        if(prevCapacity == 0)
            this->push_back(threadEntry);
        if(newElement)
        {
            if(prevCapacity)
                memcpy(newElement,threadEntry->element,sizeof(ElementWrapper) * prevCapacity);
            std::swap(newElement,threadEntry->element);
        
        }
        for (size_t i = idval; i < newCapacity; i++)
        {
            threadEntry->element[i].node.init(threadEntry,i);
        }
        
        threadEntry->setElementCapacity(newCapacity);
    }

    free(newElement);

}

void StaticMetaBase::pushBackLocked(ThreadEntry* t,uint32_t id)
{
    if(!t->removed_)
    {
        std::lock_guard<std::mutex> lk(mutex_);
        t->element[id].node.push_back(&head_);
    }
}

void StaticMetaBase::pushBackUnLocked(ThreadEntry* t,uint32_t id)
{
    if(!t->removed_)
    t->element[id].node.push_back(&head_);
}
