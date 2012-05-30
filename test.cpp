#include <iostream>
#include <string>
#include <map>
#include "processpool.h"

using namespace std;

int CreateFile(int argc, char* argv[]){
    cout << "CreateFile() called with param \"" << argv[0] << "\"" << endl;
    return 0;   
}

int main(int argc, char* argv[]){
    if(argc != 1){//ProcessPool::AmIAWorkerProcess(argc, argv)){
        ProcessPool::JobMap jobs;
        jobs["CreateFile"] = &CreateFile;
        return ProcessPool::WorkerProcessMain(jobs);
    }
    ProcessPool process_pool(4);
    process_pool.Schedule("CreateFile \"the_file.txt\"");
    std::cout << "Test!" << std::endl;
    getchar();
    return 0;
}