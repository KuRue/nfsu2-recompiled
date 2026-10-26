// Inside guest runtime namespace, after input object creation helpers.
std::atomic<bool> phoneFeedbackEnabled{false};
nfs_ff::Mixer forceMixer;
uint64_t feedbackNow(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
std::array<uint8_t,16> effectGuid(uint32_t type){
    std::array<uint8_t,16> guid={0,0,0,0,0x33,0x8e,0xd0,0x11,0x9a,0xd0,0,0xa0,0xc9,0xa0,0x6e,0x35};
    std::memcpy(guid.data(),&type,4);return guid;
}
uint32_t forceType(uint32_t guid){
    if(!guid||!memory->readable(guid,16))return 0;
    auto type=read32(guid);
    return type>=0x13541c20u&&type<=0x13541c2au&&inputGuidEquals(guid,effectGuid(type))?type:0;
}
const std::array<uint8_t,16> forceEffectIid={0xc0,0xf7,0xe1,0xe7,0xd2,0x88,0xd0,0x11,0x9a,0xd0,0,0xa0,0xc9,0xa0,0x6e,0x35};
uint32_t forceCategory(uint32_t type){return type==0x13541c20u?1:type==0x13541c21u?2:type<0x13541c27u?3:4;}
bool readForceParameters(nfs_ff::Effect& e,uint32_t p,uint32_t flags){
    if(!p||!memory->readable(p,52))return false;
    uint32_t size=read32(p);if((size!=52&&size!=56)||!memory->readable(p,size))return false;
    auto next=e;
    if(flags&1)next.durationUs=read32(p+8);
    if(flags&4)next.gain=std::min(read32(p+16),10000u);
    if((flags&0x200)&&size==56)next.startDelayUs=read32(p+52);
    if(flags&0x80){
        uint32_t env=read32(p+40);
        next.attackUs=next.fadeUs=next.attackLevel=next.fadeLevel=0;
        if(env){
            if(!memory->readable(env,20)||read32(env)!=20)return false;
            next.attackLevel=std::min(read32(env+4),10000u);next.attackUs=read32(env+8);
            next.fadeLevel=std::min(read32(env+12),10000u);next.fadeUs=read32(env+16);
        }
    }
    if(flags&0x100){
        uint32_t bytes=read32(p+44),data=read32(p+48),category=forceCategory(e.type);
        uint32_t need=category==1?4:category==2?8:category==3?16:24;
        if(bytes<need||bytes>4096||!data||!memory->readable(data,bytes))return false;
        next.magnitude=int32_t(read32(data));next.offset=0;
        if(category==2){int64_t a=int32_t(read32(data)),b=int32_t(read32(data+4));next.magnitude=int32_t(std::max(a<0?-a:a,b<0?-b:b));}
        if(category==3)next.offset=int32_t(read32(data+4));
        if(category==4){
            uint32_t axes=std::max(1u,read32(p+28));if(axes>6||bytes<axes*24)return false;
            int64_t strongest=0;
            for(uint32_t axis=0;axis<axes;++axis){uint32_t c=data+axis*24;
                int64_t positive=int32_t(read32(c+4)),negative=int32_t(read32(c+8));
                int64_t coefficient=std::min<int64_t>(10000,std::max(positive<0?-positive:positive,negative<0?-negative:negative));
                uint32_t saturation=std::min(10000u,std::max(read32(c+12),read32(c+16)));
                strongest=std::max(strongest,coefficient*saturation/10000);
            }
            next.conditionForce=int32_t(strongest);
            static unsigned loggedConditions=0;if(++loggedConditions<=8)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Phone grip condition type=%08x force=%d",e.type,next.conditionForce);
        }
    }
    e=next;return true;
}
void fillEffectInfo(uint32_t output,uint32_t type){
    std::memset(ptr(output,292),0,292);write32(output,292);auto guid=effectGuid(type);std::memcpy(ptr(output+4,16),guid.data(),16);
    write32(output+20,forceCategory(type));write32(output+24,0x3ff);write32(output+28,0x3ff);
    const char* name="Android phone vibration";std::memcpy(ptr(output+32,std::strlen(name)+1),name,std::strlen(name)+1);
}
void forceEffectDispatch(uint32_t self,uint32_t method){
    // Map calls can reenter via callbacks, but effect methods have no guest callback.
    std::lock_guard<std::mutex> guard(inputMutex);
    auto found=forceMixer.effects.find(self);if(found==forceMixer.effects.end()){ret(diInvalid,1);return;}
    auto& e=found->second;
    switch(method){
    case 3:ret(0,4);return;
    case 4:if(!arg(1)||!memory->writable(arg(1),16)){ret(diInvalid,2);return;}{auto g=effectGuid(e.type);std::memcpy(ptr(arg(1),16),g.data(),16);ret(0,2);return;}
    case 5:ret(0x80004001,3);return;
    case 6:{if(!readForceParameters(e,arg(1),arg(2))){ret(diInvalid,3);return;}
        if(arg(2)&0x20000000u)forceMixer.start(e,1,feedbackNow());else forceMixer.parametersChanged(e,feedbackNow());ret(0,3);return;}
    case 7:{if(arg(2)&1)forceMixer.stopAll(e.owner);forceMixer.start(e,arg(1),feedbackNow());
        static unsigned loggedStarts=0;if(++loggedStarts<=12)NFS_RUNTIME_LOG(ANDROID_LOG_INFO,"NFSU2","Phone force effect start type=%08x magnitude=%d duration=%u",e.type,e.magnitude,e.durationUs);ret(0,3);return;}
    case 8:case 11:e.playing=false;ret(0,1);return;
    case 9:if(!arg(1)||!memory->writable(arg(1),4)){ret(diInvalid,2);return;}write32(arg(1),e.playing&&feedbackNow()<e.end?1:0);ret(0,2);return;
    case 10:ret(0,1);return;
    case 12:ret(0x80004001,2);return;
    default:ret(diInvalid,1);return;
    }
}
