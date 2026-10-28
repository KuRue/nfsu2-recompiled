#include "d3d9_bridge.h"
#include "d3d9_program_state.h"
#include "d3d9_buffer_state.h"
#include "graphics_texture.h"
#include "runtime_log.h"
#include "dxt_shadow.h"
#include "texture_pack.h"
#include "vertex_formats.h"
#include <d3d9.h>
#include <algorithm>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <cstring>
#include <stdexcept>
#ifdef __ANDROID__
#include <android/native_window.h>
#endif
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdlib>

extern "C" IDirect3D9* Direct3DCreate9(UINT sdk);
extern "C" void dxvkAndroidSetWindow(ANativeWindow* window);
extern "C" HWND dxvkAndroidGetWindowHandle();
extern "C" void dxvkAndroidSetRenderSize(uint32_t width,uint32_t height);
void setD3D9RenderSize(unsigned width,unsigned height){dxvkAndroidSetRenderSize(width,height);}
#ifndef __ANDROID__
extern "C" ANativeWindow* dxvkCreateTestWindow();
extern "C" void dxvkDestroyTestWindow(ANativeWindow* window);
#endif
namespace {
constexpr uint32_t methodBase=0x7e000000;
enum class Kind:uint32_t {D3D9=1,Device=2,Texture=3,Cube=4,Surface=5,VertexBuffer=6,IndexBuffer=7,
    VertexShader=8,PixelShader=9,Declaration=10,StateBlock=11,Query=12,SwapChain=13,VolumeTexture=14,Volume=15};
// creation: for objects the game created through the device, the Create method and its
// arguments (blob: declaration elements or shader bytecode they point to). A restore recreates
// the object from these; objects without one are derived (surface levels, back buffers).
struct Creation {uint32_t method=0;std::vector<uint32_t> args;std::vector<uint8_t> blob;};
struct Object {IUnknown* native;Kind kind;uint32_t references=1;CompressedView compressed;uint32_t parent=0;uint32_t children=0;std::shared_ptr<Creation> creation;};
std::unordered_map<uint32_t,Object> objects;
#ifdef NFS_RESUMABLE
std::map<uint32_t,nfs_d3d9::BufferShadow> bufferShadows;
std::map<uint32_t,nfs_d3d9::TextureShadow> textureShadows;
#endif
// Nearly every call of a frame targets the same object (the device). Element
// pointers of an unordered_map survive rehashing; only erasing an object
// invalidates them, so retireGuestObject/shutdown drop this one-entry cache.
uint32_t cachedObjectAddress=0;Object* cachedObject=nullptr;
Object* findObject(uint32_t address){
    if(address==cachedObjectAddress&&cachedObject)return cachedObject;
    auto found=objects.find(address);if(found==objects.end())return nullptr;
    cachedObjectAddress=address;cachedObject=&found->second;return cachedObject;
}
std::map<IUnknown*,uint32_t> addresses;
void* guestBase;
uint32_t (*allocateGuest)(uint32_t);
void (*freeGuest)(uint32_t);
std::atomic<ANativeWindow*> hostWindow{nullptr};
std::atomic<bool> surfaceRequested{false},surfaceAvailable{false};
void* pointer(uint32_t address){return address?static_cast<uint8_t*>(guestBase)+address:nullptr;}
void write(uint32_t address,uint32_t value){if(!address)throw std::runtime_error("Null D3D9 output");std::memcpy(pointer(address),&value,4);}
uint32_t wrap(IUnknown* native,Kind kind){
    if(!native)return 0;auto previous=addresses.find(native);if(previous!=addresses.end()){++objects.at(previous->second).references;return previous->second;}
    static constexpr uint32_t sizes[]={0,17,119,22,22,17,14,14,5,5,5,6,8,10,22,11};
    uint32_t count=sizes[static_cast<uint32_t>(kind)];
    uint32_t table=allocateGuest(count*4),object=allocateGuest(4);
    for(uint32_t i=0;i<count;++i)write(table+i*4,methodBase+static_cast<uint32_t>(kind)*4096+i*16);
    write(object,table);objects.emplace(object,Object{native,kind});addresses.emplace(native,object);return object;
}
// The guest handle's kind decides the interface: D3D9 interfaces form single
// inheritance chains from IUnknown, so once the kind matches, static_cast is
// exact. dynamic_cast on every SetTexture/SetStreamSource cost ~3% of a race.
template<class T>bool kindMatches(Kind kind){
    if constexpr(std::is_same_v<T,IDirect3DSurface9>)return kind==Kind::Surface;
    else if constexpr(std::is_same_v<T,IDirect3DBaseTexture9>)return kind==Kind::Texture||kind==Kind::Cube||kind==Kind::VolumeTexture;
    else if constexpr(std::is_same_v<T,IDirect3DTexture9>)return kind==Kind::Texture;
    else if constexpr(std::is_same_v<T,IDirect3DCubeTexture9>)return kind==Kind::Cube;
    else if constexpr(std::is_same_v<T,IDirect3DVertexBuffer9>)return kind==Kind::VertexBuffer;
    else if constexpr(std::is_same_v<T,IDirect3DIndexBuffer9>)return kind==Kind::IndexBuffer;
    else if constexpr(std::is_same_v<T,IDirect3DVertexShader9>)return kind==Kind::VertexShader;
    else if constexpr(std::is_same_v<T,IDirect3DPixelShader9>)return kind==Kind::PixelShader;
    else if constexpr(std::is_same_v<T,IDirect3DVertexDeclaration9>)return kind==Kind::Declaration;
    else return false;
}
template<class T>T* nativeObject(uint32_t address){
    if(!address)return nullptr;auto found=objects.find(address);if(found==objects.end())throw std::runtime_error("Invalid D3D9 resource handle");
    if(kindMatches<T>(found->second.kind))return static_cast<T*>(found->second.native);
    auto native=dynamic_cast<T*>(found->second.native);if(!native)throw std::runtime_error("D3D9 resource interface mismatch");return native;
}
uint32_t wrapTexture(IDirect3DBaseTexture9* texture){if(!texture)return 0;switch(texture->GetType()){case D3DRTYPE_TEXTURE:return wrap(texture,Kind::Texture);case D3DRTYPE_CUBETEXTURE:return wrap(texture,Kind::Cube);case D3DRTYPE_VOLUMETEXTURE:return wrap(texture,Kind::VolumeTexture);default:throw std::runtime_error("Unknown D3D9 base texture type");}}
// Restore planning: live objects by kind and pool with estimated bytes, from each native
// object's own description. Render thread only (D3D9 objects are not thread-safe).
std::string inventoryText(){
    struct Row{unsigned count=0;double bytes=0;};
    std::map<std::pair<unsigned,unsigned>,Row> rows;
    auto pixelBytes=[](D3DFORMAT f)->double{switch(f){case D3DFMT_DXT1:return .5;case D3DFMT_DXT2:case D3DFMT_DXT3:case D3DFMT_DXT4:case D3DFMT_DXT5:case D3DFMT_A8:case D3DFMT_L8:case D3DFMT_P8:return 1;
        case D3DFMT_R5G6B5:case D3DFMT_X1R5G5B5:case D3DFMT_A1R5G5B5:case D3DFMT_A4R4G4B4:case D3DFMT_D16:case D3DFMT_A8L8:return 2;default:return 4;}};
    for(auto& [address,object]:objects){
        unsigned pool=9;double bytes=0;
        switch(object.kind){
        case Kind::Texture:{D3DSURFACE_DESC d{};auto t=static_cast<IDirect3DTexture9*>(object.native);if(SUCCEEDED(t->GetLevelDesc(0,&d))){pool=d.Pool;bytes=d.Width*double(d.Height)*pixelBytes(d.Format)*(t->GetLevelCount()>1?4.0/3:1);}break;}
        case Kind::Cube:{D3DSURFACE_DESC d{};auto t=static_cast<IDirect3DCubeTexture9*>(object.native);if(SUCCEEDED(t->GetLevelDesc(0,&d))){pool=d.Pool;bytes=6*d.Width*double(d.Height)*pixelBytes(d.Format)*(t->GetLevelCount()>1?4.0/3:1);}break;}
        case Kind::Surface:{D3DSURFACE_DESC d{};if(object.parent)continue;if(SUCCEEDED(static_cast<IDirect3DSurface9*>(object.native)->GetDesc(&d))){pool=d.Pool;bytes=d.Width*double(d.Height)*pixelBytes(d.Format);}break;}
        case Kind::VertexBuffer:{D3DVERTEXBUFFER_DESC d{};if(SUCCEEDED(static_cast<IDirect3DVertexBuffer9*>(object.native)->GetDesc(&d))){pool=d.Pool+(d.Usage&D3DUSAGE_DYNAMIC?10:0);bytes=d.Size;}break;}
        case Kind::IndexBuffer:{D3DINDEXBUFFER_DESC d{};if(SUCCEEDED(static_cast<IDirect3DIndexBuffer9*>(object.native)->GetDesc(&d))){pool=d.Pool+(d.Usage&D3DUSAGE_DYNAMIC?10:0);bytes=d.Size;}break;}
        default:break;
        }
        auto& row=rows[{unsigned(object.kind),pool}];++row.count;row.bytes+=bytes;
    }
    static const char* kinds[]={"?","D3D9","Device","Texture","Cube","Surface","VB","IB","VS","PS","Decl","StateBlock","Query","SwapChain","VolTex","Volume"};
    static const char* pools[]={"default","managed","system","scratch","?","?","?","?","?","-","default-dynamic","managed-dynamic","system-dynamic"};
    unsigned recorded=0,derived=0;for(auto& [address,object]:objects)(object.creation?recorded:derived)++;
    std::string out;char item[96];
    std::snprintf(item,sizeof item," recorded=%u derived=%u",recorded,derived);out+=item;
    for(auto& [key,row]:rows){std::snprintf(item,sizeof item," %s/%s=%u(%.1fMB)",kinds[key.first<16?key.first:0],pools[key.second<13?key.second:4],row.count,row.bytes/1048576);out+=item;}
    return out;
}
void retireGuestObject(uint32_t address){
#ifdef NFS_RESUMABLE
    bufferShadows.erase(address);textureShadows.erase(address);
#endif
    cachedObjectAddress=0;cachedObject=nullptr;
    auto found=objects.find(address);if(found==objects.end())return;
    uint32_t table;std::memcpy(&table,pointer(address),4);
    freeGuest(table);freeGuest(address);addresses.erase(found->second.native);objects.erase(found);
}
void attachTextureSurface(uint32_t texture,uint32_t surface){
    if(!surface)return;
    auto& child=objects.at(surface);
    if(!child.parent){child.parent=texture;++objects.at(texture).children;}
    else if(child.parent!=texture)throw std::runtime_error("D3D9 subresource changed parent");
}
// Underground 2 copies its initial depth surface from 0x870854 into the first
// render surface descriptor at 0x86e774, then releases both on device Reset.
// The copy does not call AddRef, so account for that owned descriptor here.
void retainInitialDepthDescriptor(uint32_t outputAddress,uint32_t surfaceAddress){
    if(outputAddress!=0x870854||!surfaceAddress)return;
    auto& surface=objects.at(surfaceAddress);surface.native->AddRef();++surface.references;
}
float floatValue(uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;}
uint32_t floatBits(float value){uint32_t bits;std::memcpy(&bits,&value,4);return bits;}
CompressedView compressionView(uint32_t address){auto found=objects.find(address);return found==objects.end()?CompressedView{}:found->second.compressed;}
// DXVK lets the CPU run at most BackBufferCount+1 frames ahead of the GPU.
// Fewer back buffers (the game's request is logged) serialise a CPU-heavy frame
// with its GPU work (Present waited 7-10 ms with the GPU 67% busy). Two
// back buffers allow DXVK's default latency of 3; the game only ever reads
// back buffer 0, so the extra buffers are invisible to it.
// Launcher "Sincronía CPU/GPU": 2 (default) or 1 for the lowest input latency.
std::atomic<uint32_t> configuredBackBuffers{2};
// Launcher "Texturas comprimidas: convertir en CPU": DXT decoded to RGBA8 even when the
// driver reports BC support, for drivers whose BC sampling is broken.
std::atomic<bool> forceCpuTextures{false};
uint32_t hostBackBufferCount(uint32_t requested){
    uint32_t count=std::max(requested,configuredBackBuffers.load());
    static bool logged=false;
    if(!logged){logged=true;NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 game back buffers %u -> host %u",requested,count);}
    return count;
}
D3DPRESENT_PARAMETERS presentation(uint32_t address){
    auto p=static_cast<const uint32_t*>(pointer(address));if(!p)throw std::runtime_error("Null D3D9 presentation parameters");
    D3DPRESENT_PARAMETERS result{};result.BackBufferWidth=p[0];result.BackBufferHeight=p[1];result.BackBufferFormat=static_cast<D3DFORMAT>(p[2]);result.BackBufferCount=hostBackBufferCount(p[3]);
    result.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(p[4]);result.MultiSampleQuality=p[5];result.SwapEffect=static_cast<D3DSWAPEFFECT>(p[6]);result.hDeviceWindow=dxvkAndroidGetWindowHandle();
    result.Windowed=p[8];result.EnableAutoDepthStencil=p[9];result.AutoDepthStencilFormat=static_cast<D3DFORMAT>(p[10]);result.Flags=p[11];result.FullScreen_RefreshRateInHz=p[12];result.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;return result;
}
#include "d3d9_locks.inc"
bool compressedFormat(D3DFORMAT format){return format>=D3DFMT_DXT1&&format<=D3DFMT_DXT5;}
D3DFORMAT nativeTextureFormat(IDirect3DDevice9* device,D3DFORMAT format,DWORD usage,D3DRESOURCETYPE type){
    if(!compressedFormat(format)||(usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL)))return format;
    if(forceCpuTextures.load())return D3DFMT_A8B8G8R8;
    IDirect3D9* d3d=nullptr;D3DDEVICE_CREATION_PARAMETERS parameters{};D3DDISPLAYMODE mode{};
    if(FAILED(device->GetDirect3D(&d3d)))return format;
    device->GetCreationParameters(&parameters);d3d->GetAdapterDisplayMode(parameters.AdapterOrdinal,&mode);
    HRESULT supported=d3d->CheckDeviceFormat(parameters.AdapterOrdinal,parameters.DeviceType,mode.Format,usage,type,format);d3d->Release();
    return SUCCEEDED(supported)?format:D3DFMT_A8B8G8R8;
}
#include "d3d9_texture_packs.inc"
void describeOriginal(uint32_t address,D3DSURFACE_DESC* desc){auto view=compressionView(address);if(view.texture&&desc)desc->Format=static_cast<D3DFORMAT>(view.texture->format);}
}
uint32_t guestD3DObjectCount(){return uint32_t(objects.size());}
void setD3D9Compatibility(bool cpuTextures,unsigned backBuffers){
    forceCpuTextures=cpuTextures;configuredBackBuffers=backBuffers<1?1u:backBuffers>3?3u:backBuffers;
}
void configureD3D9Bridge(void* base,uint32_t (*allocator)(uint32_t),void (*deallocator)(uint32_t)){
    guestBase=base;allocateGuest=allocator;freeGuest=deallocator;
#ifdef __ANDROID__
    // android_main provides a writable private cache before creating DXVK.
    // Other bring-up callers can still run without a writable working directory.
    if(!std::getenv("DXVK_STATE_CACHE_PATH"))setenv("DXVK_STATE_CACHE","0",1);
    setenv("DXVK_LOG_PATH","none",1);
#endif
#ifndef __ANDROID__
    if(std::getenv("NFS_D3D9_TEST_DISPLAY")){auto window=dxvkCreateTestWindow();if(!window)throw std::runtime_error("Xlib test display unavailable");setD3D9HostWindow(window);acknowledgeD3D9Surface(true);}
#endif
}
void shutdownD3D9Bridge(){
#ifdef NFS_RESUMABLE
    bufferShadows.clear();textureShadows.clear();
#endif
    clearTexturePacks();
    for(auto& lock:resourceLocks)freeGuest(lock.second.guest);resourceLocks.clear();
    for(uint32_t kind=15;kind;--kind)for(auto it=objects.begin();it!=objects.end();){
        if(static_cast<uint32_t>(it->second.kind)!=kind){++it;continue;}
        auto object=it->second;for(uint32_t n=0;n<object.references;++n)object.native->Release();
        cachedObjectAddress=0;cachedObject=nullptr;
        uint32_t table;std::memcpy(&table,pointer(it->first),4);freeGuest(table);freeGuest(it->first);addresses.erase(object.native);it=objects.erase(it);
    }
    surfaceRequested=false;surfaceAvailable=false;
#ifndef __ANDROID__
    if(std::getenv("NFS_D3D9_TEST_DISPLAY")){auto window=hostWindow.load();setD3D9HostWindow(nullptr);dxvkDestroyTestWindow(window);}
#endif
}
void setD3D9HostWindow(ANativeWindow* window){hostWindow=window;dxvkAndroidSetWindow(window);}
bool d3d9RequestsSurface(){return surfaceRequested;}
void acknowledgeD3D9Surface(bool available){surfaceAvailable=available;}
uint32_t createGuestD3D9(uint32_t sdk){
    auto native=Direct3DCreate9(sdk);
    NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","DXVK native Direct3DCreate9 sdk=%u",sdk);
    return wrap(native,Kind::D3D9);
}
// Logs each vertex declaration type the first time the game uses it, flagging the
// ones this GPU cannot fetch (see vertex_formats.h): the evidence for stretched geometry.
void noteVertexDeclaration(const D3DVERTEXELEMENT9* elements){
    static bool seen[32]{};
    for(unsigned i=0;elements&&i<64&&elements[i].Stream!=0xff;++i){
        unsigned type=elements[i].Type;
        if(type>=32||seen[type])continue;
        seen[type]=true;
        const char* name="?";for(auto& entry:kD3DVertexFormats)if(entry.type==type)name=entry.name;
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 game vertex type %s (usage %u)",name,unsigned(elements[i].Usage));
    }
}
// Set when DXVK could not rebuild the swap chain in Reset. DXVK's TestCooperativeLevel
// always reports D3D_OK, so the game would keep drawing on the half-reset device and
// crash (null dereference, Mali reports); until a Reset succeeds the device reads as
// lost and needing a reset, which the game handles as on Windows.
bool deviceResetPending=false;
uint32_t dispatchGuestD3D9Call(uint32_t token,const uint32_t* args,uint32_t* argumentCount);
// Device Create methods and the argument holding the new object's address.
int creationOutput(uint32_t method){
    switch(method){case 23:return 7;case 24:return 8;case 25:return 6;case 26:case 27:case 36:return 5;case 28:case 29:return 7;
        case 59:case 86:case 91:case 106:case 118:return 2;default:return -1;}
}
void recordCreation(uint32_t method,const uint32_t* args,uint32_t count){
    int output=creationOutput(method);if(output<0||!args[output])return;
    uint32_t address;std::memcpy(&address,pointer(args[output]),4);
    auto found=objects.find(address);if(found==objects.end()||found->second.creation)return;
    auto creation=std::make_shared<Creation>();creation->method=method;creation->args.assign(args,args+count);
    // Copy through the newly created native object. Scanning guest bytecode for END could
    // stop inside a comment, and the guest can free its input immediately after Create returns.
    if(method==86){creation->blob=nfs_d3d9::captureDeclaration(static_cast<IDirect3DVertexDeclaration9*>(found->second.native)).bytes;}
    if(method==91){creation->blob=nfs_d3d9::captureShader(static_cast<IDirect3DVertexShader9*>(found->second.native),nfs_d3d9::ProgramKind::VertexShader).bytes;}
    if(method==106){creation->blob=nfs_d3d9::captureShader(static_cast<IDirect3DPixelShader9*>(found->second.native),nfs_d3d9::ProgramKind::PixelShader).bytes;}
    if(method==86||method==91||method==106){creation->args[1]=0;creation->args[2]=0;}
    found->second.creation=std::move(creation);
}
// Layout of a texture or cube as the game created it: guest format and size from its Create
// arguments, level count from the native object (0 levels asks for a full chain).
bool textureLayout(const Object& object,nfs_d3d9::Texture& out){
    if((object.kind!=Kind::Texture&&object.kind!=Kind::Cube)||!object.creation)return false;
    const auto& a=object.creation->args;out=nfs_d3d9::Texture{};
    if(object.kind==Kind::Texture){if(a.size()<7)return false;out.kind=3;out.width=a[1];out.height=a[2];out.usage=a[4];out.format=a[5];out.pool=a[6];
        out.levels=static_cast<IDirect3DTexture9*>(object.native)->GetLevelCount();}
    else{if(a.size()<6)return false;out.kind=4;out.width=out.height=a[1];out.usage=a[3];out.format=a[4];out.pool=a[5];
        out.levels=static_cast<IDirect3DCubeTexture9*>(object.native)->GetLevelCount();}
    out.transient=(out.usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))!=0;
    return true;
}
#ifdef NFS_RESUMABLE
// After a successful texture or cube Unlock: keep the guest-format bytes the game wrote.
void recordTextureUnlock(uint32_t address,uint32_t subresource,HRESULT status){
    auto lock=resourceLocks.find(lockKey(address,subresource));if(lock==resourceLocks.end())return;
    if(FAILED(status)){textureShadows.erase(address);return;}
    const auto& data=lock->second;if(data.flags&D3DLOCK_READONLY)return;
    try{
        auto found=objects.find(address);nfs_d3d9::Texture layout;
        if(found==objects.end()||!textureLayout(found->second,layout)||layout.transient)return;
        auto shadow=textureShadows.find(address);
        if(shadow==textureShadows.end()){
            size_t total=0;for(const auto& entry:textureShadows)total+=entry.second.bytes();
            nfs_d3d9::TextureShadow created(layout);
            nfs_d3d9::check(total+created.bytes()<=nfs_d3d9::maxTextureTotal,"D3D9 texture shadows too large");
            shadow=textureShadows.emplace(address,std::move(created)).first;
        }
        shadow->second.update(subresource/32,subresource%32,data.shadowRow,data.shadowByte,data.rows,data.rowBytes,
            static_cast<const uint8_t*>(pointer(data.guest)),data.pitch,data.shadowWhole);
    }catch(const std::exception& error){textureShadows.erase(address);NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSCapture","Texture shadow unavailable: %s",error.what());}
}
// Writes the guest never sees in a lock: drop that texture's shadow. A surface names its parent.
void forgetTextureContents(uint32_t address){
    auto found=objects.find(address);if(found==objects.end())return;
    textureShadows.erase(found->second.parent?found->second.parent:address);
}
void invalidateTextureShadowsForDeviceCall(uint32_t method,const uint32_t* args){
    switch(method){
    case 30:case 34:forgetTextureContents(args[3]);break;   // UpdateSurface, StretchRect destination
    case 31:case 32:forgetTextureContents(args[2]);break;   // UpdateTexture, GetRenderTargetData destination
    case 35:forgetTextureContents(args[1]);break;           // ColorFill
    case 16:for(auto it=textureShadows.begin();it!=textureShadows.end();){ // Reset loses default-pool contents
        auto object=objects.find(it->first);nfs_d3d9::Texture layout;
        if(object==objects.end()||!textureLayout(object->second,layout)||layout.pool==D3DPOOL_DEFAULT)it=textureShadows.erase(it);else ++it;}
        break;
    default:break;
    }
}
#endif
#ifdef NFS_RESUMABLE
void invalidateBufferShadowsForDeviceCall(uint32_t method,const uint32_t* args){
    if(method==85)bufferShadows.erase(args[4]); // ProcessVertices may modify its destination even on failure.
    if(method==16)for(auto it=bufferShadows.begin();it!=bufferShadows.end();){
        auto object=objects.find(it->first);
        if(object==objects.end()||!object->second.creation||object->second.creation->args.size()<5)it=bufferShadows.erase(it);
        else if(object->second.creation->args[0]==args[0]&&object->second.creation->args[4]==D3DPOOL_DEFAULT)it=bufferShadows.erase(it);
        else ++it;
    }
}
#endif
uint32_t dispatchGuestD3D9(uint32_t token,const uint32_t* args,uint32_t* argumentCount){
#ifdef NFS_RESUMABLE
    if((token-methodBase)/4096==2){invalidateBufferShadowsForDeviceCall(((token-methodBase)%4096)/16,args);invalidateTextureShadowsForDeviceCall(((token-methodBase)%4096)/16,args);}
#endif
    uint32_t result=dispatchGuestD3D9Call(token,args,argumentCount);
    if((token-methodBase)/4096==2&&SUCCEEDED(static_cast<HRESULT>(result))){
        uint32_t method=((token-methodBase)%4096)/16;
        if(creationOutput(method)>=0){
            // Metadata recording must not turn a successful game Create into a failure.
            try{recordCreation(method,args,*argumentCount);}
            catch(const std::exception& error){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSCapture","D3D9 creation %u not recorded: %s",method,error.what());}
        }
    }
    return result;
}
uint32_t dispatchGuestD3D9Call(uint32_t token,const uint32_t* args,uint32_t* argumentCount){
    uint32_t kind=(token-methodBase)/4096,method=((token-methodBase)%4096)/16;
    Object* entry=findObject(args[0]);
    if(!entry||static_cast<uint32_t>(entry->kind)!=kind){
        NFS_RUNTIME_LOG(ANDROID_LOG_ERROR,"NFSU2","Invalid D3D9 COM call object=%08x token=%08x kind=%u method=%u actualKind=%u",args[0],token,kind,method,
            entry?static_cast<uint32_t>(entry->kind):0);
        throw std::runtime_error("Invalid guest D3D9 COM object");
    }
    auto object=entry->native;
    // Android may replace ANativeWindow after backgrounding, even at the same
    // pointer address. Override the destination with this surface generation so
    // DXVK recreates its presenter while keeping the game's device/resources.
    if((kind==2&&method==17)||(kind==13&&method==3)){
        *argumentCount=kind==2?5:6;auto window=dxvkAndroidGetWindowHandle();
        if(!window||deviceResetPending)return D3DERR_DEVICELOST;
        auto source=static_cast<const RECT*>(pointer(args[1])),destination=static_cast<const RECT*>(pointer(args[2]));
        auto dirty=static_cast<const RGNDATA*>(pointer(args[4]));
        return kind==2?static_cast<IDirect3DDevice9*>(object)->Present(source,destination,window,dirty):
            static_cast<IDirect3DSwapChain9*>(object)->Present(source,destination,window,dirty,args[5]);
    }
    // Log the first calls of each method; tokens span 0x7e000000..0x7e00ffff.
    static uint8_t methodCalls[4096];auto& calls=methodCalls[((token-methodBase)/16)&4095];
    if(calls<3&&++calls)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 COM kind=%u method=%u",kind,method);
    // Counts belong to the guest COM wrapper. DXVK texture subresources
    // delegate AddRef/Release to their parent texture, so native counts can
    // include ownership through a different guest object. Returning them made
    // the game's release-until-zero loop call an already freed surface wrapper.
    if(method==1){*argumentCount=1;object->AddRef();return ++entry->references;}
    if(method==2){*argumentCount=1;
        // The game's detail rebuild releases the cube parent after every face
        // lookup, although only its first Release owns a parent reference.
        // Keep the callable wrapper until the acquired faces are released.
        if(!entry->references){
            if(entry->children)return 0;
            throw std::runtime_error("D3D9 COM Release without owned reference");
        }
        auto nativeReferences=object->Release();uint32_t references=--entry->references;
        if(kind==5)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Surface Release object=%08x guestRefs=%u nativeRefs=%u",args[0],references,uint32_t(nativeReferences));
        if(!references){
            if(entry->children)return uint32_t(nativeReferences);
            auto parent=entry->parent;retireGuestObject(args[0]);
            if(parent){auto owner=objects.find(parent);if(owner!=objects.end()&&!--owner->second.children&&!owner->second.references)retireGuestObject(parent);}
        }
        return references;}
    if(method==0){
        *argumentCount=3;IUnknown* result=nullptr;HRESULT status=object->QueryInterface(*static_cast<const GUID*>(pointer(args[1])),reinterpret_cast<void**>(&result));
        if(SUCCEEDED(status))write(args[2],wrap(result,entry->kind));else write(args[2],0);return status;
    }
    if(kind==1){
        auto d3d=static_cast<IDirect3D9*>(object);
        switch(method){
        case 3:*argumentCount=2;return D3DERR_NOTAVAILABLE;
        case 4:*argumentCount=1;return d3d->GetAdapterCount();
        case 5:*argumentCount=4;return d3d->GetAdapterIdentifier(args[1],args[2],static_cast<D3DADAPTER_IDENTIFIER9*>(pointer(args[3])));
        case 6:*argumentCount=3;return d3d->GetAdapterModeCount(args[1],static_cast<D3DFORMAT>(args[2]));
        case 7:*argumentCount=5;return d3d->EnumAdapterModes(args[1],static_cast<D3DFORMAT>(args[2]),args[3],static_cast<D3DDISPLAYMODE*>(pointer(args[4])));
        case 8:*argumentCount=3;return d3d->GetAdapterDisplayMode(args[1],static_cast<D3DDISPLAYMODE*>(pointer(args[2])));
        case 9:*argumentCount=6;return d3d->CheckDeviceType(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),static_cast<D3DFORMAT>(args[4]),args[5]);
        case 10:{*argumentCount=7;auto format=static_cast<D3DFORMAT>(args[6]);auto type=static_cast<D3DRESOURCETYPE>(args[5]);
            HRESULT status=d3d->CheckDeviceFormat(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),args[4],type,format);
            if(FAILED(status)&&compressedFormat(format)&&!(args[4]&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))&&(type==D3DRTYPE_TEXTURE||type==D3DRTYPE_CUBETEXTURE||type==D3DRTYPE_SURFACE))
                status=d3d->CheckDeviceFormat(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),args[4],type,D3DFMT_A8B8G8R8);
            return status;}
        case 11:*argumentCount=7;return d3d->CheckDeviceMultiSampleType(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),args[4],static_cast<D3DMULTISAMPLE_TYPE>(args[5]),static_cast<DWORD*>(pointer(args[6])));
        case 12:*argumentCount=6;return d3d->CheckDepthStencilMatch(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),static_cast<D3DFORMAT>(args[4]),static_cast<D3DFORMAT>(args[5]));
        case 13:*argumentCount=5;return d3d->CheckDeviceFormatConversion(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DFORMAT>(args[3]),static_cast<D3DFORMAT>(args[4]));
        case 14:*argumentCount=4;return d3d->GetDeviceCaps(args[1],static_cast<D3DDEVTYPE>(args[2]),static_cast<D3DCAPS9*>(pointer(args[3])));
        case 15:*argumentCount=2;return d3d->GetAdapterMonitor(args[1])?0xf0000300:0;
        case 16:{
            *argumentCount=7;auto p=static_cast<const uint32_t*>(pointer(args[5]));if(!p||!args[6])return D3DERR_INVALIDCALL;
            if(!hostWindow.load())throw std::runtime_error("D3D9 CreateDevice requires Android surface");
            surfaceRequested=true;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(!surfaceAvailable){if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Android surface handover timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            auto window=dxvkAndroidGetWindowHandle();if(!window)return D3DERR_DEVICELOST;
            D3DPRESENT_PARAMETERS parameters{};parameters.BackBufferWidth=p[0];parameters.BackBufferHeight=p[1];parameters.BackBufferFormat=static_cast<D3DFORMAT>(p[2]);parameters.BackBufferCount=hostBackBufferCount(p[3]);
            parameters.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(p[4]);parameters.MultiSampleQuality=p[5];parameters.SwapEffect=static_cast<D3DSWAPEFFECT>(p[6]);parameters.hDeviceWindow=window;
            parameters.Windowed=p[8];parameters.EnableAutoDepthStencil=p[9];parameters.AutoDepthStencilFormat=static_cast<D3DFORMAT>(p[10]);parameters.Flags=p[11];parameters.FullScreen_RefreshRateInHz=p[12];parameters.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
            IDirect3DDevice9* device=nullptr;HRESULT status=d3d->CreateDevice(args[1],static_cast<D3DDEVTYPE>(args[2]),window,args[4],&parameters,&device);
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 backbuffer %ux%u CreateDevice result=%08x",parameters.BackBufferWidth,parameters.BackBufferHeight,uint32_t(status));
            // Without a device the game carries on and faults later in its own
            // code (PowerVR GE8322 reports): after its retry also fails, stop
            // here with the reason instead.
            static unsigned failures=0;failures=FAILED(status)?failures+1:0;
            if(FAILED(status)&&failures<2)return status;
            if(FAILED(status))throw std::runtime_error("The GPU could not create the graphics device (Vulkan, CreateDevice "+
                std::to_string(uint32_t(status))+"). Use \"Test driver\" in the launcher for details.");
            enhanceReflectionGlobals();write(args[6],wrap(device,Kind::Device));return status;
        }
        }
    }
    if(kind==2){auto device=static_cast<IDirect3DDevice9*>(object);switch(method){
        case 9:{*argumentCount=2;D3DDEVICE_CREATION_PARAMETERS p{};HRESULT status=device->GetCreationParameters(&p);if(SUCCEEDED(status)){write(args[1],p.AdapterOrdinal);write(args[1]+4,p.DeviceType);write(args[1]+8,0xf0000301);write(args[1]+12,p.BehaviorFlags);}return status;}
        case 13:{*argumentCount=3;auto p=presentation(args[1]);IDirect3DSwapChain9* result=nullptr;HRESULT status=device->CreateAdditionalSwapChain(&p,&result);write(args[2],SUCCEEDED(status)?wrap(result,Kind::SwapChain):0);return status;}
        case 16:{*argumentCount=2;auto p=presentation(args[1]);HRESULT status=D3DERR_DEVICELOST;
            // DXVK throws (DxvkError) when it cannot rebuild the swap chain; Windows
            // would report a lost device, which the game retries (Mali G615 report).
            // The old Android window can stay busy for a moment: retry once before giving up.
            for(int attempt=0;attempt<2;++attempt){
                try{status=device->Reset(&p);break;}catch(...){
                    NFS_RUNTIME_LOG(ANDROID_LOG_ERROR,"NFSU2","D3D9 Reset raised a DXVK error (attempt %d)",attempt+1);status=D3DERR_DEVICELOST;
                    if(!attempt)std::this_thread::sleep_for(std::chrono::milliseconds(100));}
            }
            deviceResetPending=status==D3DERR_DEVICELOST;
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 Reset backbuffer %ux%u result=%08x",p.BackBufferWidth,p.BackBufferHeight,uint32_t(status));return status;}
        case 23:{*argumentCount=9;if(args[8])throw std::runtime_error("Shared texture handle pending");if(!args[7])return D3DERR_INVALIDCALL;
            auto original=static_cast<D3DFORMAT>(args[5]),format=nativeTextureFormat(device,original,args[4],D3DRTYPE_TEXTURE);IDirect3DTexture9* texture=nullptr;
            HRESULT status=device->CreateTexture(args[1],args[2],args[3],args[4],format,static_cast<D3DPOOL>(args[6]),&texture,nullptr);
            if(args[4]&D3DUSAGE_RENDERTARGET)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Reflection/render target %ux%u format=%u status=%08x",args[1],args[2],unsigned(format),uint32_t(status));
            uint32_t address=SUCCEEDED(status)?wrap(texture,Kind::Texture):0;
            if(address&&format!=original){objects.at(address).compressed.texture=std::make_shared<CompressedTexture>(original,args[1],args[2],texture->GetLevelCount(),1);NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","CPU DXT fallback texture %ux%u format=%u",args[1],args[2],original);}
            write(args[7],address);return status;}
        case 25:{*argumentCount=8;if(args[7])throw std::runtime_error("Shared cube handle pending");if(!args[6])return D3DERR_INVALIDCALL;
            auto original=static_cast<D3DFORMAT>(args[4]),format=nativeTextureFormat(device,original,args[3],D3DRTYPE_CUBETEXTURE);IDirect3DCubeTexture9* texture=nullptr;
            HRESULT status=device->CreateCubeTexture(args[1],args[2],args[3],format,static_cast<D3DPOOL>(args[5]),&texture,nullptr);
            NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","D3D9 CreateCubeTexture edge=%u levels=%u usage=%08x format=%u nativeFormat=%u pool=%u status=%08x output=%08x",args[1],args[2],args[3],unsigned(original),unsigned(format),args[5],uint32_t(status),args[6]);
            uint32_t address=SUCCEEDED(status)?wrap(texture,Kind::Cube):0;
            if(address&&format!=original)objects.at(address).compressed.texture=std::make_shared<CompressedTexture>(original,args[1],args[1],texture->GetLevelCount(),6);
            write(args[6],address);return status;}
#include "d3d9_device_methods.inc"
    }}
    if(kind==3){auto texture=static_cast<IDirect3DTexture9*>(object);
        if(method==17){*argumentCount=3;auto desc=static_cast<D3DSURFACE_DESC*>(pointer(args[2]));HRESULT status=texture->GetLevelDesc(args[1],desc);if(SUCCEEDED(status))describeOriginal(args[0],desc);return status;}
        if(method==18){*argumentCount=3;IDirect3DSurface9* surface=nullptr;HRESULT status=texture->GetSurfaceLevel(args[1],&surface);uint32_t address=SUCCEEDED(status)?wrap(surface,Kind::Surface):0;if(address){attachTextureSurface(args[0],address);auto view=compressionView(args[0]);view.level=args[1];objects.at(address).compressed=view;}write(args[2],address);return status;}
        if(method==19){*argumentCount=5;return lockTexture(args[0],args[1],args[2],static_cast<const RECT*>(pointer(args[3])),args[4]);}
        if(method==20){*argumentCount=2;copyLockBack(args[0],args[1]);texturePackUpload(args[0],args[1]);HRESULT status=texture->UnlockRect(args[1]);
#ifdef NFS_RESUMABLE
            recordTextureUnlock(args[0],args[1],status);
#endif
            releaseLock(args[0],args[1]);return status;}}
    if(kind==4){auto texture=static_cast<IDirect3DCubeTexture9*>(object);
        if(method==17){*argumentCount=3;auto desc=static_cast<D3DSURFACE_DESC*>(pointer(args[2]));HRESULT status=texture->GetLevelDesc(args[1],desc);if(SUCCEEDED(status))describeOriginal(args[0],desc);return status;}
        if(method==18){*argumentCount=4;IDirect3DSurface9* surface=nullptr;HRESULT status=texture->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(args[1]),args[2],&surface);uint32_t address=SUCCEEDED(status)?wrap(surface,Kind::Surface):0;if(address){attachTextureSurface(args[0],address);auto view=compressionView(args[0]);view.face=args[1];view.level=args[2];objects.at(address).compressed=view;}write(args[3],address);return status;}
        if(method==19){*argumentCount=6;return lockCube(args[0],args[1],args[2],args[3],static_cast<const RECT*>(pointer(args[4])),args[5]);}
        if(method==20){*argumentCount=3;uint32_t subresource=args[1]*32+args[2];copyLockBack(args[0],subresource);HRESULT status=texture->UnlockRect(static_cast<D3DCUBEMAP_FACES>(args[1]),args[2]);
#ifdef NFS_RESUMABLE
            recordTextureUnlock(args[0],subresource,status);
#endif
            releaseLock(args[0],subresource);return status;}}
    if(kind==5){auto surface=static_cast<IDirect3DSurface9*>(object);
        if(method==12){*argumentCount=2;auto desc=static_cast<D3DSURFACE_DESC*>(pointer(args[1]));HRESULT status=surface->GetDesc(desc);if(SUCCEEDED(status))describeOriginal(args[0],desc);return status;}
        if(method==13){*argumentCount=4;return lockSurface(args[0],args[1],static_cast<const RECT*>(pointer(args[2])),args[3]);}
        if(method==14){*argumentCount=1;
#ifdef NFS_RESUMABLE
            {auto lock=resourceLocks.find(lockKey(args[0],0));if(lock!=resourceLocks.end()&&!(lock->second.flags&D3DLOCK_READONLY))forgetTextureContents(args[0]);}
#endif
            copyLockBack(args[0],0);disablePackedTexture(args[0]);HRESULT status=surface->UnlockRect();releaseLock(args[0],0);return status;}}
    if(kind==6||kind==7){
        if(method==11){*argumentCount=5;return kind==6?lockBuffer<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>(args[0],args[1],args[2],args[3],args[4]):lockBuffer<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>(args[0],args[1],args[2],args[3],args[4]);}
        if(method==12){*argumentCount=1;copyLockBack(args[0],0);HRESULT status=kind==6?static_cast<IDirect3DVertexBuffer9*>(object)->Unlock():static_cast<IDirect3DIndexBuffer9*>(object)->Unlock();recordBufferUnlock(args[0],status);releaseLock(args[0],0);return status;}}
#include "d3d9_resource_methods.inc"
    throw std::runtime_error("D3D9 guest method pending: kind="+std::to_string(kind)+" slot="+std::to_string(method));
}
std::string d3d9Inventory(){return inventoryText();}
// Live objects as (guest address, kind), sorted: a load within the same session checks that the
// set is unchanged, so every guest handle still names the same native object and contents.
std::vector<std::pair<uint32_t,uint32_t>> d3d9ObjectList(){
    std::vector<std::pair<uint32_t,uint32_t>> out;out.reserve(objects.size());
    for(auto& [address,object]:objects)out.emplace_back(address,static_cast<uint32_t>(object.kind));
    std::sort(out.begin(),out.end());return out;
}

