#include <iostream>
#include <string>
#include <map>
#include "processpool.h"

using namespace std;

int CreateFile(int argc, const char* argv[]){
    cout << "CreateFile() called with param \"" << argv[0] << "\"" << endl;
#ifdef _WIN32
    Sleep(4000);
#else
    sleep(4);
#endif
    return 0;   
}

int main(int argc, char* argv[]){
    if(ProcessPool::AmIAWorkerProcess(argc, argv)){
        ProcessPool::JobMap jobs;
        jobs["CreateFile"] = &CreateFile;
        return ProcessPool::WorkerProcessMain(jobs);
    }
    ProcessPool process_pool(4);
    process_pool.Schedule("CreateFile \"the_file.txt\"");
    process_pool.Schedule("CreateFile \"the_      file.txt\"");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    getchar();
    return 0;
}