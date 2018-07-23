#ifndef JAN_SELECTION_H_
#define JAN_SELECTION_H_

void jan_create_selection(JanSelection* selection, Heap* heap);
void jan_destroy_selection(JanSelection* selection);
JanSelection jan_select_all(JanMesh* mesh, Heap* heap);
void jan_toggle_edge_in_selection(JanSelection* selection, JanEdge* edge);
void jan_toggle_face_in_selection(JanSelection* selection, JanFace* face);
void jan_toggle_vertex_in_selection(JanSelection* selection, JanVertex* vertex);
bool jan_edge_selected(JanSelection* selection, JanEdge* edge);
bool jan_face_selected(JanSelection* selection, JanFace* face);
bool jan_vertex_selected(JanSelection* selection, JanVertex* vertex);

#endif // JAN_SELECTION_H_
