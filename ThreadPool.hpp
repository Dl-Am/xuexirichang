#pragma once
#include "TaskQueue.hpp"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
class ThreadPool
{
public:
    //有参构造函数
    ThreadPool(int min,int max);
    //析构
    ~ThreadPool();
    //添加任务
    void addTask(Task task);
    //获取忙线程数
    int getBusyNumber();
    //获取存活的线程数
    int getLiveNumber();
private:
    //工作(消费)线程的任务函数
    static void* worker(void* arg);
    //管理者线程的任务函数
    static void* manager(void* arg);
    //线程退出
    void threadExit();

private:
    TaskQueue* m_taskQ;
    pthread_t* m_threadIDs;
    pthread_t m_managerID;
    int m_minNum;
    int m_maxNum;
    int m_busyNum;
    int m_liveNum;
    int m_exitNum;
    pthread_mutex_t m_lock;
    pthread_cond_t m_notEmpty;
    bool m_shutdown = false;
};