/* GPL-3.0. Small standalone WebAssembly bridge to unchanged libxm v0.2. */
#include "xm_internal.h"
#include <stdint.h>

static unsigned char module_data[2 * 1024 * 1024];
static float output[4096 * 2];
static xm_context_t* context;

unsigned char* fs_input(void) { return module_data; }
float* fs_buffer(void) { return output; }
uint32_t fs_context_size(void) { return context ? (uint32_t)context->ctx_size : 0; }
int fs_load(uint32_t length, uint32_t rate) {
    if(length > sizeof(module_data) || rate != 48000) return 1;
    if(context) xm_free_context(context);
    context = NULL;
    int result = xm_create_context_safe(&context, (const char*)module_data, length, rate);
    if(!result) xm_set_max_loop_count(context, 1);
    return result;
}
int fs_render(uint32_t frames) {
    if(!context || frames > 4096) return 1;
    xm_generate_samples(context, output, frames);
    return 0;
}
