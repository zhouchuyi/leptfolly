#ifndef LEPTVECTOR_H_
#define LEPTVECTOR_H_

#include<algorithm>
#include<cassert>
#include<iterator>
#include<memory>
#include<type_traits>
#include<utility>


#define LEPTV_UNROLL_PTR(first, last, OP)        \
    do{                                          \
        for (;(last)-(first) >= 4; (first) +=4 ) \
        {                                        \
            OP(((first)+0));                     \
            OP(((first)+1));                     \
            OP(((first)+2));                     \
            OP(((first)+3));                     \
        }                                        \
        for (; (first) != (last); (first)++)     \
            OP((first));                         \
    }while(0);                                   \
    

template <typename T>
class LeptVector
{
private:
    typedef std::allocator<T> Allocator;
    typedef std::allocator_traits<Allocator> A;    
    struct Impl : public Allocator
    {
        typedef typename A::pointer pointer;
        typedef typename A::size_type size_type;
        //begin--end 
        pointer b_,e_,z_;
        Impl() : Allocator(),b_(nullptr),e_(nullptr),z_(nullptr) { }
        Impl(const Allocator& alloc) : Allocator(alloc),b_(nullptr),e_(nullptr),z_(nullptr) { }
        Impl(Allocator&& alloc) : Allocator(std::move(alloc)),b_(nullptr),e_(nullptr),z_(nullptr) { }
        Impl(size_type n, const Allocator& alloc=Allocator()) 
            : Allocator(alloc)
            {
                 init(n);
            }
        Impl(Impl&& other) noexcept
            : Allocator(std::move(other)),b_(other.b_),e_(other.e_),z_(other.z_)
            {
                other.b_=nullptr;
                other.e_=nullptr;
                other.z_=nullptr;
            }
        ~Impl()
        {
            destroy();
        }
        T* D_allocate(size_type n)
        {
            return std::allocator_traits<Allocator>::allocate(*this,n);
        }
        void D_deallocate(T* p,size_type n)
        {
            std::allocator_traits<Allocator>::deallocate(*this,p,n);
        }
        void swapData(Impl& othre)
        {
            std::swap(b_,othre.b_);
            std::swap(e_,othre.e_);
            std::swap(z_,othre.z_);
        }
        //data ops
        inline void destroy() noexcept
        {
            if(b_)
            {
                S_destroy_range(b_,e_);
                D_deallocate(b_,size_type(z_-b_));
            }
        }
        void init(size_type n)
        {
            if(n==0)
                b_=e_=z_=nullptr;
            else
            {
                b_=D_allocate(n);
                e_=b_;
                z_=b_+n;
            }
            
        }
        void set(pointer newB, size_type newSize, size_type newCap)
        {
            b_=newB;
            z_=newB+newCap;
            e_=newB+newSize;
        }
        void reset(size_type newCap)
        {
            destroy();
            init(newCap);
        }
        void reset()
        {
            destroy();
            b_=e_=z_=nullptr;
        }

    } impl_ ;
    static void swap(Impl& a,Impl& b)
    {
        std::swap(static_cast<Allocator&>(a),static_cast<Allocator&>(b));
        a.swapData(b);
    }




public:
    typedef T value_type;
    typedef value_type&  reference;
    typedef const value_type& const_reference;
    typedef T* iterator;
    typedef const T*  const_iterator;
    typedef size_t size_type;
    typedef typename std::make_signed<size_type>::type difference_type;
    typedef Allocator allocator_type;
    typedef typename A::pointer pointer;
    typedef typename A::const_pointer const_pointer;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

private:
    static constexpr bool should_pass_by_value=std::is_trivially_copyable<T>::value && sizeof(T) <= 16;
    typedef typename std::conditional<should_pass_by_value,T,const T&>::type VT;
    typedef typename std::conditional<should_pass_by_value,T,T&&>::type MT;
    


private:
    T* M_allocate(size_type n)
    {
        return impl_.D_allocate(n);
    }

    void* M_Deallocate(T* p,size_type n)
    {
        impl_.D_deallocate(p,n);
    }

    template<typename U,typename ... Args>
    void M_constuct(U* p,Args&& ... args)
    {
        new (p) U(std::forward<Args>(args)...);
    }
    template<typename U,typename ... Args>
    static void S_constuct(U* p,Args&& ... args)
    {
        new (p) U(std::forward<Args>(args)...);
    }

