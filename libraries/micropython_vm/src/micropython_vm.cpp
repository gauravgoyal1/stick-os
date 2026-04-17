#include "micropython_vm.h"

#include <Arduino.h>
#include <stdlib.h>

// The embed port provides these entry points in C; call from C++ via extern "C".
extern "C" {
    #include "port/micropython_embed.h"
}

namespace MicroPythonVM {

static uint8_t* g_heap  = nullptr;
static bool     g_ready = false;

bool init(size_t heapSize) {
    if (g_ready) return true;
    g_heap = (uint8_t*)malloc(heapSize);
    if (!g_heap) {
        Serial.println("[mpy] heap alloc failed");
        return false;
    }
    // ESP32 stack grows down — pass the current SP as the "stack top" limit.
    void* stackTop = &stackTop;
    mp_embed_init(g_heap, heapSize, stackTop);
    g_ready = true;
    Serial.printf("[mpy] VM initialized, heap=%u bytes\n",
                  (unsigned)heapSize);
    return true;
}

void execStr(const char* code) {
    if (!g_ready || code == nullptr) return;
    mp_embed_exec_str(code);
}

void deinit() {
    if (!g_ready) return;
    mp_embed_deinit();
    free(g_heap);
    g_heap  = nullptr;
    g_ready = false;
    Serial.println("[mpy] VM deinitialized");
}

bool isReady() { return g_ready; }

}  // namespace MicroPythonVM
