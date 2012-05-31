#include <iostream>
#include <sstream>
#include "processpool.h"

namespace {
const char *kWorkerProcessString = "ProcessPool::IAmAWorkerProcess";
const char *kChildInitString = "ACK_INIT: Child process reporting for duty";
}

namespace {
#ifdef _WIN32
    void ErrorExit(const std::string &msg){
        MessageBox(NULL, msg.c_str(), TEXT("Error"), MB_OK); 
        ExitProcess(1);
    }
#else
    void ErrorExit(const std::string &msg){
        std::cout << msg << std::endl;
        exit(1);
    }    
#endif
}

void ProcessPool::Resize( int _size ) {
    int old_size = (int)processes_.size();
    for(int i=_size; i<old_size; ++i){
        delete processes_[i];
    }
    processes_.resize(_size, NULL);
    for(int i=old_size; i<_size; ++i){
        processes_[i] = new ProcessHandle(this);
    }
}

int ProcessPool::GetIdleProcessIndex() {
    int num_processes = (int)processes_.size();
    for(int i=0; i<num_processes; ++i){
        if(processes_[i]->idle()){
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
    return (argc >= 2 && strcmp(argv[1],kWorkerProcessString) == 0);
}

ProcessPool::ProcessPool( int size ) {
#ifdef _WIN32
    mutex_ = CreateMutex(NULL, false, NULL);
#else
    pthread_mutex_init(&mutex_, NULL);
#endif    
    Resize(size);
}

ProcessPool::~ProcessPool() {
    Resize(0);
}

void ProcessPool::Schedule( const std::string& task ) {
#ifdef _WIN32
    WaitForSingleObject(mutex_,INFINITE);
#else
    if(pthread_mutex_lock(&mutex_)){
        ErrorExit("Error locking mutex");
    }
#endif        
    tasks_.push(task);
    ProcessFirstTaskInQueue();
#ifdef _WIN32
    if(!ReleaseMutex(mutex_)){
        ErrorExit("Error releasing mutex");
    }
#else
    if(pthread_mutex_unlock(&mutex_)){
        ErrorExit("Error releasing mutex");
    }
#endif                   
}

ProcessHandle::ProcessHandle(ProcessPool* parent_process_pool)
    :idle_(true), parent_process_pool_(parent_process_pool)
{}

bool ProcessHandle::ProcessMessageFromChild(const std::string& msg){
    int divider = (int)msg.find(": ");
    if(divider == std::string::npos){
        ErrorExit("No divider in message from child: "+msg);
    }
    std::string type = msg.substr(0, divider);
    std::string body = msg.substr(divider+2, msg.size()-(divider+2));
    if(type == "PRINT"){
        std::cout << body << std::endl;
    }
    if(type == "STATE"){
        if(body == "IDLE"){
            std::cout << "Task complete!" << std::endl;
            return true;
        }
    }
    return false;
}

void ProcessHandle::ProcessInBackground( ) {
    while(!ProcessMessageFromChild(os_process_.WaitForChildMessage())){}
    idle_ = true;
    parent_process_pool_->NotifyTaskComplete();
}

namespace {
#ifdef _WIN32
    DWORD WINAPI ThreadFunc(LPVOID process_handle_ptr){
        ((ProcessHandle*)process_handle_ptr)->ProcessInBackground();
        return 0;
    }
#else
    void* ThreadFunc(void* process_handle_ptr){
        ((ProcessHandle*)process_handle_ptr)->ProcessInBackground();
        return NULL;
    }
#endif
}

void ProcessHandle::Process( const std::string& task ) {
    os_process_.SendMessageToChild("TASK: "+task);
    idle_ = false;
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, ThreadFunc, this, 0, NULL);
    CloseHandle(thread);
#else
    pthread_t thread;
    pthread_create(&thread, NULL, ThreadFunc, this);
#endif
}

namespace {
const int kPathBufferSize = 1024;
const int kPipeBufSize = 256;    
#ifdef _WIN32 
std::wstring WStrFromCStr(const char* cstr){
    int cstr_len = (int)strlen(cstr);
    std::wstring wstr;
    wstr.resize(cstr_len);
    for(int i=0; i<cstr_len; ++i){
        wstr[i] = cstr[i];
    }
    return wstr;
}
#endif

#ifdef _WIN32
void ReadMessageFromPipe(const HANDLE &pipe_handle, std::string *msg){
    DWORD dwRead; 
    CHAR chBuf[kPipeBufSize]; 
    BOOL bSuccess = FALSE;
    std::string &message = *msg;
    message.clear();

    int offset = 0;
    bool continue_reading = true;
    while(continue_reading){
        if(!ReadFile( pipe_handle, chBuf, 1, &dwRead, NULL)){
            ExitProcess(1); // Other process was probably killed
        }
        if(chBuf[0] == '\0'){
            continue_reading = false;
        } else {
            message += chBuf[0];
        }
    }
}
#else
void ReadMessageFromPipe(int pipe_handle, std::string *msg){
    int bytes_read; 
    char buf[1]; 
    std::string &message = *msg;
    message.clear();
    
    bool continue_reading = true;
    while(continue_reading){
        bytes_read = read(pipe_handle, buf, 1);
        if(bytes_read < 0){
            exit(1); // Other process was probably killed
        }
        if(buf[0] == '\0'){
            continue_reading = false;
        } else {
            message += buf[0];
        }
    }
}
#endif
}

    
#ifdef _WIN32
void WriteMessageToPipe(const HANDLE& pipe_handle, const std::string &msg){
    int msg_length = (int)msg.length();
    int total_sent = 0;
    DWORD  num_written;
    while (total_sent < msg_length){
        if(!WriteFile(pipe_handle, &msg[0], (DWORD)msg.length(), &num_written, NULL)){
            ExitProcess(1); // Other process was probably killed
        }
        total_sent += num_written;
    }
    CHAR end_str[] = {'\0'};
    if(!WriteFile(pipe_handle, end_str, 1, &num_written, NULL) || num_written != 1){
        ExitProcess(1); // Other process was probably killed
    }
}
#else
void WriteMessageToPipe(int pipe_handle, const std::string &msg){
    int msg_length = (int)msg.length();
    int total_sent = 0;
    int bytes_written;
    while (total_sent < msg_length){
        bytes_written = write(pipe_handle, &msg[0], (int)msg.length());
        if(bytes_written<0){
            exit(1); // Other process was probably killed
        }
        total_sent += bytes_written;
    }
    char end_str[] = {'\0'};
    bytes_written = write(pipe_handle, end_str, 1);
    if(bytes_written != 1){
        exit(1); // Other process was probably killed
    }
}
#endif

std::string OSProcess::WaitForChildMessage() {
    std::string msg;
    ReadMessageFromPipe(read_pipe_, &msg);
    return msg;
}

#ifdef _WIN32
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
    startup_info.hStdError = GetStdHandle(STD_OUTPUT_HANDLE);
    
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
        
    if(WaitForChildMessage() != kChildInitString){
        ErrorExit("Invalid initial acknowledgement from child process");
    }
}

