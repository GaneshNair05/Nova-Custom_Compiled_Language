#include "raylib.h"
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nova arrays AND strings share one heap layout, every slot a double:
//   slot 0: tag    — 0.0 = array, 1.0 = string
//   slot 1: length — element count (array) or character count (string)
//   slot 2..: data — array elements, or one character code point per slot
// A Nova string is therefore NOT a null-terminated char* — passing a raw
// double* like this to something expecting a C string (as the previous
// version of nova_draw_text/nova_string_concat did) reads the wrong bytes
// entirely. See codegen.cpp's comment above nova_print_string for the
// authoritative version of this layout.
static constexpr double NOVA_TAG_STRING = 1.0;
static constexpr int NOVA_HEADER_SLOTS = 2;

// Decodes a Nova string straight into a caller-owned char buffer (no
// std::string): this file only needs to hand raylib a null-terminated
// buffer, and avoiding std::string keeps this translation unit free of any
// libc++ dependency — one less thing to get wrong if this ever ends up
// compiled/linked with plain `emcc` instead of `em++`. Longer-than-buffer
// strings are truncated rather than overflowing.
static void decodeNovaString(double* buf, char* out, size_t outCap) {
    size_t len = 0;
    if (buf && buf[1] > 0) len = (size_t)buf[1];
    if (len > outCap - 1) len = outCap - 1;
    for (size_t i = 0; i < len; ++i) {
        out[i] = (char)(int)buf[NOVA_HEADER_SLOTS + i];
    }
    out[len] = '\0';
}

extern "C" {

double nova_init_window(double width, double height, const char* title) {
    InitWindow((int)width, (int)height, title);
    return 0.0;
}

double nova_set_fps(double fps) {
    SetTargetFPS((int)fps);
    return 0.0;
}

double nova_clear_screen() {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    return 0.0;
}

double nova_render_frame() {
    EndDrawing();
    emscripten_sleep(16);
    return 0.0;
}

double nova_close_window() {
    CloseWindow();
    return 0.0;
}

double nova_is_key_down(double key) {
    return IsKeyDown((int)key) ? 1.0 : 0.0;
}

double nova_draw_rect(double x, double y, double w, double h) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, WHITE);
    return 0.0;
}

double nova_draw_rect_color(double x, double y, double w, double h, double r, double g, double b) {
    Color col = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
    DrawRectangle((int)x, (int)y, (int)w, (int)h, col);
    return 0.0;
}

// `strBuf` is a raw Nova heap pointer (see the layout comment above),
// not a C string — decode it the same way nova_print_string does natively.
double nova_draw_text(double x, double y, double fontSize, double* strBuf) {
    char buf[256];
    decodeNovaString(strBuf, buf, sizeof(buf));
    DrawText(buf, (int)x, (int)y, (int)fontSize, BLACK);
    return 0.0;
}

// Draws `value` formatted as an integer — lets scripts show a score without
// any string conversion at all.
double nova_draw_number(double x, double y, double fontSize, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f", value);
    DrawText(buf, (int)x, (int)y, (int)fontSize, BLACK);
    return 0.0;
}

double nova_random(double minVal, double maxVal) {
    if (minVal >= maxVal) return minVal;
    int imin = (int)minVal;
    int imax = (int)maxVal;
    return (double)(imin + rand() % (imax - imin + 1));
}

void nova_bounds_error(double index, double size) {
    printf("Runtime Error: Array index out of bounds! Index: %d, Size: %d\n", (int)index, (int)size);
}

// Nova-string-aware concat: allocates a new [tag][len][chars...] heap block,
// matching what codegen.cpp's visitBinaryExpr expects back from '+' on two
// strings. malloc's size_t is 32-bit here (wasm32) — this file is always
// compiled for wasm32 by emcc, so plain `size_t`/`int` is already correct;
// it's the *Nova compiler's own* emitted malloc/calloc declarations that
// needed the wasmTarget fix in codegen.cpp to match this.
double* nova_string_concat(double* a, double* b) {
    size_t lenA = (a && a[1] > 0) ? (size_t)a[1] : 0;
    size_t lenB = (b && b[1] > 0) ? (size_t)b[1] : 0;
    size_t totalLen = lenA + lenB;
    double* result = (double*)malloc((totalLen + NOVA_HEADER_SLOTS) * sizeof(double));
    result[0] = NOVA_TAG_STRING;
    result[1] = (double)totalLen;
    for (size_t i = 0; i < lenA; ++i) result[NOVA_HEADER_SLOTS + i] = a[NOVA_HEADER_SLOTS + i];
    for (size_t i = 0; i < lenB; ++i) result[NOVA_HEADER_SLOTS + lenA + i] = b[NOVA_HEADER_SLOTS + i];
    return result;
}

// The Nova compiler's entry point is called "nova_main" (double, no args),
// not "main" — see the rename in codegen.cpp. A function literally named
// "main" in the linked bitcode fights with Emscripten's own idea of the
// program entry point, which is the likely cause of the "duplicate export
// name" error from wasm-emscripten-finalize. Give the wasm module exactly
// one real C main(), and have it just call into Nova's.
double nova_main();

} // extern "C"

int main() {
    nova_main();
    return 0;
}