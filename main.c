#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include <tinyobj_loader_c.h>

#define VMD_LOADER_IMPLEMENTATION
#include "vmd_loader.h"

char *getFileData(const char *filename, size_t *length)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Error opening %s: %s", filename, strerror(errno));
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) == -1) {
        fprintf(stderr, "Error seeking the end of %s: %s", filename, strerror(errno));
        fclose(fp);
        return NULL;
    }

    long fileBytes = ftell(fp);
    if (fileBytes == -1) {
        fprintf(stderr, "Error getting length of %s: %s", filename, strerror(errno));
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    char *data = malloc(fileBytes);
    *length = fread(data, 1, fileBytes, fp);

    if (*length < fileBytes) {
        fprintf(stderr, "Error reading file %s", filename);
        fclose(fp);
        return NULL;
    }

    fclose(fp);

    return data;
}

const char *getFileExt(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

void loadObj(VmdData *model, char *data, size_t dataLen)
{
    tinyobj_attrib_t    attrib;
    tinyobj_shape_t    *shapes;
    size_t              numShapes;
    tinyobj_material_t *materials;
    size_t              numMaterials;

    unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
    int err = tinyobj_parse_obj(&attrib, &shapes, &numShapes, &materials, &numMaterials, data, dataLen, flags);
    if (err != TINYOBJ_SUCCESS) {
        printf("Unable to parse model\n");
        exit(3);
    }

    model->vertexMask = 0;
    if (attrib.num_normals > 0)
        model->vertexMask |= VMD_VERTEX_NORMAL_BIT;
    if (attrib.num_texcoords > 0)
        model->vertexMask |= VMD_VERTEX_TEXCOORD_BIT;

    size_t vertSize = vmdVertexSize(model);
    size_t vertFloats = vmdVertexSize(model) / sizeof(float);

    // Initial upper bounds allocation
    model->vertices = malloc(attrib.num_faces * vertSize);
    model->indices  = malloc(attrib.num_faces * sizeof(uint32_t));

    typedef struct {
        uint32_t v;
        uint32_t vn;
        uint32_t vt;
    } ObjVertex;

    ObjVertex *objVerts = malloc(attrib.num_faces * sizeof(ObjVertex));

    model->vertexCount = 0;
    model->indexCount  = 0;

    ObjVertex max = {
        .v = 0,
        .vn = 0,
        .vt = 0
    };

    // TODO: this only loads the first shape, maybe fix?
    for (size_t i = 0; i < shapes[0].length; i++) {
        for (size_t j = 0; j < attrib.face_num_verts[i + shapes[0].face_offset]; j++) {
            size_t vertIdx = 3 * i + j;
            tinyobj_vertex_index_t v = attrib.faces[vertIdx];

            // Vertex deduplication
            bool found = false;
            if (v.v_idx <= max.v && v.vn_idx <= max.vn && v.vt_idx <= max.vt) {
                for (size_t k = model->vertexCount; k > 0; k--) {
                    if (objVerts[k - 1].v == v.v_idx
                        && objVerts[k - 1].vn == v.vn_idx
                        && objVerts[k - 1].vt == v.vt_idx) {
                        model->indices[vertIdx] = k - 1;
                        found = true;
                        break;
                    }
                }
            } else {
                if (v.v_idx > max.v)
                    max.v = v.v_idx;
                if (v.vn_idx > max.vn)
                    max.vn = v.vn_idx;
                if (v.vt_idx > max.vt)
                    max.vt = v.vt_idx;
            }

            if (!found) {
                size_t offset = model->vertexCount * vertFloats;

                model->vertices[offset + 0] = -attrib.vertices[3 * v.v_idx + 0];
                model->vertices[offset + 1] =  attrib.vertices[3 * v.v_idx + 2];
                model->vertices[offset + 2] =  attrib.vertices[3 * v.v_idx + 1];
                offset += 3;

                if (model->vertexMask & VMD_VERTEX_NORMAL_BIT) {
                    model->vertices[offset + 0] = -attrib.normals[3 * v.vn_idx + 0];
                    model->vertices[offset + 1] =  attrib.normals[3 * v.vn_idx + 2];
                    model->vertices[offset + 2] =  attrib.normals[3 * v.vn_idx + 1];
                    offset += 3;
                }

                if (model->vertexMask & VMD_VERTEX_TEXCOORD_BIT) {
                    model->vertices[offset + 0] =        attrib.texcoords[2 * v.vt_idx + 0];
                    model->vertices[offset + 1] = 1.0f - attrib.texcoords[2 * v.vt_idx + 1];
                }

                objVerts[model->vertexCount].v = v.v_idx;
                objVerts[model->vertexCount].vn = v.vn_idx;
                objVerts[model->vertexCount].vt = v.vt_idx;
                model->indices[vertIdx]  = model->vertexCount;
                model->vertexCount++;
            }
            model->indexCount++;
        }
    }

    free(objVerts);

    model->vertices = realloc(model->vertices, model->vertexCount * vertSize);

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, numShapes);
    tinyobj_materials_free(materials, numMaterials);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s model\n", argv[0]);
        return 1;
    }

    size_t dataLen;
    char *data = getFileData(argv[1], &dataLen);

    VmdData model;

    const char *fileExt = getFileExt(argv[1]);
    if (strcmp(fileExt, "obj") == 0)
        loadObj(&model, data, dataLen);
    else {
        printf("Unsupported model format: %s\n", fileExt);
        return 2;
    }

    free(data);

    printf("Vertices: %d, Indices: %d\n", model.vertexCount, model.indexCount);

    char *filename = basename(argv[1]);
    const char *extLoc = getFileExt(filename);
    size_t nameLen = extLoc - filename;

    char vmdName[nameLen + 4];
    strncpy(vmdName, filename, nameLen - 1);
    strncpy(vmdName + nameLen - 1, ".vmd\0", 5);

    saveVmd(vmdName, &model);
}