OSProcess::~OSProcess() {
    CloseHandle(process_info_.hThread);
    TerminateProcess(process_info_.hProcess, 0);
    CloseHandle(process_info_.hProcess);
    CloseHandle(read_pipe_);
    CloseHandle(write_pipe_);
}
#else
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
OSProcess::OSProcess() {
    std::cout << "Creating OS Process" << std::endl;
    
    int pipe_in[2], pipe_out[2];
    if(pipe(pipe_in) == -1){
        ErrorExit("Error creating first pipe");
    }
    if(pipe(pipe_out) == -1){
        ErrorExit("Error creating second pipe");
    }
    
    int pid = fork();
    if(pid == 0){
        if(dup2(pipe_in[0], STDIN_FILENO) == -1){
            ErrorExit("Error assigning child STDIN pipe");
        }
        if(dup2(pipe_out[1], STDOUT_FILENO) == -1){
            ErrorExit("Error assigning child STDOUT pipe");
        }
        if(dup2(pipe_out[1], STDERR_FILENO) == -1){
            ErrorExit("Error assigning child STDERR pipe");
        }
        if(close(pipe_in[0]) == -1){
            ErrorExit("Error closing pipe a");
        }
        if(close(pipe_in[1]) == -1){
            ErrorExit("Error closing pipe b");
        }
        if(close(pipe_out[0]) == -1){
            ErrorExit("Error closing pipe c");
        }
        if(close(pipe_out[1]) == -1){
            ErrorExit("Error closing pipe d");
        }
        
#ifdef __linux___
        std::string path = readlink("/proc/self/exe");
#endif
#ifdef __APPLE__
        std::string path;
        {
            char path_buf[1024];
            uint32_t size = sizeof(path_buf);
            if(_NSGetExecutablePath(path_buf, &size) == 0){
                path = path_buf;
            } else {
                ErrorExit("Could not get application path");
            }
        }
#endif        
        std::vector<char*> args;
        args.push_back((char*)path.c_str());
        args.push_back((char*)kWorkerProcessString);
        args.push_back(NULL);
        if(execv(path.c_str(), &args[0]) == -1){
            ErrorExit("Could not exec: "+path);
        }
    } else if(pid == -1){
        ErrorExit("Problem with fork()");
    }
    
    if(close(pipe_in[0]) == -1){
        ErrorExit("Error closing pipe e");
    }
    if(close(pipe_out[1]) == -1){
        ErrorExit("Error closing pipe f");
    }
    
    process_info_ = pid; 
    read_pipe_ = pipe_out[0];
    write_pipe_ = pipe_in[1];
    
    if(WaitForChildMessage() != kChildInitString){
        ErrorExit("Invalid initial acknowledgement from child process");
    }
}

