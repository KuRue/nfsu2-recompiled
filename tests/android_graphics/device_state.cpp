#include "../../android/app/src/main/cpp/d3d9_device_state.h"
#include "../../android/app/src/main/cpp/capture_format.h"
#include <cstdio>
#include <map>
// Capture and replay of D3D9 device state through a mock device that keeps what is set and
// behaves like D3D9 where order matters: render target 0 resets viewport and scissor, an FVF
// replaces the declaration, an unset light cannot be read, getters AddRef.
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct Fake {uint32_t id;int refs=1;void AddRef(){++refs;}void Release(){--refs;}};
struct Mock {
    std::map<uint32_t,DWORD> rs,tss,samp;std::map<uint32_t,Fake*> textures;std::map<uint32_t,D3DMATRIX> transforms;
    D3DVIEWPORT9 viewport{0,0,800,600,0,1};D3DMATERIAL9 material{};std::map<uint32_t,D3DLIGHT9> lights;std::map<uint32_t,BOOL> lightOn;
    float planes[6][4]{};RECT scissor{0,0,800,600};struct S{Fake* b=nullptr;UINT offset=0,stride=0,freq=1;} streams[16];
    Fake *indices=nullptr,*decl=nullptr,*vs=nullptr,*ps=nullptr,*targets[4]{},*depth=nullptr;DWORD fvf=0;
    float vsF[256*4]{},psF[224*4]{};int vsI[64]{},psI[64]{};BOOL vsB[16]{},psB[16]{};
    static Fake* give(Fake* f,Fake** out){if(f)f->AddRef();*out=f;return f;}
    HRESULT GetRenderState(D3DRENDERSTATETYPE t,DWORD* v){*v=rs[t];return D3D_OK;}
    HRESULT SetRenderState(D3DRENDERSTATETYPE t,DWORD v){rs[t]=v;return D3D_OK;}
    HRESULT GetTextureStageState(DWORD s,D3DTEXTURESTAGESTATETYPE t,DWORD* v){*v=tss[s*64+t];return D3D_OK;}
    HRESULT SetTextureStageState(DWORD s,D3DTEXTURESTAGESTATETYPE t,DWORD v){tss[s*64+t]=v;return D3D_OK;}
    HRESULT GetSamplerState(DWORD s,D3DSAMPLERSTATETYPE t,DWORD* v){*v=samp[s*16+t];return D3D_OK;}
    HRESULT SetSamplerState(DWORD s,D3DSAMPLERSTATETYPE t,DWORD v){samp[s*16+t]=v;return D3D_OK;}
    HRESULT GetTexture(DWORD s,Fake** out){give(textures[s],out);return D3D_OK;}
    HRESULT SetTexture(DWORD s,Fake* t){textures[s]=t;return D3D_OK;}
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE t,D3DMATRIX* m){auto f=transforms.find(t);if(f==transforms.end()){*m=D3DMATRIX{};m->_11=m->_22=m->_33=m->_44=1;}else *m=f->second;return D3D_OK;}
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE t,const D3DMATRIX* m){transforms[t]=*m;return D3D_OK;}
    HRESULT GetViewport(D3DVIEWPORT9* v){*v=viewport;return D3D_OK;}
    HRESULT SetViewport(const D3DVIEWPORT9* v){viewport=*v;return D3D_OK;}
    HRESULT GetMaterial(D3DMATERIAL9* m){*m=material;return D3D_OK;}
    HRESULT SetMaterial(const D3DMATERIAL9* m){material=*m;return D3D_OK;}
    HRESULT GetLight(DWORD i,D3DLIGHT9* l){auto f=lights.find(i);if(f==lights.end())return D3DERR_INVALIDCALL;*l=f->second;return D3D_OK;}
    HRESULT SetLight(DWORD i,const D3DLIGHT9* l){lights[i]=*l;return D3D_OK;}
    HRESULT GetLightEnable(DWORD i,BOOL* on){if(!lights.count(i))return D3DERR_INVALIDCALL;*on=lightOn[i];return D3D_OK;}
    HRESULT LightEnable(DWORD i,BOOL on){if(!lights.count(i))lights[i]=D3DLIGHT9{};lightOn[i]=on;return D3D_OK;}
    HRESULT GetClipPlane(DWORD i,float* p){std::memcpy(p,planes[i],16);return D3D_OK;}
    HRESULT SetClipPlane(DWORD i,const float* p){std::memcpy(planes[i],p,16);return D3D_OK;}
    HRESULT GetScissorRect(RECT* r){*r=scissor;return D3D_OK;}
    HRESULT SetScissorRect(const RECT* r){scissor=*r;return D3D_OK;}
    HRESULT GetStreamSource(UINT i,Fake** b,UINT* o,UINT* s){give(streams[i].b,b);*o=streams[i].offset;*s=streams[i].stride;return D3D_OK;}
    HRESULT SetStreamSource(UINT i,Fake* b,UINT o,UINT s){streams[i].b=b;streams[i].offset=o;streams[i].stride=s;return D3D_OK;}
    HRESULT GetStreamSourceFreq(UINT i,UINT* f){*f=streams[i].freq;return D3D_OK;}
    HRESULT SetStreamSourceFreq(UINT i,UINT f){streams[i].freq=f;return D3D_OK;}
    HRESULT GetIndices(Fake** out){give(indices,out);return D3D_OK;}
    HRESULT SetIndices(Fake* f){indices=f;return D3D_OK;}
    HRESULT GetVertexDeclaration(Fake** out){give(decl,out);return D3D_OK;}
    HRESULT SetVertexDeclaration(Fake* f){decl=f;fvf=0;return D3D_OK;}
    HRESULT GetFVF(DWORD* f){*f=fvf;return D3D_OK;}
    HRESULT SetFVF(DWORD f){fvf=f;decl=nullptr;return D3D_OK;}
    HRESULT GetVertexShader(Fake** out){give(vs,out);return D3D_OK;}
    HRESULT SetVertexShader(Fake* f){vs=f;return D3D_OK;}
    HRESULT GetPixelShader(Fake** out){give(ps,out);return D3D_OK;}
    HRESULT SetPixelShader(Fake* f){ps=f;return D3D_OK;}
    template<class T,size_t N> static HRESULT get(const T (&from)[N],UINT start,T* out,UINT count,UINT width){if((start+count)*width>N)return D3DERR_INVALIDCALL;std::memcpy(out,from+start*width,count*width*sizeof(T));return D3D_OK;}
    template<class T,size_t N> static HRESULT put(T (&to)[N],UINT start,const T* in,UINT count,UINT width){if((start+count)*width>N)return D3DERR_INVALIDCALL;std::memcpy(to+start*width,in,count*width*sizeof(T));return D3D_OK;}
    HRESULT GetVertexShaderConstantF(UINT s,float* o,UINT c){return get(vsF,s,o,c,4);} HRESULT SetVertexShaderConstantF(UINT s,const float* i,UINT c){return put(vsF,s,i,c,4);}
    HRESULT GetVertexShaderConstantI(UINT s,int* o,UINT c){return get(vsI,s,o,c,4);} HRESULT SetVertexShaderConstantI(UINT s,const int* i,UINT c){return put(vsI,s,i,c,4);}
    HRESULT GetVertexShaderConstantB(UINT s,BOOL* o,UINT c){return get(vsB,s,o,c,1);} HRESULT SetVertexShaderConstantB(UINT s,const BOOL* i,UINT c){return put(vsB,s,i,c,1);}
    HRESULT GetPixelShaderConstantF(UINT s,float* o,UINT c){return get(psF,s,o,c,4);} HRESULT SetPixelShaderConstantF(UINT s,const float* i,UINT c){return put(psF,s,i,c,4);}
    HRESULT GetPixelShaderConstantI(UINT s,int* o,UINT c){return get(psI,s,o,c,4);} HRESULT SetPixelShaderConstantI(UINT s,const int* i,UINT c){return put(psI,s,i,c,4);}
    HRESULT GetPixelShaderConstantB(UINT s,BOOL* o,UINT c){return get(psB,s,o,c,1);} HRESULT SetPixelShaderConstantB(UINT s,const BOOL* i,UINT c){return put(psB,s,i,c,1);}
    HRESULT GetRenderTarget(DWORD i,Fake** out){if(i&&!targets[i]){*out=nullptr;return D3DERR_NOTFOUND;}give(targets[i],out);return D3D_OK;}
    HRESULT SetRenderTarget(DWORD i,Fake* f){if(!i&&!f)return D3DERR_INVALIDCALL;targets[i]=f;
        if(!i){viewport={0,0,1024,768,0,1};scissor={0,0,1024,768};}return D3D_OK;} // D3D9 resets both on target 0
    HRESULT GetDepthStencilSurface(Fake** out){if(!depth){*out=nullptr;return D3DERR_NOTFOUND;}give(depth,out);return D3D_OK;}
    HRESULT SetDepthStencilSurface(Fake* f){depth=f;return D3D_OK;}
};
// Guest addresses are the fake ids; 0xdead marks an object the guest never received.
struct Map {
    std::map<uint32_t,Fake*>* all;
    uint32_t guest(Fake* f){if(!f)return 0;require(f->id!=0xdead,"unknown");return f->id;}
    Fake* find(uint32_t a){return all->at(a);}
    Fake* texture(uint32_t a){return find(a);} Fake* surface(uint32_t a){return find(a);} Fake* vertexBuffer(uint32_t a){return find(a);}
    Fake* indexBuffer(uint32_t a){return find(a);} Fake* declaration(uint32_t a){return find(a);} Fake* vertexShader(uint32_t a){return find(a);} Fake* pixelShader(uint32_t a){return find(a);}
};
}
namespace nfs_d3d9 { template<> struct DeviceTypes<Mock> { using Texture=Fake;using VertexBuffer=Fake;using IndexBuffer=Fake;using Declaration=Fake;
    using VertexShader=Fake;using PixelShader=Fake;using Surface=Fake; }; }
