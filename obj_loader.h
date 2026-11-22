#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

typedef struct ObjModel ObjModel;

ObjModel *objLoad(const char *objPath);
void objDraw(ObjModel *model);
void objFree(ObjModel *model);

#endif
