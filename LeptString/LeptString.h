#ifndef LEPT_STRING_H_
#define LEPT_STRING_H_

#include<atomic>
#include<iosfwd>
#include<stdexcept>
#include<type_traits>
#include<utility>
#include<iterator>
#include<assert.h>
#include<cstring>
#include<algorithm>
#include<cstddef>
namespace 
{
    template<class InIt,class OutIt>
    inline std::pair<InIt,OutIt> copy_n(
        InIt b,
        typename std::iterator_traits<InIt>::difference_type n,
        OutIt d)
    {
        for (;n!=0; b++,d++,n--)
        {
            *d=*b;
        }
        return std::make_pair(b,d);
    }

    template<class Pod,class T>
    inline void podFill(Pod* b,Pod* e,T c)
    {
        assert(b && e && b<=c);
        const auto useMemeset=sizeof(T)==1;
        if(useMemeset)
            std::memset(b,e,size_t(e-b));
        else
        {
            for (; b != e; b++)
            {
                *b=c;
            }
            
        }
    }

    template<typename Pod>
    inline void podCopy(const Pod*b,const Pod* e,Pod* d)
    {
        assert(b!=NULL);
        assert(e!=NULL);
        assert(d!=NULL);
        memcpy(d,b,(e-b)*sizeof(Pod));
    }

    template<typename Pod>
    inline void podMove(const Pod* b,const Pod* e,const Pod* d)
    {
        assert(e>=b);
        memmove(b,d,sizeof(Pod)*(e-b));
    }
    inline void* checkedmalloc(size_t n)
    {
        void* p=malloc(n);
        if(!p)
        {
            throw std::bad_alloc();
        }
        return p;
    }
    inline void* checkedRealloc(void* p,size_t n)
    {
        void* re=realloc(p,n);
        if(!re)
        {
            throw std::bad_alloc();
        }
        return re;
    }
    inline void* smartRealloc(void* p,const size_t currentSize,const size_t currentCapacity,const size_t newCapacity)
    {
        assert(p);
        assert(currentSize<=currentCapacity && currentCapacity< newCapacity);
        size_t slack=currentCapacity-currentSize;
        if(slack*2>currentSize)
        {
              void* re=checkedmalloc(newCapacity);
              std::memcpy(re,p,currentSize);
              free(p);
              return re;
        }
        else
        {
            return checkedRealloc(p,newCapacity);
        }
        
    }
} 

enum class AcquireMallocatedString{};


template<typename Char>
class leptString_core
{
protected:
    const static bool kIsLittleEndian=__BYTE_ORDER==__ORDER_LITTLE_ENDIAN__;
public:
    leptString_core()
    {
        reset();
    }

    leptString_core(const leptString_core& rhs)
    {
        switch (rhs.category())
        {
        case Category::isSmall:
            copySmall(rhs);
            break;
        case Category::isMidum:
            copyMedium(rhs);
            break;
        case Category::isLarge:
            copyLarge(rhs);
            break;
        default:
            break;
        }
        assert(rhs.size()==size());
        assert(memcmp(data(),rhs.data(),size()*sizeof(Char))==0);
    }
    
    leptString_core(leptString_core&& goner) 
    {
        ml_=goner.ml_;
        goner.reset();
    }

    leptString_core(const Char* data,const size_t size)
    {
        if(size<=maxSmallSize)
        {
            initSmall(data,size);
        }
        else if(size<=maxMediumSize)
        {
            initMedium(data,size);
        }
        else
        {
            initLarge(data,size);
        }
        
        
    }

    ~leptString_core() 
    {
        if(category()==Category::isSmall)
            return;
        destroyMedimLarge();
    }
    leptString_core(Char* data,const size_t size,const size_t allocateSize,AcquireMallocatedString)
    {
        if(size>0)
        {
            assert(data[size]=='\0');
            assert(allocateSize>=size+1);
            ml_.data_=data;
            ml_.size_=size;
            ml_.setCapacity(allocateSize-1,Category::isMedium);
        }
        else
        {
            free(data);
            reset();
        }
        
    }
    
    void sawp(leptString_core& rhs)
    {
        std::swap(rhs.ml_,ml_);
    }
    const Char* data() const
    {
        return c_str();
    }

    Char* mutableData()
    {
        switch (category())
        {
        case Category::isSmall:
            return small_;
        case Category::isMedium:
            return ml_.data_;
        case Category::isLarge:
            return mutableDataLarge();      
        default:
            break;
        }
    }