int main(){try{
    using namespace nfs_d3d9;
    std::map<uint32_t,Fake*> all;Fake back{0x5000},depthSurface{0x5100},texA{0x3000},texB{0x3100},vb{0x6000},ib{0x7000},decl{0xa000},vs{0x8000},ps{0x9000},stranger{0xdead};
    for(Fake* f:{&back,&depthSurface,&texA,&texB,&vb,&ib,&decl,&vs,&ps})all[f->id]=f;
    Map map{&all};
    Mock a;a.targets[0]=&back;a.depth=&depthSurface;
    a.rs[27]=1;a.rs[19]=5;a.rs[20]=6;a.rs[60]=0x80ff00ffu;a.tss[1*64+1]=4;a.samp[0*16+5]=3;a.samp[258*16+1]=2;
    a.textures[0]=&texA;a.textures[3]=&texB;a.textures[257]=&texA;
    D3DMATRIX view{};view._11=2;view._42=-7.5f;a.transforms[D3DTS_VIEW]=view;
    a.viewport={10,20,300,200,0.25f,0.75f};a.scissor={5,6,70,80};a.material.Power=12.5f;
    D3DLIGHT9 light{};light.Type=D3DLIGHT_DIRECTIONAL;light.Direction.z=-1;a.lights[2]=light;a.lightOn[2]=TRUE;
    a.planes[1][3]=4.0f;a.streams[0]={&vb,32,24,1};a.streams[1].freq=0x40000001u;a.indices=&ib;a.decl=&decl;a.vs=&vs;a.ps=&ps;
    a.vsF[17]=1.5f;a.vsI[9]=-3;a.vsB[4]=TRUE;a.psF[223*4+3]=-0.0f;a.psI[1]=8;a.psB[15]=TRUE;
    auto state=captureDevice(&a,0x2000,map);
    for(Fake* f:{&back,&depthSurface,&texA,&texB,&vb,&ib,&decl,&vs,&ps})require(f->refs==1,"capture leaked a reference");
    require(state.viewport[2]==300&&state.lights.size()==1&&state.lights[0].index==2&&state.lights[0].enabled,"capture read the wrong values");
    // A fresh device reaches the same state, including viewport and scissor despite target 0 resetting them.
    Mock b;b.targets[0]=&texB;
    applyDevice(&b,state,map);
    require(captureDevice(&b,0x2000,map)==state,"replayed device state differs");
    require(b.viewport.X==10&&b.scissor.right==70&&b.targets[0]==&back&&b.decl==&decl,"order-sensitive state lost");
    float negativeZero=b.psF[223*4+3];uint32_t bits;std::memcpy(&bits,&negativeZero,4);require(bits==0x80000000u,"float bits not exact");
    // An FVF without a declaration replays as an FVF.
    a.decl=nullptr;a.fvf=0x152;auto fvfState=captureDevice(&a,0x2000,map);Mock c;c.targets[0]=&back;c.decl=&decl;
    applyDevice(&c,fvfState,map);require(c.fvf==0x152&&!c.decl,"FVF not replayed");
    // An object the guest never received refuses the capture.
    a.textures[5]=&stranger;bool refused=false;try{captureDevice(&a,0x2000,map);}catch(const std::runtime_error&){refused=true;}
    require(refused,"unknown object captured");a.textures[5]=nullptr;
    // The file format carries it unchanged.
    std::vector<uint32_t> words;serialize(state,words);size_t at=0;
    auto read=deserialize([&]{require(at<words.size(),"short");return words[at++];});
    require(read==state&&at==words.size(),"serialized device state differs");
    std::puts("D3D9 device state passed: capture, replay order, exact floats, FVF/declaration, references, unknown objects and serialization");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
