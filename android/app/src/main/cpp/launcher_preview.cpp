#include <jni.h>
#include <android/native_window_jni.h>
#include "guest_boot_worker.h"
#include "d3d9_bridge.h"
#include "game_data.h"
#include "preview_mode.h"
#include "guest_runtime.h"

extern const char* launcherPreviewDriverError();

// A service in :preview owns this renderer and its surface. It is never the :game process.
extern "C" JNIEXPORT jstring JNICALL Java_com_nfsu2_androidevolved_VisualPreviewService_nativeStart(
        JNIEnv* env,jclass,jobject surface,jobjectArray names,jobjectArray values,jstring profile,
        jint width,jint height,jstring language,jboolean widescreen) {
    auto text=[&](jstring value){const char* chars=env->GetStringUTFChars(value,nullptr);
        std::string result=chars?chars:"";if(chars)env->ReleaseStringUTFChars(value,chars);return result;};
    try {
        setenv("NFSU2_LAUNCHER_PREVIEW","1",1);
        setenv("NFSU2_PREVIEW_PROFILE",text(profile).c_str(),1);
        for(jsize i=0;i<env->GetArrayLength(names);++i){
            auto name=static_cast<jstring>(env->GetObjectArrayElement(names,i));
            auto value=static_cast<jstring>(env->GetObjectArrayElement(values,i));
            setenv(text(name).c_str(),text(value).c_str(),1);
            env->DeleteLocalRef(name);env->DeleteLocalRef(value);
        }
        if(*launcherPreviewDriverError())throw std::runtime_error(launcherPreviewDriverError());
        checkGameData();
        ANativeWindow* window=ANativeWindow_fromSurface(env,surface);
        if(!window)throw std::runtime_error("Preview surface unavailable");
        // Window reference lives until this isolated one-shot process exits.
        setD3D9HostWindow(window);acknowledgeD3D9Surface(true);
        setGuestDisplaySize(960,540);setGuestResolution(width,height);
        setGuestLanguage(text(language).c_str());setGuestWidescreen(widescreen);
        setGuestSkipIntro(true);setGuestFrameLimit(60);setGuestSteadyPace(false);
        setGuestPaused(false);
        processGuestWorker().start(std::string(kGameRoot)+"/SPEED2.EXE");
        return nullptr;
    }catch(const std::exception& error){return env->NewStringUTF(error.what());}
}
// Settings changed while the preview runs: applied on the game thread at the next Present.
extern "C" JNIEXPORT jint JNICALL Java_com_nfsu2_androidevolved_VisualPreviewService_nativeApply(
        JNIEnv* env,jclass,jobjectArray names,jobjectArray values){
    std::vector<std::pair<std::string,std::string>> settings;
    for(jsize i=0;i<env->GetArrayLength(names);++i){
        auto name=static_cast<jstring>(env->GetObjectArrayElement(names,i));auto value=static_cast<jstring>(env->GetObjectArrayElement(values,i));
        const char* n=env->GetStringUTFChars(name,nullptr);const char* v=env->GetStringUTFChars(value,nullptr);
        settings.emplace_back(n?n:"",v?v:"");
        env->ReleaseStringUTFChars(name,n);env->ReleaseStringUTFChars(value,v);env->DeleteLocalRef(name);env->DeleteLocalRef(value);
    }
    return jint(queuePreviewSettings(std::move(settings)));
}
// The newest settings generation whose frames have settled on screen; 0 before the first.
extern "C" JNIEXPORT jint JNICALL Java_com_nfsu2_androidevolved_VisualPreviewService_nativeReadyGeneration(JNIEnv*,jclass){
    return jint(previewReadyGeneration());
}
// Between requests the preview game is paused, so an idle settings screen costs no rendering.
extern "C" JNIEXPORT void JNICALL Java_com_nfsu2_androidevolved_VisualPreviewService_nativePause(JNIEnv*,jclass,jboolean paused){
    setGuestPaused(paused==JNI_TRUE);
}
