/* -*- mode: C -*-  */
/* 
   IGraph library.
   Copyright (C) 2005  Gabor Csardi <csardi@rmki.kfki.hu>
   MTA RMKI, Konkoly-Thege Miklos st. 29-33, Budapest 1121, Hungary
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "igraph.h"
#include "types.h"

/**
 * \ingroup conversion
 * \function igraph_get_adjacency
 * \brief Returns the adjacency matrix of a graph
 * 
 * The result is an incidence matrix, it contains numbers greater
 * than one if there are multiple edges in the graph.
 * \param graph Pointer to the graph to convert
 * \param res Pointer to an initialized matrix object, it will be
 *        resized if needed.
 * \param type Constant giving the type of the adjacency matrix to
 *        create for undirected graphs. It is ignored for directed
 *        graphs. Possible values:
 *        \clist
 *        \cli IGRAPH_GET_ADJACENCY_UPPER 
 *          the upper right triangle of the matrix is used.
 *        \cli IGRAPH_GET_ADJACENCY_LOWER 
 *          the lower left triangle of the matrix is used.
 *        \cli IGRAPH_GET_ADJACENCY_BOTH 
 *          the whole matrix is used, a symmetric matrix is returned.
 *        \endclist
 * \return Error code:
 *        \c IGRAPH_EINVAL invalid type argument.
 *
 * Time complexity: O(|V||V|),
 * |V| is the 
 * number of vertices in the graph.
 */

int igraph_get_adjacency(const igraph_t *graph, igraph_matrix_t *res,
			 igraph_get_adjacency_t type) {
  
  igraph_es_t edgeit;
  long int no_of_nodes=igraph_vcount(graph);
  bool_t directed=igraph_is_directed(graph);
  int retval=0;
  long int from, to;
  
  IGRAPH_CHECK(igraph_matrix_resize(res, no_of_nodes, no_of_nodes));
  igraph_matrix_null(res);
  IGRAPH_CHECK(igraph_es_all(graph, &edgeit));
  IGRAPH_FINALLY(igraph_es_destroy, &edgeit);
  
  if (directed) {
    while (!igraph_es_end(graph, &edgeit)) {
      from=igraph_es_from(graph, &edgeit);
      to=igraph_es_to(graph, &edgeit);
      MATRIX(*res, from, to) += 1;
      igraph_es_next(graph, &edgeit);
    }
  } else if (type==IGRAPH_GET_ADJACENCY_UPPER) {
    while (!igraph_es_end(graph, &edgeit)) {  
      from=igraph_es_from(graph, &edgeit);
      to=igraph_es_to(graph, &edgeit);
      if (to < from) {
	MATRIX(*res, to, from) += 1;
      } else {
	MATRIX(*res, from, to) += 1;    
      }
      igraph_es_next(graph, &edgeit);
    }
  } else if (type==IGRAPH_GET_ADJACENCY_LOWER) {
    while (!igraph_es_end(graph, &edgeit)) {
      from=igraph_es_from(graph, &edgeit);
      to=igraph_es_to(graph, &edgeit);
      if (to < from) {
	MATRIX(*res, from, to) += 1;
      } else {
	MATRIX(*res, to, from) += 1;
      }
      igraph_es_next(graph, &edgeit);
    }
  } else if (type==IGRAPH_GET_ADJACENCY_BOTH) {
    while (!igraph_es_end(graph, &edgeit)) {
      from=igraph_es_from(graph, &edgeit);
      to=igraph_es_to(graph, &edgeit);
      MATRIX(*res, from, to) += 1;
      if (from != to) {
	MATRIX(*res, to, from) += 1;
      }
      igraph_es_next(graph, &edgeit);
    }
  } else {
    IGRAPH_ERROR("Invalid type argument", IGRAPH_EINVAL);
  }

  igraph_es_destroy(&edgeit);
  IGRAPH_FINALLY_CLEAN(1);
  return retval;
}

/**
 * \ingroup conversion
 * \function igraph_get_edgelist
 * \brief Returns the list of edges in a graph
 * 
 * \param graph Pointer to the graph object
 * \param res Pointer to an initialized vector object, it will be
 *        resized.
 * \param bycol Logical, if true, the edges will be returned
 *        columnwise, eg. the first edge is
 *        <code>res[0]->res[|E|]</code>, the second is
 *        <code>res[1]->res[|E|+1]</code>, etc.
 * \return Error code.
 * 
 * Time complexity: O(|E|), the
 * number of edges in the graph.
 */

int igraph_get_edgelist(const igraph_t *graph, igraph_vector_t *res, bool_t bycol) {

  igraph_es_t edgeit;
  long int no_of_edges=igraph_ecount(graph);
  long int vptr=0;
  
  IGRAPH_CHECK(igraph_vector_resize(res, no_of_edges*2));
  IGRAPH_CHECK(igraph_es_all(graph, &edgeit));
  IGRAPH_FINALLY(igraph_es_destroy, &edgeit);
  
  if (bycol) {
    while (!igraph_es_end(graph, &edgeit)) {
      VECTOR(*res)[vptr]=igraph_es_from(graph, &edgeit);
      VECTOR(*res)[vptr+no_of_edges]=igraph_es_to(graph, &edgeit);
      vptr++;
      igraph_es_next(graph, &edgeit);
    }
  } else {
    while (!igraph_es_end(graph, &edgeit)) {
      VECTOR(*res)[vptr++]=igraph_es_from(graph, &edgeit);
      VECTOR(*res)[vptr++]=igraph_es_to(graph, &edgeit);
      igraph_es_next(graph, &edgeit);
    }
  }
  
  igraph_es_destroy(&edgeit);
  IGRAPH_FINALLY_CLEAN(1);
  return 0;
}
