#pragma once

#include <cstdint>

namespace Storage
{

class SliceId
{
public:
    SliceId(void): _data(0) { }
    SliceId(uint64_t value): _data(value) { }
    bool operator == (const SliceId &x) const { return _data == x._data; }
    bool operator != (const SliceId &x) const { return _data != x._data; }
    bool operator <  (const SliceId &x) const { return _data <  x._data; }
    uint64_t UInt(void) const { return _data; }
    uint64_t Cluster(void) const
    {
        return GetBits(0xFF00000000000000UL, 56);
    }
    uint64_t Machine(void) const
    {
        return GetBits(0x00FFF00000000000UL, 44);
    }
    uint64_t Disk(void) const
    {
        return GetBits(0x00000FF000000000UL, 36);
    }
    uint64_t Chunk(void) const
    {
        return GetBits(0x0000000FFFF00000UL, 20);
    }
    uint64_t Slice(void) const
    {
        return GetBits(0x00000000000FFFFFUL, 0);
    }
    void SetCluster(uint64_t value)
    {
        return SetBits(value, 0xFF00000000000000UL, 56);
    }
    void SetMachine(uint64_t value)
    {
        return SetBits(value, 0x00FFF00000000000UL, 44);
    }
    void SetDisk(uint64_t value)
    {
        return SetBits(value, 0x00000FF000000000UL, 36);
    }
    void SetChunk(uint64_t value)
    {
        return SetBits(value, 0x0000000FFFF00000UL, 20);
    }
    void SetSlice(uint64_t value)
    {
        return SetBits(value, 0x00000000000FFFFFUL, 0);
    }
private:
    uint64_t _data;
    inline uint32_t GetBits(uint64_t mask, uint64_t offset) const
    {
        return (_data & mask) >> offset;
    }
    inline void SetBits(uint64_t value, uint64_t mask, uint64_t offset)
    {
        _data = (_data & (~mask)) | ((value << offset) & mask);
    }
};

}