    const Char* c_str() const
    {
        const Char* ptr=(Category()==Category::isSmall)?small_:ml_.data_;
    }

    void shrink(const size_t delta)
    {
        if(category()==Category::isSmall)
        {
            shrinkSmall(delta);   
        }
        else if(category()==Category::isMedium || RefCounted::refs(ml_.data_)==1)
        {
            shrinkMedium(delta);
        }
        else
        {
            shrinkLarge(delta);
        }
        
    }
    void reserve(size_t minCapacity)
    {
        switch (category())
        {
        case Category::isSmall:
            reserveSmall(minCapacity);
            break;        
        case Category::isLarge:
            reserveLarge(minCapacity);
            break;        
        case Category::isMedium:
            reserveMedium(minCapacity);
            break;        
        default:
            break;
        }
    }

    Char* expandNoinit(const size_t delta,bool expGrowth=false)
    {
        size_t sz,newSz;
        if(category()==Category::isSmall)
        {
            sz=smallSzie();
            newSz=sz+delta;
            if(newSz<=maxSmallSize)
            {
                setSmallsize(newSz);
                return small_+sz;
            }
            reserveSmall(expGrowth?std::max(newSz,2*maxSmallSize):newSz);
        }
        else
        {
            sz=ml_.size_;
            newSz=sz+delta;
            if(newSz>capacity())
            {
                reserve(expGrowth?std::max(newSz,1+capacity()*3/2):newSz);
            }
        }
        ml_.size_=newSz;
        ml_.data_[newSz]='\0';
        return ml_.data_+sz;

    }
    void push_back(Char c)
    {
        *expandNoinit(1)=c;
    }
    size_t size() const
    {
        return (category()==Category::isSmall)?smallSzie():ml_.size_;
    }

    size_t capacity() const
    {
        switch (category())
        {
        case Category::isSmall:
            return maxSmallSize;
        case Category::isLarge:
            if(RefCounted::refs(ml_.data_)>1)
                return ml_.size_;
        default:
            break;
        }
        return ml_.capacity();
    }

    bool isShared() const
    {
        return category()==Category::isLarge && RefCounted::refs(ml_.data_)>1;
    }

private:
    //disable
    leptString_core& operator=(const leptString_core& rhs);
    
    void reset()
    {
        setSmallsize(0);
    }

    void destroyMedimLarge() noexcept
    {
        Category c=category();
        assert(c!=Category::isSmall);
        if(c==Category::isMidum)
        {
            free(ml_.data_);
        }
        else
        {
            RefCounted::decrementRefs(ml_.data_);
        }  
    }

    struct RefCounted
    {
        std::atomic<size_t> refCount_;
        Char data_[1];
        constexpr static size_t getDataOffset()
        {
            return offsetof(RefCounted,data_);
        }

        static RefCounted* fromData(Char* p)
        {
            return static_cast<RefCounted*>(
            static_cast<void*>(static_cast<unsigned char*>(static_cast<void*>(p))-getDataOffset()));   
        }

        static size_t refs(Char* p)
        {
            return fromData(p)->refCount_.load(std::memory_order_acquire);
        }
        
        static void incrementRefs(Char* p)
        {
            return fromData(p)->refCount_.fetch_add(1,std::memory_order_acq_rel);
        }
    
        static void decrementRefs(Char* p)
        {
            return fromData(p)->refCount_.fetch_sub(1,std::memory_order_acq_rel);
        }

        static RefCounted* creat(size_t size)
        {
            const size_t allosize=getDataOffset()+(size+1)*sizeof(Char);
            RefCounted* res=static_cast<RefCounted*>(checkedmalloc(allosize));   
            res->refCount_.store(1,std::memory_order_release);
            return res;
        }

        static RefCounted* create(const char* data,size_t size)
        {
            RefCounted* res=creat(size);
            podCopy(data,data+size,res->data_);
            return res;
        }

        static RefCounted* reallocate(Char* data,const size_t currentSize,const size_t currentCapacity,size_t newCapacity)
        {
            assert(newCapacity>0 && newCapacity>currentCapacity);
            const size_t allocNewcapacity=getDataOffset()+(newCapacity+1)*sizeof(Char);
            RefCounted* dis=fromData(data);
            assert(dis->refCount_.load(std::memory_order_acquire)==1);
            RefCounted* res=static_cast<RefCounted*>(smartRealloc(dis,
                                                                  getDataOffset()+(currentSize+1)*sizeof(Char),
                                                                  getDataOffset()+(currentCapacity+1)*sizeof(Char),
                                                                  allocNewcapacity));
            return res;
        }
    };

