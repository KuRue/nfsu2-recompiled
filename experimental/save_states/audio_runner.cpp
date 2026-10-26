#include "audio_state.h"
#include <cmath>
#include <iostream>
using namespace audio_snapshot;
// Synthetic assets (no game audio). `variant` changes one sample to model an edited asset.
std::vector<Buffer> assets(bool variant){
    std::vector<Buffer> b(3);b[0].id=10;b[1].id=11;b[2].id=12;
    for(int i=0;i<700;++i)b[0].samples.push_back(int16_t(12000*std::sin(i*0.07)));
    for(int i=0;i<97;++i)b[1].samples.push_back(int16_t((i*331)%20000-10000));
    for(int i=0;i<4000;++i)b[2].samples.push_back(int16_t(i%2?30000:-30000));
    if(variant)b[1].samples[5]^=1;
    return b;
}
// Deterministic "game" commands keyed by block index, so the schedule itself needs no saving.
void commands(Mixer& m){
    auto start=[&](uint32_t buffer,uint32_t step,uint32_t volume,uint32_t loop){m.voices.push_back({m.nextVoice++,buffer,0,step,volume,loop,1});};
    switch(m.blockIndex){
    case 0:start(10,0x10000,200,1);break;          // engine loop
    case 2:start(11,0x18000,256,0);break;          // one-shot, pitched up, ends mid-block
    case 4:start(12,0x8000,256,0);break;           // loud: exercises clipping
    case 6:m.voices[0].step=0x13333;m.voices[0].volume=128;break; // rev change
    case 9:m.voices[2].playing=0;break;            // stop
    case 10:start(11,0x10000,64,1);break;
    }
}
int main(int argc,char** argv){try{
    require(argc>=2,"usage: audio_runner baseline|capture N path|restore path [variant]");
    std::string mode=argv[1];constexpr uint32_t total=16;Mixer m;bool variant=mode=="restore"&&argc==4;auto buffers=assets(variant);
    if(mode=="capture"){require(argc==4,"capture N path");uint32_t at=uint32_t(std::stoul(argv[2]));
        while(m.blockIndex<at){commands(m);mix(m,buffers);}writeAtomicBytes(argv[3],encode(m,buffers));return 0;}
    if(mode=="restore"){require(argc>=3,"restore path");m=decode(readBytes(argv[2]),buffers);}
    else require(mode=="baseline","Unknown mode");
    // Print a CRC per block so the caller can compare each restored tail with the baseline.
    while(m.blockIndex<total){auto at=m.blockIndex;commands(m);auto pcm=mix(m,buffers);Bytes raw(reinterpret_cast<uint8_t*>(pcm.data()),reinterpret_cast<uint8_t*>(pcm.data())+pcm.size()*2);std::cout<<at<<':'<<crc(raw)<<'\n';}
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
