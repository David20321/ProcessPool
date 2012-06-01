#pragma once
#ifndef INTERNAL_PROCESS_POOL_H
#define INTERNAL_PROCESS_POOL_H

#include <string>
#include <vector>
#include <queue>
#include <map>
#include "disallow_copy_and_assign.h"

#ifdef _WIN32
#include <windows.h>
class OSProcess {
    public:
        OSProcess(const std::string &worker_path);
        ~OSProcess();
        
        std::string WaitForChildMessage();
        void SendMessageToChild(const std::string &msg);
    private:
    
        HANDLE read_pipe_;
        HANDLE write_pipe_;
        PROCESS_INFORMATION process_info_;
    
        DISALLOW_COPY_AND_ASSIGN(OSProcess);
};
#else 
#include <unistd.h>
class OSProcess {
public:
    OSProcess(const std::string &worker_path);
    ~OSProcess();
    
    std::string WaitForChildMessage();
    void SendMessageToChild(const std::string &msg);
private:
    int read_pipe_;
    int write_pipe_;
    int process_info_;
    
    DISALLOW_COPY_AND_ASSIGN(OSProcess);
};
#endif //_WIN32

class ProcessPool;
class ProcessHandle {
public:
    void Process(const std::string& task);
    inline bool idle(){return idle_;}
    void ProcessInBackground();
    
    ProcessHandle(const std::string &worker_path, ProcessPool* parent_process_pool);
   
private:
    bool idle_;
    OSProcess os_process_;
    ProcessPool* parent_process_pool_;
    
    bool ProcessMessageFromChild(const std::string& msg);
    DISALLOW_COPY_AND_ASSIGN(ProcessHandle);
};


class ProcessPool {
public:
    // Change number of process handlers
    void Resize(int _size);
    
    typedef int (*JobFunctionPtr)(int argc, const char* argv[]);
    typedef std::map<std::string, ProcessPool::JobFunctionPtr> JobMap;
    enum Error{SUCCESS = 0,
               NO_IDLE_PROCESS = -1,
               NO_TASK_IN_QUEUE = -2};
    static bool AmIAWorkerProcess( int argc, char* argv[] );
    static int WorkerProcessMain(const JobMap &job_map);

    void NotifyTaskComplete();
    void Schedule(const std::string& task);
    void WaitForTasksToComplete();
    ProcessPool(const std::string &worker_path, int size=0);
    ~ProcessPool();
private:
    
    // Attempts to start processing the first task in the queue in the first
    // idle process. Can return SUCCESS, NO_IDLE_PROCESS or NO_TASK_IN_QUEUE. 
    ProcessPool::Error ProcessFirstTaskInQueue();
    
    // Returns index of the first idle process in pool, or -1 if there
    // are no idle processes
    int GetIdleProcessIndex();
    
    std::string worker_path_;
    std::vector<ProcessHandle*> processes_;
    std::queue<std::string> tasks_;
#ifdef _WIN32
    HANDLE mutex_;
    HANDLE idle_event_;
#else
    pthread_mutex_t mutex_;
#endif
    
    DISALLOW_COPY_AND_ASSIGN(ProcessPool);
};

#endif //INTERNAL_PROCESS_POOL_H