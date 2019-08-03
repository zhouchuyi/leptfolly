#include<iostream>
int main(int argc, char const *argv[])
{
    const char* str="\n";
    if(str[0]=='\\')
        std::cout<<*(str+1);
    return 0;
}
