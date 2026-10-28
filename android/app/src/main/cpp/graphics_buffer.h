#pragma once
#include "graphics_program.h"
#include <algorithm>
#include <utility>
namespace nfs_d3d9 {
constexpr size_t maxBufferBytes = 64u << 20;
struct Buffer {
    uint32_t kind = 6, usage = 0, format = 100, pool = 0, fvf = 0;
    std::vector<uint8_t> bytes;
};
struct BufferRecord {
    uint32_t address = 0, table = 0, device = 0, references = 0;
    Buffer definition;
};
inline bool sameMetadata(const BufferRecord& a, const BufferRecord& b) {
    return a.address == b.address && a.table == b.table && a.device == b.device && a.references == b.references
        && a.definition.kind == b.definition.kind && a.definition.usage == b.definition.usage
        && a.definition.format == b.definition.format && a.definition.pool == b.definition.pool
        && a.definition.fvf == b.definition.fvf && a.definition.bytes.size() == b.definition.bytes.size();
}
inline bool operator==(const BufferRecord& a,const BufferRecord& b) {return sameMetadata(a,b)&&a.definition.bytes==b.definition.bytes;}
inline void validate(const Buffer& buffer) {
    check(!buffer.bytes.empty() && buffer.bytes.size() <= maxBufferBytes, "Invalid D3D9 buffer size");
    check(buffer.pool <= 3 && (buffer.kind == 6 || buffer.kind == 7), "Invalid D3D9 buffer kind/pool");
    check(buffer.kind == 6 ? buffer.format == 100 : (buffer.fvf == 0 && (buffer.format == 101 || buffer.format == 102)), "Invalid D3D9 buffer format");
    if(buffer.kind == 7)check(buffer.bytes.size() % (buffer.format == 101 ? 2 : 4) == 0, "Unaligned D3D9 index buffer");
}
// A write-only buffer cannot legally be read back. Retain only bytes actually uploaded by
// successful guest Unlocks. DISCARD makes all previous contents unavailable, including holes.
class BufferShadow {
    std::vector<uint8_t> data;
    std::vector<std::pair<size_t,size_t>> ranges;
public:
    explicit BufferShadow(size_t size) {check(size && size <= maxBufferBytes,"Invalid buffer shadow size");data.resize(size);}
    size_t size() const {return data.size();}
    bool complete() const {return ranges.size()==1&&ranges[0].first==0&&ranges[0].second==data.size();}
    const std::vector<uint8_t>& bytes() const {check(complete(),"Buffer shadow has uninitialized ranges");return data;}
    void update(size_t offset,const uint8_t* source,size_t size,bool discard) {
        check(source&&size&&offset<=data.size()&&size<=data.size()-offset,"Invalid buffer shadow update");
        if(discard)ranges.clear();
        std::memcpy(data.data()+offset,source,size);ranges.emplace_back(offset,offset+size);
        std::sort(ranges.begin(),ranges.end());
        size_t out=0;
        for(auto range:ranges){if(out&&range.first<=ranges[out-1].second)ranges[out-1].second=std::max(ranges[out-1].second,range.second);else ranges[out++]=range;}
        ranges.resize(out);
    }
};
}
