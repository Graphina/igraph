/* -*- mode: C -*-  */
/* 
   IGraph R library.
   Copyright (C) 2003, 2004  Gabor Csardi <csardi@rmki.kfki.hu>
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
#include "random.h"
#include "memory.h"

#include <math.h>

/**
 * \section about_games
 * 
 * <para>Games are randomized graph generators. Randomization means that
 * they generate a different graph every time you call them. </para>
 */

/**
 * \ingroup generators
 * \function igraph_barabasi_game
 * \brief Generates a graph based on the Barab&aacute;si-Albert model.
 *
 * \param graph An uninitialized graph object.
 * \param n The number of vertices in the graph.
 * \param m The number of outgoing edges generated for each 
 *        vertex. (Only if \p outseq is \c NULL.) 
 * \param outseq Gives the (out-)degrees of the vertices. If this is
 *        constant, this can be a NULL pointer or an empty (but
 *        initialized!) vector, in this case \p m contains
 *        the constant out-degree. The very first vertex has by definition 
 *        no outgoing edges, so the first number in this vector is 
 *        ignored.
 * \param outpref Boolean, if true not only the in- but also the out-degree
 *        of a vertex increases its citation probability. Ie. the
 *        citation probability is determined by the total degree of
 *        the vertices.
 * \param directed Boolean, whether to generate a directed graph.
 * \return Error code:
 *         \c IGRAPH_EINVAL: invalid \p n,
 *         \p m or \p outseq parameter.
 * 
 * Time complexity: O(|V|+|E|), the
 * number of vertices plus the number of edges.
 */

int igraph_barabasi_game(igraph_t *graph, integer_t n, integer_t m, 
			 const igraph_vector_t *outseq, bool_t outpref, 
			 bool_t directed) {

  long int no_of_nodes=n;
  long int no_of_neighbors=m;
  long int *bag;
  long int bagp;
  long int no_of_edges;
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;
  
  long int resp=0;

  long int i,j;

  if (n<0) {
    IGRAPH_ERROR("Invalid number of vertices", IGRAPH_EINVAL);
  }
  if (outseq != 0 && igraph_vector_size(outseq) != 0 && igraph_vector_size(outseq) != n) {
    IGRAPH_ERROR("Invalid out degree sequence length", IGRAPH_EINVAL);
  }
  if ( (outseq == 0 || igraph_vector_size(outseq) == 0) && m<0) {
    IGRAPH_ERROR("Invalid out degree", IGRAPH_EINVAL);
  }

  if (outseq==0 || igraph_vector_size(outseq) == 0) {
    no_of_neighbors=m;
    bag=Calloc(no_of_nodes * no_of_neighbors + no_of_nodes +
	       outpref * no_of_nodes * no_of_neighbors,
	       long int);
    no_of_edges=(no_of_nodes-1)*no_of_neighbors;
  } else {
    no_of_edges=0;
    for (i=1; i<igraph_vector_size(outseq); i++) {
      no_of_edges+=VECTOR(*outseq)[i];
    }
    bag=Calloc(no_of_nodes + no_of_edges + outpref * no_of_edges,
	       long int);
  }
  
  if (bag==0) {
    IGRAPH_ERROR("barabasi_game failed", IGRAPH_ENOMEM);
  }
  IGRAPH_FINALLY(free, bag); 	/* TODO: hack */
  IGRAPH_VECTOR_INIT_FINALLY(&edges, no_of_edges*2);

  /* The first node */

  bagp=0;
  bag[bagp++]=0;
  
  RNG_BEGIN();

  /* and the others */
  
  for (i=1; i<no_of_nodes; i++) {
    /* draw edges */
    if (outseq != 0 && igraph_vector_size(outseq)!=0) { no_of_neighbors=VECTOR(*outseq)[i]; }
    for (j=0; j<no_of_neighbors; j++) {
      long int to=bag[RNG_INTEGER(0, bagp-1)];
      VECTOR(edges)[resp++] = i;
      VECTOR(edges)[resp++] = to;
    }
    /* update bag */
    bag[bagp++] = i;
    for (j=0; j<no_of_neighbors; j++) {
      bag[bagp++] = VECTOR(edges)[resp-2*j-1];
      if (outpref) {
	bag[bagp++] = i;
      }
    }
  }

  RNG_END();

  Free(bag);
  IGRAPH_CHECK(igraph_create(graph, &edges, 0, directed));
  igraph_vector_destroy(&edges);
  IGRAPH_FINALLY_CLEAN(2);

  return 0;
}

/**
 * \ingroup internal
 */