    typedef unsigned char category_type;
    
    enum class Category : category_type
    {
        isSmall=0,
        isMedium=kIsLittleEndian? 0x80 : 0x2,
        isLarge=kIsLittleEndian? 0x40 : 0x1
    };
    
    
    Category category() const
    {
        return static_cast<Category>(bytes_[lastChar]&categoryExtraMask);
    }
    struct MediumLarge
    {
        Char* data_;
        size_t size_;
        size_t capacity_;

        size_t capacity() const
        {
            return kIsLittleEndian? capacity_&capacityExtracMask :capacity_>>2;
        }
        
        void setcapacity(size_t cap,Category cat)
        {
            capacity_=kIsLittleEndian
                      ?cap | (static_cast<size_t>(cat)<<kCategoryShift)
                      :(cap<<2) | static_cast<size_t>(cat);
        }
    };

    union 
    {
        unsigned char bytes_[sizeof(MediumLarge)];
        Char small_[sizeof(MediumLarge)/sizeof(Char)];
        MediumLarge ml_;
    };
    
    const static size_t lastChar=sizeof(MediumLarge)-1;
    const static size_t maxSmallSize=lastChar/sizeof(Char);
    const static size_t maxMediumSize=254/sizeof(Char);
    const static size_t categoryExtraMask=kIsLittleEndian? 0xC0 :0x3;
    const static size_t kCategoryShift=(sizeof(size_t)-1)*8;
    const static size_t capacityExtracMask=kIsLittleEndian?~(size_t(categoryExtraMask)<<kCategoryShift):0x0;
    
    size_t smallSzie() const
    {
        assert(category()==Category::isSmall);
        const int shift=kIsLittleEndian?0:2;
        size_t smallshift=static_cast<size_t>(small_[lastChar])>>shift
        assert(smallshift<=maxSmallSize);
        return maxSmallSize-smallshift;
    }

    size_t setSmallsize(size_t s)
    {
        assert(s<=maxSmallSize);
        const int shift=kIsLittleEndian?0:2;
        small_[lastChar]=char((maxSmallSize-s)<<shift);
        small_[s]='\0';
        assert(category()==Category::isSmall && size()==s);
    }

    void copySmall(const leptString_core& rhs)
    {
        assert(rhs.categpry()==Category::isSmall);
        ml_=rhs.ml_;
    }
    void copyMedium(const leptString_core& rhs)
    {
        assert(rhs.categpry()==Category::isMedium);
        size_t allocsize=(rhs.ml_.size+1)*sizeof(Char);
        ml_.data_=static_cast<char*>(checkedmalloc(allocsize));
        podCopy(rhs.ml_.data_,rhs.ml_.data_+rhs.ml.size_+1,ml_.data_);
        ml_.size_=rhs.ml_.size_;
    }
    void copyLarge(const leptString_core& rhs)
    {
        ml_=rhs.ml_;
        RefCounted::incrementRefs(ml_.data_);
        assert(category()==Category::isLarge && size()==rhs.size());
    }

    void initSmall(const Char* data,size_t size)
    {
        if(size!=0)
        {
            podCopy(data,data+size,small_);
        }
        setSmallsize(size);
    }
    void initMedium(const Char* data,size_t size)
    {
        if(size!=0)
        {
            const size_t allocsize=(size+1)*sizeof(Char);
            ml_.data_=checkedmalloc(allocsize);
            podCopy(data,data+size,ml_.data_);
        }
        ml_.size_=size;
        ml_.setcapacity(size,Category::isMedium);
        ml_.data_[size]='\0';
        
    }
    void initLarge(const Char* data,size_t size)
    {
        RefCounted* res=RefCounted::create(data,size);        
        ml_.data_=res->data_;
        ml_.size_=size;
        ml_.setcapacity(size,Category::isLarge);
        ml_.data_[size]='\0';
    }

