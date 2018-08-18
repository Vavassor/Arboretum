#ifndef JAN_INTERNAL_H_
#define JAN_INTERNAL_H_

int jan_count_border_edges(JanBorder* border);
int jan_count_face_borders(JanFace* face);
bool jan_edge_contains_vertex(JanEdge* edge, JanVertex* vertex);
JanSpoke* jan_get_spoke(JanEdge* edge, JanVertex* vertex);

#endif // JAN_INTERNAL_H_