int igraph_erdos_renyi_game_gnp(igraph_t *graph, integer_t n, real_t p,
				bool_t directed, bool_t loops) {

  long int no_of_nodes=n;
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;
  igraph_vector_t s=IGRAPH_VECTOR_NULL;
  int retval=0;

  if (n<0) {
    IGRAPH_ERROR("Invalid number of vertices", IGRAPH_EINVAL);
  }
  if (p<0.0 || p>1.0) {
    IGRAPH_ERROR("Invalid probability given", IGRAPH_EINVAL);
  }
  
  if (p==0.0 || no_of_nodes<=1) {
    IGRAPH_CHECK(retval=igraph_empty(graph, n, directed));
  } else if (p==1.0) { 
    IGRAPH_CHECK(retval=igraph_full(graph, n, directed, loops));
  } else {

    long int i;
    double maxedges;
    if (directed && loops) 
      { maxedges = n * n; }
    else if (directed && !loops)
      { maxedges = n * (n-1); }
    else if (!directed && loops) 
      { maxedges = n * (n+1)/2; }
    else 
      { maxedges = n * (n-1)/2; }
    
    IGRAPH_VECTOR_INIT_FINALLY(&s, 0);
    IGRAPH_CHECK(igraph_vector_reserve(&s, maxedges*p*1.1));

    RNG_BEGIN();

    IGRAPH_CHECK(igraph_vector_push_back(&s, RNG_GEOM(p)+1));
    while (igraph_vector_tail(&s) < maxedges) {
      IGRAPH_CHECK(igraph_vector_push_back(&s, igraph_vector_tail(&s)+RNG_GEOM(p)+1));
    }
    if (igraph_vector_tail(&s) > maxedges+1) {
      igraph_vector_pop_back(&s);
    }

    RNG_END();

    IGRAPH_VECTOR_INIT_FINALLY(&edges, 0);
    IGRAPH_CHECK(igraph_vector_reserve(&edges, igraph_vector_size(&s)*2));

    if (directed && loops) {
      for (i=0; i<igraph_vector_size(&s); i++) {
	igraph_vector_push_back(&edges, ((long int)(VECTOR(s)[i]-1))/no_of_nodes);
	igraph_vector_push_back(&edges, ((long int)(VECTOR(s)[i]-1))%no_of_nodes);
      }
    } else if (directed && !loops) {
      for (i=0; i<igraph_vector_size(&s); i++) {
	long int from=((long int)(VECTOR(s)[i]-1))/(no_of_nodes-1);
	long int to=((long int)VECTOR(s)[i])%(no_of_nodes-1);
	if (from==to) {
	  to=no_of_nodes-1;
	}
	igraph_vector_push_back(&edges, from);
	igraph_vector_push_back(&edges, to);
      }
    } else if (!directed && loops) {
      for (i=0; i<igraph_vector_size(&s); i++) {
	real_t from=ceil((sqrt(8*(VECTOR(s)[i])+1)-1)/2);
	igraph_vector_push_back(&edges, from-1);
	igraph_vector_push_back(&edges, VECTOR(s)[i]-from*(from-1)/2-1);
      }
    } else {
      for (i=0; i<igraph_vector_size(&s); i++) {
	real_t from=ceil((sqrt(8*VECTOR(s)[i]+1)-1)/2)+1;
	igraph_vector_push_back(&edges, from-1);
	igraph_vector_push_back(&edges, VECTOR(s)[i]-(from-1)*(from-2)/2-1);
      }
    }      

    igraph_vector_destroy(&s);
    IGRAPH_FINALLY_CLEAN(1);
    IGRAPH_CHECK(retval=igraph_create(graph, &edges, n, directed));
    igraph_vector_destroy(&edges);
    IGRAPH_FINALLY_CLEAN(1);
  }

  return retval;
}

