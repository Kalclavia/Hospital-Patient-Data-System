#include "BoundedBuffer.h"

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    vector<char> byte_sequence_charvec(msg, msg + size);

    // 2. Wait until there is room in the queue (i.e., queue length is less than cap)
    unique_lock<mutex> unique_lock(queue_mutex); // unique_lock


    data_push_notify.wait(unique_lock, [this] {return (int)q.size() < cap;});

    // 3. Then push the vector at the end of the queue
    q.push(byte_sequence_charvec);

    // 4. Wake up threads that were waiting for push
    unique_lock.unlock(); // unlock
    slot_pop_notify.notify_one();
}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    unique_lock<mutex> unique_lock(queue_mutex); // unique_lock
  
    slot_pop_notify.wait(unique_lock, [this] {return (int)q.size() > 0;});
    
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> popped_item = q.front();
    q.pop();    

    unique_lock.unlock();

    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    assert((int)popped_item.size() <= size);
    memcpy(msg, popped_item.data(), popped_item.size());    

    // 4. Wake up threads that were waiting for pop
    data_push_notify.notify_one();

    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return popped_item.size();
}

size_t BoundedBuffer::size () {
    return q.size();
}
