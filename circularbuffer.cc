#include "stm32f1xx_hal.h"
#include "circularbuffer.hh"

template<typename T, size_t N>
CircularBuffer<T, N>::CircularBuffer(){}

template<typename T, size_t N>
void CircularBuffer<T,N>::put(T item)
{
    buf_[head_] = item;
    if(full_)
    {
        tail_ = (tail_ + 1) % max_size_;
    }
    head_ = (head_ + 1) % max_size_;
    full_ = head_ == tail_;
}

template<typename T, size_t N>
T CircularBuffer<T,N>::get()
{
    if(empty()) return T();
    //Read data and advance the tail (we now have a free space)
    auto val = buf_[tail_];
    full_ = false;
    tail_ = (tail_ + 1) % max_size_;
    return val;
}

template<typename T, size_t N>
T CircularBuffer<T,N>::peek(size_t i)
{
    if(empty()) return T();
    auto val = buf_[(tail_+i)%max_size_];
    return val;
}

template<typename T, size_t N>
void CircularBuffer<T,N>::reset()
{
    head_ = tail_;
    full_ = false;
}

template<typename T, size_t N>
bool CircularBuffer<T,N>::empty() const
{
    //if head and tail are equal, we are empty
    return (!full_ && (head_ == tail_));
}

template<typename T, size_t N>
bool CircularBuffer<T,N>::full() const
{
    //If tail is ahead the head by 1, we are full
    return full_;
}

template<typename T, size_t N>
size_t CircularBuffer<T,N>::capacity() const
{
    return max_size_;
}

template<typename T, size_t N>
size_t CircularBuffer<T,N>::size() const
{
    size_t size = max_size_;
    if(!full_)
    {
        if(head_ >= tail_)
        {
            size = head_ - tail_;
        }
        else
        {
            size = max_size_ + head_ - tail_;
        }
    }
    return size;
}





