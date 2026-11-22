#include "arabic_font.h"
#include "arabic_font_data.h"

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
    uint16_t base, isolated, final_form, initial_form, medial_form;
} ArabicShape;

typedef struct {
    short x, y, z;
} FontVertex;

static FontGlyph g_glyphs[ARABIC_FONT_GLYPH_COUNT];
static int g_font_ready = 0;

static const ArabicShape g_shapes[] = {
    {0x0621,0xFE80,0x0000,0x0000,0x0000},
    {0x0622,0xFE81,0xFE82,0x0000,0x0000},
    {0x0623,0xFE83,0xFE84,0x0000,0x0000},
    {0x0624,0xFE85,0xFE86,0x0000,0x0000},
    {0x0625,0xFE87,0xFE88,0x0000,0x0000},
    {0x0626,0xFE89,0xFE8A,0xFE8B,0xFE8C},
    {0x0627,0xFE8D,0xFE8E,0x0000,0x0000},
    {0x0628,0xFE8F,0xFE90,0xFE91,0xFE92},
    {0x0629,0xFE93,0xFE94,0x0000,0x0000},
    {0x062A,0xFE95,0xFE96,0xFE97,0xFE98},
    {0x062B,0xFE99,0xFE9A,0xFE9B,0xFE9C},
    {0x062C,0xFE9D,0xFE9E,0xFE9F,0xFEA0},
    {0x062D,0xFEA1,0xFEA2,0xFEA3,0xFEA4},
    {0x062E,0xFEA5,0xFEA6,0xFEA7,0xFEA8},
    {0x062F,0xFEA9,0xFEAA,0x0000,0x0000},
    {0x0630,0xFEAB,0xFEAC,0x0000,0x0000},
    {0x0631,0xFEAD,0xFEAE,0x0000,0x0000},
    {0x0632,0xFEAF,0xFEB0,0x0000,0x0000},
    {0x0633,0xFEB1,0xFEB2,0xFEB3,0xFEB4},
    {0x0634,0xFEB5,0xFEB6,0xFEB7,0xFEB8},
    {0x0635,0xFEB9,0xFEBA,0xFEBB,0xFEBC},
    {0x0636,0xFEBD,0xFEBE,0xFEBF,0xFEC0},
    {0x0637,0xFEC1,0xFEC2,0xFEC3,0xFEC4},
    {0x0638,0xFEC5,0xFEC6,0xFEC7,0xFEC8},
    {0x0639,0xFEC9,0xFECA,0xFECB,0xFECC},
    {0x063A,0xFECD,0xFECE,0xFECF,0xFED0},
    {0x0641,0xFED1,0xFED2,0xFED3,0xFED4},
    {0x0642,0xFED5,0xFED6,0xFED7,0xFED8},
    {0x0643,0xFED9,0xFEDA,0xFEDB,0xFEDC},
    {0x0644,0xFEDD,0xFEDE,0xFEDF,0xFEE0},
    {0x0645,0xFEE1,0xFEE2,0xFEE3,0xFEE4},
    {0x0646,0xFEE5,0xFEE6,0xFEE7,0xFEE8},
    {0x0647,0xFEE9,0xFEEA,0xFEEB,0xFEEC},
    {0x0648,0xFEED,0xFEEE,0x0000,0x0000},
    {0x0649,0xFEEF,0xFEF0,0x0000,0x0000},
    {0x064A,0xFEF1,0xFEF2,0xFEF3,0xFEF4}
};

