#ifndef TEXTURE_H
#define TEXTURE_H

typedef struct {
    unsigned int *data;
    int width;
    int height;
} Texture;

Texture *loadTexturePNG(const char *path);
void bindTexture(Texture *tex);
void freeTexture(Texture *tex);

#endif
