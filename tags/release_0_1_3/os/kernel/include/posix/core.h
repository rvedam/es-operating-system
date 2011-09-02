/*
 * Copyright 2008 Google Inc.
 * Copyright 2006 Nintendo Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NINTENDO_ES_KERNEL_POSIX_CORE_H_INCLUDED
#define NINTENDO_ES_KERNEL_POSIX_CORE_H_INCLUDED

#include <pthread.h>
#include <es.h>
#include <es/ref.h>
#include <es/base/IMonitor.h>
#include <es/base/IProcess.h>
#include <es/base/IThread.h>

using namespace es;

class Core;
class Thread;
class Monitor;
class SpinLock;

class Core : public ICurrentThread, public ICurrentProcess
{
    Ref ref;

public:
    // ICurrentProcess
    void exit(int status);
    void* map(const void* start, long long length, unsigned int prot, unsigned int flags, IPageable* pageable, long long offset);
    void unmap(const void* start, long long length);
    ICurrentThread* currentThread();
    IThread* createThread(void* (*start)(void* param), void* param);
    void yield();
    IMonitor* createMonitor();
    IContext* getRoot();
    IStream* getInput();
    IStream* getOutput();
    IStream* getError();
    void* setBreak(long long increment);
    long long getNow();
    void setStartup(void (*startup)(void* (*start)(void* param), void* param));

    // ICurrentThread
    void exit(const void* val);
    void sleep(long long timeout);
    int setCancelState(int state);
    int setCancelType(int type);
    void testCancel();

    // IInterface
    void* queryInterface(const Guid& riid);
    unsigned int addRef(void);
    unsigned int release(void);
};

class Monitor : public IMonitor
{
    Ref             ref;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;

public:
    Monitor();
    ~Monitor();

    // IMonitor
    void lock();
    bool tryLock();
    void unlock();
    bool wait();
    bool wait(s64 timeout);
    void notify();
    void notifyAll();

    // IInterface
    void* queryInterface(const Guid& riid);
    unsigned int addRef(void);
    unsigned int release(void);

    // Internal
    class Synchronized
    {
        Monitor&    monitor;

        Synchronized& operator=(const Synchronized&);

    public:
        Synchronized(Monitor& monitor) : monitor(monitor)
        {
            monitor.lock();
        }
        ~Synchronized()
        {
            monitor.unlock();
        }
    };
};

class Thread : public IThread
{
    static pthread_key_t cleanupKey;
    static const int MaxSpecific = 32;
    static void (*dtorTable[MaxSpecific])(void*);

    Ref             ref;
    IThread::State  state;
    int             priority;
    void*         (*run)(void*);
    void*           param;
    pthread_t       thread;
    const void*     errorCode;
    const void*     specific[MaxSpecific];

    void exit(const void* errorCode);
    void* getSpecific(int index);
    void setSpecific(int index, const void* ptr);

    static void* init(void* arg);
    static void cleanup(void* arg);

public:
    Thread(void* (*run)(void*), void* param, int priority,
        void* stack = 0, unsigned stackSize = 0);
    ~Thread();

    // IThread
    int getState();
    void start();
    int getPriority();
    void setPriority(int priority);
    void* join();
    void cancel();

    // IInterface
    void* queryInterface(const Guid& riid);
    unsigned int addRef(void);
    unsigned int release(void);

    static Thread* getCurrentThread();
    static void reschedule();

    friend void Core::exit(const void* val);

    friend void Monitor::lock();
    friend bool Monitor::wait();
    friend bool Monitor::wait(s64 timeout);

    friend void esInitThread();
    friend void esSleep(s64 timeout);
};

class Lock : public Monitor
{
    using Monitor::wait;
    using Monitor::notify;
    using Monitor::notifyAll;
    using Monitor::queryInterface;
    using Monitor::addRef;
    using Monitor::release;
};

class SpinLock : public Monitor
{
    using Monitor::wait;
    using Monitor::notify;
    using Monitor::notifyAll;
    using Monitor::queryInterface;
    using Monitor::addRef;
    using Monitor::release;
};

#include "cache.h"

int esInit(IInterface** nameSpace);
IThread* esCreateThread(void* (*start)(void* param), void* param);
IMonitor* esCreateMonitor();

#ifdef __cplusplus
extern "C" {
#endif

int stricmp(const char *s1, const char *s2);
int strnicmp(const char *s1, const char *s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif // NINTENDO_ES_KERNEL_POSIX_CORE_H_INCLUDED