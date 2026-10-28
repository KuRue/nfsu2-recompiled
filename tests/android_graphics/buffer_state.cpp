#define NFS_RESUMABLE
#include "../../android/app/src/main/cpp/capture_format.h"
#include "../../android/app/src/main/cpp/guest_memory.h"
#include "../../android/app/src/main/cpp/guest_heap.h"
#include "../../android/app/src/main/cpp/android_resolution.h"
#include "../../android/app/src/main/cpp/d3d9_bridge.cpp"

extern "C" IDirect3D9* Direct3DCreate9(UINT){return nullptr;}
extern "C" void dxvkAndroidSetWindow(ANativeWindow*){}
extern "C" HWND dxvkAndroidGetWindowHandle(){return nullptr;}
extern "C" void dxvkAndroidSetRenderSize(uint32_t,uint32_t){}
#ifndef __ANDROID__
extern "C" ANativeWindow* dxvkCreateTestWindow(){return nullptr;}
extern "C" void dxvkDestroyTestWindow(ANativeWindow*){}
#endif
namespace {
int live=0;
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
template<class F> void rejects(F action){bool caught=false;try{action();}catch(const std::runtime_error&){caught=true;}require(caught,"Invalid buffer operation accepted");}
template<class Interface,class Desc> struct NativeBuffer final: Interface {
    Desc desc{};std::vector<uint8_t> bytes;ULONG refs=1;bool locked=false,failLock=false,failUnlock=false;DWORD lastFlags=0;unsigned locks=0;
    explicit NativeBuffer(const nfs_d3d9::Buffer& buffer):bytes(buffer.bytes.size(),0xcd){
        ++live;desc.Size=UINT(bytes.size());desc.Usage=buffer.usage;desc.Format=D3DFORMAT(buffer.format);desc.Pool=D3DPOOL(buffer.pool);
        if constexpr(std::is_same_v<Desc,D3DVERTEXBUFFER_DESC>){desc.FVF=buffer.fvf;desc.Type=D3DRTYPE_VERTEXBUFFER;}else desc.Type=D3DRTYPE_INDEXBUFFER;
    }
    ~NativeBuffer(){--live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out) override{*out=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto count=--refs;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override{*out=nullptr;return E_FAIL;}
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,const void*,DWORD,DWORD) override{return E_FAIL;}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,void*,DWORD*) override{return E_FAIL;}
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override{return E_FAIL;}
    DWORD STDMETHODCALLTYPE SetPriority(DWORD value) override{return value;}
    DWORD STDMETHODCALLTYPE GetPriority() override{return 0;}
    void STDMETHODCALLTYPE PreLoad() override{}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override{return desc.Type;}
    HRESULT STDMETHODCALLTYPE GetDesc(Desc* out) override{*out=desc;return D3D_OK;}
    HRESULT STDMETHODCALLTYPE Lock(UINT offset,UINT size,void** output,DWORD flags) override{
        ++locks;if(failLock||locked||offset>=bytes.size()||(size&&size>bytes.size()-offset)
            ||((flags&D3DLOCK_READONLY)&&(desc.Usage&D3DUSAGE_WRITEONLY)))return D3DERR_INVALIDCALL;
        if(flags&D3DLOCK_DISCARD)std::fill(bytes.begin(),bytes.end(),0xcd);
        locked=true;lastFlags=flags;*output=bytes.data()+offset;return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock() override{if(!locked)return D3DERR_INVALIDCALL;locked=false;return failUnlock?E_FAIL:D3D_OK;}
};
using VB=NativeBuffer<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>;
using IB=NativeBuffer<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>;
struct Device {
    nfs_d3d9::Buffer expected;bool fail=false,failLock=false,failUnlock=false;
    template<class Interface,class Native> HRESULT create(UINT size,DWORD usage,uint32_t format,D3DPOOL pool,Interface** out){
        require(size==expected.bytes.size()&&usage==expected.usage&&uint32_t(pool)==expected.pool,"Buffer creation parameters differ");
        require(format==(expected.kind==6?expected.fvf:expected.format),"Buffer FVF/index format differs");
        auto native=new Native(expected);native->failLock=failLock;native->failUnlock=failUnlock;*out=native;return fail?E_FAIL:D3D_OK;
    }
    HRESULT CreateVertexBuffer(UINT size,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer9** out,HANDLE*){return create<IDirect3DVertexBuffer9,VB>(size,usage,fvf,pool,out);}
    HRESULT CreateIndexBuffer(UINT size,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DIndexBuffer9** out,HANDLE*){return create<IDirect3DIndexBuffer9,IB>(size,usage,uint32_t(format),pool,out);}
};
GuestMemory memory;GuestHeaps heaps(memory);
uint32_t alloc(uint32_t size){return heaps.alloc(GuestHeaps::process,8,size);}
void freeAllocation(uint32_t address){require(heaps.free(GuestHeaps::process,address),"Guest buffer wrapper double free");}
std::vector<nfs_d3d9::Buffer> fixtures(){return {{6,D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,100,0,0x112,{1,2,3,4,5,6,7,8}},
    {7,D3DUSAGE_WRITEONLY,101,1,0,{0,0,1,0,2,0}}};}
uint32_t invoke(uint32_t address,uint32_t kind,uint32_t method,std::initializer_list<uint32_t> tail){
    std::vector<uint32_t> args{address};args.insert(args.end(),tail);uint32_t count=0;
    auto result=dispatchGuestD3D9(methodBase+kind*4096+method*16,args.data(),&count);require(count==args.size(),"Buffer ABI cleanup differs");return result;
}
void writeBuffer(uint32_t address,const nfs_d3d9::Buffer& fixture,uint32_t offset,uint32_t size,uint32_t flags,uint8_t fill=0){
    auto output=alloc(4);require(SUCCEEDED(HRESULT(invoke(address,fixture.kind,11,{offset,size,output,flags}))),"Guest buffer lock failed");
    uint32_t mapping=0;std::memcpy(&mapping,pointer(output),4);
    if(fill)std::memset(pointer(mapping),fill,size?size:fixture.bytes.size()-offset);
    else std::memcpy(pointer(mapping),fixture.bytes.data()+offset,size?size:fixture.bytes.size()-offset);
    require(SUCCEEDED(HRESULT(invoke(address,fixture.kind,12,{}))),"Guest buffer unlock failed");freeAllocation(output);
}
}
int main(int argc,char** argv){try{
    using namespace nfs_d3d9;
    auto data=fixtures();bool writing=argc==3&&std::string(argv[1])=="--write",reading=argc==3&&std::string(argv[1])=="--read";
    require(argc==1||writing||reading,"Invalid buffer test arguments");
    if(reading){for(size_t i=0;i<data.size();++i){
        std::ifstream input(std::filesystem::path(argv[2])/(std::to_string(i)+".nfs"),std::ios::binary);require(bool(input),"Cannot read buffer archive");
        nfs_capture::Bytes bytes((std::istreambuf_iterator<char>(input)),{});auto archive=nfs_capture::decode(bytes);require(archive.buffers.size()==1,"Buffer archive count differs");
        auto restored=archive.buffers[0].definition;require(restored.bytes==data[i].bytes&&restored.kind==data[i].kind,"Buffer archive bytes/kind differ");
        Device device{data[i]};{auto staged=recreateBuffer(&device,restored);
            require(restored.kind==6?static_cast<VB*>(staged.get())->bytes==data[i].bytes:static_cast<IB*>(staged.get())->bytes==data[i].bytes,"Fresh recreated buffer bytes differ");}
        require(live==0&&objects.empty(),"Fresh recreation leaked or reused a proxy");
    }std::puts("Fresh-process VB/IB archive recreation passed");return 0;}
    BufferShadow shadow(8);uint8_t bytes[8]={1,2,3,4,5,6,7,8};shadow.update(4,bytes+4,4,false);require(!shadow.complete(),"Partial shadow was complete");
    shadow.update(0,bytes,4,false);require(shadow.complete()&&shadow.bytes()==data[0].bytes,"Shadow merge differs");
    shadow.update(2,bytes+2,2,true);require(!shadow.complete(),"DISCARD retained old initialized ranges");rejects([&]{shadow.bytes();});
    rejects([&]{shadow.update(7,bytes,2,false);});rejects([&]{BufferShadow oversized(maxBufferBytes+1);});
    configureD3D9Bridge(memory.base(),alloc,freeAllocation);auto root=new VB(data[0]);auto deviceAddress=wrap(root,Kind::Device);
    for(size_t i=0;i<data.size();++i){auto fixture=data[i];IUnknown* native=fixture.kind==6?static_cast<IUnknown*>(new VB(fixture)):static_cast<IUnknown*>(new IB(fixture));
        auto address=wrap(native,Kind(fixture.kind));auto output=alloc(4);write(output,address);
        uint32_t args[]={deviceAddress,UINT(fixture.bytes.size()),fixture.usage,fixture.kind==6?fixture.fvf:fixture.format,fixture.pool,output,0};recordCreation(fixture.kind==6?26:27,args,7);
        unsigned locks=fixture.kind==6?static_cast<VB*>(native)->locks:static_cast<IB*>(native)->locks;
        rejects([&]{d3d9BufferList();});require((fixture.kind==6?static_cast<VB*>(native)->locks:static_cast<IB*>(native)->locks)==locks,"Write-only capture tried native readback");
        writeBuffer(address,fixture,0,0,fixture.usage&D3DUSAGE_DYNAMIC?D3DLOCK_DISCARD:0);
        auto records=d3d9BufferList();require(records.size()==1&&records[0].definition.bytes==fixture.bytes,"Bridge buffer capture differs");
        writeBuffer(address,fixture,0,0,fixture.usage&D3DUSAGE_DYNAMIC?D3DLOCK_DISCARD:0,99);
        restoreD3D9Buffers(records);require((fixture.kind==6?static_cast<VB*>(native)->bytes:static_cast<IB*>(native)->bytes)==fixture.bytes,"Same-session buffer restore differs");
        uint32_t gpuArgs[]={deviceAddress,0,0,0,address,0,0};
        if(fixture.kind==6){invalidateBufferShadowsForDeviceCall(85,gpuArgs);rejects([&]{d3d9BufferList();});writeBuffer(address,fixture,0,0,D3DLOCK_DISCARD);}
        invalidateBufferShadowsForDeviceCall(16,gpuArgs);
        if(fixture.pool==D3DPOOL_DEFAULT){rejects([&]{d3d9BufferList();});writeBuffer(address,fixture,0,0,D3DLOCK_DISCARD);}
        else require(d3d9BufferList()[0].definition.bytes==fixture.bytes,"Reset invalidated managed buffer shadow");
        auto bad=records;bad[0].references++;rejects([&]{validateD3D9Buffers(bad);});
        auto lockedOutput=alloc(4);require(SUCCEEDED(HRESULT(invoke(address,fixture.kind,11,{0,0,lockedOutput,0}))),"Test lock failed");rejects([&]{d3d9BufferList();});invoke(address,fixture.kind,12,{});freeAllocation(lockedOutput);
        nfs_capture::Capture archive;archive.build=std::string(64,'a');archive.game=std::string(64,'b');nfs_capture::ThreadImage main;
        main.id=1;main.capturer=true;main.resumable=true;main.wait=nfs_capture::WaitKind::Present;main.frames.push_back({0x401000,0x401020,0x7e002110,0,0});archive.threads.push_back(main);
        archive.graphics=d3d9ObjectList();archive.buffers=records;auto encoded=nfs_capture::encode(archive);
        if(writing)nfs_capture::writeAtomic(std::filesystem::path(argv[2])/(std::to_string(i)+".nfs"),encoded);
        auto decoded=nfs_capture::decode(encoded);require(decoded.buffers==records,"Buffer archive metadata differs");Device device{fixture};int before=live;
        {auto staged=recreateBuffer(&device,decoded.buffers[0].definition);require(staged.get()&&live==before+1,"Buffer not staged");}
        require(live==before,"Buffer staging leaked");device.fail=true;rejects([&]{recreateBuffer(&device,fixture);});require(live==before,"Failed create leaked");
        device.fail=false;device.failLock=true;rejects([&]{recreateBuffer(&device,fixture);});require(live==before,"Failed upload lock leaked");
        device.failLock=false;device.failUnlock=true;rejects([&]{recreateBuffer(&device,fixture);});require(live==before,"Failed upload unlock leaked");
        if(fixture.usage&D3DUSAGE_DYNAMIC){writeBuffer(address,fixture,0,4,D3DLOCK_DISCARD);rejects([&]{d3d9BufferList();});writeBuffer(address,fixture,4,4,D3DLOCK_NOOVERWRITE);require(d3d9BufferList()[0].definition.bytes==fixture.bytes,"Partial DISCARD/NOOVERWRITE shadow differs");}
        if(fixture.kind==6)static_cast<VB*>(native)->failUnlock=true;else static_cast<IB*>(native)->failUnlock=true;
        auto failureOutput=alloc(4);require(SUCCEEDED(HRESULT(invoke(address,fixture.kind,11,{0,0,failureOutput,0}))),"Failed-unlock setup lock failed");
        require(FAILED(HRESULT(invoke(address,fixture.kind,12,{}))),"Driver unlock failure was hidden");freeAllocation(failureOutput);
        rejects([&]{d3d9BufferList();});require(!bufferShadows.count(address),"Failed unlock retained shadow");
        native->Release();retireGuestObject(address);freeAllocation(output);require(bufferShadows.empty(),"Retired buffer shadow leaked");
    }
    auto readable=data[0];readable.usage=0;VB readback(readable);readback.bytes=readable.bytes;
    require(captureBuffer<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>(&readback,6,nullptr).bytes==readable.bytes&&readback.lastFlags==D3DLOCK_READONLY,"Readable buffer capture failed");
    root->Release();retireGuestObject(deviceAddress);require(live==1&&objects.empty()&&resourceLocks.empty(),"Buffer ownership/locks leaked");
    std::puts("D3D9 buffers passed: write-only shadows, partial/discard uploads, capture/restore, reconstruction, failures and ownership");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
