//
//  thread_pool.hpp
//  gomoku - High-performance thread pool for parallel AI computation
//
//  Based on dp::thread-pool library with C++23 compatibility
//

#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <type_traits>

namespace gomoku {

/**
 * High-performance thread pool implementation for parallel AI computation
 */
class ThreadPool {
public:
    /**
     * Constructor - creates thread pool with specified number of threads
     * @param threads Number of threads (default: hardware_concurrency - 1)
     */
    explicit ThreadPool(size_t threads = std::max(1u, std::thread::hardware_concurrency() - 1))
        : stop_(false) {
        
        if (threads == 0) {
            threads = 1; // Fallback for single-core systems
        }
        
        // Create worker threads
        workers_.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        
                        if (stop_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    /**
     * Destructor - stops all threads and waits for completion
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        condition_.notify_all();
        
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    /**
     * Enqueue a task for execution with return value
     * @param f Function to execute
     * @param args Arguments for the function
     * @return Future for the result
     */
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result_t<F, Args...>> {
        
        using return_type = typename std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task](){ (*task)(); });
        }
        
        condition_.notify_one();
        return result;
    }
    
    /**
     * Enqueue a task without return value (fire and forget)
     * @param f Function to execute
     * @param args Arguments for the function
     */
    template<class F, class... Args>
    void enqueue_detach(F&& f, Args&&... args) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("enqueue_detach on stopped ThreadPool");
            }
            
            tasks_.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        }
        
        condition_.notify_one();
    }
    
    /**
     * Get the number of threads in the pool
     */
    size_t size() const {
        return workers_.size();
    }
    
    /**
     * Check if thread pool is empty (no pending tasks)
     */
    bool empty() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.empty();
    }
    
    /**
     * Clear all pending tasks
     */
    void clear() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        std::queue<std::function<void()>> empty_queue;
        tasks_.swap(empty_queue);
    }
    
    /**
     * Wait for all tasks to complete
     */
    void wait_for_tasks() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (tasks_.empty()) {
                    break;
                }
            }
            std::this_thread::yield();
        }
    }
    
private:
    // Thread pool workers
    std::vector<std::thread> workers_;
    
    // Task queue
    std::queue<std::function<void()>> tasks_;
    
    // Synchronization
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
    
    // Non-copyable and non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
};

} // namespace gomoku