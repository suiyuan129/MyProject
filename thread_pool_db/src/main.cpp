#include "thread_pool.hpp"

int main() {
    std::cout << "=== ThreadPool Demo (Optimized Shrink) ===\n" << std::endl;

    // 1. 创建线程池：2核心，4最大，保留1个空闲线程，单次最多删2个
    ThreadPool pool(2, 4, 1, 2);
    pool.printStats();

    // 2. 提交不同优先级任务
    std::cout << "\n=== Submitting priority tasks ===" << std::endl;
    auto [low_id, low_future] = pool.submit(TaskPriority::Low, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "[Low Priority] Task completed" << std::endl;
        return "Low";
    });
    auto [high_id, high_future] = pool.submit(TaskPriority::High, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "[High Priority] Task completed" << std::endl;
        return "High";
    });

    // 3. 提交批量任务，触发线程池扩容
    std::cout << "\n=== Submitting batch tasks (trigger expansion) ===" << std::endl;
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        auto [id, fut] = pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * (i % 3 + 1)));
            return i * i;
        });
        futures.push_back(std::move(fut));
    }

    // 4. 获取优先级任务结果
    std::cout << "\nHigh priority task result: " << high_future.get() << std::endl;
    std::cout << "Low priority task result: " << low_future.get() << std::endl;

    // 5. 计算平方和
    int sum = 0;
    for (auto& fut : futures) {
        sum += fut.get();
    }
    std::cout << "\nSum of squares (0-9): " << sum << std::endl;

    // 6. 打印统计（触发缩容）
    pool.printStats();

    // 7. 等待所有任务完成，验证缩容
    std::cout << "\n=== Waiting for all tasks to complete ===" << std::endl;
    if (pool.waitAllTasks(3000)) {
        std::cout << "All tasks completed!\n";
    } else {
        std::cout << "Timeout waiting for tasks!\n";
    }

    // 8. 再次打印统计，查看缩容效果
    pool.printStats();

    // 9. 测试任务取消
    std::cout << "\n=== Testing task cancellation ===" << std::endl;
    auto [cancel_id, cancel_fut] = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "This task should be cancelled!" << std::endl;
    });
    if (pool.cancelTask(cancel_id)) {
        std::cout << "Task " << cancel_id << " cancelled successfully\n";
    }
    try {
        cancel_fut.get();
        std::cout << "Cancelled task future completed without error\n";
    } catch (const std::exception& e) {
        std::cout << "Cancelled task threw: " << e.what() << std::endl;
    }

    // 10. 提交新任务，验证缩容后线程池重新创建线程
    std::cout << "\n=== Submitting new task after shrink ===" << std::endl;
    auto [new_id, new_fut] = pool.submit([]() {
        std::cout << "[New Task] Executed after shrink" << std::endl;
        return 42;
    });
    std::cout << "New task result: " << new_fut.get() << std::endl;

    // 11. 最终统计
    pool.printStats();

    // 12. 停止线程池
    std::cout << "\n=== Stopping ThreadPool ===" << std::endl;
    pool.stop(true);
    pool.printStats();

    return 0;
}