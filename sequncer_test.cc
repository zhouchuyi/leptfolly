#include<iostream>
#include<vector>
#include<thread>
#include"TurnSequncer.h"
std::vector<int> vec_;
TurnSequncer seq_;
std::atomic<uint32_t> spin(200);
std::atomic<uint32_t> put_cnt(0);
std::atomic<uint32_t> get_cnt(0);
void get()
{
    uint32_t turn;
    while((turn=get_cnt.fetch_add(1))<50000)
    {
        
        seq_.waitForTurn(turn*2+1,spin,false);
        vec_.pop_back();
        seq_.completeTurn(turn*2+1);
    }
    
}
void put()
{
    uint32_t turn;
    while((turn=put_cnt.fetch_add(1))<50000)
    {
        seq_.waitForTurn(turn*2,spin,false);
        vec_.push_back(1);
        seq_.completeTurn(turn*2);
    }
    
}
int main(int argc, char const *argv[])
{
    // std::thread t_get(get);
    // std::thread t_put(put);
    // t_get.join();
    // t_put.join();
    
    std::vector<std::thread> put_threads;
    std::vector<std::thread> get_threads;
    for (size_t i = 0; i < 4; i++)
    {
        put_threads.push_back(std::thread(put));
        get_threads.push_back(std::thread(get));
    }
    for (size_t i = 0; i < 4; i++)
    {
        put_threads[i].join();
        get_threads[i].join();
    }
    
    return 0;
}
