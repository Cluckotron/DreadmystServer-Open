// Async Saver - Background thread for database saves
// Task 2.10: Async Save Operations

#pragma once

#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <atomic>

// Save operation callback type
using SaveOperation = std::function<void()>;

class AsyncSaver
{
public:
    static AsyncSaver& instance();

    // Start the background save thread
    void start();

    // Stop the background thread (waits for queue to drain)
    void stop();

    // Queue a save operation for background execution
    void queueSave(SaveOperation saveFunc);

    // Wait for all pending saves to complete
    void flush();

    // Check if the saver is running
    bool isRunning() const { return m_running; }

    // Get pending save count
    size_t getPendingCount() const;

    // Configuration
    void setSaveInterval(int milliseconds);
    int getSaveInterval() const { return m_saveInterval; }

    // Default save interval (30 seconds)
    static constexpr int DEFAULT_SAVE_INTERVAL = 30000;

private:
    AsyncSaver();
    ~AsyncSaver();

    // Non-copyable
    AsyncSaver(const AsyncSaver&) = delete;
    AsyncSaver& operator=(const AsyncSaver&) = delete;

    // Background thread function
    void workerThread();

    std::thread m_thread;
    std::queue<SaveOperation> m_saveQueue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    int m_saveInterval = DEFAULT_SAVE_INTERVAL;
};

#define sAsyncSaver AsyncSaver::instance()
