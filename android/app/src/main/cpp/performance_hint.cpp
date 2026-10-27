#include "performance_hint.h"
#include <android/api-level.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>

namespace {
struct HintApi {
    using Manager=void*;
    using Session=void*;
    Manager (*getManager)()=nullptr;
    Session (*create)(Manager,const int32_t*,size_t,int64_t)=nullptr;
    int (*update)(Session,int64_t)=nullptr;
    int (*report)(Session,int64_t)=nullptr;
    void (*close)(Session)=nullptr;
    int (*prefer)(Session,bool)=nullptr;
    bool resolved=false;
    bool ready=false;
};
HintApi& hintApi(){
    static HintApi api;
    if(api.resolved)return api;
    api.resolved=true;
    if(android_get_device_api_level()<33)return api;
    void* library=dlopen("libandroid.so",RTLD_NOW);
    if(!library)return api;
    api.getManager=reinterpret_cast<HintApi::Manager(*)()>(dlsym(library,"APerformanceHint_getManager"));
    api.create=reinterpret_cast<HintApi::Session(*)(HintApi::Manager,const int32_t*,size_t,int64_t)>(dlsym(library,"APerformanceHint_createSession"));
    api.update=reinterpret_cast<int(*)(HintApi::Session,int64_t)>(dlsym(library,"APerformanceHint_updateTargetWorkDuration"));
    api.report=reinterpret_cast<int(*)(HintApi::Session,int64_t)>(dlsym(library,"APerformanceHint_reportActualWorkDuration"));
    api.close=reinterpret_cast<void(*)(HintApi::Session)>(dlsym(library,"APerformanceHint_closeSession"));
    api.prefer=reinterpret_cast<int(*)(HintApi::Session,bool)>(dlsym(library,"APerformanceHint_setPreferPowerEfficiency"));
    api.ready=api.getManager&&api.create&&api.update&&api.report&&api.close;
    return api;
}
struct SessionState {
    HintApi::Session session=nullptr;
    int32_t tid=0;
    uint64_t target=0;
    bool unavailable=false;
};
SessionState& hintState(){static SessionState state;return state;}
void closeHint(){
    SessionState& state=hintState();
    HintApi& api=hintApi();
    if(state.session&&api.close)api.close(state.session);
    if(state.session)__android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint off");
    state.session=nullptr;state.tid=0;state.target=0;
}
bool openHint(uint64_t target){
    SessionState& state=hintState();
    if(state.unavailable)return false;
    HintApi& api=hintApi();
    if(!api.ready){state.unavailable=true;return false;}
    int32_t tid=int32_t(gettid());
    if(state.session&&state.tid!=tid)closeHint();
    if(!state.session){
        HintApi::Manager manager=api.getManager();
        if(!manager){state.unavailable=true;__android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint unavailable");return false;}
        state.session=api.create(manager,&tid,1,int64_t(target));
        if(!state.session){state.unavailable=true;__android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint session was not created");return false;}
        state.tid=tid;state.target=target;
        if(api.prefer&&api.prefer(state.session,true)!=0)
            __android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint efficiency preference was not accepted");
        __android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint on, target %llu ns, tid %d",static_cast<unsigned long long>(target),int(tid));
        return true;
    }
    if(state.target==target)return true;
    if(api.update(state.session,int64_t(target))!=0){closeHint();state.unavailable=true;return false;}
    state.target=target;
    __android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint target %llu ns",static_cast<unsigned long long>(target));
    return true;
}
}

void reportGuestPerformanceHint(uint64_t busyNanos,unsigned limit,unsigned refreshHz){
    PerformanceHintDecision decision=decidePerformanceHint(busyNanos,limit,refreshHz);
    if(!decision.capped){closeHint();return;}
    if(!openHint(decision.targetNanos)||!decision.report)return;
    HintApi& api=hintApi();
    if(api.report(hintState().session,int64_t(decision.actualNanos))!=0){
        __android_log_print(ANDROID_LOG_INFO,"NFSU2","Performance hint report failed");
        closeHint();
        hintState().unavailable=true;
    }
}