    void reserveSmall(size_t minCapacity)
    {
        assert(category()==Category::isSmall);
        if(minCapacity<=maxSmallSize)
            return;
        else if (minCapacity<=maxMediumSize)
        {
            const size_t allocbytes=(minCapacity+1)*sizeof(Char);
            const size_t size=smallSzie();
            char* res=static_cast<char*>(checkedmalloc(allocbytes));
            podCopy(small_,small_+size+1,res);
            ml_.data_=res;
            ml_.size_=size;
            ml_.setcapacity(minCapacity,Category::isMedium);
        }
        else
        {
            RefCounted* res=RefCounted::creat(minCapacity);
            const size_t size=smallSzie();
            podCopy(small_,small_+size+1,res->data_);
            ml_.size_=size;
            ml_.setcapacity(minCapacity,Category::isLarge);
        }
        
        
    }
    void reserveMedium(size_t minCapacity)
    {
        if(ml_.capacity()<=maxMediumSize)
        {
            size_t capacityBytes=sizeof(Char)*(minCapacity+1);
            ml_.data_=static_cast<char*>(smartRealloc(ml_.data_,
                                                      (ml_.size_+1)*sizeof(Char),
                                                      (ml_.capacity()+1)*sizeof(Char),
                                                      capacityBytes));
            ml_.setcapacity(minCapacity,Category::isMedium);
        }
        else
        {
            leptString_core temp;
            temp.reserve(minCapacity);
            temp.ml_.size_=ml_.size_;
            podCopy(ml_.data_,ml_.data_+ml_.size_+1,temp.ml_.data_);
            temp.sawp(*this);
        }
        

    }
    void reserveLarge(size_t minCapacity)
    {
        assert(category()==Category::isLarge);
        if(RefCounted::refs(ml_.data_)>1)
        {
            unshare(minCapacity);
        }
        else
        {
            if(minCapacity>ml_.capacity())
            {
                RefCounted* res=RefCounted::reallocate(ml_.data_,ml_.size_,ml_.capacity(),minCapacity);
                ml_.data_=res->data_;
                ml_.setcapacity(minCapacity,Category::isLarge);
            }
        }
        
    }
    
    void shrinkSmall(size_t delta)
    {
        setSmallsize(smallSzie()-delta);
    }
    void shrinkMedium(size_t delta)
    {
        ml_.size_-=delta;
        ml_.data_[ml_.size_]='\0';
    }
    void shrinkLarge(size_t delta)
    {
        if(delta)
        {
            leptString_core(ml_.data_,ml_.size_-delta).sawp(*this);
        }
    }

    void unshare(size_t minCapacity=0)
    {
        size_t effectiveCapacity=std::max(minCapacity,ml_.capacity());
        RefCounted* res=RefCounted::create(effectiveCapacity);
        podCopy(ml_.data_,ml_.data_+ml_.size_+1,res->data_);//should copy null terminate
        RefCounted::decrementRefs(ml_.data_);
        ml_.data_=res->data_;
        ml_.setcapacity(effectiveCapacity,Category::isLarge);
    }
    Char* mutableDataLarge()
    {
        assert(category()==Category::isLarge);
        if(RefCounted::refs(ml_.data_)>1)
        {
            unshare();
        }
        return ml_.data_;
    }
};


