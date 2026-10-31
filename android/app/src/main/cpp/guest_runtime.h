#pragma once
#include <cstdint>
#include <string>
#include <vector>
// Maps the owner's PE data, binds imports, tests lifted code and attempts CRT
// entry until the first unsupported host service. Returns an honest status.
std::string connectGuestRuntime(const char* executable);
void requestGuestRuntimeStop();
// True when the guest called ExitProcess; the host returns to the launcher.
bool guestExitRequested();
// Development hook: same path as the guest ExitProcess.
void requestGuestExit();
void setGuestPaused(bool paused);
void setGuestOverlayPaused(bool paused);
// True once the game has presented at least one frame (i.e. it is live, not booting).
bool guestHasPresented();
void setGuestDisplaySize(unsigned width,unsigned height);
void setGuestKey(unsigned scan,bool down);
void clearGuestInput();
bool guestDrivingControls();
// Physical gamepad seen by the game as a DirectInput joystick: axes X, Y, Z
// (triggers), Rx, Ry, Rz in -1..1; buttons A B X Y LB RB Back Start LS RS as bits.
void setGuestPad(const float axes[6],uint32_t buttons,int hatX,int hatY);
void setGuestPadConnected(bool connected);
// True while the game has the joystick acquired (then the pad stops emulating keys).
bool guestGamepadInUse();
// Launcher option; only before the guest boots.
void setGuestWidescreen(bool enabled);
void setGuestOnlineServer(const std::string& host);
// Free-roam host bridge (roam_host_android.cpp): guest memory and calls for
// the portable roam_game.c, and the per-Present game hook.
uint8_t* guestMemoryBase();
bool guestMemoryReadable(uint32_t address,size_t length);
bool guestMemoryWritable(uint32_t address,size_t length);
// Calls a guest function with ECX/EDX set and restores the whole guest state
// afterwards (the callee may use any calling convention).
uint32_t guestCallRoam(uint32_t function,uint32_t ecx,uint32_t edx,const uint32_t* args,size_t count);
uint64_t guestMonotonicNs();
void setGuestRoamEnabled(bool enabled);
bool guestRoamEnabled();
void roamGuestFrame();
bool guestSwitchActive();
bool guestSwitchSendRaw(uint8_t kind,uint32_t dst,uint16_t sport,uint16_t dport,const uint8_t* data,size_t length);
// "ea", "lan" or "nfsu2": the virtual LAN backend is enabled for nfsu2 only.
void setGuestNetworkProvider(const std::string& provider);
// The Java /lan client: welcome, a binary switch frame, or the link closing.
void guestSwitchWelcome(uint32_t vip,uint32_t mask);
void guestSwitchFrame(const uint8_t* frame,size_t length);
void guestSwitchClosed(const char* reason);
// Next frame the virtual LAN queued for the switch; false on timeout.
bool guestSwitchPoll(std::vector<uint8_t>& frame,int timeoutMs);
/** Touch as mouse: x/y as a fraction of the game view; phase 0 down, 1 move, 2 up (left button). */
void setGuestPointer(float x,float y,int phase);
/** Posts WM_CHAR messages (the game's text fields read those); 8 is backspace. */
void typeGuestText(const std::vector<uint32_t>& characters);
void setGuestLanguage(const char* language);
void setGuestResolution(unsigned width,unsigned height);
void setGuestFrameLimit(unsigned framesPerSecond);
void setGuestDisplayRefresh(unsigned hertz);
void setGuestSteadyPace(bool steady);
// Queue a game display value for the next live Present (free roam/race only).
void queueGuestDisplayValue(const std::string& name,unsigned value);
// Refresh rates available at the current display size, 30..120 Hz.
void setGuestDisplayModes(const unsigned* hertz,unsigned count);
// Latest request for the display. False until a schedule exists, or while a
// concurrent update makes the triple inconsistent.
bool guestDisplaySchedule(unsigned& contentFps,unsigned& panelHz,unsigned& generation);
// True while a datagram socket has broadcast enabled or is bound to UDP 9999.
bool guestLanBroadcasts();
// seconds 0 disables the run. resolution is the launcher key ("75", "720").
// The handler is called on the guest thread after the report is written.
void setGuestBenchmark(unsigned seconds,const std::string& reportPath,const std::string& resolution);
void setGuestBenchmarkHandler(void (*handler)(const char* reportPath));
void setGuestBenchmarkStatsHandler(void (*handler)(const float*,unsigned));
bool guestBenchmarkActive();
void setGuestSkipIntro(bool skip);

void setGuestForceFeedback(bool enabled);
unsigned guestForceFeedbackLevel();
void stopGuestForceFeedback();

void setGuestFeedbackSource(bool physics);