#include <signal.h>
OSProcess::~OSProcess() {
    kill(process_info_, SIGKILL);
}
#endif


void OSProcess::SendMessageToChild( const std::string &msg ) {
    WriteMessageToPipe(write_pipe_, msg);
}

namespace {

void SendMessageToParent(const std::string &msg){
#ifdef _WIN32
    WriteMessageToPipe(GetStdHandle(STD_OUTPUT_HANDLE), msg);
#else
    WriteMessageToPipe(STDOUT_FILENO, msg);
#endif
}
std::string WaitForParentMessage(){
    std::string msg;
#ifdef _WIN32    
    ReadMessageFromPipe(GetStdHandle(STD_INPUT_HANDLE), &msg);
#else
    ReadMessageFromPipe(STDIN_FILENO, &msg);
#endif
    return msg;
}

int ChildProcessMessage(const std::string& msg, const ProcessPool::JobMap &job_map){
    int divider = (int)msg.find(": ");
    if(divider == std::string::npos){
        ErrorExit("No divider in message from parent: "+msg);
    }
    std::string type = msg.substr(0, divider);
    std::string body = msg.substr(divider+2, msg.size()-(divider+2));
    if(type == "TASK"){
        SendMessageToParent("PRINT: Starting task \""+body+"\"");
        int space = (int)body.find(' ');
        std::string job;
        std::string params;
        if(space == std::string::npos){
            job = body;
        } else {
            job = body.substr(0, space);
            params = body.substr(space + 1, body.size()-(space+1));
        }
        ProcessPool::JobMap::const_iterator iter = job_map.find(job);
        if(iter == job_map.end()){
            ErrorExit("No job named: "+job);
        }
        ProcessPool::JobFunctionPtr func = iter->second;
        std::vector<std::string> params_separated;
        while(!params.empty()){
            bool in_quotes = false;
            int space = std::string::npos;
            for(int i=0; i<params.size(); ++i){
                if(params[i] == '\"'){
                    in_quotes = !in_quotes;
                }
                if(!in_quotes && params[i] == ' '){
                    space = i;
                    break;
                }
            }
            if(space == std::string::npos){
                params_separated.push_back(params);
                params.clear();
            } else {
                params_separated.push_back(params.substr(0, space));
                params = params.substr(space + 1, params.size()-(space+1));
            }
        }
        int argc = (int)params_separated.size();
        std::vector<const char*> argv;
        for(int i=0; i<argc; ++i){
            argv.push_back(params_separated[i].c_str());
        }
#ifdef _WIN32
        HANDLE temp_pipe_read, temp_pipe_write;
        if(!CreatePipe(&temp_pipe_read, &temp_pipe_write, NULL, 0)){
            ErrorExit("Error creating temporary process pipe");
        }
        HANDLE parent_communicate = GetStdHandle(STD_OUTPUT_HANDLE);
        if(!SetStdHandle(STD_OUTPUT_HANDLE, temp_pipe_write)){
            ErrorExit("Error assigning STD_OUTPUT_HANDLE to temporary process pipe");
        }
        if(!SetStdHandle(STD_ERROR_HANDLE, temp_pipe_write)){
            ErrorExit("Error assigning STD_ERROR_HANDLE to temporary process pipe");
        }
        int ret_val = func(argc, &argv[0]);
        SetStdHandle(STD_OUTPUT_HANDLE, parent_communicate);
        SetStdHandle(STD_ERROR_HANDLE, parent_communicate);
        CloseHandle(temp_pipe_read);
        CloseHandle(temp_pipe_write);
#else
        int ret_val = func(argc, &argv[0]);
#endif
        SendMessageToParent("NULL: "); // TODO: Why does it stop working without this?
        return ret_val;
    }
    return 0;
}

} // namespace ""

int ProcessPool::WorkerProcessMain(const JobMap &job_map) {
    SendMessageToParent(kChildInitString);
    while(1){
        std::string msg = WaitForParentMessage();
        ChildProcessMessage(msg, job_map);
        SendMessageToParent("STATE: IDLE");
    }
    return 0;
}

void ProcessPool::NotifyTaskComplete() {
#ifdef _WIN32
    WaitForSingleObject(mutex_,INFINITE);
#else
    if(pthread_mutex_lock(&mutex_)){
        ErrorExit("Error locking mutex");
    }
#endif        
    ProcessFirstTaskInQueue();
#ifdef _WIN32
    if(!ReleaseMutex(mutex_)){
        ErrorExit("Error releasing mutex");
    }
#else
    if(pthread_mutex_unlock(&mutex_)){
        ErrorExit("Error releasing mutex");
    }
#endif        
}
