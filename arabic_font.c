#include "arabic_font.h"
#include "arabic_font_data.h"
#include "arabic_font_ascii_data.h"
#include "arabic_shaper.h"

#include <pspgu.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FONT_MAX_CHARS 192
#define FONT_GLYPH_BYTES 34
#define FONT_ADVANCE 14
#define FONT_WIDTH 12
#define FONT_HEIGHT 16

typedef struct {
    uint16_t codepoint;
    uint16_t rows[16];
} FontGlyph;

typedef struct {
    short x, y, z;
} FontVertex;

static FontGlyph g_glyphs[ARABIC_FONT_GLYPH_COUNT];
static FontGlyph g_ascii_glyphs[ASCII_FONT_GLYPH_COUNT];
static int g_font_ready = 0;

static int build_visual_order(const char *utf8, uint32_t *visual) {
    return arabic_process_line(utf8, visual, FONT_MAX_CHARS, 1);
}

float arabicMeasureText(float scale, const char *utf8) {
    uint32_t visual[FONT_MAX_CHARS];
    int count, i, visible = 0;
    if (scale <= 0.0f) return 0.0f;
    count = build_visual_order(utf8, visual);
    for (i = 0; i < count; ++i)
        if (find_glyph(visual[i]) || visual[i] == ' ') ++visible;
    return (float)(visible * FONT_ADVANCE) * scale;
}

static void draw_text_at(float left_x, float y, float scale,
                         unsigned int color, const char *utf8) {
    uint32_t visual[FONT_MAX_CHARS];
    const FontGlyph *glyphs[FONT_MAX_CHARS];
    int count, i, x, pixel_size, lit = 0, used = 0;
    FontVertex *vertices;
    if (!utf8 || scale <= 0.0f) return;
    if (scale > 2.0f) scale = 2.0f;
    if (scale < 0.5f) scale = 0.5f;
    pixel_size = (int)(scale + 0.5f);
    if (pixel_size < 1) pixel_size = 1;
    count = build_visual_order(utf8, visual);
    if (!count) return;

    for (i = 0; i < count; ++i) {
        int row, col;
        glyphs[i] = find_glyph(visual[i]);
        if (!glyphs[i]) continue;
        for (row = 0; row < FONT_HEIGHT; ++row)
            for (col = 0; col < FONT_WIDTH; ++col)
                if (glyphs[i]->rows[row] & (1u << (FONT_WIDTH - 1 - col)))
                    ++lit;
    }
    if (!lit || lit > 16000) return;
    vertices = (FontVertex *)sceGuGetMemory((size_t)lit * 2 * sizeof(FontVertex));
    if (!vertices) return;

    x = (int)left_x;
    for (i = 0; i < count; ++i) {
        int row, col;
        const FontGlyph *g = glyphs[i];
        if (g) {
            for (row = 0; row < FONT_HEIGHT; ++row) {
                for (col = 0; col < FONT_WIDTH; ++col) {
                    if (g->rows[row] & (1u << (FONT_WIDTH - 1 - col))) {
                        int px = x + (int)(col * scale);
                        int py = (int)y + (int)(row * scale);
                        vertices[used].x = (short)px;
                        vertices[used].y = (short)py;
                        vertices[used].z = 0;
                        ++used;
                        vertices[used].x = (short)(px + pixel_size);
                        vertices[used].y = (short)(py + pixel_size);
                        vertices[used].z = 0;
                        ++used;
                    }
                }
            }
        }
        x += (int)(FONT_ADVANCE * scale);
    }

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);
    sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   used, NULL, vertices);
}

void arabicDrawTextRTL(float right_x, float y, float scale,
                       unsigned int color, const char *utf8) {
    float width = arabicMeasureText(scale, utf8);
    draw_text_at(right_x - width, y, scale, color, utf8);
}

void arabicDrawTextCenter(float center_x, float y, float scale,
                          unsigned int color, const char *utf8) {
    float width = arabicMeasureText(scale, utf8);
    draw_text_at(center_x - width * 0.5f, y, scale, color, utf8);
}

void arabicDrawTextLeft(float left_x, float y, float scale,
                        unsigned int color, const char *utf8) {
    draw_text_at(left_x, y, scale, color, utf8);
}
