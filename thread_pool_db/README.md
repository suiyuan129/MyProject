# C++ 高性能线程池（ThreadPool）

## 项目介绍
这是一个基于 **C++17** 实现的轻量级、高性能线程池，支持动态扩缩容、任务优先级、任务取消、异常安全等核心特性，适用于多线程任务调度场景（如异步计算、IO 任务分发等）。


## 功能特性
- **动态扩缩容**：自动根据任务量创建线程（最大不超过 `max_threads`），空闲时自动缩容至核心线程数（保留指定空闲线程）
- **任务优先级**：支持 `Low/Normal/High` 三级任务优先级，高优先级任务优先执行
- **任务取消**：支持取消未执行的任务
- **异常安全**：隔离单个任务的异常，避免线程池崩溃；非法操作主动抛异常（如向已停止的线程池提交任务）
- **状态监控**：提供线程数、任务数、执行统计等状态查询与打印
- **优雅停止**：支持等待所有任务完成后停止，或强制停止并清空任务队列


## 编译与运行
### 依赖
- C++17 及以上编译器（如 GCC 7+、Clang 5+）
- POSIX 线程库（`pthread`，Linux/macOS 自带）

### 编译命令
将代码文件（`thread_pool.hpp`、`thread_pool.cpp`、`main.cpp`）放在同一目录下，执行：
```bash
g++ -std=c++17 -pthread main.cpp thread_pool.cpp -o thread_pool_demo
```

### 运行示例
```bash
./thread_pool_demo
```


## 快速使用示例
以下是线程池的核心使用场景示例：

```cpp
#include "thread_pool.hpp"
#include <iostream>
#include <chrono>

int main() {
    // 1. 创建线程池：2核心线程，4最大线程，保留1个空闲线程，单次最多缩容2个
    ThreadPool pool(2, 4, 1, 2);

    // 2. 提交不同优先级任务
    auto [low_id, low_fut] = pool.submit(TaskPriority::Low, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "[Low] Task Done" << std::endl;
        return "Low Result";
    });
    auto [high_id, high_fut] = pool.submit(TaskPriority::High, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "[High] Task Done" << std::endl;
        return "High Result";
    });

    // 3. 提交批量任务
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 5; ++i) {
        auto [id, fut] = pool.submit([i]() {
            return i * i; // 计算平方
        });
        futures.push_back(std::move(fut));
    }

    // 4. 获取任务结果
    std::cout << "High Task Result: " << high_fut.get() << std::endl;
    std::cout << "Low Task Result: " << low_fut.get() << std::endl;

    // 5. 计算平方和
    int sum = 0;
    for (auto& fut : futures) sum += fut.get();
    std::cout << "Sum of Squares: " << sum << std::endl;

    // 6. 取消任务
    auto [cancel_id, cancel_fut] = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "This Task Should Be Cancelled" << std::endl;
    });
    pool.cancelTask(cancel_id); // 取消未执行的任务

    // 7. 等待所有任务完成 + 打印统计
    pool.waitAllTasks();
    pool.printStats();

    // 8. 停止线程池
    pool.stop(true);
    return 0;
}
```


## 核心设计要点
1. **状态管理**：通过原子变量 + CAS 操作（`compare_exchange_strong`）实现线程安全的状态切换（`Running/Stopping/Stopped`）
2. **动态扩缩容**：
   - 扩容：无空闲线程且未达最大线程数时，自动创建新线程
   - 缩容：空闲线程数超过保留数时，主动触发空闲线程退出
3. **异常处理**：
   - 任务执行时捕获所有异常，避免线程崩溃
   - 非法操作（如向非运行状态的线程池提交任务）主动抛异常
4. **线程安全**：通过互斥锁（`std::mutex`）、条件变量（`std::condition_variable`）、原子变量保证多线程操作安全


## 注意事项
1. 确保 `core_threads ≤ max_threads`（构造函数会校验，非法时抛 `std::invalid_argument`）
2. 线程池析构前需调用 `stop()`（析构函数已自动调用，但建议显式调用以控制停止行为）
3. 提交的任务函数应避免未定义行为（如访问已释放内存），线程池仅捕获任务抛出的异常
4. 任务取消仅对**未执行的任务**有效，已执行的任务无法取消


## 扩展建议
- 支持任务超时机制：为任务添加超时时间，超时后自动取消
- 支持线程亲和性：将特定任务绑定到指定线程
- 支持任务依赖：实现任务间的依赖调度（如 Task B 依赖 Task A 的结果）