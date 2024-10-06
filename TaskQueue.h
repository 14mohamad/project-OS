#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>
#include "Task.h"

// Thread-safe logging function (implemented elsewhere)
extern void logMessage(const std::string& message);

class TaskQueue {
private:
    std::queue<Task> tasks;           // Queue holding tasks
    std::mutex mtx;                   // Mutex for protecting the task queue
    std::condition_variable cv;       // Condition variable for notifying threads
    bool running;                     // Flag to indicate if the queue is running

public:
    TaskQueue() : running(true) {}

    // Add a task to the queue
    void enqueue(Task task) {
    std::unique_lock<std::mutex> lock(mtx);  // Lock for modifying the queue
    tasks.push(task);  // Push the task onto the queue
    logMessage("[TaskQueue] Task enqueued: " + task.command);
    cv.notify_one();  // Notify a worker thread that a new task is available while the mutex is still held
    // The mutex is released when the unique_lock goes out of scope
}


    // Retrieve a task from the queue (returns false if no tasks are available and queue is stopped)
    bool dequeue(Task& task) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait for either a task to be available or for the queue to stop
        cv.wait(lock, [this]() { return !tasks.empty() || !running; });

        // Check the condition again after waking up (to handle spurious wakeups)
        if (!tasks.empty()) {
            task = tasks.front();
            tasks.pop();
            logMessage("[TaskQueue] Task dequeued: " + task.command);
            return true;
        } else if (!running) {
            logMessage("[TaskQueue] Queue stopped and no tasks available.");
            return false;
        }
        
        return false;
    }

    // Stop the task queue and notify all waiting threads
    void stop() {
    {
        std::unique_lock<std::mutex> lock(mtx);
        running = false;  // Set running flag to false

        // Notify all waiting threads while holding the lock
        logMessage("[TaskQueue] Task queue stopped.");
        cv.notify_all();  // Wake up all waiting threads
    }  // Mutex is released when 'lock' goes out of scope
}

};
