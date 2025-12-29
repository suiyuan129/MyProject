#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include<iostream>

#include<vector>
#include<queue>
#include<list>
#include<unordered_map>

#include<thread>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include<future>

#include<functional>
#include<optional>

#include<stdexcept>
#include<memory>
#include<chrono>
#include<algorithm>

enum class ThreadPoolState{
    Running,
    Stopping,
    Stopped
};

enum class TaskPriority{
    Low=0,
    Normal=1,
    High=2
};

struct Task{
    using TaskFunc=std::function<void()>;
    TaskFunc func;
    TaskPriority priority;
    size_t id;
    std::shared_ptr<std::atomic<bool>> cancelled;

    Task()=delete;
    Task(TaskFunc&& f,TaskPriority p,size_t tid);
    bool operator<(const Task& oht) const;
    void cancel();
    bool isCancelled()const;
};

class ThreadPool{
private:
    std::list<std::thread> _workers;
    std::priority_queue<Task> _tasks;
    mutable std::mutex mtx;
    std::condition_variable _cv_tasks;
    std::condition_variable _cv_idle;
    std::atomic<ThreadPoolState> _state;
    std::atomic<size_t> _task_id_counter;
    size_t _core_threads;
    size_t _max_threads;
    std::atomic<size_t> _active_threads;
    std::atomic<size_t> _idle_threads;
    std::atomic<size_t> _total_tasks_submitted{0};
    std::atomic<size_t> _total_tasks_completed{0};
    std::atomic<size_t> _total_tasks_failed{0};
    std::unordered_map<size_t,std::shared_ptr<std::atomic<bool>>> task_cancel_map;

    size_t _idle_threads_to_keep; 
    size_t _max_shrink_count; 
    std::atomic<size_t> _shrink_signal;

    thread_local static inline size_t thread_local_id=0;
    void cleanExitedThreads();
    bool shouldShrink();
    size_t shrinkIdleThreads();
    void workerLoop(size_t thread_id);
    void checkState() const;

public:
    explicit ThreadPool(
        size_t core_threads=std::thread::hardware_concurrency(),
        size_t max_threads=std::thread::hardware_concurrency()*2,
        size_t idle_threads_to_keep = 1,
        size_t max_shrink_count = 2
    );
    ThreadPool(const ThreadPool& oht)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;
    ThreadPool(ThreadPool&& oht)=delete;
    ThreadPool& operator=(ThreadPool&&)=delete;
    ~ThreadPool();

    template<typename F,typename... Args>
    auto submit(TaskPriority priority,F&& f,Args&&... args)->std::pair<size_t,std::future<std::invoke_result_t<F,Args...>>>{
        checkState();
        using ReturnType=std::invoke_result_t<F,Args...>;
        auto packaged_task=std::make_shared<std::packaged_task<ReturnType()>>(std::bind(std::forward<F> (f),std::forward<Args> (args)...));
        std::future<ReturnType> future=packaged_task->get_future();
        size_t task_id=_task_id_counter.fetch_add(1,std::memory_order_relaxed);
        _total_tasks_submitted.fetch_add(1,std::memory_order_relaxed);

        Task task([packaged_task](){( *packaged_task)(); },priority,task_id);
        bool need_notify=false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            cleanExitedThreads();
            task_cancel_map[task_id]=task.cancelled;
            _tasks.push(std::move(task));
            if(_idle_threads.load(std::memory_order_relaxed)>0){
                need_notify=true;
            }
            else if(_workers.size()<_max_threads){
                size_t new_thread_id=_workers.size();
                _workers.emplace_back(&ThreadPool::workerLoop,this,new_thread_id);
                _idle_threads.fetch_add(1,std::memory_order_relaxed);
                std::cout<<"[ThreadPool] Created new thread,total: "<<_workers.size()<<std::endl;
                need_notify=true;
            }
        }
        if(need_notify){
            _cv_tasks.notify_one();
        }
        shrinkIdleThreads();
        return {task_id,std::move(future)};
    }

    template<typename F,typename... Args>
    auto submit(F&& f,Args&&... args){
        return submit(TaskPriority::Normal,std::forward<F> (f),std::forward<Args> (args)...);
    }

    template<typename F,typename... Args>
    auto async(F&& f,Args&&... args){
        auto [id,future]=submit(std::forward<F> (f),std::forward<Args> (args)...);
        return future;
    }
    bool cancelTask(size_t task_id);
    void stop(bool wait_all_tasks=true);
    bool waitAllTasks(int timeout_ms=0);
    ThreadPoolState getState() const;
    size_t getCoreThreads() const;
    size_t getMaxThreads() const;
    size_t getThreadCount() const;
    size_t getActiveThreads() const;
    size_t getIdleThreads() const;
    size_t getPendingTaskCount() const;
    size_t getTotalSubmittedTasks() const;
    size_t getTotalCompletedTasks() const;
    size_t getTotalFailedTasks() const;
    void printStats() const;
};

#endif