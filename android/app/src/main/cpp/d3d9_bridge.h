#pragma once
#include "graphics_program.h"
#include "graphics_buffer.h"
#include "graphics_texture.h"
#include "graphics_device.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
struct ANativeWindow;
void configureD3D9Bridge(void* guestBase,uint32_t (*allocator)(uint32_t),void (*deallocator)(uint32_t));
void shutdownD3D9Bridge();
void setD3D9HostWindow(ANativeWindow* window);
void setD3D9RenderSize(unsigned width,unsigned height);
// Launcher compatibility options; call before the game creates its device.
void setD3D9Compatibility(bool cpuTextures,unsigned backBuffers);
bool d3d9RequestsSurface();
void acknowledgeD3D9Surface(bool available);
uint32_t createGuestD3D9(uint32_t sdk);
std::string d3d9Inventory();
std::vector<nfs_d3d9::ProgramRecord> d3d9ProgramList();
std::vector<nfs_d3d9::BufferRecord> d3d9BufferList();
void validateD3D9Buffers(const std::vector<nfs_d3d9::BufferRecord>& records);
void restoreD3D9Buffers(const std::vector<nfs_d3d9::BufferRecord>& records);
std::vector<nfs_d3d9::TextureRecord> d3d9TextureList();
void validateD3D9Textures(const std::vector<nfs_d3d9::TextureRecord>& records);
void restoreD3D9Textures(const std::vector<nfs_d3d9::TextureRecord>& records);
std::vector<nfs_d3d9::DeviceState> d3d9DeviceStates();
void validateD3D9DeviceStates(const std::vector<nfs_d3d9::DeviceState>& states);
void restoreD3D9DeviceStates(const std::vector<nfs_d3d9::DeviceState>& states);
std::vector<std::pair<uint32_t,uint32_t>> d3d9ObjectList();
uint32_t dispatchGuestD3D9(uint32_t token,const uint32_t* args,uint32_t* argumentCount);
uint32_t guestD3DObjectCount();