    template<typename U,typename ... Args>
    static void S_constuct_a(Allocator& a,U* p,Args&& ... args)
    {
        std::allocator_traits<Allocator>::construct(a,p,std::forward<Args>(args)...);
    }
    
    
    //scalar optmization
    template<
        typename U,
        typename  Enable=typename std::enable_if<std::is_scalar<U>::value>::type>
    void M_construct(U* p,U arg)
        {
            *p=arg;
        }

    template<
        typename U,
        typename  Enable=typename std::enable_if<std::is_scalar<U>::value>::type>
    static void S_construct(U* p,U arg)
        {
            *p=arg;
        }

    template<
        typename U,
        typename  Enable=typename std::enable_if<std::is_scalar<U>::value>::type>
    static void S_construct_a(Allocator& a,U* p,U arg)
        {
            std::allocator_traits<Allocator>::construct(a,p,arg);
        }
    template<
        typename U,
        typename  Enable=typename std::enable_if<!std::is_scalar<U>::value>::type>
    void M_construct(U* p,const U& arg)
        {
            new (p) U(arg);
        }

    template<
        typename U,
        typename  Enable=typename std::enable_if<!std::is_scalar<U>::value>::type>
    static void S_construct(U* p,const U& arg)
        {
            new (p) U(arg);
        }
    template<
        typename U,
        typename  Enable=typename std::enable_if<!std::is_scalar<U>::value>::type>
    static void S_construct_a(Allocator& a,U* p,const U& arg)
        {
            std::allocator_traits<Allocator>::construct(a,p,arg);
        }

    void M_destroy(T* p) noexcept
    {
        if(!std::is_trivially_destructible<T>::value)
        {
            p->~T();
        }
        else
        {
            std::allocator_traits<Allocator>::destroy(impl_,p);
        }
    }

private:
    void M_destory_range_e(T* pos) noexcept
    {
        D_destory_range_a(pos,impl_.e_);
        impl_.e_=pos;
    }

    void D_destory_range_a(T* first,T* last) noexcept
    {
        S_destroy_range(first,last);
    }

    static void S_destory_range_a(Allocator&  a,T* first,T* last) noexcept
    {
        for (; first != last; first++)
        {
            std::allocator_traits<Allocator>::destroy(a,first);
        }
        
    }
    static void S_destroy_range(T* first,T* last) noexcept
    {
        if(!std::is_trivially_destructible<T>::value)
        {
#define LEPTV_OP(p) (p)->~T()
               LEPTV_UNROLL_PTR(first,last,LEPTV_OP);
#undef  LEPTV_OP       
        }
    }

    void M_uninitialized_fill_n_e(size_type sz)
    {
        D_uninitialized_fill_n_a(impl_.e_,sz);
        impl_.e_+=sz;
    }
    void M_uninitialized_fill_n_e(size_type sz,VT value)
    {
        D_uninitialized_fill_n_a(impl_.e_,sz,value);
        impl_.e_+=sz;
    }

    void D_uninitialized_fill_n_a(T* dest,size_type sz)
    {
        S_uninitialized_fill_n(dest,sz);
    }

    void D_uninitialized_fill_n_a(T* dest,size_type sz,VT value)
    {
        S_uninitialized_fill_n(dest,sz,value);
    }

    template<typename... Args>
    static void S_uninitialized_fill_n_a(Allocator& a,T* dest,size_type sz,Args&& ... args)
    {
        auto b=dest;
        auto e=dest+sz;
        for (; b != e; b++)
        {
            std::allocator_traits<Allocator>::construct(a,b,std::forward<Args>(args)...);
        }
        
    }

    static void S_uninitialized_fill_n(T* dest,size_type n)
    {
        auto b=dest;
        auto e=dest+n;
        for (; b != e; b++)
        {
            S_constuct(b);
        }       
    }

    static void S_uninitialized_fill_n(T* dest,size_type n,const T& value)
    {
        auto b=dest;
        auto e=dest+n;
        for (; b != e; b++)
        {
            S_constuct(b,value);
        }
    }

    template<typename It>
    void M_uninitialized_copy_e(It first, It last)
    {
        D_uninitialized_copy_a(impl_.e_,first,last);
        impl_.e_+=std::distance(first,last);
    }

