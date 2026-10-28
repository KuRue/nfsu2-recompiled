#pragma once
#include "graphics_buffer.h"
#include <type_traits>
#include "d3d9_program_state.h"
namespace nfs_d3d9 {
template<class Native> void uploadBuffer(Native* native,const Buffer& buffer) {
    validate(buffer);void* output=nullptr;
    DWORD flags=(buffer.usage&D3DUSAGE_DYNAMIC)?D3DLOCK_DISCARD:0;
    check(SUCCEEDED(native->Lock(0,UINT(buffer.bytes.size()),&output,flags)),"Cannot lock restored D3D9 buffer");
    if(!output){native->Unlock();check(false,"Null restored D3D9 buffer mapping");}
    std::memcpy(output,buffer.bytes.data(),buffer.bytes.size());
    check(SUCCEEDED(native->Unlock()),"Cannot unlock restored D3D9 buffer");
}
template<class Native,class Desc> Buffer captureBuffer(Native* native,uint32_t kind,const BufferShadow* shadow) {
    Desc desc{};check(SUCCEEDED(native->GetDesc(&desc)),"Cannot describe D3D9 buffer");
    check(desc.Size&&desc.Size<=maxBufferBytes,"Invalid D3D9 buffer size");
    Buffer buffer{kind,desc.Usage,uint32_t(desc.Format),uint32_t(desc.Pool),0,{}};
    if constexpr(std::is_same_v<Desc,D3DVERTEXBUFFER_DESC>)buffer.fvf=desc.FVF;
    if(shadow&&shadow->size()==desc.Size&&shadow->complete())buffer.bytes=shadow->bytes();
    else {
        check(!(desc.Usage&D3DUSAGE_WRITEONLY),"Write-only D3D9 buffer lacks a complete shadow");
        buffer.bytes.resize(desc.Size);void* input=nullptr;
        check(SUCCEEDED(native->Lock(0,desc.Size,&input,D3DLOCK_READONLY)),"Cannot read D3D9 buffer");
        if(!input){native->Unlock();check(false,"Null captured D3D9 buffer mapping");}
        std::memcpy(buffer.bytes.data(),input,desc.Size);
        check(SUCCEEDED(native->Unlock()),"Cannot unlock captured D3D9 buffer");
    }
    validate(buffer);return buffer;
}
template<class Device> StagedResource recreateBuffer(Device* device,const Buffer& buffer) {
    validate(buffer);check(device!=nullptr,"Missing D3D9 buffer device");
    if(buffer.kind==6){
        IDirect3DVertexBuffer9* native=nullptr;auto status=device->CreateVertexBuffer(UINT(buffer.bytes.size()),buffer.usage,buffer.fvf,D3DPOOL(buffer.pool),&native,nullptr);
        StagedResource staged(native);check(SUCCEEDED(status)&&native,"Cannot recreate D3D9 vertex buffer");uploadBuffer(native,buffer);return staged;
    }
    IDirect3DIndexBuffer9* native=nullptr;auto status=device->CreateIndexBuffer(UINT(buffer.bytes.size()),buffer.usage,D3DFORMAT(buffer.format),D3DPOOL(buffer.pool),&native,nullptr);
    StagedResource staged(native);check(SUCCEEDED(status)&&native,"Cannot recreate D3D9 index buffer");uploadBuffer(native,buffer);return staged;
}
}
