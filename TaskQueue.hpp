#pragma once
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <queue>
using callback = void(*)(void*);
class Task
{
public:
    Task()
    {
        function = nullptr;
        arg = nullptr;
    }
    Task(callback f,void* arg)
    {
        function = f;
        this->arg = arg;
    }
    callback function;
    void* arg;
};

//任务队列
class TaskQueue
{
public:
    //构造函数
    TaskQueue();
    //析构
    ~TaskQueue();
    //添加任务
    void addTask(Task& task);
    void addTask(callback func,void* arg);
    //取出一个任务
    Task takeTask();
    //获取当前队列任务数
    inline int taskNumber()
    {
        return m_queue.size();
    }
private:
    pthread_mutex_t m_mutex;    //互斥锁
    std::queue<Task> m_queue;   //任务队列
};