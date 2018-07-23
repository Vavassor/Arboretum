#ifndef JAN_TRIANGULATE_H_
#define JAN_TRIANGULATE_H_

void jan_make_pointcloud(JanMesh* mesh, Heap* heap, Float4 colour, PointVertex** vertices, uint16_t** indices);
void jan_make_pointcloud_selection(JanMesh* mesh, Float4 colour, JanVertex* hovered, Float4 hover_colour, JanSelection* selection, Float4 select_colour, Heap* heap, PointVertex** vertices, uint16_t** indices);
void jan_make_wireframe(JanMesh* mesh, Heap* heap, Float4 colour, LineVertex** vertices, uint16_t** indices);
void jan_make_wireframe_selection(JanMesh* mesh, Heap* heap, Float4 colour, JanEdge* hovered, Float4 hover_colour, JanSelection* selection, Float4 select_colour, LineVertex** vertices, uint16_t** indices);
void jan_triangulate(JanMesh* mesh, Heap* heap, VertexPNC** vertices, uint16_t** indices);
void jan_triangulate_selection(JanMesh* mesh, JanSelection* selection, Heap* heap, VertexPNC** vertices, uint16_t** indices);

#endif // JAN_TRIANGULATE_H_