int igraph_erdos_renyi_game_gnm(igraph_t *graph, integer_t n, real_t m,
				bool_t directed, bool_t loops) {

  long int no_of_nodes=n;
  long int no_of_edges=m;
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;
  igraph_vector_t s=IGRAPH_VECTOR_NULL;
  int retval=0;

  if (n<0) {
    IGRAPH_ERROR("Invalid number of vertices", IGRAPH_EINVAL);
  }
  if (m<0) {
    IGRAPH_ERROR("Invalid number of edges", IGRAPH_EINVAL);
  }
  
  if (m==0.0 || no_of_nodes<=1) {
    IGRAPH_CHECK(retval=igraph_empty(graph, n, directed));
  } else {

    long int i;    
    double maxedges;
    if (directed && loops) 
      { maxedges = n * n; }
    else if (directed && !loops)
      { maxedges = n * (n-1); }
    else if (!directed && loops) 
      { maxedges = n * (n+1)/2; }
    else 
      { maxedges = n * (n-1)/2; }
    
    if (no_of_edges > maxedges) {
      IGRAPH_ERROR("Invalid number (too large) of edges", IGRAPH_EINVAL);
    }
    
    if (maxedges == no_of_edges) {
      retval=igraph_full(graph, n, directed, loops);
    } else {
    
      IGRAPH_VECTOR_INIT_FINALLY(&s, 0);
      IGRAPH_CHECK(igraph_random_sample(&s, 1, maxedges, no_of_edges));
      
      IGRAPH_VECTOR_INIT_FINALLY(&edges, 0);
      IGRAPH_CHECK(igraph_vector_reserve(&edges, igraph_vector_size(&s)*2));
      
      if (directed && loops) {
	for (i=0; i<igraph_vector_size(&s); i++) {
	  igraph_vector_push_back(&edges, ((long int)(VECTOR(s)[i]-1))/no_of_nodes);
	  igraph_vector_push_back(&edges, ((long int)(VECTOR(s)[i]-1))%no_of_nodes);
	}
      } else if (directed && !loops) {
	for (i=0; i<igraph_vector_size(&s); i++) {
	  long int from=((long int)(VECTOR(s)[i]-1))/(no_of_nodes-1);
	  long int to=((long int)VECTOR(s)[i])%(no_of_nodes-1);
	  if (from==to) {
	    to=no_of_nodes-1;
	  }
	  igraph_vector_push_back(&edges, from);
	  igraph_vector_push_back(&edges, to);
	}
      } else if (!directed && loops) {
	for (i=0; i<igraph_vector_size(&s); i++) {
	  real_t from=ceil((sqrt(8*(VECTOR(s)[i])+1)-1)/2);
	  igraph_vector_push_back(&edges, from-1);
	  igraph_vector_push_back(&edges, VECTOR(s)[i]-from*(from-1)/2-1);
	}
      } else {
	for (i=0; i<igraph_vector_size(&s); i++) {
	  real_t from=ceil((sqrt(8*VECTOR(s)[i]+1)-1)/2)+1;
	  igraph_vector_push_back(&edges, from-1);
	  igraph_vector_push_back(&edges, VECTOR(s)[i]-(from-1)*(from-2)/2-1);
	}
      }  

      igraph_vector_destroy(&s);
      retval=igraph_create(graph, &edges, n, directed);
      igraph_vector_destroy(&edges);
      IGRAPH_FINALLY_CLEAN(2);
    }
  }
  
  return retval;
}

/**
 * \ingroup generators
 * \function igraph_erdos_renyi_game
 * \brief Generates a random (Erdos-Renyi) graph.
 * 
 * \param graph Pointer to an uninitialized graph object.
 * \param type The type of the random graph, possible values:
 *        \clist
 *        \cli IGRAPH_ERDOS_RENYI_GNM
 *          G(n,m) graph,  
 *          m edges are
 *          selected uniformly randomly in a graph with
 *          n vertices.
 *        \cli IGRAPH_ERDOS_RENYI_GNP
 *          G(n,p) graph,
 *          every possible edge is included in the graph with
 *          probability p.
 *        \endclist
 * \param n The number of vertices in the graph.
 * \param p_or_m This is the p parameter for
 *        G(n,p) graphs and the
 *        m 
 *        parameter for G(n,m) graphs.
 * \param directed Logical, whether to generate a directed graph.
 * \param loops Logical, whether to generate loops (self) edges.
 * \return Error code:
 *         \c IGRAPH_EINVAL: invalid
 *         \p type, \p n,
 *         \p p or \p m
 *          parameter.
 *         \c IGRAPH_ENOMEM: there is not enought
 *         memory for the operation.
 * 
 * Time complexity: O(|V|+|E|), the
 * number of vertices plus the number of edges in the graph.
 * 
 * \sa \ref igraph_barabasi_game(), \ref igraph_growing_random_game()
 */

int igraph_erdos_renyi_game(igraph_t *graph, igraph_erdos_renyi_t type,
			    integer_t n, real_t p_or_m,
			    bool_t directed, bool_t loops) {
  int retval=0;
  if (type == IGRAPH_ERDOS_RENYI_GNP) {
    retval=igraph_erdos_renyi_game_gnp(graph, n, p_or_m, directed, loops);
  } else if (type == IGRAPH_ERDOS_RENYI_GNM) {
    retval=igraph_erdos_renyi_game_gnm(graph, n, p_or_m, directed, loops);
  } else {
    IGRAPH_ERROR("Invalid type", IGRAPH_EINVAL);
  }
  
  return retval;
}

