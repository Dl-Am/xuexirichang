#include "ThreadPool.hpp"
//这里两种方法都可以
//#define NUMBER 2
const int NUMBER = 2;
ThreadPool::ThreadPool(int minNum,int maxNum)
{
    //实例化任务队列
    m_taskQ = new TaskQueue;
    do
    {
        //初始化线程池
        m_minNum = minNum;
        m_maxNum = maxNum;
        m_busyNum = 0;
        m_liveNum = minNum;
        //根据最大线程数来给线程数组分配空间
        m_threadIDs = new pthread_t[maxNum];
        if(m_threadIDs == nullptr)
        {
            std::cout<<"malloc thread_t[] 失败..."<<std::endl;
            break;
        }
        //将一段空间里面的值清零
        memset(m_threadIDs,0,sizeof(pthread_t)*maxNum);
        //初始化互斥锁和条件变量
        if(pthread_mutex_init(&m_lock,NULL) != 0 || 
        pthread_cond_init(&m_notEmpty,NULL) != 0)
        {
            std::cout<<"init mutex or cond fail..."<<std::endl;
            break;
        }
        //创建消费者线程
        int i = 0;
        for(i=0;i<minNum;++i)
        {
            pthread_create(&m_threadIDs[i],NULL,worker,this);
            std::cout<<"创建子线程,ID: "<<std::to_string(m_threadIDs[i])<<std::endl;
        }  
        //创建管理者线程
        pthread_create(&m_managerID,NULL,manager,this);
    } while (0);  
}

ThreadPool::~ThreadPool()
{
    //m_shutdown = 1表示要销毁线程池
    m_shutdown = 1;
    //阻塞回收管理者线程
    pthread_join(m_managerID,NULL);
    //唤醒消费者线程
    int i = 0;
    for(i=0;i<m_liveNum;++i)
    {
        pthread_cond_signal(&m_notEmpty);
    }
    //释放空间
    if(m_taskQ)
        delete m_taskQ;
    if(m_threadIDs)
        delete[]m_threadIDs;
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_notEmpty);
}

void ThreadPool::addTask(Task task)
{
    //判断一下线程池是否被销毁
    if(m_shutdown)
    {
        return ;
    }
    //走到这里,说明线程池没有被销毁,可以添加任务,不需要加锁,任务队列对面有锁
    m_taskQ->addTask(task);
    //唤醒消费线程(工作线程)
    pthread_cond_signal(&m_notEmpty);
}

int ThreadPool::getBusyNumber()
{
    int busynum = 0;
    //做完事情立即进行解锁,保证粒度最小
    pthread_mutex_lock(&m_lock);
    busynum = m_busyNum;
    pthread_mutex_unlock(&m_lock);
    return busynum;
}

int ThreadPool::getLiveNumber()
{
    int livenum = 0;
    //做完事情立即进行解锁,保证粒度最小
    pthread_mutex_lock(&m_lock);
    livenum = m_liveNum;
    pthread_mutex_unlock(&m_lock);
    return livenum;
}

void* ThreadPool::worker(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);

    while(true)
    {
        //访问共享资源,需要加锁
        pthread_mutex_lock(&pool->m_lock);
        //判断一下任务数是不是为0,如果是0的话就阻塞一下消费者线程
        while(pool->m_taskQ->taskNumber() == 0 && pool->m_shutdown == 0)
        {
            std::cout<<"thread"<<std::to_string(pthread_self())<<"wait..."<<std::endl;
            //阻塞消费者线程
            pthread_cond_wait(&pool->m_notEmpty,&pool->m_lock);
            //判断是否要销毁线程
            if(pool->m_exitNum > 0)
            {
                pool->m_exitNum--;
                if(pool->m_liveNum > pool->m_minNum)
                {
                    pool->m_liveNum--;
                    pthread_mutex_unlock(&pool->m_lock);
                    pool->threadExit();
                }
            }
        }
        //判断线程池是否关闭
        if(pool->m_shutdown)
        {
            pthread_mutex_unlock(&pool->m_lock);
            pool->threadExit();
        }
        //从任务队列里面取任务
        Task task = pool->m_taskQ->takeTask();
        pool->m_busyNum++;
        pthread_mutex_unlock(&pool->m_lock);
        //执行任务
        std::cout<<"thread"<<std::to_string(pthread_self())<<"start working..."<<std::endl;
        task.function(task.arg);
        //注意一下delete void* 是不能直接进行delete的,需要进行强转一下
        //delete (ThreadPool*)task.arg;
        delete task.arg;
        task.arg = nullptr;
        //任务处理结束
        std::cout<<"thread"<<std::to_string(pthread_self())<<"end working..."<<std::endl;
        pthread_mutex_lock(&pool->m_lock);
        pool->m_busyNum--;
        pthread_mutex_unlock(&pool->m_lock);
    }
    return nullptr;
}

void* ThreadPool::manager(void* arg)
{
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    //判断线程池没有被销毁就一直检测
    while(pool->m_shutdown == 0)
    {
        sleep(3);
        //获取线程池中的任务数,存活线程数和忙线程数
        pthread_mutex_lock(&pool->m_lock);
        int queuesize = pool->m_taskQ->taskNumber();
        int livenum = pool->m_liveNum;
        int busynum = pool->m_busyNum;
        pthread_mutex_unlock(&pool->m_lock);
        //创建线程
        //这里也是前面c语言实现一些简单的算法,不是真正的实现算法
        if(queuesize > livenum && livenum < pool->m_maxNum)
        {
            pthread_mutex_lock(&pool->m_lock);
            int num = 0;
            int i = 0;
            for(i=0;i<pool->m_maxNum && num<NUMBER
             && pool->m_liveNum < pool->m_maxNum;++i)
            {
                if(pool->m_threadIDs[i] == 0)
                {
                    pthread_create(&pool->m_threadIDs[i],NULL,worker,pool);
                    num++;
                    pool->m_liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->m_lock);
        }

        if(busynum * 2 < livenum && livenum > pool->m_minNum)
        {
            pthread_mutex_lock(&pool->m_lock);
            pool->m_exitNum = NUMBER;
            pthread_mutex_unlock(&pool->m_lock);
            int i = 0;
            for(i=0;i<NUMBER;++i)
            {
                pthread_cond_signal(&pool->m_notEmpty);
            }
        }
    }
    return nullptr;
}

void ThreadPool::threadExit()
{
    pthread_t tid = pthread_self();
    int i = 0;
    for(i=0;i<m_maxNum;++i)
    {
        if(m_threadIDs[i] == tid)
        {
            std::cout<<"threadExit() function: thread"<<
            std::to_string(pthread_self())<<"exiting..."<<std::endl;
            m_threadIDs[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}
