#include "ThreadPool.h"
const int NUMBER = 2;
ThreadPool* threadPoolCreate(int min,int max,int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do
    {
        if(pool == NULL)
        {
            printf("malloc threadpool fail...\n");
            break;
        }
        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t)*max);
        if(pool->threadIDs == NULL)
        {
            printf("malloc threadIDs fail...\n");
            break;
        }
        memset(pool->threadIDs,0,sizeof(pthread_t)*max);
        pool->busyNum = 0;
        pool->minNum = min;
        pool->maxNum = max;
        pool->liveNum = min;
        pool->exitNum = 0;

        if(pthread_mutex_init(&pool->mutexPool,NULL) != 0 || 
        pthread_mutex_init(&pool->mutexBusy,NULL) != 0 || 
        pthread_cond_init(&pool->notEmpty,NULL) != 0 ||
        pthread_cond_init(&pool->notFull,NULL) != 0) 
        {
            printf("mutex or condition init fail...\n");
            break;
        }

        //任务队列
        pool->taskQ = (Task*)malloc(sizeof(Task)*queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;
        //创建线程
        pthread_create(&pool->managerID,NULL,manager,pool);
        int i = 0;
        for(i=0;i<min;++i)
        {
            pthread_create(&pool->threadIDs[i],NULL,worker,pool);
        }
        return pool;
    }while(0);
    
    if(pool && pool->threadIDs)
        free(pool->threadIDs);
    if(pool && pool->taskQ)
        free(pool->taskQ);
    if(pool)
        free(pool);

    return NULL;
}

int threadPoolDestory(ThreadPool* pool)
{
    if(pool == NULL)
    {
        return -1;
    }
    //关闭线程池
    pool->shutdown = 1;
    //阻塞回收管理者线程
    pthread_join(pool->managerID,NULL);
    //唤醒阻塞的消费者线程
    int i = 0;
    for(i=0;i<pool->liveNum;++i)
    {
        pthread_cond_signal(&pool->notEmpty);
    }

    if(pool->taskQ)
    {
        free(pool->taskQ);
    }
    if(pool->threadIDs)
    {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;
    return 0;
}

void threadPoolAdd(ThreadPool* pool,void(*func)(void*),void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    //判断一下任务队列是不是满的
    while(pool->queueCapacity == pool->queueSize && pool->shutdown == 0)
    {
        //如果满了就阻塞生产者线程
        pthread_cond_wait(&pool->notFull,&pool->mutexPool);
    }
    if(pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return ;
    }
    //添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    //环形队列的思想
    pool->queueRear = (pool->queueRear+1)%pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busynum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busynum;
}

int threadPoolLiveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int livenum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return livenum;
}

void* worker(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;

    while(1)
    {
        pthread_mutex_lock(&pool->mutexPool);
        //判断一下当前的任务队列是不是空的
        while(pool->queueSize == 0 && pool->shutdown == 0)
        {
            //如果是空的就阻塞消费者线程
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);
            //判断一下是不是要销毁线程
            if(pool->exitNum > 0)
            {
                pool->exitNum--;
                if(pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }
        //从任务队列中取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        //移动头结点,环形队列的思想，和移动尾节点的思想是一样的
        pool->queueFront = (pool->queueFront+1)%pool->queueCapacity;
        pool->queueSize--;

        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        printf("thread %ld start working...\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;

        printf("thread %ld end working...\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void* manager(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    while(1)
    {
        //每隔3秒检查一下
        sleep(3);
        //取出线程池中任务数和存活的线程数
        pthread_mutex_lock(&pool->mutexPool);
        int queuesize = pool->queueSize;
        int livenum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);
        //取出忙线程数
        pthread_mutex_lock(&pool->mutexBusy);
        int busynum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        //添加线程
        //这里,在源码中是有一套算法的,在这里简单实现一下其他的一些方法
        //任务数 > 存活数  &&  存活数 < 最大线程数
        if(queuesize > livenum && livenum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            int i = 0;
            for(i=0;i<pool->maxNum && counter < NUMBER 
            && pool->liveNum < pool->maxNum;++i)
            {
                if(pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }

        //销毁线程
        //忙的线程 * 2 < 存活的线程数  &&  存活的线程数 > 最小线程数
        if(busynum * 2 < livenum && livenum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //让消费者线程自杀
            int i = 0;
            for(i=0;i<NUMBER;++i)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return ;
}

void threadExit(ThreadPool* pool)
{
    pthread_t tid = pthread_self();
    int i = 0;
    for(i=0;i<pool->minNum;++i)
    {
        if(pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n",tid);
            break;
        }
    }

    pthread_exit(NULL);
}