int igraph_degree_sequence_game_simple(igraph_t *graph, 
				       const igraph_vector_t *out_seq, 
				       const igraph_vector_t *in_seq) {

  long int outsum=0, insum=0;
  bool_t directed=(in_seq != 0 && igraph_vector_size(in_seq)!=0);
  long int no_of_nodes, no_of_edges;
  long int *bag1=0, *bag2=0;
  long int bagp1=0, bagp2=0;
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;
  long int i,j;

  if (igraph_vector_any_smaller(out_seq, 0)) {
    IGRAPH_ERROR("Negative out degree", IGRAPH_EINVAL);
  }
  if (directed && igraph_vector_any_smaller(in_seq, 0)) {
    IGRAPH_ERROR("Negative in degree", IGRAPH_EINVAL);
  }
  if (directed && 
      igraph_vector_size(out_seq) != igraph_vector_size(in_seq)) { 
    IGRAPH_ERROR("Length of `out_seq' and `in_seq' differ for directed graph",
		 IGRAPH_EINVAL);
  }
  
  outsum=igraph_vector_sum(out_seq);
  insum=igraph_vector_sum(in_seq);
  
  if (!directed && outsum % 2 != 0) {
    IGRAPH_ERROR("Total degree not even for undirected graph", IGRAPH_EINVAL);
  }
  
  if (directed && outsum != insum) {
    IGRAPH_ERROR("Total in-degree and out-degree differ for directed graph",
		  IGRAPH_EINVAL);
  }

  no_of_nodes=igraph_vector_size(out_seq);
  if (directed) {
    no_of_edges=outsum;
  } else {
    no_of_edges=outsum/2;
  }

  bag1=Calloc(outsum, long int);
  if (bag1==0) {
    IGRAPH_ERROR("degree sequence game (simple)", IGRAPH_ENOMEM);
  }
  IGRAPH_FINALLY(free, bag1); 	/* TODO: hack */
    
  for (i=0; i<no_of_nodes; i++) {
    for (j=0; j<VECTOR(*out_seq)[i]; j++) {
      bag1[bagp1++]=i;
    }
  }
  if (directed) {
    bag2=Calloc(insum, long int);
    if (bag2==0) {
      IGRAPH_ERROR("degree sequence game (simple)", IGRAPH_ENOMEM);
    }
    IGRAPH_FINALLY(free, bag2);
    for (i=0; i<no_of_nodes; i++) {
      for (j=0; j<VECTOR(*in_seq)[i]; j++) {
	bag2[bagp2++]=i;
      }
    }
  }

  IGRAPH_VECTOR_INIT_FINALLY(&edges, 0);
  IGRAPH_CHECK(igraph_vector_reserve(&edges, no_of_edges));

  RNG_BEGIN();

  if (directed) {
    for (i=0; i<no_of_edges; i++) {
      long int from=RNG_INTEGER(0, bagp1-1);
      long int to=RNG_INTEGER(0, bagp2-1);
      igraph_vector_push_back(&edges, bag1[from]); /* safe, already reserved */
      igraph_vector_push_back(&edges, bag2[to]);   /* detto */
      bag1[from]=bag1[bagp1-1];
      bag2[to]=bag2[bagp2-1];
      bagp1--; bagp2--;
    }
  } else {
    for (i=0; i<no_of_edges; i++) {
      long int from=RNG_INTEGER(0, bagp1-1);
      long int to;
      igraph_vector_push_back(&edges, bag1[from]); /* safe, already reserved */
      bag1[from]=bag1[bagp1-1];
      bagp1--;
      to=RNG_INTEGER(0, bagp1-1);
      igraph_vector_push_back(&edges, bag1[to]);   /* detto */
      bag1[to]=bag1[bagp1-1];
      bagp1--;
    }
  }
  
  RNG_END();

  Free(bag1);
  IGRAPH_FINALLY_CLEAN(1);
  if (directed) {
    Free(bag2);
    IGRAPH_FINALLY_CLEAN(1);
  }

  IGRAPH_CHECK(igraph_create(graph, &edges, no_of_nodes, directed));
  igraph_vector_destroy(&edges);
  IGRAPH_FINALLY_CLEAN(1);
  
  return 0;
}

