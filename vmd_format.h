#ifndef vmd_format_h_INCLUDED
#define vmd_format_h_INCLUDED

typedef struct {
    uint32_t vertexCount;
    uint32_t indexCount;

    float *vertices;
    uint32_t *indices;

    char vertexMask;

    // TODO: material information
    //char *texName;
} VmdData;

#define VMD_VERTEX_NORMAL_BIT   (1 << 0)
#define VMD_VERTEX_COLOR_BIT    (1 << 1)
#define VMD_VERTEX_TEXCOORD_BIT (1 << 2)


#endif // vmd_format_h_INCLUDED
