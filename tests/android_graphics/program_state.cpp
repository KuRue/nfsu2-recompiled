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
template<class Interface> struct Shader final: Interface {
    std::vector<uint8_t> bytes; ULONG refs=1; bool fail=false,oversize=false,shortCopy=false;
    explicit Shader(const std::vector<uint8_t>& data):bytes(data){++live;}
    ~Shader(){--live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out) override {*out=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {auto count=--refs;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override{*out=nullptr;return E_FAIL;}
    HRESULT STDMETHODCALLTYPE GetFunction(void* out,UINT* size) override {
        if(fail)return E_FAIL;
        if(!out){*size=oversize?UINT(nfs_d3d9::maxProgramBytes+4):UINT(bytes.size());return D3D_OK;}
        if(*size<bytes.size())return D3DERR_INVALIDCALL;
        std::memcpy(out,bytes.data(),bytes.size());*size=UINT(bytes.size()-(shortCopy?4:0));return D3D_OK;
    }
};
struct Declaration final: IDirect3DVertexDeclaration9 {
    std::vector<uint8_t> bytes; ULONG refs=1;
    explicit Declaration(const std::vector<uint8_t>& data):bytes(data){++live;}
    ~Declaration(){--live;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out) override{*out=nullptr;return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{auto count=--refs;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override{*out=nullptr;return E_FAIL;}
    HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9* out,UINT* count) override{
        if(out){if(*count<bytes.size()/8)return D3DERR_INVALIDCALL;std::memcpy(out,bytes.data(),bytes.size());}
        *count=UINT(bytes.size()/8);return D3D_OK;
    }
};
struct Device {
    nfs_d3d9::Program expected; unsigned calls=0; bool fail=false,nullOutput=false;
    template<class Interface,class Object,class Input> HRESULT create(Input* input,Interface** output){
        ++calls;require(!std::memcmp(input,expected.bytes.data(),expected.bytes.size()),"Recreation changed program bytes");
        *output=nullOutput?nullptr:new Object(expected.bytes);return fail?E_FAIL:D3D_OK;
    }
    HRESULT CreateVertexShader(const DWORD* input,IDirect3DVertexShader9** out){return create<IDirect3DVertexShader9,Shader<IDirect3DVertexShader9>>(input,out);}
    HRESULT CreatePixelShader(const DWORD* input,IDirect3DPixelShader9** out){return create<IDirect3DPixelShader9,Shader<IDirect3DPixelShader9>>(input,out);}
    HRESULT CreateVertexDeclaration(const D3DVERTEXELEMENT9* input,IDirect3DVertexDeclaration9** out){return create<IDirect3DVertexDeclaration9,Declaration>(input,out);}
};
template<class F> void rejects(F action){bool caught=false;try{action();}catch(const std::runtime_error&){caught=true;}require(caught,"Bad program or failed driver accepted");}
GuestMemory memory;
GuestHeaps heaps(memory);
uint32_t alloc(uint32_t size){return heaps.alloc(GuestHeaps::process,8,size);}
void freeAllocation(uint32_t address){require(heaps.free(GuestHeaps::process,address),"Double-free guest program wrapper");}
std::vector<uint8_t> shaderBytes(uint32_t version){
    // END inside a two-DWORD comment must not truncate the recorded shader.
    const uint32_t tokens[]={version,0x0002fffeu,0x0000ffffu,0x12345678u,0x0000ffffu};
    std::vector<uint8_t> bytes(sizeof tokens);std::memcpy(bytes.data(),tokens,sizeof tokens);return bytes;
}
}
int main(int argc,char** argv){try{
    using namespace nfs_d3d9;
    configureD3D9Bridge(memory.base(),alloc,freeAllocation);
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},D3DDECL_END()};
    std::vector<uint8_t> declaration(sizeof elements);std::memcpy(declaration.data(),elements,sizeof elements);
    std::vector<Program> programs={{ProgramKind::VertexShader,shaderBytes(0xfffe0200u)},
        {ProgramKind::PixelShader,shaderBytes(0xffff0200u)},{ProgramKind::Declaration,declaration}};
    bool writeArchive=argc==3&&std::string(argv[1])=="--write";
    bool readArchive=argc==3&&std::string(argv[1])=="--read";
    require(argc==1||writeArchive||readArchive,"Invalid archive test arguments");
    if(readArchive){
        require(objects.empty()&&live==0,"Fresh reader unexpectedly owns graphics resources");
        for(size_t i=0;i<programs.size();++i){
            std::ifstream input(std::filesystem::path(argv[2])/(std::to_string(i)+".nfs"),std::ios::binary);
            require(bool(input),"Cannot read test capture");
            nfs_capture::Bytes bytes((std::istreambuf_iterator<char>(input)),{});
            auto archive=nfs_capture::decode(bytes);require(archive.programs.size()==1,"Fresh archive count differs");
            const auto& record=archive.programs[0];
            require(record.definition.kind==programs[i].kind&&record.definition.bytes==programs[i].bytes&&record.references==1,
                "Fresh-process definition/references differ");
            Device device{programs[i]};{auto staged=recreateProgram(&device,record.definition);require(staged.get(),"Fresh-process recreation failed");}
            require(live==0&&objects.empty(),"Fresh-process recreation leaked or reused a proxy");
        }
        std::puts("Fresh-process captured program recreation passed for VS, PS and declaration");return 0;
    }
    size_t archiveIndex=0;
    auto deviceNative=new Shader<IDirect3DVertexShader9>(programs[0].bytes);
    auto deviceAddress=wrap(deviceNative,Kind::Device);
    for(auto& program:programs){
        IUnknown* native=nullptr;Program captured;
        if(program.kind==ProgramKind::VertexShader){auto shader=new Shader<IDirect3DVertexShader9>(program.bytes);native=shader;captured=captureShader(shader,program.kind);}
        else if(program.kind==ProgramKind::PixelShader){auto shader=new Shader<IDirect3DPixelShader9>(program.bytes);native=shader;captured=captureShader(shader,program.kind);}
        else{auto decl=new Declaration(program.bytes);native=decl;captured=captureDeclaration(decl);}
        require(captured.bytes==program.bytes,"Native program capture differs");
        auto address=wrap(native,static_cast<Kind>(program.kind));auto output=alloc(4);write(output,address);
        uint32_t args[]={deviceAddress,0xdeadbeefu,output};uint32_t method=program.kind==ProgramKind::VertexShader?91:program.kind==ProgramKind::PixelShader?106:86;
        recordCreation(method,args,3);auto creation=objects.at(address).creation;
        require(creation&&creation->blob==program.bytes&&creation->args[1]==0&&creation->args[2]==0,"Creation record retained pointers or lost bytes");
        auto records=d3d9ProgramList();require(records.size()==1,"Program list omitted or duplicated a definition");
        uint32_t table=0;std::memcpy(&table,pointer(address),4);
        require(records[0].address==address&&records[0].table==table&&records[0].device==deviceAddress
            &&records[0].references==1&&records[0].definition.bytes==program.bytes,"Program proxy metadata differs");
        auto savedRecipe=objects.at(address).creation;objects.at(address).creation.reset();
        rejects([&]{d3d9ProgramList();});objects.at(address).creation=savedRecipe;
        nfs_capture::Capture archive;archive.build=std::string(64,'a');archive.game=std::string(64,'b');
        nfs_capture::ThreadImage main;main.id=1;main.capturer=true;main.resumable=true;main.wait=nfs_capture::WaitKind::Present;
        main.frames.push_back({0x401000,0x401020,0x7e002110,0,0});archive.threads.push_back(main);
        archive.graphics=d3d9ObjectList();archive.programs=records;
        auto encoded=nfs_capture::encode(archive);
        if(writeArchive)nfs_capture::writeAtomic(std::filesystem::path(argv[2])/(std::to_string(archiveIndex++)+".nfs"),encoded);
        auto decoded=nfs_capture::decode(encoded);
        require(decoded.programs==records,"Archived program/proxy metadata differs");
        auto restored=decoded.programs.front().definition;
        Device device{captured};int before=live;
        {auto staged=recreateProgram(&device,restored);require(staged.get(),"Archived definition did not recreate");}
        {auto staged=recreateProgram(&device,captured);require(staged.get()&&live==before+1,"Program not staged");
         auto moved=std::move(staged);require(!staged.get()&&moved.get(),"Staging move duplicated ownership");}
        require(live==before,"Staged program leaked");
        {auto staged=recreateProgram(&device,captured);auto transferred=staged.release();require(!staged.get(),"Transfer retained ownership");transferred->Release();}
        device.fail=true;rejects([&]{recreateProgram(&device,captured);});require(live==before,"Failed creation leaked output");
        device.fail=false;device.nullOutput=true;rejects([&]{recreateProgram(&device,captured);});
        for(size_t count=0;count<captured.bytes.size();++count){auto bad=captured;bad.bytes.resize(count);
            unsigned calls=device.calls;rejects([&]{recreateProgram(&device,bad);});require(calls==device.calls,"Malformed program reached driver");}
        native->Release();retireGuestObject(address);freeAllocation(output);
    }
    Shader<IDirect3DVertexShader9> broken(programs[0].bytes);broken.fail=true;rejects([&]{captureShader(&broken,ProgramKind::VertexShader);});
    broken.fail=false;broken.oversize=true;rejects([&]{captureShader(&broken,ProgramKind::VertexShader);});
    broken.oversize=false;broken.shortCopy=true;rejects([&]{captureShader(&broken,ProgramKind::VertexShader);});
    auto bad=programs[0];bad.kind=ProgramKind::PixelShader;rejects([&]{validate(bad);});
    bad=programs[2];bad.bytes[8+4]=D3DDECLTYPE_FLOAT3;rejects([&]{validate(bad);});
    for (uint32_t version : {0xfffe0101u,0xffff0104u,0xfffe0300u,0xffff0300u}) {
        auto kind=(version>>16)==0xfffe?ProgramKind::VertexShader:ProgramKind::PixelShader;
        auto bytes=shaderBytes(version);validate({kind,bytes});
    }
    { // SM1 DEF constants can contain the END bit pattern as data.
        const uint32_t tokens[]={0xfffe0101u,81,0xa00f0000u,0xffffu,0,0,0,0xffffu};
        std::vector<uint8_t> bytes(sizeof tokens);std::memcpy(bytes.data(),tokens,sizeof tokens);
        validate({ProgramKind::VertexShader,bytes});
        bytes.resize(bytes.size()-4);rejects([&]{validate({ProgramKind::VertexShader,bytes});});
    }
    { // ps_1_4 TEX has two operands rather than one.
        const uint32_t tokens[]={0xffff0104u,66,0x800f0000u,0xb00e0000u,0xffffu};
        std::vector<uint8_t> bytes(sizeof tokens);std::memcpy(bytes.data(),tokens,sizeof tokens);validate({ProgramKind::PixelShader,bytes});
    }
    bad=programs[0];bad.bytes.resize(maxProgramBytes+4);rejects([&]{validate(bad);});
    bad=programs[2];bad.bytes.resize(66*8);rejects([&]{validate(bad);});
    deviceNative->Release();retireGuestObject(deviceAddress);
    require(objects.empty()&&addresses.empty()&&live==1,"Program lifetime accounting differs");
    std::puts("D3D9 program capture/recreation passed: shader comments, declarations, pointer removal, bounds, failures and staged ownership");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
