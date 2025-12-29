#include "thread_pool.hpp"

Task::Task(TaskFunc&& f,TaskPriority p,size_t tid)
    : func(std::move(f)),priority(p),id(tid),cancelled(std::make_shared<std::atomic<bool>>(false)) {}

bool Task::operator<(const Task& oht) const{
    if(priority!=oht.priority){ return priority<oht.priority; }
    else return id>oht.id;
}

void Task::cancel(){ cancelled->store(true,std::memory_order_relaxed); }
bool Task::isCancelled() const{ return cancelled->load(std::memory_order_relaxed); }

void ThreadPool::cleanExitedThreads(){
    size_t old_size=_workers.size();
    _workers.remove_if([](const std::thread& t){ return !t.joinable(); });
    size_t new_size=_workers.size();
    if(old_size>new_size){
        std::cout<<"[ThreadPool] Cleaned"<<(old_size-new_size)
                 <<" exited threads, remaining: "<<new_size<<std::endl;
    }
}

bool ThreadPool::shouldShrink(){
    return _shrink_signal.load(std::memory_order_acquire)>0&&_workers.size()>_core_threads;
}

size_t ThreadPool::shrinkIdleThreads(){
    std::lock_guard<std::mutex> lock(mtx);
    if(_state.load(std::memory_order_acquire)!=ThreadPoolState::Running){
        return 0;
    }
    cleanExitedThreads();
    size_t current_thread_count=_workers.size();
    size_t current_idle=_idle_threads.load(std::memory_order_acquire);
    size_t shrinkable=std::max(static_cast<size_t>(0),current_idle-_idle_threads_to_keep);
    size_t to_shrink=std::min({shrinkable,_max_shrink_count,current_thread_count-_core_threads});
    if(to_shrink==0){
        return 0;
    }
    _shrink_signal.store(to_shrink,std::memory_order_release);
    _cv_tasks.notify_all();
    std::cout<<"[ThreadPool] Shrink: try to remove "<<to_shrink
             <<" idle threads (idle: "<<current_idle
             <<",keep: "<<_idle_threads_to_keep<<")"<<std::endl;
    return to_shrink;
}

void ThreadPool::workerLoop(size_t thread_id){
    thread_local_id=thread_id;
    _idle_threads.fetch_add(1,std::memory_order_relaxed);
    while(true){
        std::optional<Task> task;
        {
            std::unique_lock<std::mutex> lock(mtx);
            _cv_tasks.wait(lock,[this](){
                auto s=_state.load(std::memory_order_acquire);
                return s!=ThreadPoolState::Running||!_tasks.empty()||shouldShrink();
            });
            auto current_state=_state.load(std::memory_order_acquire);
            if(current_state==ThreadPoolState::Stopped){
                _idle_threads.fetch_sub(1,std::memory_order_relaxed);
                _cv_idle.notify_all();
                return;
            }
            if(current_state==ThreadPoolState::Stopping&&_tasks.empty()){
                _idle_threads.fetch_sub(1,std::memory_order_relaxed);
                _cv_idle.notify_all();
                return;
            }
            if(shouldShrink()){
                _shrink_signal.fetch_sub(1,std::memory_order_relaxed);
                _idle_threads.fetch_sub(1,std::memory_order_relaxed);
                std::cout<<"[ThreadPool] Idle Thread "<<thread_id
                         <<" exited (shrink),total threads: "<<_workers.size()<<std::endl;
                return;
            }
            if(!_tasks.empty()){
                task=_tasks.top();
                _tasks.pop();
                task_cancel_map.erase(task->id);
                _idle_threads.fetch_sub(1,std::memory_order_relaxed);
                _active_threads.fetch_add(1,std::memory_order_relaxed);
            }
        }
        if(task){
            bool task_executed=false;
            try{
                if(!task->isCancelled()){
                    task->func();
                    task_executed=true;
                }
            }catch(const std::exception& e){
                std::cerr<<"[Thread "<<thread_local_id<<"] Task "<<task->id
                         <<" threw: "<<e.what()<<std::endl;
                _total_tasks_failed.fetch_add(1,std::memory_order_relaxed);
            }catch(...){
                std::cerr<<"[Thread "<<thread_local_id<<"] Task "<<task->id
                         <<" threw unknown exception"<<std::endl;
                _total_tasks_failed.fetch_add(1,std::memory_order_relaxed);
            }
            if(task_executed){
                _total_tasks_completed.fetch_add(1,std::memory_order_relaxed);
            }
            _active_threads.fetch_sub(1,std::memory_order_relaxed);
            _idle_threads.fetch_add(1,std::memory_order_relaxed);
            _cv_idle.notify_all();
        }
    }
}

void ThreadPool::checkState() const{
    auto s=_state.load(std::memory_order_acquire);
    if(s!=ThreadPoolState::Running){
        throw std::runtime_error("ThreadPool is not running (state: "+
                                 std::to_string(static_cast<int>(s))+")");
    }
}

