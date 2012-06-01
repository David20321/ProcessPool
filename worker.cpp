#include <iostream>
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
    if(!ProcessPool::AmIAWorkerProcess(argc, argv)){
        exit(1);
    }
    ProcessPool::JobMap jobs;
    jobs["CreateFile"] = &CreateFile;
    return ProcessPool::WorkerProcessMain(jobs);
}
