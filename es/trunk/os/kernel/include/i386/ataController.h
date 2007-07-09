/*
 * Copyright (c) 2006, 2007
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#ifndef NINTENDO_ES_KERNEL_I386_ATACONTROLLER_H_INCLUDED
#define NINTENDO_ES_KERNEL_I386_ATACONTROLLER_H_INCLUDED

#include <es/types.h>
#include <es/ref.h>
#include <es/synchronized.h>
#include <es/base/ICallback.h>
#include <es/base/IMonitor.h>
#include <es/base/IStream.h>
#include <es/device/IDiskManagement.h>
#include <es/device/IRemovableMedia.h>
#include <es/naming/IContext.h>
#include "ata.h"
#include "thread.h" // XXX

class AtaController;
class AtaDevice;

class AtaDma
{
    friend class AtaController;

    virtual int setup(u8 cmd, void* buffer, int count /* in byte */) = 0;
    virtual void start(u8 cmd) = 0;
    virtual int stop() = 0;
    virtual void interrupt(int count /* in byte */) = 0;
};

class AtaController : public ICallback
{
    friend class AtaDevice;
    friend class AtaPacketDevice;

    IMonitor*       monitor;
    Lock            lock;
    Ref             ref;

    int             cmdPort;
    int             ctlPort;
    int             irq;
    AtaDma*         dma;
    AtaDevice*      device[2];

    AtaDevice*      current;
    u8              cmd;
    u8*             data;
    u8*             limit;
    volatile bool   done;
    Rendezvous      rendezvous;

    u8              packet[16];
    u8              features;

    Thread          thread;

    bool softwareReset();
    bool detectDevice(int dev, u8* signature);

    int condDone(int);
    void wait();
    void notify();

    static bool isAtaDevice(const u8* signature);
    static bool isAtapiDevice(const u8* signature);
    static void* run(void* param);

public:
    AtaController(int cmdPort, int ctlPort, int irq, AtaDma* dma, IContext* ata);
    ~AtaController();
    void select(u8 device);
    u8 sync(u8 status);
    int issue(AtaDevice* device, u8 cmd,
               void* buffer, int count, long long lba);
    int issue(AtaDevice* device, u8* packet, int packetSize,
              void* buffer = 0, int count = 0, u8 features = 0);
    int invoke(int);
    bool queryInterface(const Guid& riid, void** objectPtr);
    unsigned int addRef();
    unsigned int release();

    void detect();
};

class AtaDevice : public IStream, public IContext, public IDiskManagement
{
    friend class AtaController;

protected:
    IMonitor*       monitor;
    Ref             ref;
    AtaController*  ctlr;
    u8              device;
    u16             id[256];
    long long       size;
    u16             sectorSize;
    u8              readCmd;
    u8              writeCmd;
    u16             multiple;

    u16             packetSize;
    u16             dma;
    bool            removal;

    IContext*       partition;

    bool identify(u8* signature);
    IContext* getPartition();

public:
    AtaDevice(AtaController* ctlr, u8 device, u8* signature);
    virtual ~AtaDevice();

    // IStream
    long long getPosition();
    void setPosition(long long pos);
    long long getSize();
    void setSize(long long size);
    int read(void* dst, int count);
    int read(void* dst, int count, long long offset);
    int write(const void* src, int count);
    int write(const void* src, int count, long long offset);
    void flush();

    // IContext
    IBinding* bind(const char* name, IInterface* object);
    IContext* createSubcontext(const char* name);
    int destroySubcontext(const char* name);
    IInterface* lookup(const char* name);
    int rename(const char* oldName, const char* newName);
    int unbind(const char* name);
    IIterator* list(const char* name);

    // IDiskManagement
    int initialize();
    int getGeometry(Geometry* geometry);
    int getLayout(Partition* partition);
    int setLayout(Partition* partition);

    bool queryInterface(const Guid& riid, void** objectPtr);
    unsigned int addRef();
    unsigned int release();

    virtual bool detect();
};

class AtaPacketDevice : public AtaDevice, public IRemovableMedia
{
    u8 testUnitReady();
    int requestSense(void* sense, int count);
    int modeSense(u8 pageCtrl, u8 pageCode, void* modeParamList, int count);
    int readCapacity();
    int startStopUnit(bool immediate, bool loEj, bool start, u8 powerCondition);
    int stopDisc();
    int startDisc();
    int preventAllowMediumRemoval(bool prevent, bool persistent = false);

public:
    AtaPacketDevice(AtaController* ctlr, u8 device, u8* signature);
    ~AtaPacketDevice();

    int eject();
    int load();
    int lock();
    int unlock();

    int read(void* dst, int count, long long offset);
    int write(const void* src, int count, long long offset);
    bool queryInterface(const Guid& riid, void** objectPtr);
    unsigned int addRef();
    unsigned int release();

    virtual bool detect();
};

#endif // NINTENDO_ES_KERNEL_I386_ATACONTROLLER_H_INCLUDED
