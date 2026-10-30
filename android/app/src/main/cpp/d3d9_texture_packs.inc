// Included inside d3d9_bridge.cpp's private namespace, after the lock/format helpers.
struct PackedTexture {
    IDirect3DTexture9* original=nullptr;
    IDirect3DTexture9* replacement=nullptr;
    CompressedView compressed;
    nfs_tex::Pack* pack=nullptr;const nfs_tex::Entry* entry=nullptr;
    bool active=false;
    ~PackedTexture(){if(replacement)replacement->Release();if(original)original->Release();}
};
std::vector<std::unique_ptr<nfs_tex::Pack>> texturePacks;
std::unordered_map<IDirect3DTexture9*,std::unique_ptr<PackedTexture>> packedTextures;
std::unordered_map<IDirect3DBaseTexture9*,PackedTexture*> packedOriginals;
bool packsInitialized=false;
uint64_t packedBytes=0;
constexpr uint64_t texturePackBudget=256ull*1024*1024;
void initTexturePacks(){
    if(packsInitialized)return;
    const char* on=std::getenv("NFSU2_NATIVE_HD");if(!on||std::strcmp(on,"1"))return;
    const char* root=std::getenv("NFSU2_NATIVE_TEXROOT");if(!root)return;
    packsInitialized=true;
    for(const char* name:{"uhud.pak","world.pak"})try{
        auto pack=std::make_unique<nfs_tex::Pack>();pack->open(std::string(root)+"/"+name);
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","HD pack %s: %zu indexed textures",name,pack->entries.size());
        texturePacks.emplace_back(std::move(pack));
    }catch(const std::exception& e){NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","HD pack skipped: %s",e.what());}
}
void clearTexturePacks(){packedOriginals.clear();packedTextures.clear();texturePacks.clear();packedBytes=0;packsInitialized=false;}
void disablePackedTexture(uint32_t address){
    auto obj=objects.find(address);if(obj==objects.end())return;
    if(obj->second.kind==Kind::Surface){if(obj->second.parent)disablePackedTexture(obj->second.parent);return;}
    if(obj->second.kind!=Kind::Texture)return;
    auto it=packedTextures.find(static_cast<IDirect3DTexture9*>(obj->second.native));if(it!=packedTextures.end())it->second->active=false;
}
void installPackedTexture(uint32_t address,nfs_tex::Pack* pack,const nfs_tex::Entry* e){
    auto texture=nativeObject<IDirect3DTexture9>(address);
    auto previous=packedTextures.find(texture);
    if(previous!=packedTextures.end()){
        auto& old=*previous->second;
        if(old.entry==e&&old.pack==pack){old.active=true;return;}
        old.active=false; // Don't serve stale pixels when a dynamic texture changes identity.
    }
    D3DSURFACE_DESC desc{};if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))return;
    IDirect3DDevice9* device=nullptr;if(FAILED(texture->GetDevice(&device)))return;
    auto format=nativeTextureFormat(device,static_cast<D3DFORMAT>(e->fmt),0,D3DRTYPE_TEXTURE);
    uint64_t replacementBytes=nfs_tex::bytes(e->w,e->h,format,e->levels);
    uint64_t originalBytes=nfs_tex::bytes(desc.Width,desc.Height,desc.Format,texture->GetLevelCount());
    if(!replacementBytes||packedTextures.size()>=4096||replacementBytes+originalBytes>texturePackBudget-packedBytes){device->Release();return;}
    auto started=std::chrono::steady_clock::now();
    IDirect3DTexture9* replacement=nullptr;
    HRESULT status=device->CreateTexture(e->w,e->h,e->levels,0,format,D3DPOOL_MANAGED,&replacement,nullptr);device->Release();
    if(FAILED(status))return;
    try{
        auto bytes=pack->read(*e);size_t offset=0;
        for(uint32_t level=0;level<e->levels;++level){
            uint32_t w=std::max(1u,uint32_t(e->w)>>level),h=std::max(1u,uint32_t(e->h)>>level);
            uint32_t pitch=nfs_tex::pitch(w,e->fmt),rows=nfs_tex::rows(h,e->fmt);
            const uint8_t* source=bytes.data()+offset;std::vector<uint8_t> decoded;
            uint32_t sourcePitch=pitch,sourceRows=rows;
            if(uint32_t(format)!=e->fmt){
                uint32_t mode=e->fmt==nfs_tex::DXT1?1:e->fmt==nfs_tex::DXT3?3:5;
                decoded=decodeDXT(mode,w,h,source,size_t(pitch)*rows);source=decoded.data();sourcePitch=w*4;sourceRows=h;
            }
            D3DLOCKED_RECT lock{};if(FAILED(replacement->LockRect(level,&lock,nullptr,0)))throw std::runtime_error("HD texture lock failed");
            if(lock.Pitch<static_cast<int32_t>(sourcePitch)){replacement->UnlockRect(level);throw std::runtime_error("HD texture pitch differs");}
            for(uint32_t row=0;row<sourceRows;++row)std::memcpy(static_cast<uint8_t*>(lock.pBits)+size_t(row)*lock.Pitch,source+size_t(row)*sourcePitch,sourcePitch);
            if(FAILED(replacement->UnlockRect(level)))throw std::runtime_error("HD texture unlock failed");
            offset+=size_t(pitch)*rows;
        }
        if(previous!=packedTextures.end()){
            // Keep accounting conservative: previously resident replacements are included
            // until shutdown, even if the device still references a replaced binding.
            if(previous->second->replacement)packedOriginals.erase(previous->second->replacement);
            packedTextures.erase(previous);
        }
        auto record=std::make_unique<PackedTexture>();texture->AddRef();record->original=texture;record->replacement=replacement;replacement=nullptr;
        record->compressed=compressionView(address);record->pack=pack;record->entry=e;record->active=true;
        auto key=record->replacement;
        auto inserted=packedTextures.emplace(texture,std::move(record));
        try{packedOriginals.emplace(key,inserted.first->second.get());}
        catch(...){packedTextures.erase(inserted.first);throw;}
        packedBytes+=replacementBytes+originalBytes;
        auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","HD texture %08x %ux%u -> %ux%u, resident budget %.1f MiB, %lld ms",address,e->ow,e->oh,e->w,e->h,packedBytes/1048576.0,(long long)elapsed);
    }catch(const std::exception& error){if(replacement)replacement->Release();NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","HD texture fallback: %s",error.what());}
}
void texturePackUpload(uint32_t address,uint32_t level){
    if(level)return;initTexturePacks();if(texturePacks.empty())return;
    auto lock=resourceLocks.find(lockKey(address,level));if(lock==resourceLocks.end()||(lock->second.flags&D3DLOCK_READONLY))return;
    auto& data=lock->second;auto texture=nativeObject<IDirect3DTexture9>(address);D3DSURFACE_DESC desc{};
    if(FAILED(texture->GetLevelDesc(0,&desc))||desc.Usage&(D3DUSAGE_RENDERTARGET|D3DUSAGE_DEPTHSTENCIL))return;
    uint32_t format=data.compressed.texture?data.compressed.texture->format:uint32_t(desc.Format);
    uint32_t rowBytes=nfs_tex::pitch(desc.Width,format),rows=nfs_tex::rows(desc.Height,format);
    if(!rowBytes||data.rowBytes!=rowBytes||data.rows!=rows){disablePackedTexture(address);return;}
    nfs_tex::Digest hash{};bool hashed=false;
    for(auto& pack:texturePacks){
        if(!pack->mayMatch(desc.Width,desc.Height,format))continue;
        if(!hashed){hash=nfs_tex::digest(static_cast<const uint8_t*>(pointer(data.guest)),rowBytes,rows,data.pitch);hashed=true;}
        if(auto e=pack->find(hash,desc.Width,desc.Height,format)){installPackedTexture(address,pack.get(),e);return;}
    }
    disablePackedTexture(address);
}
IDirect3DBaseTexture9* sampledTexture(uint32_t address){
    auto original=nativeObject<IDirect3DBaseTexture9>(address);if(!original||texturePacks.empty()||objects.at(address).kind!=Kind::Texture)return original;
    auto record=packedTextures.find(static_cast<IDirect3DTexture9*>(original));
    return record!=packedTextures.end()&&record->second->active?record->second->replacement:original;
}
uint32_t logicalTexture(IDirect3DBaseTexture9* texture){
    auto record=packedOriginals.find(texture);
    if(record==packedOriginals.end())return wrapTexture(texture);
    auto& item=*record->second;item.original->AddRef();texture->Release();
    uint32_t address=wrap(item.original,Kind::Texture);objects.at(address).compressed=item.compressed;return address;
}
void copyPackedTexture(uint32_t src,uint32_t dst){
    auto object=objects.find(src),destination=objects.find(dst);
    if(object==objects.end()||destination==objects.end()||object->second.kind!=Kind::Texture||destination->second.kind!=Kind::Texture){disablePackedTexture(dst);return;}
    auto record=packedTextures.find(static_cast<IDirect3DTexture9*>(object->second.native));
    if(record!=packedTextures.end()&&record->second->active)installPackedTexture(dst,record->second->pack,record->second->entry);
    else disablePackedTexture(dst);
}
HRESULT filteredSampler(IDirect3DDevice9* device,DWORD sampler,D3DSAMPLERSTATETYPE type,DWORD value){
    // Read every time: the in-game Graphics panel changes filtering live.
    const char* option=std::getenv("NFSU2_NATIVE_AF");
    unsigned desired=option?unsigned(std::atoi(option)):0u;
    if(desired>1&&sampler<16&&(type==D3DSAMP_MINFILTER||type==D3DSAMP_MAXANISOTROPY)){
        static std::unordered_map<IDirect3DDevice9*,unsigned> limits;
        auto found=limits.find(device);
        if(found==limits.end()) {D3DCAPS9 caps{};device->GetDeviceCaps(&caps);found=limits.emplace(device,caps.MaxAnisotropy).first;}
        if(found->second>1){
            desired=std::min(16u,std::min(desired,found->second));
            if(type==D3DSAMP_MAXANISOTROPY)value=desired;
            else if(value==D3DTEXF_LINEAR){device->SetSamplerState(sampler,D3DSAMP_MAXANISOTROPY,desired);value=D3DTEXF_ANISOTROPIC;}
        }
    }
    return device->SetSamplerState(sampler,type,value);
}
void enhanceReflectionGlobals(){
#ifndef NFSU2_EXE_F4BB0D99
    const char* option=std::getenv("NFSU2_NATIVE_REFLECTIONS");if(!option||std::strcmp(option,"1"))return;
    // Exact supported executable, same globals documented by th0mk's enhance.c.
    // Change the game's own dimensions so viewport AND matching depth allocation agree.
    for(auto setting:{std::pair<uint32_t,uint32_t>{0x7fee80,256}, {0x7fee84,256}, {0x7fee6c,128}}){
        uint32_t old=0;std::memcpy(&old,pointer(setting.first),4);
        if(old==setting.second){uint32_t value=old*2;std::memcpy(pointer(setting.first),&value,4);NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Reflection dimension %08x: %u -> %u",setting.first,old,value);}
        else NFS_RUNTIME_LOG(ANDROID_LOG_WARN,"NFSU2","Reflection dimension guard %08x: expected %u, got %u; unchanged",setting.first,setting.second,old);
    }
#endif
}