// Complete immutable-program records, sorted by stable guest proxy address. Missing recipes
// refuse a capture rather than silently claiming the resource can later be reconstructed.
std::vector<nfs_d3d9::ProgramRecord> d3d9ProgramList(){
    std::vector<nfs_d3d9::ProgramRecord> out;
    for(const auto& [address,object]:objects){
        if(object.kind!=Kind::VertexShader&&object.kind!=Kind::PixelShader&&object.kind!=Kind::Declaration)continue;
        if(!object.creation||object.creation->blob.empty()||object.creation->args.empty())throw std::runtime_error("D3D9 program recipe missing");
        uint32_t table;std::memcpy(&table,pointer(address),4);
        nfs_d3d9::ProgramRecord record{address,table,object.creation->args[0],object.references,
            {static_cast<nfs_d3d9::ProgramKind>(object.kind),object.creation->blob}};
        nfs_d3d9::validate(record.definition);out.push_back(std::move(record));
    }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.address<b.address;});return out;
}

namespace {
nfs_d3d9::BufferRecord describeBuffer(uint32_t address,const Object& object){
    if(!object.creation||object.creation->args.empty())throw std::runtime_error("D3D9 buffer recipe missing");
    uint32_t table;std::memcpy(&table,pointer(address),4);
    nfs_d3d9::BufferRecord record{address,table,object.creation->args[0],object.references,{}};
    if(object.kind==Kind::VertexBuffer){D3DVERTEXBUFFER_DESC desc{};nfs_d3d9::check(SUCCEEDED(static_cast<IDirect3DVertexBuffer9*>(object.native)->GetDesc(&desc)),"Cannot describe VB");
        nfs_d3d9::check(desc.Size&&desc.Size<=nfs_d3d9::maxBufferBytes,"Invalid VB size");record.definition={6,desc.Usage,uint32_t(desc.Format),uint32_t(desc.Pool),desc.FVF,std::vector<uint8_t>(desc.Size)};
    }else{D3DINDEXBUFFER_DESC desc{};nfs_d3d9::check(SUCCEEDED(static_cast<IDirect3DIndexBuffer9*>(object.native)->GetDesc(&desc)),"Cannot describe IB");
        nfs_d3d9::check(desc.Size&&desc.Size<=nfs_d3d9::maxBufferBytes,"Invalid IB size");record.definition={7,desc.Usage,uint32_t(desc.Format),uint32_t(desc.Pool),0,std::vector<uint8_t>(desc.Size)};}
    return record;
}
}
std::vector<nfs_d3d9::BufferRecord> d3d9BufferList(){
    std::vector<nfs_d3d9::BufferRecord> out;size_t total=0;
    for(const auto& [address,object]:objects){
        if(object.kind!=Kind::VertexBuffer&&object.kind!=Kind::IndexBuffer)continue;
        if(resourceLocks.count(lockKey(address,0)))throw std::runtime_error("D3D9 buffer is locked");
        auto record=describeBuffer(address,object);total+=record.definition.bytes.size();nfs_d3d9::check(total<=(256u<<20),"D3D9 buffers too large");
        const nfs_d3d9::BufferShadow* shadow=nullptr;
#ifdef NFS_RESUMABLE
        auto found=bufferShadows.find(address);if(found!=bufferShadows.end())shadow=&found->second;
#endif
        record.definition=object.kind==Kind::VertexBuffer
            ?nfs_d3d9::captureBuffer<IDirect3DVertexBuffer9,D3DVERTEXBUFFER_DESC>(static_cast<IDirect3DVertexBuffer9*>(object.native),6,shadow)
            :nfs_d3d9::captureBuffer<IDirect3DIndexBuffer9,D3DINDEXBUFFER_DESC>(static_cast<IDirect3DIndexBuffer9*>(object.native),7,shadow);
        out.push_back(std::move(record));
    }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.address<b.address;});return out;
}
void validateD3D9Buffers(const std::vector<nfs_d3d9::BufferRecord>& records){
    size_t count=0;for(const auto& [address,object]:objects)if(object.kind==Kind::VertexBuffer||object.kind==Kind::IndexBuffer)++count;
    nfs_d3d9::check(count==records.size(),"D3D9 buffer set changed");
    uint32_t previous=0;
    for(const auto& record:records){
        nfs_d3d9::check(record.address>previous,"D3D9 buffers must be sorted and unique");previous=record.address;
        nfs_d3d9::validate(record.definition);auto found=objects.find(record.address);
        nfs_d3d9::check(found!=objects.end()&&uint32_t(found->second.kind)==record.definition.kind,"D3D9 buffer identity changed");
        nfs_d3d9::check(!resourceLocks.count(lockKey(record.address,0)),"D3D9 buffer is locked");
        nfs_d3d9::check(nfs_d3d9::sameMetadata(record,describeBuffer(record.address,found->second)),"D3D9 buffer metadata/references changed");
    }
}
void restoreD3D9Buffers(const std::vector<nfs_d3d9::BufferRecord>& records){
    validateD3D9Buffers(records);
    for(const auto& record:records){auto native=objects.at(record.address).native;
        if(record.definition.kind==6)nfs_d3d9::uploadBuffer(static_cast<IDirect3DVertexBuffer9*>(native),record.definition);
        else nfs_d3d9::uploadBuffer(static_cast<IDirect3DIndexBuffer9*>(native),record.definition);
#ifdef NFS_RESUMABLE
        bufferShadows.erase(record.address);auto& shadow=bufferShadows.try_emplace(record.address,record.definition.bytes.size()).first->second;
        shadow.update(0,record.definition.bytes.data(),record.definition.bytes.size(),true);
#endif
    }
}