    template<typename It>
    void D_uninitialized_copy_a(T* dest,It first,It last)
    {
        if(std::is_trivially_copyable<T>::value)
        {
            S_uninitialized_copy_bits(dest,first,last);
        }
        else
        {
            S_uninitialized_copy(dest,first,last);
        }
        
    }

    template<typename It>
    static void S_uninitialized_copy_a(Allocator& a,T* dest,It first,It last)
    {
        auto b=dest;
        for (; first != last; first++,b++)
        {
            std::allocator_traits<Allocator>::construct(a,b,*first);
        } 
    }

    template<typename It>
    static void S_uninitialized_copy(T* dest,It first,It last)
    {
        auto b=dest;
        for (; first != last; first++,b++)
        {
            S_constuct(dest,*first);
        } 
    }
    static void S_uninitialized_copy_bits(T* dest,const T* first,const T* last)
    {
        if(last!=first)
            std::memcmp((void*)dest,(void*)first,(last-first)*sizeof(T));
    }
    static void  S_uninitialized_copy_bits(T* dest,std::move_iterator<T*> first,std::move_iterator<T*> last)
    {
        T* bFirst=first.base();
        T* bLast=last.base();
        if(bFirst!=bLast)
            std::memcmp((void*)dest,(void*)bFirst,(bLast-bFirst)*sizeof(T));
    
    }
    template<typename It>
    static void  S_uninitialized_copy_bits(T* dest,It first,It last)
    {
        S_uninitialized_copy(dest,first,last);
    }

    template<typename It>
    void M_uninitialized_move_e(It first, It last)
    {
        D_uninitialized_move_a(impl_.e_,first,last);
        impl_.e_+=std::distance(first,last);   
    }

    template<typename It>
    void D_uninitialized_move_a(T* dest,It first,It last)
    {
        D_uninitialized_copy_a(dest,std::make_move_iterator(first),std::make_move_iterator(last));
    }

    

    //copy n
    template<typename It>
    static It S_copy_n(T* dest,It first,size_type n)
    {
        auto e=dest+n;
        for (; dest != e; dest++,first++)
        {
            *dest=*first;
        }
        return  first;
    }

    static const T* S_copy_n(T* dest,const T* frist,size_type n)
    {
        if(std::is_trivially_copyable<T>::value)
        {    std::memcpy((void*)dest,(void*)frist,n*sizeof(T));
                return frist+n;
        }
        else
        {
            return S_copy_n<const T*>(dest,frist,n);
        }
    }
    static std::move_iterator<T*>
    S_copy_n(T* dest,std::move_iterator<T*> mIt,size_type n)
    {
        if(std::is_trivially_copyable<T>::value)
        {
            T* first=mIt.base();
            std::memcpy((void*)dest,(void*)first,n*sizeof(T));
            return std::make_move_iterator(first+n);
        }
        else
        {
            return S_copy_n<std::move_iterator<T*>>(dest,mIt,n);
        }
    }

    //need to add relocate
    void M_relocate(T* newB)
    {
        relocate_move(newB,impl_.b_,impl_.e_);
        // relocate_done()
    }

    void relocate_move(T* dest,T* first,T* last)
    {
        if(first!=nullptr)
            std::memcpy((void*)dest,(void*)first,(last-first)*sizeof(T));
    }
    void relocate_done(T* dest,T* last,T* first)
    {

    }
    typedef std::bool_constant<(std::is_nothrow_move_constructible<T>::value) || !std::is_copy_constructible<T>::value>

public:

    LeptVector()=default;
    explicit LeptVector(size_type n,const Allocator& a=Allocator())
        : impl_(n,a) 
        {
            M_uninitialized_fill_n_e(n);
        }
    explicit LeptVector(size_type n,VT value,const Allocator& a=Allocator())
        : impl_(n,a)
        {
            M_uninitialized_fill_n_e(n,value);
        }

    template<class It,class Category=typename std::iterator_traits<It>::iterator_category>
    LeptVector(It first,It last,const Allocator& a=Allocator())
        : LeptVector(first,last,Category()){}
    LeptVector(const LeptVector& other)
        : impl_(other.size())
        {
            M_uninitialized_copy_e(other.begin(),other.end());
        }
    LeptVector(LeptVector&& other) noexcept
        : impl_(std::move(other.impl_)){}

