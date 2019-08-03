#include"Futex.h"
#include<thread>
#include<iostream>
#include<unistd.h>
Futex a(1);
uint32_t mask=1;
void f()
{
    std::cout<<"in thread\n"<<std::flush;
    futexWait(&a,1,1);
    std::cout<<"end sleep\n"<<std::flush;
}
int main(int argc, char const *argv[])
{
    std::thread t(f);
    sleep(1);
    futexWake(&a,1,1);
    t.join();
    return 0;
}

