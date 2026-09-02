// Async Saver - Background thread for database saves
// Task 2.10: Async Save Operations

#include "stdafx.h"
#include "Database/AsyncSaver.h"
#include "Core/Logger.h"

AsyncSaver& AsyncSaver::instance()
{
    static AsyncSaver instance;
    return instance;
}

AsyncSaver::AsyncSaver()
{
}

AsyncSaver::~AsyncSaver()
{
    stop();
}

void AsyncSaver::start()
{
    if (m_running) {
        return;  // Already running
    }

    m_stopRequested = false;
    m_running = true;
    m_thread = std::thread(&AsyncSaver::workerThread, this);

    LOG_INFO("Async saver started (interval: %dms)", m_saveInterval);
}

void AsyncSaver::stop()
{
    if (!m_running) {
        return;
    }

    LOG_INFO("Stopping async saver...");

    // Signal stop
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = true;
    }
    m_condition.notify_one();

    // Wait for thread to finish
    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_running = false;
    LOG_INFO("Async saver stopped");
}

void AsyncSaver::queueSave(SaveOperation saveFunc)
{
    if (!saveFunc) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_saveQueue.push(std::move(saveFunc));
    }
    m_condition.notify_one();

    LOG_DEBUG("Queued save operation (pending: %zu)", getPendingCount());
}

void AsyncSaver::flush()
{
    LOG_INFO("Flushing %zu pending save operations...", getPendingCount());

    // Process all remaining saves in current thread
    while (true) {
        SaveOperation saveOp;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_saveQueue.empty()) {
                break;
            }
            saveOp = std::move(m_saveQueue.front());
            m_saveQueue.pop();
        }

        try {
            saveOp();
        } catch (const std::exception& e) {
            LOG_ERROR("Flush save failed: %s", e.what());
        }
    }

    LOG_INFO("Flush complete");
}

size_t AsyncSaver::getPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_saveQueue.size();
}

void AsyncSaver::setSaveInterval(int milliseconds)
{
    m_saveInterval = milliseconds > 0 ? milliseconds : DEFAULT_SAVE_INTERVAL;
}

void AsyncSaver::workerThread()
{
    LOG_DEBUG("Async saver worker thread started");

    while (true) {
        SaveOperation saveOp;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait for work or stop signal
            m_condition.wait(lock, [this] {
                return m_stopRequested || !m_saveQueue.empty();
            });

            // Check for stop signal
            if (m_stopRequested && m_saveQueue.empty()) {
                break;
            }

            // Get next save operation
            if (!m_saveQueue.empty()) {
                saveOp = std::move(m_saveQueue.front());
                m_saveQueue.pop();
            }
        }

        // Execute save operation outside of lock
        if (saveOp) {
            try {
                saveOp();
            } catch (const std::exception& e) {
                LOG_ERROR("Async save failed: %s", e.what());
            }
        }
    }

    // Process remaining saves before exiting
    while (true) {
        SaveOperation saveOp;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_saveQueue.empty()) {
                break;
            }
            saveOp = std::move(m_saveQueue.front());
            m_saveQueue.pop();
        }

        if (saveOp) {
            try {
                saveOp();
            } catch (const std::exception& e) {
                LOG_ERROR("Final save failed: %s", e.what());
            }
        }
    }

    LOG_DEBUG("Async saver worker thread stopped");
}
