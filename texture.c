#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#include "texture.h"

#include <pspgu.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>

static int next_pow2(int n) {
    int p = 16;
    while (p < n && p < 512) p <<= 1;
    return p;
}

Texture *loadTexturePNG(const char *path) {
    FILE *fp;
    long length;
    unsigned char *encoded = NULL, *rgba = NULL;
    unsigned int *pixels = NULL;
    int w = 0, h = 0, channels = 0, tw, th, x, y;
    Texture *tex = NULL;

    if (!path || !(fp = fopen(path, "rb"))) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    length = ftell(fp);
    if (length <= 0 || length > 8 * 1024 * 1024 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp); return NULL;
    }
    encoded = (unsigned char *)malloc((size_t)length);
    if (!encoded) { fclose(fp); return NULL; }
    if (fread(encoded, 1, (size_t)length, fp) != (size_t)length) goto done;
    fclose(fp); fp = NULL;
    rgba = stbi_load_from_memory(encoded, (int)length, &w, &h, &channels, 4);
    if (!rgba || w <= 0 || h <= 0) goto done;
    tw = next_pow2(w); th = next_pow2(h);
    pixels = (unsigned int *)memalign(16, (size_t)tw * th * sizeof(unsigned int));
    tex = (Texture *)calloc(1, sizeof(*tex));
    if (!pixels || !tex) goto done;
    for (y = 0; y < th; ++y) {
        int sy = y * h / th;
        for (x = 0; x < tw; ++x) {
            int sx = x * w / tw;
            const unsigned char *s = rgba + (sy * w + sx) * 4;
            pixels[y * tw + x] = (unsigned int)s[0] |
                ((unsigned int)s[1] << 8) | ((unsigned int)s[2] << 16) |
                ((unsigned int)s[3] << 24);
        }
    }
    tex->data = pixels; tex->width = tw; tex->height = th;
    pixels = NULL;
    sceKernelDcacheWritebackInvalidateAll();
done:
    if (fp) fclose(fp);
    free(encoded);
    if (rgba) stbi_image_free(rgba);
    free(pixels);
    if (!tex || !tex->data) { free(tex); return NULL; }
    return tex;
}

void bindTexture(Texture *tex) {
    if (!tex || !tex->data) { sceGuDisable(GU_TEXTURE_2D); return; }
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_TRUE);
    sceGuTexImage(0, tex->width, tex->height, tex->width, tex->data);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
}

void freeTexture(Texture *tex) {
    if (!tex) return;
    free(tex->data);
    free(tex);
}
