#ifndef JAN_TRIANGULATE_H_
#define JAN_TRIANGULATE_H_

typedef struct Pointcloud
{
    PointVertex* vertices;
    uint16_t* indices;
} Pointcloud;

typedef struct PointcloudSpec
{
    Float4 colour;
    Float4 hover_colour;
    Float4 select_colour;
    JanVertex* hovered;
    JanSelection* selection;
} PointcloudSpec;

typedef struct Triangulation
{
    VertexPNC* vertices;
    uint16_t* indices;
} Triangulation;

typedef struct Wireframe
{
    LineVertex* vertices;
    uint16_t* indices;
} Wireframe;

typedef struct WireframeSpec
{
    Float4 colour;
    Float4 hover_colour;
    Float4 select_colour;
    JanEdge* hovered;
    JanSelection* selection;
} WireframeSpec;

Pointcloud jan_make_pointcloud(JanMesh* mesh, Heap* heap, PointcloudSpec* spec);
Wireframe jan_make_wireframe(JanMesh* mesh, Heap* heap, WireframeSpec* spec);
Triangulation jan_triangulate(JanMesh* mesh, Heap* heap);
Triangulation jan_triangulate_selection(JanMesh* mesh, JanSelection* selection, Heap* heap);

#endif // JAN_TRIANGULATE_H_
