#include "graphics_state.h"
#include <iostream>
using namespace graphics_snapshot;
// Synthetic procedural "asset files" (no game data). `variant` edits one texel.
static bool variant=false;
std::vector<uint32_t> asset(uint32_t id,uint32_t w,uint32_t h){
    require(id==1||id==2,"Unknown asset");std::vector<uint32_t> p(size_t(w)*h);
    for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x)p[y*w+x]=id==1?pack(((x^y)&4)?220:30,x*8,y*8,255):pack(255,128,0,uint32_t(x*255/(w-1)));
    if(variant&&id==2)p[3]^=1;
    return p;
}
uint32_t create(Device& d,uint32_t w,uint32_t h,uint32_t source){
    Texture t;t.id=d.nextHandle++;t.width=w;t.height=h;
    if(source){t.origin=Asset;t.asset=source;t.pixels=asset(source,w,h);}else{t.origin=RenderTarget;t.pixels.assign(size_t(w)*h,0);}
    d.textures.push_back(std::move(t));return d.textures.back().id;
}
// Deterministic per-frame commands. State set in one frame persists into later frames,
// so a restore must reproduce bound texture/target, blend, scissor, tint and RT contents.
std::vector<uint32_t> frame(Device& d){
    std::vector<uint32_t> fb(screen*screen,0xff202020u);auto& s=d.state;auto f=d.frame;
    if(f==0){create(d,16,16,1);create(d,8,8,2);create(d,32,32,0);}
    if(f==2){s.target=3;s.texture=1;s.blend=Opaque;s.scissor[2]=32;s.scissor[3]=32;draw(d,fb,0,0,32,32);
             s.texture=2;s.blend=Alpha;draw(d,fb,8,8,24,24);s.target=0;s.scissor[2]=screen;s.scissor[3]=screen;}
    if(f==5){s.target=3;s.texture=0;s.blend=Additive;s.tint=0x40102030u;s.scissor[2]=s.scissor[3]=32;draw(d,fb,0,0,16,32);s.target=0;s.scissor[2]=s.scissor[3]=screen;}
    if(f==7){s.scissor[0]=10;s.scissor[1]=5;s.scissor[2]=50;s.scissor[3]=60;}
    if(f==9){s.tint=0xff80ff80u;}
    if(f==11){d.textures.erase(d.textures.begin());s.texture=0;} // free an asset texture
    s.texture=d.textures.size()==3?1:0;s.blend=Opaque;draw(d,fb,0,0,64,64);
    if(f>=2){s.texture=3;s.blend=f%2?Additive:Alpha;draw(d,fb,f,4,f+30,34);}
    if(d.textures.size()>=2){for(auto& t:d.textures)if(t.origin==Asset&&t.asset==2)s.texture=t.id;s.blend=Alpha;draw(d,fb,40,40,60,60);}
    ++d.frame;return fb;
}
int main(int argc,char** argv){try{
    require(argc>=2,"usage: graphics_runner baseline|capture N path|restore path [variant]");std::string mode=argv[1];constexpr uint32_t total=14;Device d;
    if(mode=="capture"){require(argc==4,"capture N path");auto at=uint32_t(std::stoul(argv[2]));while(d.frame<at)frame(d);writeAtomicBytes(argv[3],encode(d));return 0;}
    if(mode=="restore"){require(argc>=3,"restore path");variant=argc==4;d=decode(readBytes(argv[2]),asset);}else require(mode=="baseline","Unknown mode");
    while(d.frame<total){auto at=d.frame;auto fb=frame(d);std::cout<<at<<':'<<pixelCrc(fb)<<'\n';}
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
