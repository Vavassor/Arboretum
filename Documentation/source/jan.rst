Jan
====

.. c:type:: JanBorder

    A border corresponds to a boundary edge. So, either an outer edge of a face
    or an edge of a hole in a face.

.. c:type:: JanEdge

    An edge in a :c:type:`JanMesh`.

.. c:type:: JanFace

    A face in a :c:type:`JanMesh`.

.. c:type:: JanLink

    Links are also known as half-edges. These have the additional responsibility
    of storing vertex attributes that are specific to a face.

.. c:type:: JanMesh

    An editable polygon mesh.

.. c:type:: JanPart

    A reference to one edge, face, or vertex within a :c:type:`JanSelection`.

.. c:type:: JanSelection

    A selection is a collection of edges, faces, or vertices within a single
    mesh.

.. c:type:: JanSelectionType

    The type of a :c:type:`JanSelection`.

.. c:type:: JanSpoke

    Spokes are for navigating edges that all meet at the same vertex "hub".

.. c:type:: JanVertex

    A vertex in a :c:type:`JanMesh`.

.. c:function:: void jan_add_and_link_border(JanMesh* mesh, JanFace* face, \
        JanVertex** vertices, JanEdge** edges, int edges_count)

    Add a hole to a face.

    :param mesh: the mesh
    :param face: the face to add the hole to
    :param vertices: an array of vertices for the hole
    :param edges: an array of edges for the hole
    :param edges_count: the number of edges

.. c:function:: JanEdge* jan_add_edge(JanMesh* mesh, JanVertex* start, \
        JanVertex* end)

    Add an edge between two vertices.

    :param mesh: the mesh
    :param start: the first vertex in the edge
    :param end: the second vertex in the edge
    :return: the edge added

.. c:function:: JanFace* jan_add_face(JanMesh* mesh, JanVertex** vertices, \
        JanEdge** edges, int edges_count)

    Add a face between edges and vertices that are already connected.

    :param mesh: the mesh
    :param vertices: an array of vertices for the face
    :param edges: an array of edges for the face
    :param edges_count: the number of edges
    :return: the face added

.. c:function:: JanVertex* jan_add_vertex(JanMesh* mesh, Float3 position)

    Add an isolated vertex to the mesh.
    
    :param mesh: the mesh
    :param position: the vertex's position
    :return: the vertex added

.. c:function:: JanFace* jan_connect_disconnected_vertices_and_add_face( \
        JanMesh* mesh, JanVertex** vertices, int vertices_count, Stack* stack)

    Add a face, first connecting the vertices with edges.

    :param mesh: the mesh
    :param vertices: an array of vertices to connect
    :param vertices_count: the number of vertices
    :param stack: needed for temporary memory

.. c:function:: void jan_create_mesh(JanMesh* mesh)

    Create a mesh.

    :param mesh: the mesh

.. c:function:: void jan_destroy_mesh(JanMesh* mesh)

    Destroy a mesh.

    :param mesh: the mesh

.. c:function:: void jan_remove_vertex(JanMesh* mesh, JanVertex* vertex)

    Remove a vertex, and destroy connected edges and faces.

    :param mesh: the mesh
    :param vertex: the vertex to remove

.. c:function:: void jan_remove_edge(JanMesh* mesh, JanEdge* edge)

    Remove an edge and its component vertices. Also destroy any connected edges
    and faces.

    :param mesh: the mesh
    :param edge: the edge to remove

.. c:function:: void jan_remove_face(JanMesh* mesh, JanFace* face)

    Remove a face, but leave the edges and vertices that make up its borders.

    :param mesh: the mesh
    :param face: the face to remove

.. c:function:: void jan_remove_face_and_its_unlinked_edges_and_vertices( \
        JanMesh* mesh, JanFace* face)

    Remove a face and any edges and vertices on its borders that are unlinked
    to any other face.

    :param mesh: the mesh
    :param face: the face to remove

.. c:function:: void jan_update_normals(JanMesh* mesh)

    Update the face normals for the whole mesh.

    :param mesh: the mesh

