#pragma once
#include <iostream>
#include "TaskQueue.h"
#include <string>
#include <mutex>

// Declare the thread-safe logging function
extern void logMessage(const std::string& message);

class PipelineWorker {
private:
    TaskQueue& taskQueue;

public:
    PipelineWorker(TaskQueue& queue) : taskQueue(queue) {}

    // Worker thread function
    void operator()() {
        while (true) {
           logMessage("[PipelineWorker] Waiting for task...");
            Task task;
            if (taskQueue.dequeue(task)) {
                logMessage("[PipelineWorker] Task dequeued: " + task.command);
                task.operation();  // Execute the task
                logMessage("[PipelineWorker] Task executed: " + task.command);
            } else {
                // No more tasks and task queue is stopped
                logMessage("[PipelineWorker] No more tasks and task queue is stopped. Exiting.");
                break;
            }
        }
    }
};
