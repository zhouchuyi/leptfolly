#include<iostream>
#include<thread>
#include<chrono>
#include<condition_variable>
#include"leptVar.h"

// ThreadLocal<int> T;
std::condition_variable con_;
std::mutex mutex_;
int flag = 0;
leptVar<int> var;

void add()
{
    for (size_t i = 0; i < 50; i++)
    {
        ++var;
    }
    {
            std::lock_guard<std::mutex> lock(mutex_);
            flag++;
            con_.notify_one();

    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
}

void add_forever()
{
    while (true)
    {
        ++var;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
}

int main(int argc, char const *argv[])
{
    std::vector<std::thread> threads_;
    threads_.emplace_back(add_forever);
    threads_.emplace_back(add_forever);
    threads_.emplace_back(add_forever);
    // {
    //     std::unique_lock<std::mutex> lock(mutex_);
    //     con_.wait(lock,[](){ return flag == 3; });
    // }
    while (true)
    {
        int total = var.readFull();
        printf("now is %d \n",total);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // int sum = 0;
    // {
    //     auto accessor = T.accessAllThreads();
    //     for (auto it = accessor.begin(); it != accessor.end(); it++)
    //     {
    //         sum += *it;
    //     }
    // }
    // printf("sum = %d \n",sum);
    for (auto &t : threads_)
    {
        t.join();
    }
        
    return 0;

}