static int base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static size_t decode_base64(unsigned char *out, size_t capacity) {
    size_t used = 0;
    uint32_t value = 0;
    int bits = -8;
    const char *p = g_arabic_font_b64;
    while (*p && used < capacity) {
        int v;
        if (*p == '=') break;
        v = base64_value(*p++);
        if (v < 0) continue;
        value = (value << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 0) {
            if (used >= capacity) break;
            out[used++] = (unsigned char)((value >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return used;
}

void arabicFontInit(void) {
    unsigned char packed[4096];
    unsigned char raw[ARABIC_FONT_GLYPH_COUNT * FONT_GLYPH_BYTES];
    size_t packed_len, src = 0, dst = 0;
    int i, row;

    if (g_font_ready) return;
    packed_len = decode_base64(packed, sizeof(packed));
    while (src < packed_len && dst < sizeof(raw)) {
        unsigned char control = packed[src++];
        size_t count;
        if (control & 0x80) {
            count = (size_t)(control & 0x7F) + 1;
            if (src + count > packed_len || dst + count > sizeof(raw)) break;
            memcpy(raw + dst, packed + src, count);
            src += count;
            dst += count;
        } else {
            count = (size_t)control + 1;
            if (dst + count > sizeof(raw)) break;
            memset(raw + dst, 0, count);
            dst += count;
        }
    }
    if (dst != sizeof(raw)) return;

    src = 0;
    for (i = 0; i < ARABIC_FONT_GLYPH_COUNT; ++i) {
        g_glyphs[i].codepoint = (uint16_t)(((uint16_t)raw[src] << 8) | raw[src + 1]);
        src += 2;
        for (row = 0; row < FONT_HEIGHT; ++row) {
            g_glyphs[i].rows[row] = (uint16_t)(((uint16_t)raw[src] << 8) | raw[src + 1]);
            src += 2;
        }
    }
    g_font_ready = 1;
}

static const FontGlyph *find_glyph(uint32_t cp) {
    int lo = 0, hi = ARABIC_FONT_GLYPH_COUNT - 1;
    if (!g_font_ready) arabicFontInit();
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        uint32_t found = g_glyphs[mid].codepoint;
        if (found == cp) return &g_glyphs[mid];
        if (found < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

static const ArabicShape *find_shape(uint32_t cp) {
    size_t i;
    for (i = 0; i < sizeof(g_shapes) / sizeof(g_shapes[0]); ++i)
        if (g_shapes[i].base == cp) return &g_shapes[i];
    return NULL;
}

static int is_mark(uint32_t cp) {
    return (cp >= 0x064B && cp <= 0x065F) || cp == 0x0670 ||
           (cp >= 0xFE70 && cp <= 0xFE7F);
}

static int decode_utf8(const char **cursor, uint32_t *out) {
    const unsigned char *p = (const unsigned char *)*cursor;
    uint32_t cp;
    if (!*p) return 0;
    if (p[0] < 0x80) {
        cp = p[0]; *cursor += 1;
    } else if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        cp = ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *cursor += 2;
        if (cp < 0x80) cp = '?';
    } else if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
        cp = ((uint32_t)(p[0] & 0x0F) << 12) |
             ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *cursor += 3;
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) cp = '?';
    } else if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 &&
               (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
        cp = ((uint32_t)(p[0] & 0x07) << 18) |
             ((uint32_t)(p[1] & 0x3F) << 12) |
             ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        *cursor += 4;
        if (cp < 0x10000 || cp > 0x10FFFF) cp = '?';
    } else {
        cp = '?'; *cursor += 1;
    }
    *out = cp;
    return 1;
}

static int strong_direction(uint32_t cp) {
    if ((cp >= '0' && cp <= '9') || (cp >= 0x0660 && cp <= 0x0669))
        return 0;
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z'))
        return 0;
    if (find_shape(cp) || (cp >= 0x0600 && cp <= 0x06FF) ||
        (cp >= 0xFE70 && cp <= 0xFEFF))
        return 1;
    return -1;
}

static int next_shape_index(const uint32_t *items, int count, int index, int step) {
    int i;
    for (i = index + step; i >= 0 && i < count; i += step) {
        if (is_mark(items[i])) continue;
        if (find_shape(items[i])) return i;
        return -1;
    }
    return -1;
}

static void shape_arabic(uint32_t *items, int count) {
    uint32_t result[FONT_MAX_CHARS];
    int i;
    memcpy(result, items, (size_t)count * sizeof(uint32_t));
    for (i = 0; i < count; ++i) {
        const ArabicShape *current = find_shape(result[i]);
        const ArabicShape *previous, *next;
        int pi, ni, join_previous = 0, join_next = 0;
        if (!current) continue;
        pi = next_shape_index(result, count, i, -1);
        ni = next_shape_index(result, count, i, 1);
        previous = pi >= 0 ? find_shape(result[pi]) : NULL;
        next = ni >= 0 ? find_shape(result[ni]) : NULL;
        if (previous && previous->initial_form && current->final_form)
            join_previous = 1;
        if (next && current->initial_form && next->final_form)
            join_next = 1;
        if (join_previous && join_next && current->medial_form)
            items[i] = current->medial_form;
        else if (join_previous && current->final_form)
            items[i] = current->final_form;
        else if (join_next && current->initial_form)
            items[i] = current->initial_form;
        else
            items[i] = current->isolated;
    }
}

static int build_visual_order(const char *utf8, uint32_t *visual) {
    uint32_t logical[FONT_MAX_CHARS], shaped[FONT_MAX_CHARS];
    int direction[FONT_MAX_CHARS], run_start[FONT_MAX_CHARS];
    int count = 0, runs = 0, i, out = 0;
    const char *p = utf8;

    if (!utf8) return 0;
    while (*p && count < FONT_MAX_CHARS) {
        uint32_t cp;
        if (!decode_utf8(&p, &cp)) break;
        if (cp == 0xFEFF) continue;
        if (is_mark(cp)) continue;
        logical[count++] = cp;
    }
    if (!count) return 0;

    memcpy(shaped, logical, (size_t)count * sizeof(uint32_t));
    shape_arabic(shaped, count);

    for (i = 0; i < count; ++i) {
        direction[i] = strong_direction(logical[i]);
        if (direction[i] < 0)
            direction[i] = (i > 0) ? direction[i - 1] : 1;
    }
    /* Resolve leading neutral characters from the first strong character. */
    for (i = 0; i < count && strong_direction(logical[i]) < 0; ++i) {
        int j;
        for (j = i + 1; j < count; ++j) {
            int d = strong_direction(logical[j]);
            if (d >= 0) { direction[i] = d; break; }
        }
    }

    for (i = 0; i < count; ++i) {
        if (i == 0 || direction[i] != direction[i - 1])
            run_start[runs++] = i;
    }
    run_start[runs] = count;
    while (runs > 0) {
        int start = run_start[runs - 1];
        int end = run_start[runs];
        if (direction[start] == 1) {
            int j;
            for (j = end - 1; j >= start; --j) visual[out++] = shaped[j];
        } else {
            int j;
            for (j = start; j < end; ++j) visual[out++] = shaped[j];
        }
        --runs;
    }

    for (i = 0; i < out; ++i) {
        if (visual[i] == '(') visual[i] = ')';
        else if (visual[i] == ')') visual[i] = '(';
        else if (visual[i] == '[') visual[i] = ']';
        else if (visual[i] == ']') visual[i] = '[';
    }
    return out;
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
