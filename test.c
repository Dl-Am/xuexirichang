#include "ThreadPool.h"

void taskfunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is working,number = %d\n",pthread_self(),num);
    sleep(1);
}

int main()
{
    ThreadPool* pool = threadPoolCreate(3,10,100);
    int i = 0;
    for(i=0;i<100;++i)
    {
        int* num = (int*)malloc(sizeof(int));
        *num = i+100;
        threadPoolAdd(pool,taskfunc,num);
    }
    return 0;
}