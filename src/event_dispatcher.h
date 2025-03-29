#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

/* -- Includes ------------------------------------------------------------ */
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include "httplib.h"

/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */
class EventDispatcher
{
public:
    EventDispatcher()
    {
    }

    void wait_event(httplib::DataSink *sink)
    {
        std::unique_lock<std::mutex> lk(mtx);
        const int cnt = this->cnt;
        cv.wait(lk, [&]{ return this->cnt != cnt; }); //wait until this->cnt has changed => new message!
        sink->write("data: ", 6);
        sink->write(message.data(), message.size());
        sink->write("\r\n\r\n", 4);
    }

    //message shall not contain any newline char!!!
    void send_event(std::string &&message)
    {
        std::lock_guard<std::mutex> lk(mtx);
        this->message = message; //move message
        cnt++; //increase message counter => indicate availability of a new message
        cv.notify_all();
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic_int cnt{0};
    std::string message;
};

/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */

/* -- Implementation ------------------------------------------------------ */

#endif