    LeptVector(std::initializer_list<T> il)
        : LeptVector(il.begin(),il.end())
        {

        }
    ~LeptVector()=default;

    LeptVector& operator=(const LeptVector& other)
    {
        if(this==&other)
            return *this;
        else
        {
            assign(other.begin(),other.end());
            return *this;
        }
        
    }
    LeptVector& operator=(LeptVector&& other)
    {
        if(this==&other)
            return *this;
        // moveFrom(std::move(other),moveIs)
        return *this;
    }
    LeptVector& operator=(std::initializer_list<T> il)
    {
        assign(il.begin(),il.end());
        return *this;
    }

    template<class It,class Category=typename std::iterator_traits<It>::iterator_category>
    void assign(It first,It last)
    {
        assign(first,last,Category());
    }

    void assign(size_type n,VT value)
    {
        
    }
    void assign(std::initializer_list<T> il)
    {
        assign(il.begin(),il.end());
    }

private:
    template<typename ForwardIterator>
    LeptVector(ForwardIterator first,ForwardIterator last,std::forward_iterator_tag)
        : impl_(size_type(std::distance(first,last)))
    {
        M_uninitialized_copy_e(first,last);
    }
    template<typename InputIterator>
    LeptVector(InputIterator first,InputIterator last,std::input_iterator_tag)
        : impl_()
    {
        // M_uninitialized_copy_e(first,last);
    } 

    void moveFrom(LeptVector&& other,std::true_type)
    {
        swap(impl_,other.impl_);
    }
    void moveFrom(LeptVector&& other,std::false_type)
    {
        if(impl_==other.impl_)
        {
            impl_.swapData(other.impl_);
        }
        else
        {
            impl_.reset(other.size());
            M_uninitialized_move_e(other.begin(),other.end());
        }
        
    }

    template<class ForwardIterator>
    void assign(ForwardIterator first,
                ForwardIterator last,
                std::forward_iterator_tag)
    {
        const auto newsize=size_type(std::distance(first,last));
        if(newsize>capacity())
        {
            impl_.reset(newsize);
            M_uninitialized_copy_e(first,last);
        }
        else if (newsize<=capacity())
        {
            auto newend=std::copy(first,last,impl_.b_);
            M_destory_range_e(newend);
        }      
    }

    template<class InputIterator>
    void assign(InputIterator first,InputIterator last,std::input_iterator_tag)
    {
        auto p=impl_.b_;
        for (;first!=last && p!=impl_.e_ ;++first,++p)
        {
            *p=*first;
        }
        if(p!=impl_.e_)
        {
            M_destory_range_e(p);
        }
        else
        {
            for (; first!=last;++first)
            {
                // emp
            }
            
        }
        
        
    }

    bool dataIsInternalAndNotVt(const T& t)
    {
        if(should_pass_by_value)
            return false;
        return dataIsInternal(t);
    }
    bool dataIsInternal(const T& t)
    {
        return (impl_.b_ <= std::addressof(t) && std::addressof(t)<=impl_.e_);
    }


public:
    iterator begin() noexcept
    {
        return impl_.b_;
    }
    
    const_iterator begin() const noexcept
    {
        return impl_.b_;
    }

    iterator end() noexcept
    {
        return impl_.e_;
    }

    const_iterator end() const noexcept
    {
        return impl_.e_;
    }

    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    
    const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(begin());
    }

    const_iterator cbegin() const noexcept
    {
        return impl_.b_;
    }

    const_iterator cend() const noexcept
    {
        return impl_.e_;
    }



