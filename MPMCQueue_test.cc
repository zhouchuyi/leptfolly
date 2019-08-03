#include"MPMCQueue.h"
#include<iostream>
#include<vector>
#include<thread>
// #include<boost/lockfree/queue.hpp>
// SingleElementQueue<int> q;
// std::atomic<uint32_t> spincutoff(2000);
// std::atomic<uint32_t> put_cnt(0);
// std::atomic<uint32_t> get_cnt(0);
// boost::lockfree::queue<int> b_q(128);
MPMCQueue<int> q(24);
void get()
{
    // uint32_t turn_;
    // int data_;
    // while ((turn_=get_cnt.fetch_add(1))<50000)
    // {
    //     q.enqueue(turn_,spincutoff,false,1);
    //     // b_q.pop(data_);
    // }
    size_t cnt;
    int data_;
    while (cnt++<500000)
    {
        // while (!q.read(data_));
        if(!q.read(data_))
            q.blockingRead(data_);
        // b_q.pop(data_);
    }
}

void put()
{
    // uint32_t turn_;
    // int data_;
    // while ((turn_=put_cnt.fetch_add(1))<50000)
    // {
    //     q.dequeue(turn_,spincutoff,false,data_);
    //     // b_q.push(1);
    // }
    size_t cnt;
    while (cnt++<500000)
    {
        // while (!q.write(1));        
        if(!q.write(1))
        q.blockingWrite(1);
        // b_q.push(1);
    }

}
int main(int argc, char const *argv[])
{
    // SingleElementQueue<int> q;
    // std::atomic<uint32_t> spincutoff(0);
    // int data_;
    // for (size_t i = 0; i < 50000; i++)
    // {
    //     q.enqueue(i,spincutoff,true,i);
    //     q.dequeue(i,spincutoff,true,data_);
    // }
    std::vector<std::thread> put_threads;
    std::vector<std::thread> get_threads;
    for (size_t i = 0; i < 2; i++)
    {
        put_threads.push_back(std::thread(put));
        get_threads.push_back(std::thread(get));
    }
    for (size_t i = 0; i < 2; i++)
    {
        put_threads[i].join();
        get_threads[i].join();
    }
    // for (size_t i = 0; i < 100; i++)
    // {
    //     q.blockingWrite(1);
    // }
    // int data_;
    // for (size_t i = 0; i < 100; i++)
    // {
    //     q.blockingRead(data_);
    // }
    return 0;
}