ThreadPool::ThreadPool(size_t core_threads,size_t max_threads,
                       size_t idle_threads_to_keep,size_t max_shrink_count)
    :_core_threads(core_threads==0?1:core_threads)
    ,_max_threads(std::max(max_threads,_core_threads))
    ,_state(ThreadPoolState::Running)
    ,_task_id_counter(0)
    ,_active_threads(0)
    ,_idle_threads(0)
    ,_idle_threads_to_keep(std::min(idle_threads_to_keep,core_threads))
    ,_max_shrink_count(max_shrink_count)
    ,_shrink_signal(0){
        if(_core_threads>_max_threads){
            throw std::invalid_argument("_core_threads>max_threads");
        }
        std::lock_guard<std::mutex> lock(mtx);
        for(size_t i=0;i<_core_threads;++i){
            _workers.emplace_back(&ThreadPool::workerLoop,this,i);
        }
        std::cout<<"[ThreadPool] Created with "<<_core_threads<<" core threads,"
                 <<_max_threads<<" max threads"<<std::endl;
}

ThreadPool::~ThreadPool(){
    try{
        stop(true);
    }catch(...){
        std::cerr<<"[ThreadPool] Error stopping pool in destructor"<<std::endl;
    }
}

bool ThreadPool::cancelTask(size_t task_id){
    std::lock_guard<std::mutex> lock(mtx);
    auto it=task_cancel_map.find(task_id);
    if(it!=task_cancel_map.end()){
        it->second->store(true,std::memory_order_release);
        return true;
    }
    return false;
}

void ThreadPool::stop(bool wait_all_tasks){
    ThreadPoolState expected=ThreadPoolState::Running;
    if(!_state.compare_exchange_strong(expected,
        wait_all_tasks?ThreadPoolState::Stopping:ThreadPoolState::Stopped),
        std::memory_order_acq_rel,std::memory_order_release){
            return;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        if(!wait_all_tasks){
            while(!_tasks.empty()){
                _tasks.pop();
            }
        }
        cleanExitedThreads();
    }
    _cv_tasks.notify_all();
    for(auto& worker:_workers){
        if(worker.joinable()){
            worker.join();
        }
    }
    _workers.clear();
    task_cancel_map.clear();
    _state.store(ThreadPoolState::Stopped,std::memory_order_release);
    std::cout << "[ThreadPool] Stopped. Completed " << _total_tasks_completed.load(std::memory_order_acquire) 
              << "/" << _total_tasks_submitted.load(std::memory_order_acquire) 
              << " tasks (failed: " << _total_tasks_failed.load(std::memory_order_acquire) << ")" << std::endl;
}

bool ThreadPool::waitAllTasks(int timeout_ms){
    std::unique_lock<std::mutex> lock(mtx);
    auto condition=[this](){
        return _tasks.empty()&&(_active_threads.load(std::memory_order_acquire)==0);
    };
    if(timeout_ms<=0){
        _cv_idle.wait(lock,condition);
        return true;
    }else{
        auto timeout=std::chrono::seconds(timeout_ms);
        return _cv_idle.wait_for(lock,timeout,condition);
    }
}

ThreadPoolState ThreadPool::getState() const { 
    return _state.load(std::memory_order_acquire); 
}

size_t ThreadPool::getCoreThreads() const { return _core_threads; }
size_t ThreadPool::getMaxThreads() const { return _max_threads; }

size_t ThreadPool::getThreadCount() const { 
    std::lock_guard<std::mutex> lock(mtx); 
    return _workers.size(); 
}

size_t ThreadPool::getActiveThreads() const { 
    return _active_threads.load(std::memory_order_acquire); 
}

size_t ThreadPool::getIdleThreads() const { 
    return _idle_threads.load(std::memory_order_acquire); 
}

size_t ThreadPool::getPendingTaskCount() const { 
    std::lock_guard<std::mutex> lock(mtx); 
    return _tasks.size(); 
}

size_t ThreadPool::getTotalSubmittedTasks() const { 
    return _total_tasks_submitted.load(std::memory_order_acquire); 
}

size_t ThreadPool::getTotalCompletedTasks() const { 
    return _total_tasks_completed.load(std::memory_order_acquire); 
}

size_t ThreadPool::getTotalFailedTasks() const { 
    return _total_tasks_failed.load(std::memory_order_acquire); 
}

void ThreadPool::printStats() const {
    const_cast<ThreadPool*>(this)->shrinkIdleThreads();
    const_cast<ThreadPool*>(this)->cleanExitedThreads();

    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "\n========== ThreadPool Stats ==========\n";
    std::cout << "State: ";
    switch (getState()) {
        case ThreadPoolState::Running: std::cout << "Running"; break;
        case ThreadPoolState::Stopping: std::cout << "Stopping"; break;
        case ThreadPoolState::Stopped: std::cout << "Stopped"; break;
    }
    std::cout << "\nThreads: " << _workers.size() 
              << " (Active: " << getActiveThreads() 
              << ", Idle: " << getIdleThreads() << ")\n";
    std::cout << "Pending tasks: " << _tasks.size() << "\n";
    std::cout << "Total submitted: " << getTotalSubmittedTasks() << "\n";
    std::cout << "Total completed: " << getTotalCompletedTasks() << "\n";
    std::cout << "Total failed: " << getTotalFailedTasks() << "\n";
    std::cout << "Shrink config: keep " << _idle_threads_to_keep 
              << " idle, max shrink " << _max_shrink_count << "\n";
    std::cout << "======================================\n" << std::endl;
}