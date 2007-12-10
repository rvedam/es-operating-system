/*
 * Copyright (c) 2006
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

#include <es/base/IInterface.h>
using namespace es;
int esInit(IInterface** nameSpace);

#ifdef __unix__

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <es.h>
#include <es/ref.h>
#include <es/endian.h>
#include <es/base/IStream.h>
#include <es/device/IDiskManagement.h>

class VDisk : public IStream, public IDiskManagement
{
    Ref      ref;
    int      fd;
    Geometry geometry;

public:
    VDisk(char* vdisk)
    {
        geometry.cylinders = 0;
        geometry.heads = 0;
        geometry.sectorsPerTrack = 0;
        geometry.bytesPerSector = 0;
        geometry.diskSize = 0;

        fd = open(vdisk, O_RDWR);
        if (fd < 0)
        {
            fd = open(vdisk, O_RDWR | O_CREAT, 0777);
            if (fd < 0)
            {
                esThrow(ENOSPC);
            }
            setSize(512 * 2880);
        }
        long long size;
        size = getSize();
        if (size <= 512 * 2880 * 2)
        {
            switch (size / 512)
            {
            case 2880:  // 3-1/2 2HD
                geometry.cylinders = 80;
                geometry.heads = 2;
                geometry.sectorsPerTrack = 18;
                geometry.bytesPerSector = 512;
                geometry.diskSize = size;
                break;
            case 1440:  // 3-1/2 2DD
                geometry.cylinders = 80;
                geometry.heads = 2;
                geometry.sectorsPerTrack = 9;
                geometry.bytesPerSector = 512;
                geometry.diskSize = size;
                break;
            default:
                geometry.cylinders = 0;
                geometry.heads = 0;
                geometry.sectorsPerTrack = 0;
                geometry.bytesPerSector = 512;
                geometry.diskSize = size;
                break;
            }
        }
        else
        {
            // VPC 2004 vhd format
            u8 chs[4];
            read(chs, 4, size + 0x38);
            geometry.cylinders = BigEndian::word(chs + 0);
            geometry.heads = BigEndian::byte(chs + 2);
            geometry.sectorsPerTrack = BigEndian::byte(chs + 3);
            geometry.bytesPerSector = 512;
            geometry.diskSize = size;
            setPosition(0);
        }
        esReport("CHS: %u %u %u\n",
                 geometry.cylinders, geometry.heads, geometry.sectorsPerTrack);
    }

    ~VDisk()
    {
        close(fd);
    }

    //
    // IStream
    //

    long long getPosition()
    {
        int err;
        long long pos;
        pos = lseek64(fd, 0, SEEK_CUR);
        if (pos < 0)
        {
            esThrow(errno);
        }
        return pos;
    }

    void setPosition(long long pos)
    {
        int err;

        if (geometry.diskSize && (pos % 512))
        {
            esThrow(EINVAL);
        }

        err = lseek64(fd, pos, SEEK_SET);
        if (err < 0)
        {
            esThrow(errno);
        }
    }

    long long getSize()
    {
        long long tmp;
        tmp = getPosition();
        lseek64(fd, 0, SEEK_END);
        long long size;
        size = getPosition();
        setPosition(tmp);
        if (512 * 2880 * 2 < size)
        {
            // .vhd
            size -= 512;
        }
        return size;
    }

    void setSize(long long size)
    {
        int err;

        err = ftruncate(fd, size);
        if (err)
        {
            esThrow(errno);
        }
    }

    int read(void* buffer, int size)
    {
        esThrow(EINVAL);

        size_t n = ::read(fd, buffer, size);
        return (int) n;
    }

    int read(void* buffer, int size, long long offset)
    {
#ifdef VERBOSE
        esReport("vdisk::read %ld byte at 0x%llx to %p.\n", size, offset, buffer);
#endif
        setPosition(offset);
        size_t n = ::read(fd, buffer, size);
        return (int) n;
    }

    int write(const void* buffer, int size)
    {
        esThrow(EINVAL);

        size_t n = ::write(fd, buffer, size);
        return (int) n;
    }

    int write(const void* buffer, int size, long long offset)
    {
#ifdef VERBOSE
        esReport("vdisk::write %ld byte at 0x%llx from %p.\n", size, offset, buffer);
#endif
        setPosition(offset);
        size_t n = ::write(fd, buffer, size);
        return (int) n;
    }

    void flush()
    {
    }

    //
    // IDiskManagement
    //

    int initialize()
    {
        return -1;
    }

    void getGeometry(Geometry* geometry)
    {
        memmove(geometry, &this->geometry, sizeof(Geometry));
    }

    void getLayout(Partition* partition)
    {
    }

    void setLayout(const Partition* partition)
    {
    }

    //
    // IInterface
    //

    void* queryInterface(const Guid& riid)
    {
        void* objectPtr;
        if (riid == IStream::iid())
        {
            objectPtr = static_cast<IStream*>(this);
        }
        else if (riid == IDiskManagement::iid())
        {
            objectPtr = static_cast<IDiskManagement*>(this);
        }
        else if (riid == IInterface::iid())
        {
            objectPtr = static_cast<IStream*>(this);
        }
        else
        {
            return NULL;
        }
        static_cast<IInterface*>(objectPtr)->addRef();
        return objectPtr;
    }

    unsigned int addRef(void)
    {
        return ref.addRef();
    }

    unsigned int release(void)
    {
        unsigned int count = ref.release();
        if (count == 0)
        {
            delete this;
            return 0;
        }
        return count;
    }
};

#endif  // __endif__