public:
    size_type size() const noexcept
    {
        return size_type(impl_.e_-impl_.b_);
    }

    void resize(size_type n)
    {
        if(n<=size())
            M_destory_range_e(impl_.b_+n);
        else
        {
            reserve(n);
            M_uninitialized_fill_n_e(n-size());
        }
        
    }

    void resize(size_type  n,VT  t)
    {
        if(n<=size())
            M_destory_range_e(impl_.b_+n);
        else if(dataIsInternalAndNotVt(t) && n > capacity())
        {
           T copy(t);
           reserve(n;
           M_uninitialized_fill_n_e(n-size(),copy);
        }
        else
        {
            reserve(n);
            M_uninitialized_fill_n_e(n-szie(),t);
        }
    }

    size_type capacity() const noexcept
    {
        return  size_type(impl_.z_-impl_.b_);
    }

    bool empty() const noexcept
    {
        return impl_.b_==impl_.e_;
    }

    void reserve(size_type n)
    {
        if(n<=capacity())
            return;
        else
        {
            auto newCap=n;
            auto newB=M_allocate(newCap);
            M_relocate(newB);
        }
        if(impl_.b_)
        {
            M_Deallocate(impl_.b_,size_type(impl_.z_-impl_.b_));
        }
        impl_.z_=newB+newCap;
        impl_.e_=newB+size_type(impl_.e_-impl_.b_);
        impl_.b_=newB;  
    }

public:
    reference operator[] (size_type n)
    {
        assert(n<=szie());
        return impl_.b_[n];
    }
    const_reference operator[] (size_type n)
    {
        assert(n<=size());
        return impl_.b_[n];
    }
    
    reference front()
    {
        assert(!empty());
        return *impl_.b_;
    }
    const_reference front() const
    {
        assert(!empty());
        return *impl_.b_;
    }
    reference back()
    {
        assert(!empty());
        return *impl_.e_[-1];
    }
    const_reference back() const
    {
        assert(!empty());
        return *impl_.e_[-1];
    }

public:

    T* data() noexcept
    {
        return impl_.b_;
    }
    const T* data() const noexcept
    {
        return impl_.b_;
    }

    template<typename ... Args>
    reference emplace_back(Args&&... args)
    {
        if(impl_.e_!=impl_.z_)
        {
            M_construct(impl_.e_,std::forward<Args>(args)...);
            ++impl_.e;
        }
        else
        {
            emplace_back_aux(std::forward<Args>(args)...);
        }
        return back();  
    }
    void push_back(const T& value)
    {
        if(impl_.e_!=impl_.z_)
        {
            M_constuct(impl_.e_,value);
            ++impl_.e_;
        }
        else
        {
            emplace_back_aux(value);
        } 
    }
    void push_back(T&& value)
    {
        if(impl_.e_!=impl_.z_)
        {
            M_constuct(impl_.e_,std::move(value));
            ++impl_.e_;
        }
        else
        {
            emplace_back_aux(std::move(value));
        }
    }

    void pop_back()
    {
        assert(!empty());
        --impl_.e_;
        M_destroy(impl_.e_);
    }
    void swap(LeptVector& other) noexcept
    {
        impl_.swapData(other.impl_);
    }
    void clear() noexcept
    {
        M_destory_range_e(impl_.b_);
    }


private:
    size_type computePushBackCapacity() const
    {
        if(capacity()==0)
            return std::max(64/sizeof(T),size_type(1));   
        if(capacity()<4096/sizeof(T))
            return capacity()*2;
        if(capacity>4096*32/sizeof(T))
            return capacity()*2;
        return (capacity()*3+1)/2;    
    }
    template<typename... Args>
    void empalce_back_aux(Args&& ...args)
    {
        size_type byte_sz=computePushBackCapacity()*sizeof(T);
        size_type sz=byte_sz/sizeof(T);
        auto newB=M_allocate(sz);
        auto newE=newB+size();
        relocate_move(newB,impl_.b_,impl_.e_);
        M_construct(newE,std::forward<Args>(args)...);
        ++newE;
        if(impl_.b_)
        {
            M_Deallocate(impl_.b_,size());
        }
        impl_.b_=newB;
        impl_.e_=newE;
        impl_.z_=newB+sz;    
    }

public:
    iterator erase(const_iterator position)
    {
        return erase(position,position+1);
    }
    iterator erase(const_iterator first,const_iterator last)
    {
        assert(isValid(first)&&isValid(last));
        assert(first<=last);
        if(first!=last)
        {
            if(last==end())
            {
                M_destory_range_e((iterator)first);               
            }
            else
            {
                if(last-first>=cend()-last)
                    std::memcpy((void*)first,(void*)last,(cend()-last)*sizeof(T));
                else    
                    std::memmove((void)*first,(void*)last,(cend()-last)*sizeof(T));
                impl_.e_-=(last-first);           
            }
        }
        return (iterator)first;
    }

private:
    bool isValid(const_iterator it)
    {
        return cbegin()<= it && it<= cend();
    }

    size_type computeInsertCapacity(size_type n)
    {
        // size_type nc=std::max(computePushBackCapacity(),size()+n)
    }
};





#endif