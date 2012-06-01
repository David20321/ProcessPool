#include <iostream>
#include <cstdio>
#include "processpool.h"

using namespace std;

int main(int argc, char* argv[]){
    ProcessPool process_pool("worker.exe", 4);
    process_pool.Schedule("CreateFile \"the_file.txt\"");
    process_pool.Schedule("CreateFile \"the_      file.txt\"");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    process_pool.Schedule("CreateFile \"the_file.txt\" \"Other file.txt\" param1 param2");
    cout << "Main process: Waiting for tasks to finish..." << endl;
    process_pool.WaitForTasksToComplete();
    cout << "Main process: All tasks completed!" << endl << "Press enter to quit" << endl;
    getchar();
    return 0;
}
