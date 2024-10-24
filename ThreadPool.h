#pragma once
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
//任务结构体
typedef struct Task
{
    void (*function)(void* arg);
    void* arg;
}Task;
//线程池结构体
typedef struct ThreadPool
{
    //任务队列
    Task* taskQ;
    int queueCapacity;  //容量
    int queueSize;      //任务个数
    int queueFront;     //队头(取数据)
    int queueRear;      //队尾(放数据)

    pthread_t managerID;    //管理者线程ID
    pthread_t* threadIDs;   //工作的线程ID
    int minNum;             //最小线程数
    int maxNum;             //最大线程数
    int busyNum;            //忙的线程数
    int liveNum;            //存活的线程数
    int exitNum;            //要销毁的线程数
    pthread_mutex_t mutexPool;  //锁整个线程池
    pthread_mutex_t mutexBusy;  //锁忙线程数变量
    pthread_cond_t notFull;     //任务队列是不是满了
    pthread_cond_t notEmpty;    //任务队列是不是空了

    int shutdown;   //是不是要销毁线程池,销毁为1,不销毁为0
}ThreadPool;
//声明一下
//typedef struct ThreadPool ThreadPool;   
//创建线程池并进行初始化
ThreadPool* threadPoolCreate(int min,int max,int queueSize);
//销毁线程池
int threadPoolDestory(ThreadPool* pool);
//往线程池里面添加任务
void threadPoolAdd(ThreadPool* pool,void(*func)(void*),void* arg);
//获取线程池中忙的线程数
int threadPoolBusyNum(ThreadPool* pool);
//获取线程池中存活的线程数
int threadPoolLiveNum(ThreadPool* pool);
//消费者线程任务函数
void* worker(void* arg);
//管理者线程任务函数
void* manager(void* arg);
//单个线程退出
void threadExit(ThreadPool* pool);