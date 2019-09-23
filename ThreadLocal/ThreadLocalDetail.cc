#include<string.h>

#include"ThreadLocalDetail.h"

//the factor when reserve the space of elements
constexpr auto GrowthFactor = 1.5;

//invoked by Element::set
//add node to the double list
void ThreadEntryNode::initIfZero()
{
    if(next_ == nullptr)
    {
        assert(id_ != InvalidEntryId);
        parent_->meta->pushBack(parent_,id_);   
    }
}

void ThreadEntryNode::push_back(ThreadEntry* head)
{
    auto& nnode = head->elements_[id_].node_;
    auto& pnode = (*nnode.getPrev());
    next_ = head;
    prev_ = pnode.parent_;

    nnode.prev_ = parent_;
    pnode.next_ = parent_;
}

ThreadEntryNode* ThreadEntryNode::getNext()
{
    assert(next_ && id_ != InvalidEntryId);
    return &next_->elements_[id_].node_;
}

ThreadEntryNode* ThreadEntryNode::getPrev()
{
    assert(prev_ && id_ != InvalidEntryId);
    return &prev_->elements_[id_].node_;
}

//remove node from the double list
void ThreadEntryNode::eraseZero()
{
    if(prev_ == nullptr)
        return;
    auto& pnode = (*getPrev());
    auto& nnode = (*getNext());

    pnode.next_ = nnode.parent_;
    nnode.prev_ = pnode.parent_;

    next_ = prev_ = nullptr;
}



StaticMetaBase::StaticMetaBase(const std::function<ThreadEntry*()>& threadEntry)
 : threadEntry_(threadEntry),
   mutex_(),
   head_(),
   nextId_(1)
   {
       head_.prev_ = head_.next_ = &head_;
        pthread_key_create(&key_,&StaticMetaBase::onThreadExit);
   }



//give an id to entry
uint32_t StaticMetaBase::allocate(EntryID* entry)
{
    //init value_
    ThreadEntry* t = threadEntry_();
    uint32_t id = nextId_++;
    uint32_t oldidval = entry->value_.exchange(id);
    assert(oldidval == InvalidEntryId);
    //reserve head_
    std::lock_guard<std::mutex> lock(mutex_);
    reserveHead(id);
    return id;
}


//TODO ensure thread-safe
//invoked by StaticMetaBase::allocate
void StaticMetaBase::reserveHead(uint32_t idval)
{
    if(idval < head_.getElementCapacity())
        return;
    size_t prevcapacity = head_.getElementCapacity();
    size_t newcapacity;
    Element* reallocated = reallocate(&head_,idval,newcapacity);
    assert(reallocated);
    if(prevcapacity != 0)
    {
        memcpy(reallocated,head_.elements_,sizeof(Element) * prevcapacity);
    }
    std::swap(head_.elements_,reallocated);
    for (size_t i = prevcapacity; i < newcapacity; i++)
    {
        head_.elements_[i].node_.init(&head_,i);
    }
    
    head_.setElementCapacity(newcapacity);
    free(reallocated);
}

void StaticMetaBase::reserve(ThreadEntry* t,uint32_t idval)
{
    size_t prevcapacity = t->getElementCapacity();
    size_t newcapacity;
    if(prevcapacity > idval)
        return;
    Element* reallocated = reallocate(t,idval,newcapacity);
    assert(reallocated);
    if(prevcapacity != 0)
    {
        memcpy(reallocated,t->elements_,sizeof(Element) * prevcapacity);
    }
    std::swap(t->elements_,reallocated);
    for (size_t i = prevcapacity; i < newcapacity; i++)
    {
        t->elements_[i].node_.initZero(t,i);
    }
    
    t->setElementCapacity(newcapacity);
    free(reallocated);
}

Element* StaticMetaBase::reallocate(ThreadEntry* t,uint32_t idval,size_t& newcapacity)
{
    newcapacity = (idval + 5) * GrowthFactor;
    Element* reallocated = static_cast<Element*>(calloc(newcapacity,sizeof(Element)));
    if(reallocated == nullptr)
    {
        throw std::bad_alloc();
    }
    return reallocated;
}

void StaticMetaBase::pushBack(ThreadEntry* threadEntry,uint32_t id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& node = threadEntry->elements_[id].node_;
    node.push_back(&head_);
}


void StaticMetaBase::onThreadExit(void* ptr)
{
    ThreadEntry* threadEntry = static_cast<ThreadEntry*>(ptr);
    auto& meta = StaticMeta::instance();
    size_t capacity = threadEntry->getElementCapacity();
    {
        std::lock_guard<std::mutex> lock(meta.mutex_);
        meta.erase(threadEntry);
        for (size_t i = 0; i < capacity; i++)
        {
            threadEntry->elements_[i].node_.eraseZero();
        }
    }    

    for (size_t i = 0; i < capacity; i++)
    {
        threadEntry->elements_[i].dispose();
    }
    pthread_setspecific(meta.key_,nullptr);
    free(threadEntry->elements_);

}

void StaticMetaBase::destroy(EntryID* ent)
{

    uint32_t idval = ent->getID();
    if(idval == InvalidEntryId)
        return;
    std::vector<Element> elements;

    {
        auto& node = head_.elements_[idval].node_;
        std::lock_guard<std::mutex> lock(mutex_);
        while(!node.empty())
        {
            auto& nnode = (*node.getNext());
            nnode.eraseZero();
            elements.push_back(nnode.parent_->elements_[idval]);
        }
                
    }

    for (auto & element : elements)
    {
        element.dispose();
    }
            
}