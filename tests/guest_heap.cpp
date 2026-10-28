#include "../android/app/src/main/cpp/guest_heap.h"
#include <cstdio>
int main() {
    GuestMemory memory;
    testGuestHeaps(memory);
    std::puts("Guest heap passed");
}
