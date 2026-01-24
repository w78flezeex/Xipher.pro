#ifndef LOCKFREE_RING_BUFFER_HPP
#define LOCKFREE_RING_BUFFER_HPP

#include <atomic>
#include <vector>
#include <cstdint>
#include <cstring>

namespace xipher {
namespace voip {

/**
 * Lock-Free Single Producer Single Consumer (SPSC) Ring Buffer
 * 
 * High-performance circular buffer for audio data transfer between threads:
 * - Producer thread: writes audio samples
 * - Consumer thread: reads audio samples
 * - No locks required, uses atomic operations
 * 
 * Thread-safe for exactly one producer and one consumer
 */
template<typename T>
class LockFreeRingBuffer {
public:
    /**
     * @param size Must be a power of 2 for efficient modulo operations
     */
    explicit LockFreeRingBuffer(size_t size) 
        : size_(nextPowerOfTwo(size))
        , mask_(size_ - 1)
        , buffer_(size_)
        , write_pos_(0)
        , read_pos_(0) {
    }
    
    /**
     * Push data into the buffer (producer thread)
     * @param data Pointer to data
     * @param count Number of elements to push
     * @return Number of elements actually pushed (may be less if buffer is full)
     */
    size_t push(const T* data, size_t count) {
        const size_t write_pos = write_pos_.load(std::memory_order_relaxed);
        const size_t read_pos = read_pos_.load(std::memory_order_acquire);
        
        const size_t available = size_ - (write_pos - read_pos) - 1;
        const size_t to_write = (count < available) ? count : available;
        
        if (to_write == 0) {
            return 0; // Buffer full
        }
        
        // Write data
        const size_t pos = write_pos & mask_;
        const size_t first_part = std::min(to_write, size_ - pos);
        
        std::memcpy(&buffer_[pos], data, first_part * sizeof(T));
        if (to_write > first_part) {
            std::memcpy(&buffer_[0], data + first_part, (to_write - first_part) * sizeof(T));
        }
        
        write_pos_.store(write_pos + to_write, std::memory_order_release);
        return to_write;
    }
    
    /**
     * Pop data from the buffer (consumer thread)
     * @param data Pointer to destination buffer
     * @param count Maximum number of elements to pop
     * @return Number of elements actually popped
     */
    size_t pop(T* data, size_t count) {
        const size_t read_pos = read_pos_.load(std::memory_order_relaxed);
        const size_t write_pos = write_pos_.load(std::memory_order_acquire);
        
        const size_t available = write_pos - read_pos;
        const size_t to_read = (count < available) ? count : available;
        
        if (to_read == 0) {
            return 0; // Buffer empty
        }
        
        // Read data
        const size_t pos = read_pos & mask_;
        const size_t first_part = std::min(to_read, size_ - pos);
        
        std::memcpy(data, &buffer_[pos], first_part * sizeof(T));
        if (to_read > first_part) {
            std::memcpy(data + first_part, &buffer_[0], (to_read - first_part) * sizeof(T));
        }
        
        read_pos_.store(read_pos + to_read, std::memory_order_release);
        return to_read;
    }
    
    /**
     * Get number of elements available to read
     */
    size_t available() const {
        const size_t write_pos = write_pos_.load(std::memory_order_acquire);
        const size_t read_pos = read_pos_.load(std::memory_order_acquire);
        return write_pos - read_pos;
    }
    
    /**
     * Get number of free slots available to write
     */
    size_t free() const {
        return size_ - available() - 1;
    }
    
    /**
     * Check if buffer is empty
     */
    bool empty() const {
        return available() == 0;
    }
    
    /**
     * Check if buffer is full
     */
    bool full() const {
        return free() == 0;
    }
    
    /**
     * Clear the buffer
     */
    void clear() {
        read_pos_.store(write_pos_.load(std::memory_order_relaxed), std::memory_order_release);
    }

private:
    const size_t size_;
    const size_t mask_;  // For efficient modulo: pos & mask_ instead of pos % size_
    std::vector<T> buffer_;
    
    // Atomic positions (must be separate to avoid false sharing)
    alignas(64) std::atomic<size_t> write_pos_;  // Producer writes here
    alignas(64) std::atomic<size_t> read_pos_;   // Consumer reads from here
    
    static size_t nextPowerOfTwo(size_t n) {
        if (n == 0) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if (sizeof(size_t) > 4) {
            n |= n >> 32;
        }
        return n + 1;
    }
};

} // namespace voip
} // namespace xipher

#endif // LOCKFREE_RING_BUFFER_HPP