// Texture contents for captures, in the game's own format. Transient textures (render targets,
// depth) keep none. A level without a complete shadow is read back only when the native object
// holds the game's format as uploaded: not a CPU DXT fallback, not replaced by an HD pack, not a
// default-pool texture (which cannot be locked); otherwise the capture is refused.
namespace {
nfs_d3d9::TextureRecord describeTexture(uint32_t address,const Object& object){
    nfs_d3d9::TextureRecord record;record.address=address;std::memcpy(&record.table,pointer(address),4);
    nfs_d3d9::check(object.creation&&!object.creation->args.empty()&&textureLayout(object,record.definition),"D3D9 texture recipe missing");
    record.device=object.creation->args[0];record.references=object.references;
    return record;
}
bool anyLock(uint32_t address){auto it=resourceLocks.lower_bound(lockKey(address,0));return it!=resourceLocks.end()&&(it->first>>32)==address;}
std::vector<uint8_t> readTextureLevel(const Object& object,const nfs_d3d9::Texture& texture,uint32_t face,uint32_t level){
    nfs_d3d9::check(!object.compressed.texture,"D3D9 texture uses the CPU DXT fallback and lacks a shadow");
    nfs_d3d9::check(texture.pool!=D3DPOOL_DEFAULT||(texture.usage&D3DUSAGE_DYNAMIC),"Default-pool D3D9 texture lacks a shadow");
    auto base=static_cast<IDirect3DBaseTexture9*>(static_cast<IDirect3DTexture9*>(object.native));
    nfs_d3d9::check(!packedOriginals.count(base),"HD-pack D3D9 texture lacks a shadow");
    D3DSURFACE_DESC desc{};D3DLOCKED_RECT locked{};HRESULT status;
    if(texture.kind==3){auto native=static_cast<IDirect3DTexture9*>(object.native);status=native->GetLevelDesc(level,&desc);
        nfs_d3d9::check(SUCCEEDED(status)&&uint32_t(desc.Format)==texture.format,"D3D9 texture native format differs");
        status=native->LockRect(level,&locked,nullptr,D3DLOCK_READONLY);}
    else{auto native=static_cast<IDirect3DCubeTexture9*>(object.native);status=native->GetLevelDesc(level,&desc);
        nfs_d3d9::check(SUCCEEDED(status)&&uint32_t(desc.Format)==texture.format,"D3D9 cube native format differs");
        status=native->LockRect(static_cast<D3DCUBEMAP_FACES>(face),level,&locked,nullptr,D3DLOCK_READONLY);}
    nfs_d3d9::check(SUCCEEDED(status)&&locked.pBits,"Cannot read D3D9 texture");
    uint32_t height=texture.kind==4?texture.width:texture.height;
    uint32_t row=nfs_d3d9::textureRowBytes(texture.format,nfs_d3d9::mipExtent(texture.width,level)),rows=nfs_d3d9::textureRows(texture.format,nfs_d3d9::mipExtent(height,level));
    std::vector<uint8_t> out;bool okay=locked.Pitch>0&&uint32_t(locked.Pitch)>=row;
    if(okay){out.resize(size_t(row)*rows);for(uint32_t r=0;r<rows;++r)std::memcpy(out.data()+size_t(r)*row,static_cast<const uint8_t*>(locked.pBits)+size_t(r)*locked.Pitch,row);}
    HRESULT unlocked=texture.kind==3?static_cast<IDirect3DTexture9*>(object.native)->UnlockRect(level):static_cast<IDirect3DCubeTexture9*>(object.native)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(face),level);
    nfs_d3d9::check(okay&&SUCCEEDED(unlocked),"Invalid D3D9 texture readback");
    return out;
}
}
std::vector<nfs_d3d9::TextureRecord> d3d9TextureList(){
    std::vector<nfs_d3d9::TextureRecord> out;size_t total=0;
    for(const auto& [address,object]:objects){
        if(object.kind!=Kind::Texture&&object.kind!=Kind::Cube)continue;
        nfs_d3d9::check(!anyLock(address),"D3D9 texture is locked");
        auto record=describeTexture(address,object);auto& texture=record.definition;
        if(!texture.transient){
            const nfs_d3d9::TextureShadow* shadow=nullptr;
#ifdef NFS_RESUMABLE
            auto found=textureShadows.find(address);if(found!=textureShadows.end())shadow=&found->second;
#endif
            for(uint32_t face=0;face<nfs_d3d9::textureFaces(texture);++face)for(uint32_t level=0;level<texture.levels;++level){
                nfs_d3d9::TextureLevel item{face,level,{}};
                item.bytes=shadow&&shadow->complete(face,level)?shadow->level(face,level):readTextureLevel(object,texture,face,level);
                total+=item.bytes.size();nfs_d3d9::check(total<=nfs_d3d9::maxTextureTotal,"D3D9 textures too large");
                texture.contents.push_back(std::move(item));
            }
        }
        nfs_d3d9::validate(texture);out.push_back(std::move(record));
    }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.address<b.address;});return out;
}
void validateD3D9Textures(const std::vector<nfs_d3d9::TextureRecord>& records){
    size_t count=0;for(const auto& [address,object]:objects)if(object.kind==Kind::Texture||object.kind==Kind::Cube)++count;
    nfs_d3d9::check(count==records.size(),"D3D9 texture set changed");
    uint32_t previous=0;
    for(const auto& record:records){
        nfs_d3d9::check(record.address>previous,"D3D9 textures must be sorted and unique");previous=record.address;
        nfs_d3d9::validate(record.definition);auto found=objects.find(record.address);
        nfs_d3d9::check(found!=objects.end()&&uint32_t(found->second.kind)==record.definition.kind,"D3D9 texture identity changed");
        nfs_d3d9::check(!anyLock(record.address),"D3D9 texture is locked");
        nfs_d3d9::check(nfs_d3d9::sameMetadata(record,describeTexture(record.address,found->second)),"D3D9 texture metadata/references changed");
    }
}
// Same session: replay each level through the guest lock path (as the game's own upload), so the
// DXT fallback and texture packs see it, then make the shadow match the capture.
void restoreD3D9Textures(const std::vector<nfs_d3d9::TextureRecord>& records){
    validateD3D9Textures(records);
    uint32_t scratch=allocateGuest(8);
    try{
        for(const auto& record:records){
            const auto& texture=record.definition;if(texture.transient)continue;
#ifdef NFS_RESUMABLE
            textureShadows.erase(record.address);
#endif
            for(const auto& item:texture.contents){
                uint32_t subresource=item.face*32+item.level;
                HRESULT status=texture.kind==3?lockTexture(record.address,item.level,scratch,nullptr,0):lockCube(record.address,item.face,item.level,scratch,nullptr,0);
                nfs_d3d9::check(SUCCEEDED(status),"Cannot lock restored D3D9 texture");
                uint32_t pitch,guest;std::memcpy(&pitch,pointer(scratch),4);std::memcpy(&guest,pointer(scratch+4),4);
                uint32_t height=texture.kind==4?texture.width:texture.height;
                uint32_t row=nfs_d3d9::textureRowBytes(texture.format,nfs_d3d9::mipExtent(texture.width,item.level)),rows=nfs_d3d9::textureRows(texture.format,nfs_d3d9::mipExtent(height,item.level));
                for(uint32_t r=0;r<rows;++r)std::memcpy(static_cast<uint8_t*>(pointer(guest))+size_t(r)*pitch,item.bytes.data()+size_t(r)*row,row);
                copyLockBack(record.address,subresource);
                if(texture.kind==3)texturePackUpload(record.address,item.level);
                auto native=objects.at(record.address).native;
                status=texture.kind==3?static_cast<IDirect3DTexture9*>(native)->UnlockRect(item.level)
                    :static_cast<IDirect3DCubeTexture9*>(native)->UnlockRect(static_cast<D3DCUBEMAP_FACES>(item.face),item.level);
#ifdef NFS_RESUMABLE
                recordTextureUnlock(record.address,subresource,status);
#endif
                releaseLock(record.address,subresource);
                nfs_d3d9::check(SUCCEEDED(status),"Cannot unlock restored D3D9 texture");
            }
        }
    }catch(...){freeGuest(scratch);throw;}
    freeGuest(scratch);
}
