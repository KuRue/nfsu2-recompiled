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
template<class F> void rejects(F action,const char* message){bool caught=false;try{action();}catch(const std::runtime_error&){caught=true;}require(caught,message);}
// A2-level A8R8G8B8 texture, 8x8: 32-byte rows at level 0, 16-byte rows at level 1.
struct NativeTexture final: IDirect3DTexture9 {
    UINT width,height,levels;DWORD usage;D3DPOOL pool;ULONG refs=1;
    std::vector<std::vector<uint8_t>> data;int lockedLevel=-1;DWORD lastFlags=0;unsigned readbacks=0;bool failUnlock=false;
    NativeTexture(UINT w,UINT h,UINT l,DWORD u,D3DPOOL p):width(w),height(h),levels(l),usage(u),pool(p){
        ++live;for(UINT i=0;i<l;++i)data.emplace_back(size_t(std::max(1u,w>>i))*4*std::max(1u,h>>i),0xcd);}
    ~NativeTexture(){--live;}
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
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override{return D3DRTYPE_TEXTURE;}
    DWORD STDMETHODCALLTYPE SetLOD(DWORD value) override{return value;}
    DWORD STDMETHODCALLTYPE GetLOD() override{return 0;}
    DWORD STDMETHODCALLTYPE GetLevelCount() override{return levels;}
    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override{return D3D_OK;}
    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override{return D3DTEXF_LINEAR;}
    void STDMETHODCALLTYPE GenerateMipSubLevels() override{}
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT level,D3DSURFACE_DESC* out) override{
        if(level>=levels)return D3DERR_INVALIDCALL;*out=D3DSURFACE_DESC{};out->Format=D3DFMT_A8R8G8B8;out->Type=D3DRTYPE_TEXTURE;out->Usage=usage;out->Pool=pool;
        out->Width=std::max(1u,width>>level);out->Height=std::max(1u,height>>level);return D3D_OK;}
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT,IDirect3DSurface9** out) override{*out=nullptr;return E_FAIL;}
    HRESULT STDMETHODCALLTYPE LockRect(UINT level,D3DLOCKED_RECT* out,const RECT* rect,DWORD flags) override{
        if(level>=levels||lockedLevel>=0)return D3DERR_INVALIDCALL;
        if(pool==D3DPOOL_DEFAULT&&!(usage&D3DUSAGE_DYNAMIC))return D3DERR_INVALIDCALL;   // as on Windows
        UINT pitch=std::max(1u,width>>level)*4;lockedLevel=int(level);lastFlags=flags;if(flags&D3DLOCK_READONLY)++readbacks;
        out->Pitch=INT(pitch);out->pBits=data[level].data()+(rect?size_t(rect->top)*pitch+size_t(rect->left)*4:0);return D3D_OK;}
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT level) override{if(int(level)!=lockedLevel)return D3DERR_INVALIDCALL;lockedLevel=-1;return failUnlock?E_FAIL:D3D_OK;}
    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT*) override{return D3D_OK;}
};
struct Root final: IUnknown {
    ULONG refs=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out) override{*out=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto count=--refs;if(!count)delete this;return count;}
};
GuestMemory memory;GuestHeaps heaps(memory);
uint32_t alloc(uint32_t size){return heaps.alloc(GuestHeaps::process,8,size);}
void freeAllocation(uint32_t address){require(heaps.free(GuestHeaps::process,address),"Guest texture wrapper double free");}
uint32_t invoke(uint32_t address,uint32_t method,std::initializer_list<uint32_t> tail){
    std::vector<uint32_t> args{address};args.insert(args.end(),tail);uint32_t count=0;
    auto result=dispatchGuestD3D9(methodBase+3*4096+method*16,args.data(),&count);require(count==args.size(),"Texture ABI cleanup differs");return result;
}
// The game's upload: LockRect through the bridge, write rows at the returned pitch, UnlockRect.
HRESULT upload(uint32_t address,uint32_t level,const RECT* rect,uint8_t fill,uint32_t flags=0){
    uint32_t out=alloc(8),rectangle=0;
    if(rect){rectangle=alloc(16);std::memcpy(pointer(rectangle),rect,16);}
    require(SUCCEEDED(HRESULT(invoke(address,19,{level,out,rectangle,flags}))),"Guest texture lock failed");
    uint32_t pitch,guest;std::memcpy(&pitch,pointer(out),4);std::memcpy(&guest,pointer(out+4),4);
    uint32_t side=std::max(1u,8u>>level),width=rect?uint32_t(rect->right-rect->left):side,height=rect?uint32_t(rect->bottom-rect->top):side;
    for(uint32_t row=0;row<height;++row)std::memset(static_cast<uint8_t*>(pointer(guest))+size_t(row)*pitch,fill+row,width*4);
    HRESULT status=HRESULT(invoke(address,20,{level}));freeAllocation(out);if(rectangle)freeAllocation(rectangle);return status;
}
std::vector<uint8_t> expectedLevel(uint32_t level,uint8_t fill){
    uint32_t side=std::max(1u,8u>>level);std::vector<uint8_t> out(size_t(side)*side*4);
    for(uint32_t row=0;row<side;++row)std::memset(out.data()+size_t(row)*side*4,fill+row,side*4);return out;
}
uint32_t makeTexture(uint32_t device,DWORD usage,D3DPOOL pool,NativeTexture*& native){
    native=new NativeTexture(8,8,2,usage,pool);auto address=wrap(native,Kind::Texture);auto output=alloc(4);write(output,address);
    uint32_t args[]={device,8,8,2,usage,uint32_t(D3DFMT_A8R8G8B8),uint32_t(pool),output,0};recordCreation(23,args,9);freeAllocation(output);
    return address;
}
}
int main(){try{
    configureD3D9Bridge(memory.base(),alloc,freeAllocation);
    auto root=new Root;auto device=wrap(root,Kind::Device);
    // Managed texture with no uploads seen: read back from the native object, READONLY.
    NativeTexture* managed=nullptr;auto first=makeTexture(device,0,D3DPOOL_MANAGED,managed);
    auto records=d3d9TextureList();
    require(records.size()==1&&records[0].definition.contents.size()==2&&records[0].definition.contents[0].bytes==managed->data[0],"Managed readback differs");
    require(managed->readbacks==2&&managed->lastFlags==D3DLOCK_READONLY,"Readback was not READONLY per level");
    // Whole-level uploads are shadowed: capture uses them without reading the GPU.
    require(SUCCEEDED(upload(first,0,nullptr,0x10))&&SUCCEEDED(upload(first,1,nullptr,0x40)),"Upload failed");
    unsigned before=managed->readbacks;records=d3d9TextureList();
    require(managed->readbacks==before,"Shadowed capture read the GPU");
    require(records[0].definition.contents[0].bytes==expectedLevel(0,0x10)&&records[0].definition.contents[1].bytes==expectedLevel(1,0x40),"Shadow capture differs");
    // A partial lock updates a complete level in place.
    RECT patch{2,3,6,5};require(SUCCEEDED(upload(first,0,&patch,0x70)),"Partial upload failed");
    auto partial=d3d9TextureList()[0].definition.contents[0].bytes,want=expectedLevel(0,0x10);
    for(uint32_t row=0;row<2;++row)std::memset(want.data()+size_t(3+row)*32+8,0x70+row,16);
    require(partial==want,"Partial lock shadow differs");
    // Same-session restore replays the capture through the guest lock path.
    require(SUCCEEDED(upload(first,0,nullptr,0x99))&&SUCCEEDED(upload(first,1,nullptr,0x99)),"Overwrite failed");
    restoreD3D9Textures(records);
    require(managed->data[0]==expectedLevel(0,0x10)&&managed->data[1]==expectedLevel(1,0x40),"Same-session texture restore differs");
    require(d3d9TextureList()[0].definition.contents[0].bytes==expectedLevel(0,0x10),"Restore did not reset the shadow");
    require(resourceLocks.empty(),"Restore left a lock");
    // Default-pool texture: cannot be read back, so it needs complete shadows.
    NativeTexture* gpuOnly=nullptr;auto second=makeTexture(device,0,D3DPOOL_DEFAULT,gpuOnly);
    rejects([]{d3d9TextureList();},"Default-pool texture without shadow was captured");
    // The bridge's own LockRect on a default-pool non-dynamic texture fails like Windows; use a dynamic one for uploads.
    gpuOnly->usage=D3DUSAGE_DYNAMIC;objects.at(second).creation->args[4]=D3DUSAGE_DYNAMIC;
    require(SUCCEEDED(upload(second,0,nullptr,0x20))&&SUCCEEDED(upload(second,1,nullptr,0x30)),"Dynamic upload failed");
    require(d3d9TextureList().size()==2,"Shadowed default texture refused");
    // GPU-side writes and Reset drop shadows; a partial lock cannot complete a level.
    uint32_t updateArgs[]={device,0,second,0};invalidateTextureShadowsForDeviceCall(31,updateArgs);
    gpuOnly->usage=0;objects.at(second).creation->args[4]=0;
    rejects([]{d3d9TextureList();},"UpdateTexture destination kept its shadow");
    gpuOnly->usage=D3DUSAGE_DYNAMIC;objects.at(second).creation->args[4]=D3DUSAGE_DYNAMIC;
    require(SUCCEEDED(upload(second,0,&patch,0x50)),"Partial upload failed");
    require(!textureShadows.at(second).complete(0,0),"Partial lock completed an unseen level");
    require(SUCCEEDED(upload(second,0,nullptr,0x20))&&SUCCEEDED(upload(second,1,nullptr,0x30)),"Re-upload failed");
    uint32_t resetArgs[]={device,0};invalidateTextureShadowsForDeviceCall(16,resetArgs);
    require(!textureShadows.count(second)&&textureShadows.count(first),"Reset dropped the wrong shadows");
    // Locked textures refuse capture; a failed Unlock drops the shadow.
    uint32_t out=alloc(8);require(SUCCEEDED(HRESULT(invoke(first,19,{0,out,0,0}))),"Test lock failed");
    rejects([]{d3d9TextureList();},"Locked texture was captured");
    invoke(first,20,{0});freeAllocation(out);
    managed->failUnlock=true;require(FAILED(upload(first,0,nullptr,0x11)),"Driver unlock failure hidden");managed->failUnlock=false;
    require(!textureShadows.count(first),"Failed unlock kept the shadow");
    // Metadata checks before a load.
    auto bad=records;bad[0].references++;rejects([&]{validateD3D9Textures(bad);},"Reference change accepted");
    // Render targets are transient: no contents, no readback.
    NativeTexture* target=nullptr;auto third=makeTexture(device,D3DUSAGE_RENDERTARGET,D3DPOOL_DEFAULT,target);
    {gpuOnly->usage=0;objects.at(second).creation->args[4]=0;}
    retireGuestObject(second);gpuOnly->Release();
    auto list=d3d9TextureList();
    require(list.size()==2&&list[0].address==first&&list[1].address==third&&list[1].definition.transient&&list[1].definition.contents.empty()&&target->readbacks==0,"Render target not transient");
    // The capture format carries them.
    nfs_capture::Capture archive;archive.build=std::string(64,'a');archive.game=std::string(64,'b');nfs_capture::ThreadImage main;
    main.id=1;main.capturer=true;main.resumable=true;main.wait=nfs_capture::WaitKind::Present;main.frames.push_back({0x401000,0x401020,0x7e002110,0,0});archive.threads.push_back(main);
    archive.graphics=d3d9ObjectList();archive.hasDevices=false;archive.textures=list;
    auto decoded=nfs_capture::decode(nfs_capture::encode(archive));
    require(decoded.textures.size()==2&&decoded.textures[0]==list[0]&&decoded.textures[1]==list[1],"Texture archive differs");
    // Retirement frees shadows and wrappers.
    retireGuestObject(first);managed->Release();retireGuestObject(third);target->Release();
    require(textureShadows.empty(),"Retired texture shadow leaked");
    root->Release();retireGuestObject(device);
    require(live==0&&objects.empty()&&resourceLocks.empty(),"Texture ownership/locks leaked");
    std::puts("D3D9 textures passed: readback, shadows, partial locks, same-session restore, invalidation, transient targets and archive");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