/**
 * \ingroup generators
 * \function igraph_degree_sequence_game
 * \brief Generates a random graph with a given degree sequence 
 * 
 * \param graph Pointer to an uninitialized graph object.
 * \param out_deg The degree sequence for an undirected graph (if
 *        \p in_seq is of length zero), or the out-degree
 *        sequence of a directed graph (if \p in_deq is not
 *        of length zero.
 * \param in_deg It is either a zero-length vector or
 *        \c NULL (if an undirected 
 *        graph is generated), or the in-degree sequence.
 * \param method The method to generate the graph. Possible values: 
 *        \c IGRAPH_DEGSEQ_SIMPLE, for undirected graphs this
 *        method puts all vertex ids in a bag, the multiplicity of a
 *        vertex in the bag is the same as its degree. Then it 
 *        draws pairs from the bag, until it is empty. This method can 
 *        generate both loop (self) edges and multiple edges.
 *        For directed graphs, the algorithm is basically the same,
 *        but two separate bags are used for the in- and out-degrees. 
 * \return Error code: 
 *          \c IGRAPH_ENOMEM: there is not enough
 *           memory to perform the operation.
 *          \c IGRAPH_EINVAL: invalid method parameter, or
 *           invalid in- and/or out-degree vectors. The degree vectors
 *           should be non-negative, \p out_deg should sum
 *           up to an even integer for undirected graphs; the length
 *           and sum of \p out_deg and
 *           \p in_deg 
 *           should match for directed graphs.
 * 
 * Time complexity: O(|V|+|E|), the
 * number of vertices plus the number of edges.
 * 
 * \sa \ref igraph_barabasi_game(), \ref igraph_erdos_renyi_game()
 */

int igraph_degree_sequence_game(igraph_t *graph, const igraph_vector_t *out_deg,
				const igraph_vector_t *in_deg, 
				igraph_degseq_t method) {

  int retval;

  if (method==IGRAPH_DEGSEQ_SIMPLE) {
    retval=igraph_degree_sequence_game_simple(graph, out_deg, in_deg);
  } else {
    IGRAPH_ERROR("Invalid degree sequence game method", IGRAPH_EINVAL);
  }

  return retval;
}

/**
 * \ingroup generators
 * \function igraph_growing_random_game
 * \brief Generates a growing random graph.
 *
 * This function simulates a growing random graph. In each discrete
 * time step a new vertex is added and a number of new edges are also
 * added. These graphs are known to be different from standard (not
 * growing) random graphs.
 * \param graph Uninitialized graph object.
 * \param n The number of vertices in the graph.
 * \param m The number of edges to add in a time step (ie. after
 *        adding a vertex).
 * \param directed Boolean, whether to generate a directed graph.
 * \param citation Boolean, if \c TRUE, the edges always
 *        originate from the most recently added vertex.
 * \return Error code:
 *          \c IGRAPH_EINVAL: invalid
 *          \p n or \p m
 *          parameter. 
 *
 * Time complexity: O(|V|+|E|), the
 * number of vertices plus the number of edges.
 */
int igraph_growing_random_game(igraph_t *graph, integer_t n, 
			       integer_t m, bool_t directed,
			       bool_t citation) {

  long int no_of_nodes=n;
  long int no_of_neighbors=m;
  long int no_of_edges;
  igraph_vector_t edges=IGRAPH_VECTOR_NULL;
  
  long int resp=0;

  long int i,j;

  if (n<0) {
    IGRAPH_ERROR("Invalid number of vertices", IGRAPH_EINVAL);
  }
  if (m<0) {
    IGRAPH_ERROR("Invalid number of edges per step (m)", IGRAPH_EINVAL);
  }

  no_of_edges=(no_of_nodes-1) * no_of_neighbors;

  IGRAPH_VECTOR_INIT_FINALLY(&edges, no_of_edges*2);  

  RNG_BEGIN();

  for (i=1; i<no_of_nodes; i++) {
    for (j=0; j<no_of_neighbors; j++) {
      if (citation) {
	long int to=RNG_INTEGER(0, i-1);
	VECTOR(edges)[resp++] = i;
	VECTOR(edges)[resp++] = to;
      } else {
	long int from=RNG_INTEGER(0, i);
	long int to=RNG_INTEGER(1,i);
	VECTOR(edges)[resp++] = from;
	VECTOR(edges)[resp++] = to;
      }
    }
  }

  RNG_END();

  IGRAPH_CHECK(igraph_create(graph, &edges, n, directed));
  igraph_vector_destroy(&edges);
  IGRAPH_FINALLY_CLEAN(1);

  return 0;
}

int igraph_aging_prefatt_game(igraph_t *graph, integer_t n, integer_t m,
			      integer_t aging_type, real_t aging_exp) {
  /* TODO */
  return 0;
}
