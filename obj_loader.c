#include "obj_loader.h"

#include <pspgu.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

typedef struct {
    float u, v;
    float x, y, z;
} ObjVertex;

typedef struct { float x, y, z; } Vec3;
typedef struct { float u, v; } Vec2;

struct ObjModel {
    int vertexCount;
    ObjVertex *vertices;
};

static int grow(void **ptr, size_t *capacity, size_t needed, size_t itemSize) {
    size_t next;
    void *tmp;
    if (needed <= *capacity) return 1;
    if (itemSize == 0 || needed > SIZE_MAX / itemSize) return 0;
    next = *capacity ? *capacity : 64;
    while (next < needed) {
        if (next > SIZE_MAX / 2) { next = needed; break; }
        next *= 2;
    }
    if (next > SIZE_MAX / itemSize) return 0;
    tmp = realloc(*ptr, next * itemSize);
    if (!tmp) return 0;
    *ptr = tmp;
    *capacity = next;
    return 1;
}

static int obj_index(int index, int count) {
    if (index > 0) return index - 1;
    if (index < 0) return count + index;
    return -1;
}

static int parse_face_ref(const char *token, int *vi, int *ti) {
    int v = 0, t = 0;
    if (sscanf(token, "%d/%d", &v, &t) == 2) {
        *vi = v; *ti = t; return 1;
    }
    if (sscanf(token, "%d//%*d", &v) == 1) {
        *vi = v; *ti = 0; return 1;
    }
    if (sscanf(token, "%d", &v) == 1) {
        *vi = v; *ti = 0; return 1;
    }
    return 0;
}

ObjModel *objLoad(const char *objPath) {
    FILE *fp;
    char line[2048];
    Vec3 *positions = NULL;
    Vec2 *uvs = NULL;
    ObjVertex *out = NULL;
    size_t pc = 0, pcap = 0, uc = 0, ucap = 0, oc = 0, ocap = 0;
    ObjModel *model = NULL;

    if (!objPath || !(fp = fopen(objPath, "r"))) return NULL;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 p;
            if (sscanf(line + 2, "%f %f %f", &p.x, &p.y, &p.z) == 3) {
                if (!grow((void **)&positions, &pcap, pc + 1, sizeof(Vec3))) goto done;
                positions[pc++] = p;
            }
        } else if (line[0] == 'v' && line[1] == 't') {
            Vec2 uv;
            if (sscanf(line + 2, "%f %f", &uv.u, &uv.v) >= 2) {
                if (!grow((void **)&uvs, &ucap, uc + 1, sizeof(Vec2))) goto done;
                uvs[uc++] = uv;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            char *save = NULL, *tok;
            ObjVertex face[64];
            int count = 0, i, valid = 1;
            tok = strtok_r(line + 2, " \t\r\n", &save);
            while (tok) {
                int vi, ti, pi, ui;
                if (count >= 64 || !parse_face_ref(tok, &vi, &ti)) {
                    valid = 0;
                    break;
                }
                pi = obj_index(vi, (int)pc);
                ui = ti ? obj_index(ti, (int)uc) : -1;
                if (pi < 0 || pi >= (int)pc || (ti && (ui < 0 || ui >= (int)uc))) {
                    valid = 0;
                    break;
                }
                face[count].x = positions[pi].x;
                face[count].y = positions[pi].y;
                face[count].z = positions[pi].z;
                face[count].u = ui >= 0 ? uvs[ui].u : 0.0f;
                face[count].v = ui >= 0 ? 1.0f - uvs[ui].v : 0.0f;
                ++count;
                tok = strtok_r(NULL, " \t\r\n", &save);
            }
            if (valid && count >= 3) {
                for (i = 1; i + 1 < count; ++i) {
                    if (oc > (size_t)INT_MAX - 3 ||
                        !grow((void **)&out, &ocap, oc + 3, sizeof(ObjVertex)))
                        goto done;
                    out[oc++] = face[0];
                    out[oc++] = face[i];
                    out[oc++] = face[i + 1];
                }
            }
        }
    }
    if (oc) {
        model = (ObjModel *)calloc(1, sizeof(*model));
        if (!model) goto done;
        model->vertices = (ObjVertex *)memalign(16, oc * sizeof(ObjVertex));
        if (!model->vertices) { free(model); model = NULL; goto done; }
        memcpy(model->vertices, out, oc * sizeof(ObjVertex));
        model->vertexCount = (int)oc;
        sceKernelDcacheWritebackInvalidateAll();
    }
done:
    fclose(fp);
    free(positions); free(uvs); free(out);
    return model;
}

void objDraw(ObjModel *model) {
    if (!model || !model->vertices || model->vertexCount <= 0) return;
    sceGuDrawArray(GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        model->vertexCount, NULL, model->vertices);
}

void objFree(ObjModel *model) {
    if (!model) return;
    free(model->vertices);
    free(model);
}