template<typename E,
        class T=std::char_traits<E>,
        class A=std::allocator<E>,
        class Storage=leptString_core<E>>
    class basic_leptstring
    {
        bool isSane() const
        {

        }
        struct Invariant
        {
            Invariant& operator=(const Invariant&)=delete;
            Invariant(const basic_leptstring& s)noexcept:s_(s)
            {
                assert(s_.isSane());
            }
            ~Invariant()noexcept
            {
                assert(s_.isSane());
            }
            private:
                const basic_leptstring& s_; 
        };
        public:
            typedef T traits_type;
            typedef typename traits_type::char_type value_type;        
            typedef A allocator_type;
            
            typedef typename A::size_type size_type;
            typedef typename A::difference_type difference_type;
            typedef typename A::reference reference;
            typedef typename A::const_reference const_reference;
            typedef typename A::pointer pointer;
            typedef typename A::const_pointer const_pointer;
            
            typedef E* iterator; 
            typedef const E* const_iterator;
            typedef std::reverse_iterator<iterator> reverse_iterator;
            typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

            static constexpr size_type npos=size_type(-1);

        private:
            static void procrutes(size_type &n,size_type nmax)
            {
                if(n>nmax)
                    n=nmax;
            }

            static size_type traitsLength(const value_type* s)
            {
                return s?traits_type::length(s):0;
            }

        public:
            basic_leptstring() noexcept{}

            explicit basic_leptstring(const A&) noexcept{}

            basic_leptstring(const basic_leptstring& str): store_(str.store_){}
            
            basic_leptstring(basic_leptstring&& gonner) noexcept : store_(std::move(gonner.store_)){} 

            template<typename A2>
            basic_leptstring(const std::basic_string<E,T,A2>& str) : store_(str.data(),str.size()){}

            basic_leptstring(const basic_leptstring& str,
                            size_type pos,
                            size_type n=npos)
            {
                assign(str,pos,n);
            }            

            basic_leptstring(const value_type* s):store_(s,traitsLength(s)){}

            basic_leptstring(const value_type* s,size_t n):store_(s,n){}
            
            basic_leptstring(size_type n,value_type c)
            {
                auto pdata=store_.expandNoinit(c);
                podFill(pdata,pdata+n,c);
            }

            template<typename InIt>
            basic_leptstring(
                InIt begin,
                InIt end,
                typename std::enable_if<!std::is_same<InIt,value_type*>::value,const A>::type& 
            )
            {
                assign(begin,end);
            }    
    
            basic_leptstring(const value_type* b,const value_type* e,const A&) : store_(b,size_type(e-b)){}
            basic_leptstring(
                const value_type* s,
                size_type n,
                size_type c,
                AcquireMallocatedString a) : store_(s,n,c,a) {}

            basic_leptstring(const std::initializer_list<value_type>& il)
            {
                assign(il.begin(),il.end());
            }
            
            ~basic_leptstring() noexcept{}
    
            basic_leptstring& operator=(const basic_leptstring& rhs)
            {
                Invariant check(*this);
                if(this==&rhs)
                    return *this;
                assign(rhs.data(),rhs.size());
                return *this;
            }
            basic_leptstring& operator=(basic_leptstring&& gonner)noexcept
            {
                if(this==&gonner)
                {
                    return this;
                }
                this->~basic_leptstring();
                new (store_) Storage(std::move(gonner.store_));
                return *this;
            }
            
            template<typename A2>
            basic_leptstring& operator=(const std::basic_string<E,T,A2>& str)
            {
                return assign(str.data(),str.size());
            }

            std::basic_string<E,T,A> toStdString() const
            {
                return std::basic_string<E,T,A>(data(),size());
            }
    
            basic_leptstring& operator=(value_type c)
            {
                Invariant check(*this);
                if(empty())
                    store_.expandNoinit(1);
                else if (store_.isShared())
                {
                    basic_leptstring(1,c).swap(*this);
                }
                else
                {
                    store_.shrink(size()-1);
                }
                front()=c;
                return *this;
            }
    
            basic_leptstring& operator=(const std::initializer_list<value_type>& il)
            {
                assign(il.begin(),il.end());
                return *this;
            }

            iterator begin()
            {
                return store_.mutableData();
            }

            const_iterator begin() const
            {
                return store_.data();
            }
            const_iterator cbegin() const
            {
                return store_.data();
            }
            
            reverse_iterator rbegin()
            {
                return reverse_iterator(store_.mutableData());
            }
            
            const_reverse_iterator crbein() const
            {
                return reverse_iterator(store_.data());
            }
            
            iterator end()
            {
                return store_.mutableData()+store_.size();
            }

            const_iterator cend() const
            {
                return store_.data()+store_.size();
            }
    
            reverse_iterator rend()
            {
                return reverse_iterator(store_.mutableData()+store_.size());
            }
            
            const_reverse_iterator crend()const
            {
                return reverse_iterator(store_.data()+store_.size());
            }
    
            const value_type& front() const
            {
                return *begin();
            }

            value_type& front()
            {
                return *begin();
            }
            const value_type& back() const
            {
                assert(!empty());
                return *(end()-1);
            }

            value_type& back()
            {
                assert(!empty());
                return *(end()-1);
            }

            const value_type* data() const
            {
                c_str();
            }

            const value_type* c_str() const
            {
                return store_.c_str();
            }
            

            void pop_back()
            {
                assert(!empty());
                store_.shrink(1);
            }

            size_type size() const
            {
                return store_.size();
            }
    
            size_type length() const
            {
                return size();
            }
    
            void resize(size_type n,value_type c=value_type())
            {
                Invariant check(*this);
                auto size=this->size();
                if(n<=size)
                    store_.shrink(n);
                else
                {
                    auto delta=n-size;
                    auto b=store_.expandNoinit(delta);
                    podFill(b,b+delta,c);
                }
                
                    
            }

            size_t capacity() const
            {
                return store_.capacity();
            }

            void reserve(size_type res_arg=0)
            {
                store_.reserve(res_arg);
            }

            void shrink_to_fit()
            {
                if(capacity()<size()*3/2)
                    return;
                
            }

            void clear()
            {
                resize(0);
            }

            bool empty() const
            {
                return size()==0;
            }

            void swap(basic_leptstring& rhs)
            {
                std::swap(store_,rhs.store_);
            }

            const_reference operator[](size_t n) const
            {
                return begin()[n];
            }

            reference operator[](size_t n)
            {
                return begin()[n];
            }

            basic_leptstring& operator+=(const basic_leptstring& str)
            {
                return append(str);
            }

            basic_leptstring& operator+=(const value_type* str)
            {
                return append(str);
            }
            
            basic_leptstring& operator+=(value_type c)
            {
                push_back(c);
                return *this;
            }

            basic_leptstring& operator+=(const std::initializer_list<value_type>& c)
            {
                return append(c.begin(),c.end());
            }

            basic_leptstring& append(const basic_leptstring& str)
            {
                return append(str.data(),str.size());
            }

            basic_leptstring& append(const basic_leptstring& str,const size_type pos,size_type n)
            {
                return append(str.data()+pos,n);
            }

            basic_leptstring& append(const value_type* s)
            {
                return append(s,traitsLength(s));
            }

            basic_leptstring& append(const value_type* s,size_t n)
            {
                Invariant check(*this);
                auto pData=store_.expandNoinit(n,true);
                podCopy(s,s+n,pData);
                return *this;
            }


            // basic_leptstring& append(value_type c,size_type n);

            template<typename InputIterator>
            basic_leptstring& append(InputIterator first,InputIterator last)
            {
                return insert(begin(),first,last);
            }

            basic_leptstring& append(const std::initializer_list<value_type>& il)
            {
                return append(il.begin(),il.end());
            }

            void push_back(const value_type& c)
            {
                store_.push_back(c);
            }

            basic_leptstring& assign(const basic_leptstring& str)
            {
                if(this==&str)
                    return *this;
                else
                {
                    return assign(str.begin(),str.end());
                }   
            }

            basic_leptstring& assign(basic_leptstring&& gonner)
            {
                return *this=std::move(gonner);
            }

            basic_leptstring& assign(const basic_leptstring& str,const size_type pos,size_type n);

            basic_leptstring& assign(const std::initializer_list<value_type>& il)
            {
                return assign(il.begin(),il.end());
            }

            basic_leptstring& assign(const value_type* s)
            {
                return assign(s,traitsLength(s));
            }

            basic_leptstring& assign(const value_type* s,size_t n);

            template<typename ItOrLength,typename ItOrChar>
            basic_leptstring& assign(ItOrLength first_or_n,ItOrChar last_or_c)
            {
                return replace(begin(),end(),first_or_n,last_or_c);
            }

            basic_leptstring& insert(size_type pos1,const basic_leptstring& str)
            {
                return insert(pos1,str.data().str.size());
            }

            basic_leptstring& insert(size_type pos1,const basic_leptstring& str,size_type pos2,size_type n)
            {
                return insert(pos1,str.data()+pos2,n);
            }

            basic_leptstring& insert(size_type pos,const value_type* s,size_type n)
            {
                return insert(begin()+pos,s,s+n);
            }

            basic_leptstring& erase(size_type pos=0,size_type n=npos)
            {
                Invariant check(*this);
                procrutes(n,size()-pos);
                std::copy(begin()+pos+n,end(),begin()+pos);
                resize(size()-n);
                return *this;
            }


            private:

            iterator insertImplDiscr(const_iterator i,size_type n,value_type c,std::true_type)
            {
                Invariant check(*this);
                auto oldSize=size();
                size_type pos=i-cbegin();
                store_.expandNoinit(n,true);
                auto b=begin();
                podMove(b+pos,b+oldSize,b+pos+n);
                podFill(b+pos,b+pos+n,c);
            }
            
            template<typename InputIterator>
            iterator insertImplDiscr(const_iterator i,InputIterator b,InputIterator e,std::false_type)
            {
                return insertImpl(i,b,e,typename std::iterator_traits<InputIterator>::iterator_categoey());
            }

            template<typename FwdIterator>
            iterator insertImpl(const_iterator i,FwdIterator s1,FwdIterator s2,std::forward_iterator_tag)
            {
                auto oldSize=size();
                const size_type pos=i=cbegin();
                size_type n=std::distance(s1,s2);
                store_.expandNoinit(n,true);
                auto b=begin();
                podMove(b+pos,b+oldSize,b+pos+n);
                podCopy(s1,s2,b+pos);
                return b+pos;
            }

            template<typename InputIterator>
            iterator insertImpl(const_iterator i,InputIterator s1,InputIterator s2,std::input_iterator_tag)
            {
                auto pos=i-cbegin();
                basic_leptstring temp(cbegin(),i);
                for (; s1!=s2; s1++)
                {
                    temp.push_back(*s1);
                }
                temp.append(i,end());
                swap(temp);
                return begin()+pos;
                
            }

            basic_leptstring& replaceImplDiscr(iterator i1,iterator i2,const value_type* s,size_type n,std::integral_constant<int,2>)
            {
                return replace(i1,i2,s,s+n);
            }

            basic_leptstring& replaceImplDiscr(iterator i1,iterator i2,size_type n2,value_type c,std::integral_constant<int,1>)
            {
                const size_type n1=i2-i1;
                std::fill(i1,i2,c);
                
            }
            
            template<typename InputIter>
            basic_leptstring& replaceImplDiscr(iterator i1,iterator i2,InputIter b,InputIter e,std::integral_constant<int,0>)
            {
                return replaceImpl(i1,i2,b,e,std::iterator_traits<InputIter>::iterator_category());    
            }
            
            template<typename FwdIterator>
            bool replaceAliasec(iterator,iterator,FwdIterator,FwdIterator,std::false_type)
            {
                return false;
            }
            
            template<typename FwdIterator>
            bool replaceAliasec(iterator,iterator,FwdIterator,FwdIterator,std::true_type);
            
            template<typename FwdIterator>
            void replaceImpl(iterator i1,iterator i2,FwdIterator s1,FwdIterator s2,std::forward_iterator_tag)
            {
                basic_leptstring temp(cbegin(),i1);
                temp.append(s1,s2);
                temp.append(i1,end());
                swap(temp);

            }

            template<typename InputIterator>
            void replaceImpl(iterator i1,iterator i2,InputIterator s1,InputIterator s2,std::input_iterator_tag);

            public:
            template<typename ItOrLength,typename ItOrChar>
            iterator insert(const_iterator p,ItOrLength first_or_n,ItOrChar last_or_c)
            {
                using sel=std::integral_constant<bool,std::numeric_limits<ItOrLength>::is_specialized>;
                return insertImplDiscr(p,first_or_n,last_or_c,sel());
            }

            basic_leptstring& replace(size_type pos1,size_type n1,const basic_leptstring& str)
            {
                return replace(pos1,n1,str.begin(),str.size());
            }

            basic_leptstring& replace(size_type pos1,size_type n1,const basic_leptstring& str,size_type pos2,size_type n2)
            {
                return replace(pos1,n1,str.data()+pos2,std::min(size()-pos2,n2));
            }

            template<typename StrOrLength,typename NumOrChar>
            basic_leptstring& replace(size_type pos,size_type n1,StrOrLength s_or_n2,NumOrChar n_or_c)
            {
                Invariant check(*this);
                procrutes(n1,size()-pos);
                const_iterator b=begin()+pos;
                return replace(b,b+n1,s_or_n2,n_or_c);
            }      

            basic_leptstring& replace(iterator i1,iterator i2,const basic_leptstring& str)
            {
                return replace(i1,i2,str.data(),str.size());
            }
            template<typename T1,typename T2>
            basic_leptstring& replace(iterator i1,iterator i2,T1 first_or_n_or_s,T2 last_or_c_or_n)
            {
                constexpr bool num1=std::numeric_limits<T1>::is_specialized;
                constexpr bool num2=std::numeric_limits<T2>::is_specialized;
                using sel=std::integral_constant<int, num1?(num2?1:-1):(num2?2:0)>;
                return replaceImplDiscr(i1,i2,first_or_n_or_s,last_or_c_or_n,sel());
            }
            
            private:
                Storage store_;    
    };
        









#endif