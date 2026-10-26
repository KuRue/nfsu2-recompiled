#include "../android/app/src/main/cpp/guest_boot_worker.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>
static std::atomic<unsigned> boots{0};
static std::atomic<bool> stop{false};
std::string connectGuestRuntime(const char*){
    ++boots;while(!stop.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));return "finished";
}
void requestGuestRuntimeStop(){stop=true;}
int main(){
    auto* original=&processGuestWorker();
    {auto& activityWorker=processGuestWorker();activityWorker.start("test.exe");}
    auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!boots.load()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
    assert(boots==1&&original==&processGuestWorker());
    processGuestWorker().start("test.exe");assert(boots==1);
    requestGuestRuntimeStop();
    while(processGuestWorker().status()!="finished"&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
    assert(processGuestWorker().status()=="finished");
}
