#include <iostream>
#include "processpool.h"

namespace {
const char *kWorkerProcessString = "ProcessPool::IAmAWorkerProcess";
const char *kChildInitString = "Child process reporting for duty";
}

void ProcessPool::Resize( int _size ) {
    int old_size = (int)processes_.size();
    for(int i=_size; i<old_size; ++i){
        delete processes_[i];
    }
    processes_.resize(_size, NULL);
    for(int i=old_size; i<_size; ++i){
        processes_[i] = new ProcessHandle();
    }
}

int ProcessPool::GetIdleProcessIndex() {
    int num_processes = (int)processes_.size();
    for(int i=0; i<num_processes; ++i){
        if(!processes_[i]->idle()){
            return i;
        }   
    }
    return -1;
}

ProcessPool::Error ProcessPool::ProcessFirstTaskInQueue() {
    if(tasks_.empty()){
        return ProcessPool::NO_TASK_IN_QUEUE;
    }
    int idle_process = GetIdleProcessIndex();
    if(idle_process == -1){
        return ProcessPool::NO_IDLE_PROCESS;   
    }
    processes_[idle_process]->Process(tasks_.front());
    tasks_.pop();
    return ProcessPool::SUCCESS;
}

bool ProcessPool::AmIAWorkerProcess( int argc, char* argv[] ) {
    return (argc >= 2 && strcmp(argv[1],kWorkerProcessString));
}

ProcessPool::ProcessPool( int size ) {
    Resize(size);
}

ProcessPool::~ProcessPool() {
    Resize(0);
}

void ProcessPool::Schedule( const std::string& task ) {
    tasks_.push(task);
    ProcessFirstTaskInQueue();
}

ProcessHandle::ProcessHandle()
    :idle_(true)
{}

void ProcessHandle::Process( const std::string& task ) {

}

#ifdef _WIN32

namespace {
    std::wstring WStrFromCStr(const char* cstr){
        int cstr_len = (int)strlen(cstr);
        std::wstring wstr;
        wstr.resize(cstr_len);
        for(int i=0; i<cstr_len; ++i){
            wstr[i] = cstr[i];
        }
        return wstr;
    }
    const int kPathBufferSize = 1024;
    const int kPipeBufSize = 256;
    
    void ErrorExit(const char* msg){
        MessageBox(NULL, msg, TEXT("Error"), MB_OK); 
        ExitProcess(1);
    }
}


OSProcess::OSProcess() {
    std::cout << "Creating OS Process" << std::endl;
    wchar_t path_utf16_buf[kPathBufferSize];
    GetModuleFileNameW( NULL, path_utf16_buf, kPathBufferSize);

    // This uses UTF16 for the first arg and UTF8 for subsequent args
    std::wstring param; 
    param.reserve(kPathBufferSize);
    param += path_utf16_buf;
    param += L" ";
    param += WStrFromCStr(kWorkerProcessString);

    STARTUPINFOW startup_info;  
    memset(&startup_info, 0, sizeof(startup_info)); 
    memset(&process_info_, 0, sizeof(process_info_)); 
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags |= STARTF_USESTDHANDLES;
    
    SECURITY_ATTRIBUTES inheritable; 
    inheritable.nLength = sizeof(SECURITY_ATTRIBUTES); 
    inheritable.bInheritHandle = TRUE; 
    inheritable.lpSecurityDescriptor = NULL; 

    // Create pipes for communication with child
    if(!CreatePipe(&read_pipe_, &startup_info.hStdOutput, &inheritable, 0)){
        ErrorExit("Error creating first child process pipe");
    }
    if(!CreatePipe(&startup_info.hStdInput, &write_pipe_, &inheritable, 0)){
        ErrorExit("Error creating second child process pipe");
    }
    startup_info.hStdError = startup_info.hStdOutput;
    
    // Set parent end of pipes to not be inheritable
    if(!SetHandleInformation(read_pipe_, HANDLE_FLAG_INHERIT, 0)){
        ErrorExit("Error setting inherit properties of first child process pipe");
    }
    if(!SetHandleInformation(write_pipe_, HANDLE_FLAG_INHERIT, 0)){
        ErrorExit("Error setting inherit properties of second child process pipe");
    }

    CreateProcessW(path_utf16_buf, 
        &param[0], 
        NULL, NULL, true, 
        NORMAL_PRIORITY_CLASS,
        NULL, NULL, &startup_info,
        &process_info_);
        
    DWORD dwRead, dwWritten; 
    CHAR chBuf[kPipeBufSize]; 
    BOOL bSuccess = FALSE;
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

    int offset = 0;
    bool continue_reading = true;
    while(continue_reading){
        bSuccess = ReadFile( read_pipe_, &chBuf[offset], kPipeBufSize, &dwRead, NULL);
        offset += dwRead;
        for(int i=0; i<offset; ++i){
            if(chBuf[i] == '\0'){
                continue_reading = false;
            }
        }
    }
    if(!bSuccess || strcmp(chBuf, kChildInitString) != 0){
        ErrorExit("Problem communicating with child process");
    }
}

OSProcess::~OSProcess() {
    CloseHandle(read_pipe_);
    CloseHandle(write_pipe_);
    CloseHandle(process_info_.hProcess);
    CloseHandle(process_info_.hThread);
}

int ProcessPool::WorkerProcessMain(JobMap job_map) {
    CHAR end_str[1];
    end_str[0] = '\0';
    DWORD  num_written;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), kChildInitString, strlen(kChildInitString), &num_written, NULL);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), end_str, 1, &num_written, NULL);
    return 0;
}

#endif //_WIN32