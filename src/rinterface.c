/* -*- mode: C -*-  */
/* 
   IGraph library R interface.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 
   02110-1301 USA 

*/

#include "igraph.h"
#include "error.h"

#include "config.h"

#define USE_RINTERNALS
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>

#include <stdio.h>

int igraph_free(void *p);

SEXP R_igraph_vector_to_SEXP(igraph_vector_t *v);
SEXP R_igraph_matrix_to_SEXP(igraph_matrix_t *m);
SEXP R_igraph_strvector_to_SEXP(const igraph_strvector_t *m);
SEXP R_igraph_to_SEXP(igraph_t *graph);

int R_igraph_SEXP_to_strvector(SEXP rval, igraph_strvector_t *sv);
int R_igraph_SEXP_to_strvector_copy(SEXP rval, igraph_strvector_t *sv);
int R_SEXP_to_vector(SEXP sv, igraph_vector_t *v);
int R_SEXP_to_vector_copy(SEXP sv, igraph_vector_t *v);
int R_SEXP_to_matrix(SEXP pakl, igraph_matrix_t *akl);
int R_SEXP_to_igraph_matrix_copy(SEXP pakl, igraph_matrix_t *akl);
int R_SEXP_to_igraph(SEXP graph, igraph_t *res);
int R_SEXP_to_igraph_copy(SEXP graph, igraph_t *res);
int R_SEXP_to_igraph_vs(SEXP rit, igraph_t *graph, igraph_vs_t *it);
int R_SEXP_to_igraph_es(SEXP rit, igraph_t *graph, igraph_es_t *it);

/* get the list element named str, or return NULL */
/* from the R Manual */

SEXP R_igraph_getListElement(SEXP list, const char *str)
{
  SEXP elmt = R_NilValue, names = getAttrib(list, R_NamesSymbol);
  int i;
  
  for (i = 0; i < length(list); i++)
    if(strcmp(CHAR(STRING_ELT(names, i)), str) == 0) {
      elmt = VECTOR_ELT(list, i);
      break;
    }
  return elmt;
}

/******************************************************
 * Attributes                                         *
 *****************************************************/
/* TODO: add graph attributes */
int R_igraph_attribute_init(igraph_t *graph, igraph_vector_ptr_t *attr) {
  SEXP result;
  long int i;
  PROTECT(result=NEW_LIST(4));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(2));
  REAL(VECTOR_ELT(result, 0))[0]=0; /* R objects */
  REAL(VECTOR_ELT(result, 0))[1]=1; /* igraph_t objects */
  for (i=0; i<3; i++) {
    SET_VECTOR_ELT(result, i+1, NEW_LIST(0)); /* gal, val, eal */
  }
  graph->attr=result;
  return 0;
}

void R_igraph_attribute_destroy(igraph_t *graph) {
  SEXP attr=graph->attr;
  REAL(VECTOR_ELT(attr, 0))[1] -= 1; /* refcount for igraph_t */
  if (REAL(VECTOR_ELT(attr, 0))[1]==0) {
    UNPROTECT_PTR(attr);
  }
  graph->attr=0;
}

int R_igraph_attribute_copy(igraph_t *to, const igraph_t *from) {
  SEXP fromattr=from->attr;
  to->attr=from->attr;
  REAL(VECTOR_ELT(fromattr, 0))[1] += 1; /* refcount only */
  if (REAL(VECTOR_ELT(fromattr, 0))[1] == 1) {
    PROTECT(to->attr);
  }
  return 0;
}

int R_igraph_attribute_add_vertices(igraph_t *graph, long int nv, 
				    igraph_vector_ptr_t *nattr) {
  SEXP attr=graph->attr;
  SEXP val, rep=0, names, newnames;
  igraph_vector_t news;
  long int valno, i, origlen, nattrno, newattrs;
  if (REAL(VECTOR_ELT(attr, 0))[0]+REAL(VECTOR_ELT(attr, 0))[1] > 1) {
    SEXP newattr;
    PROTECT(newattr=duplicate(attr));
    REAL(VECTOR_ELT(attr, 0))[1] -= 1;
    if (REAL(VECTOR_ELT(attr, 0))[1] == 0) {
      UNPROTECT_PTR(attr);
    }
    REAL(VECTOR_ELT(newattr, 0))[0] = 0;
    REAL(VECTOR_ELT(newattr, 0))[1] = 1;
    attr=graph->attr=newattr;
  }

  val=VECTOR_ELT(attr, 2);
  valno=GET_LENGTH(val);  
  names=GET_NAMES(val);
  if (nattr==NULL) { 
    nattrno=0;
  } else {
    nattrno=igraph_vector_ptr_size(nattr); 
  }
  origlen=igraph_vcount(graph)-nv;

  /* First add the new attributes, if any */
  newattrs=0;
  IGRAPH_VECTOR_INIT_FINALLY(&news, 0);
  for (i=0; i<nattrno; i++) {
    igraph_i_attribute_record_t *nattr_entry=VECTOR(*nattr)[i];
    const char *nname=nattr_entry->name;
    long int j; 
    igraph_bool_t l=0;
    for (j=0; !l && j<valno; j++) {
      l=!strcmp(nname, CHAR(STRING_ELT(names, j)));
    }
    if (!l) {
      newattrs++;
      IGRAPH_CHECK(igraph_vector_push_back(&news, i));
    }
  }
  if (newattrs != 0) {
    SEXP app, newval;
    PROTECT(app=NEW_LIST(newattrs));
    PROTECT(newnames=NEW_CHARACTER(newattrs));
    PROTECT(rep=EVAL(lang3(install("rep"), ScalarLogical(NA_LOGICAL), 
			   ScalarInteger(origlen))));
    for (i=0; i<newattrs; i++) {
      igraph_i_attribute_record_t *tmp=
	VECTOR(*nattr)[(long int)VECTOR(news)[i]];
      SET_VECTOR_ELT(app, i, rep);
      SET_STRING_ELT(newnames, i, CREATE_STRING_VECTOR(tmp->name));
    }
    UNPROTECT(1); 		/* rep */
    PROTECT(newval=EVAL(lang3(install("c"), val, app)));
    PROTECT(newnames=EVAL(lang3(install("c"), names, newnames)));
    SET_NAMES(newval, newnames);
    SET_VECTOR_ELT(attr, 2, newval);
    val=VECTOR_ELT(attr, 2);    
    valno=GET_LENGTH(val);  
    names=GET_NAMES(val);
    UNPROTECT(4);
    rep=0;
  }
  igraph_vector_destroy(&news);
  IGRAPH_FINALLY_CLEAN(1);	/* news */

  /* Now append the new values */
  for (i=0; i<valno; i++) {
    SEXP oldva=VECTOR_ELT(val, i), newva;
    const char *sexpname=CHAR(STRING_ELT(names,i));
    igraph_bool_t l=0;
    long int j;
    for (j=0; !l && j<nattrno; j++) {
      igraph_i_attribute_record_t *tmp=VECTOR(*nattr)[j];
      l=!strcmp(sexpname, tmp->name);
    }
    if (l) {
      /* This attribute is present in nattr */
      SEXP app=0;
      igraph_i_attribute_record_t *tmprec=VECTOR(*nattr)[j-1];
      switch (tmprec->type) {
      case IGRAPH_ATTRIBUTE_NUMERIC:
	if (nv != igraph_vector_size(tmprec->value)) {
	  IGRAPH_ERROR("Invalid attribute length", IGRAPH_EINVAL);
	}
	PROTECT(app=NEW_NUMERIC(nv));
	igraph_vector_copy_to(tmprec->value, REAL(app));
	break;
      case IGRAPH_ATTRIBUTE_STRING:
	if (nv != igraph_strvector_size(tmprec->value)) {
	  IGRAPH_ERROR("Invalid attribute length", IGRAPH_EINVAL);
	}
	PROTECT(app=R_igraph_strvector_to_SEXP(tmprec->value));
	break;
      case IGRAPH_ATTRIBUTE_R_OBJECT:
	/* TODO */
	IGRAPH_ERROR("R_objects not implemented yet", IGRAPH_UNIMPLEMENTED);
	break;
      default:
	warning("Ignoring unknown attribute type");
	break;
      }
      if (app!=0) {
	PROTECT(newva=EVAL(lang3(install("c"), oldva, app)));
	SET_VECTOR_ELT(val, i, newva);
	UNPROTECT(2);		/* app & newva */
      }
    } else {
      /* No such attribute, append NA's */
      if (rep==0) {
	PROTECT(rep=EVAL(lang3(install("rep"), ScalarLogical(NA_LOGICAL), 
			       ScalarInteger(nv))));
      }
      PROTECT(newva=EVAL(lang3(install("c"), oldva, rep)));
      SET_VECTOR_ELT(val, i, newva);
      UNPROTECT(1); 		/* newva */
    }
  }
  if (rep != 0) {
    UNPROTECT(1);
  } 
  
  return 0;
}

void R_igraph_attribute_delete_vertices(igraph_t *graph, 
					const igraph_vector_t *eidx,
					const igraph_vector_t *vidx) {
  SEXP attr=graph->attr;
  SEXP eal, val;
  long int valno, ealno, i;
  if (REAL(VECTOR_ELT(attr, 0))[0]+REAL(VECTOR_ELT(attr, 0))[1] > 1) {
    SEXP newattr;
    PROTECT(newattr=duplicate(attr));
    REAL(VECTOR_ELT(attr, 0))[1] -= 1;
    if (REAL(VECTOR_ELT(attr, 0))[1] == 0) {
      UNPROTECT_PTR(attr);
    }
    REAL(VECTOR_ELT(newattr, 0))[0] = 0;
    REAL(VECTOR_ELT(newattr, 0))[1] = 1;
    attr=graph->attr=newattr;
  }

  /* Vertices */
  val=VECTOR_ELT(attr, 2);
  valno=GET_LENGTH(val);
  for (i=0; i<valno; i++) {
    SEXP oldva=VECTOR_ELT(val, i), newva, ss;
    long int origlen=GET_LENGTH(oldva);
    long int newlen=0, j;
    for (j=0; j<igraph_vector_size(vidx); j++) {
      if (VECTOR(*vidx)[j] > 0) {
	newlen++;
      }
    }
    PROTECT(ss=NEW_NUMERIC(newlen));
    for (j=0; j<origlen; j++) {
      if (VECTOR(*vidx)[j]>0) {
	REAL(ss)[(long int)VECTOR(*vidx)[j]-1]=j+1;
      }
    }
    PROTECT(newva=EVAL(lang3(install("["), oldva, ss)));
    SET_VECTOR_ELT(val, i, newva);
    UNPROTECT(2);
  }    

  /* Edges */
  eal=VECTOR_ELT(attr, 3);
  ealno=GET_LENGTH(eal);
  for (i=0; i<ealno; i++) {
    SEXP oldea=VECTOR_ELT(eal, i), newea, ss;
    long int origlen=GET_LENGTH(oldea);
    long int newlen=0, j;
    /* calculate new length */
    for (j=0; j<origlen; j++) {
      if (VECTOR(*eidx)[j] > 0) {
	newlen++;
      }
    }    
    PROTECT(ss=NEW_NUMERIC(newlen));
    for (j=0; j<origlen; j++) {
      if (VECTOR(*eidx)[j]>0) {
	REAL(ss)[(long int)VECTOR(*eidx)[j]-1]=j+1;
      }
    }
    PROTECT(newea=EVAL(lang3(install("["), oldea, ss)));
    SET_VECTOR_ELT(eal, i, newea);
    UNPROTECT(2);
  }
}


int R_igraph_attribute_add_edges(igraph_t *graph, 
				 const igraph_vector_t *edges,
				 igraph_vector_ptr_t *nattr) {
  SEXP attr=graph->attr;
  SEXP eal, rep=0, names, newnames;
  igraph_vector_t news;
  long int ealno, i, origlen, nattrno, newattrs;  
  long int ne=igraph_vector_size(edges)/2;
  if (REAL(VECTOR_ELT(attr, 0))[0]+REAL(VECTOR_ELT(attr, 0))[1] > 1) {
    SEXP newattr;
    PROTECT(newattr=duplicate(attr));
    REAL(VECTOR_ELT(attr, 0))[1] -= 1;
    if (REAL(VECTOR_ELT(attr, 0))[1] == 0) {
      UNPROTECT_PTR(attr);
    }
    REAL(VECTOR_ELT(newattr, 0))[0] = 0;
    REAL(VECTOR_ELT(newattr, 0))[1] = 1;
    attr=graph->attr=newattr;
  }

  eal=VECTOR_ELT(attr, 3);
  ealno=GET_LENGTH(eal);
  names=GET_NAMES(eal);
  if (nattr==NULL) {
    nattrno=0; 
  } else {
    nattrno=igraph_vector_ptr_size(nattr);
  }
  origlen=igraph_ecount(graph)-ne;

  /* First add the new attributes, if any */
  newattrs=0;
  IGRAPH_VECTOR_INIT_FINALLY(&news, 0);
  for (i=0; i<nattrno; i++) {
    igraph_i_attribute_record_t *nattr_entry=VECTOR(*nattr)[i];
    const char *nname=nattr_entry->name;
    long int j;
    igraph_bool_t l=0;
    for (j=0; !l && j<ealno; j++) {
      l=!strcmp(nname, CHAR(STRING_ELT(names, j)));
    }
    if (!l) {
      newattrs++;
      IGRAPH_CHECK(igraph_vector_push_back(&news, i));
    }
  }
  if (newattrs != 0) {
    SEXP app, neweal;
    PROTECT(app=NEW_LIST(newattrs));
    PROTECT(newnames=NEW_CHARACTER(newattrs));
    PROTECT(rep=EVAL(lang3(install("rep"), ScalarLogical(NA_LOGICAL),
			   ScalarInteger(origlen))));
    for (i=0; i<newattrs; i++) {
      igraph_i_attribute_record_t *tmp=
	VECTOR(*nattr)[ (long int) VECTOR(news)[i]];
      SET_VECTOR_ELT(app, i, rep);
      SET_STRING_ELT(newnames, i, CREATE_STRING_VECTOR(tmp->name));
    }
    UNPROTECT(1);		/* rep */
    PROTECT(neweal=EVAL(lang3(install("c"), eal, app)));
    PROTECT(newnames=EVAL(lang3(install("c"), names, newnames)));
    SET_NAMES(neweal, newnames);
    SET_VECTOR_ELT(attr, 3, neweal);
    eal=VECTOR_ELT(attr, 3);
    ealno=GET_LENGTH(eal);
    names=GET_NAMES(eal);
    UNPROTECT(4);
    rep=0;
  }
  igraph_vector_destroy(&news);
  IGRAPH_FINALLY_CLEAN(1);

  /* Now append the new values */
  for (i=0; i<ealno; i++) {
    SEXP oldea=VECTOR_ELT(eal, i), newea;
    const char *sexpname=CHAR(STRING_ELT(names, i));
    igraph_bool_t l=0;
    long int j;
    for (j=0; !l && j<nattrno; j++) {
      igraph_i_attribute_record_t *tmp=VECTOR(*nattr)[j];
      l=!strcmp(sexpname, tmp->name);
    }
    if (l) {
      /* This attribute is present in nattr */
      SEXP app=0;
      igraph_i_attribute_record_t *tmprec=VECTOR(*nattr)[j-1];
      switch (tmprec->type) {
      case IGRAPH_ATTRIBUTE_NUMERIC:
	if (ne != igraph_vector_size(tmprec->value)) {
	  IGRAPH_ERROR("Invalid attribute length", IGRAPH_EINVAL);
	}
	PROTECT(app=NEW_NUMERIC(ne));
	igraph_vector_copy_to(tmprec->value, REAL(app));
	break;
      case IGRAPH_ATTRIBUTE_STRING:
	if (ne != igraph_strvector_size(tmprec->value)) {
	  IGRAPH_ERROR("Invalid attribute length", IGRAPH_EINVAL);
	}
	PROTECT(app=R_igraph_strvector_to_SEXP(tmprec->value));
	break;
      case IGRAPH_ATTRIBUTE_R_OBJECT:
	/* TODO */
	IGRAPH_ERROR("R objects not implemented yet", IGRAPH_UNIMPLEMENTED);
	break;
      default:
	warning("Ignoring unknown attribute type");
	break;
      }
      if (app!=0) {
	PROTECT(newea=EVAL(lang3(install("c"), oldea, app)));
	SET_VECTOR_ELT(eal, i, newea);
	UNPROTECT(2);		/* app & newea */
      }
    } else {
      /* No such attribute, append NA's */
      if (rep==0) {
	PROTECT(rep=EVAL(lang3(install("rep"), ScalarLogical(NA_LOGICAL),
			       ScalarInteger(ne))));
      }
      PROTECT(newea=EVAL(lang3(install("c"), oldea, rep)));
      SET_VECTOR_ELT(eal, i, newea);
      UNPROTECT(1);		/* newea */
    }
  }
  if (rep != 0) {
    UNPROTECT(1);
  }

  return 0;
}

void R_igraph_attribute_delete_edges(igraph_t *graph, 
				     const igraph_vector_t *idx) {
  SEXP attr=graph->attr;
  SEXP eal;
  long int ealno, i;
  if (REAL(VECTOR_ELT(attr, 0))[0]+REAL(VECTOR_ELT(attr, 0))[1] > 1) {
    SEXP newattr;
    PROTECT(newattr=duplicate(attr));
    REAL(VECTOR_ELT(attr, 0))[1] -= 1;
    if (REAL(VECTOR_ELT(attr, 0))[1] == 0) {
      UNPROTECT_PTR(attr);
    }
    REAL(VECTOR_ELT(newattr, 0))[0] = 0;
    REAL(VECTOR_ELT(newattr, 0))[1] = 1;
    attr=graph->attr=newattr;
  }

  eal=VECTOR_ELT(attr, 3);
  ealno=GET_LENGTH(eal);
  for (i=0; i<ealno; i++) {
    SEXP oldea=VECTOR_ELT(eal, i), newea, ss;
    long int origlen=GET_LENGTH(oldea);
    long int newlen=0, j;
    /* create subscript vector */
    for (j=0; j<origlen; j++) {
      if (VECTOR(*idx)[j] > 0) {
	newlen++;
      }
    }
    PROTECT(ss=NEW_NUMERIC(newlen));
    for (j=0; j<origlen; j++) {
      if (VECTOR(*idx)[j] > 0) {
	REAL(ss)[(long int)VECTOR(*idx)[j]-1] = j+1;
      }
    }
    PROTECT(newea=EVAL(lang3(install("["), oldea, ss)));
    SET_VECTOR_ELT(eal, i, newea);
    UNPROTECT(2);
  }
}

int R_igraph_attribute_permute_edges(igraph_t *graph,
				     const igraph_vector_t *idx) {
  SEXP attr=graph->attr;
  SEXP eal;
  long int ealno, i;
  if (REAL(VECTOR_ELT(attr, 0))[0]+REAL(VECTOR_ELT(attr, 0))[1] > 1) {
    SEXP newattr;
    PROTECT(newattr=duplicate(attr));
    REAL(VECTOR_ELT(attr, 0))[1] -= 1;
    if (REAL(VECTOR_ELT(attr, 0))[1] == 0) {
      UNPROTECT_PTR(attr);
    }
    REAL(VECTOR_ELT(newattr, 0))[0] = 0;
    REAL(VECTOR_ELT(newattr, 0))[1] = 1;
    attr=graph->attr=newattr;
  }

  eal=VECTOR_ELT(attr, 3);
  ealno=GET_LENGTH(eal);
  for (i=0; i<ealno; i++) {
    SEXP oldea=VECTOR_ELT(eal, i), newea, ss;
    long int j;
    long int newlen=igraph_vector_size(idx);
    /* create subscript vector */
    PROTECT(ss=NEW_NUMERIC(newlen));
    for (j=0; j<newlen; j++) {
      REAL(ss)[j] = VECTOR(*idx)[j];
    }
    PROTECT(newea=EVAL(lang3(install("["), oldea, ss)));
    SET_VECTOR_ELT(eal, i, newea);
    UNPROTECT(2);
  }
  return 0;
}

int R_igraph_attribute_get_info(const igraph_t *graph,
				igraph_strvector_t *gnames,
				igraph_vector_t *gtypes,
				igraph_strvector_t *vnames,
				igraph_vector_t *vtypes,
				igraph_strvector_t *enames,
				igraph_vector_t *etypes) {
  igraph_strvector_t *names[3] = { gnames, vnames, enames };
  igraph_vector_t *types[3] = { gtypes, vtypes, etypes };
  long int i, j;

  SEXP attr=graph->attr;

  for (i=0; i<3; i++) {
    igraph_strvector_t *n=names[i];
    igraph_vector_t *t=types[i];
    SEXP al=VECTOR_ELT(attr, i+1);

    if (n) {			/* return names */
      SEXP names=GET_NAMES(al);
      R_igraph_SEXP_to_strvector_copy(names, n);
    }

    if (t) {			/* return types */
      igraph_vector_resize(t, GET_LENGTH(al));
      for (j=0; j<GET_LENGTH(al); j++) {
	SEXP a=VECTOR_ELT(al, j);
	if (TYPEOF(a)==REALSXP || TYPEOF(a)==INTSXP) {
	  VECTOR(*t)[j]=IGRAPH_ATTRIBUTE_NUMERIC;
	} else if (IS_CHARACTER(a)) {
	  VECTOR(*t)[j]=IGRAPH_ATTRIBUTE_STRING;
	} else {
	  VECTOR(*t)[j]=IGRAPH_ATTRIBUTE_R_OBJECT;
	}
      }
    }
  }

  return 0;
}

igraph_bool_t R_igraph_attribute_has_attr(const igraph_t *graph,
					  igraph_attribute_elemtype_t type,
					  const char *name) {
  long int attrnum;
  SEXP res;
  
  switch (type) {
  case IGRAPH_ATTRIBUTE_GRAPH:
    attrnum=1;
    break;
  case IGRAPH_ATTRIBUTE_VERTEX:
    attrnum=2;
    break;
  case IGRAPH_ATTRIBUTE_EDGE:
    attrnum=3;
    break;
  default:
    IGRAPH_ERROR("Unkwown attribute element type", IGRAPH_EINVAL);
    break;
  }
  
  res=R_igraph_getListElement(VECTOR_ELT(graph->attr, attrnum), name);
  return res != R_NilValue;
}

int R_igraph_attribute_gettype(const igraph_t *graph,
			       igraph_attribute_type_t *type,
			       igraph_attribute_elemtype_t elemtype,
			       const char *name) {
  long int attrnum;
  SEXP res;
  
  switch (elemtype) {
  case IGRAPH_ATTRIBUTE_GRAPH:
    attrnum=1;
    break;
  case IGRAPH_ATTRIBUTE_VERTEX:
    attrnum=2;
    break;
  case IGRAPH_ATTRIBUTE_EDGE:
    attrnum=3;
    break;
  default:
    IGRAPH_ERROR("Unkwown attribute element type", IGRAPH_EINVAL);
    break;
  }
  
  res=R_igraph_getListElement(VECTOR_ELT(graph->attr, attrnum), name);
  if (IS_NUMERIC(res) || IS_INTEGER(res)) {
    *type=IGRAPH_ATTRIBUTE_NUMERIC;
  } else if (IS_CHARACTER(res)) {
    *type=IGRAPH_ATTRIBUTE_STRING;
  } else {
    *type=IGRAPH_ATTRIBUTE_R_OBJECT;
  }
  return 0;
}

int R_igraph_attribute_get_numeric_graph_attr(const igraph_t *graph,
					      const char *name, 
					      igraph_vector_t *value) {
  SEXP gal=VECTOR_ELT(graph->attr, 1);
  SEXP ga=R_igraph_getListElement(gal, name);
  
  if (ga == R_NilValue) {
    IGRAPH_ERROR("No such attribute", IGRAPH_EINVAL);
  }
  
  IGRAPH_CHECK(igraph_vector_resize(value, 1));
  VECTOR(*value)[0]=REAL(ga)[0];

  return 0;
}

int R_igraph_attribute_get_string_graph_attr(const igraph_t *graph,
					     const char *name,
					     igraph_strvector_t *value) {
  /* TODO: serialization */
  SEXP gal=VECTOR_ELT(graph->attr, 1);
  SEXP ga=R_igraph_getListElement(gal, name);
  
  if (ga == R_NilValue) {
    IGRAPH_ERROR("No such attribute", IGRAPH_EINVAL);
  }
  
  IGRAPH_CHECK(igraph_strvector_resize(value, 1));
  IGRAPH_CHECK(igraph_strvector_set(value, 0, CHAR(STRING_ELT(ga, 0))));
  
  return 0;
}

int R_igraph_attribute_get_numeric_vertex_attr(const igraph_t *graph, 
					       const char *name,
					       igraph_vs_t vs,
					       igraph_vector_t *value) {
  /* TODO: serialization */
  SEXP val=VECTOR_ELT(graph->attr, 2);
  SEXP va=R_igraph_getListElement(val, name);
  igraph_vector_t newvalue;

  if (va == R_NilValue) {
    IGRAPH_ERROR("No such attribute", IGRAPH_EINVAL);
  }
  PROTECT(va=AS_NUMERIC(va));

  if (igraph_vs_is_all(&vs)) {
    R_SEXP_to_vector_copy(va, &newvalue);
    igraph_vector_destroy(value);
    *value=newvalue;
  } else {
    igraph_vit_t it;
    long int i=0;
    IGRAPH_CHECK(igraph_vit_create(graph, vs, &it));
    IGRAPH_FINALLY(igraph_vit_destroy, &it);
    IGRAPH_CHECK(igraph_vector_resize(value, IGRAPH_VIT_SIZE(it)));
    while (!IGRAPH_VIT_END(it)) {
      long int v=IGRAPH_VIT_GET(it);
      VECTOR(*value)[i]=REAL(va)[v];
      IGRAPH_VIT_NEXT(it);
      i++;
    }
    igraph_vit_destroy(&it);
    IGRAPH_FINALLY_CLEAN(1);
  }

  UNPROTECT(1);
  return 0;
}

int R_igraph_attribute_get_string_vertex_attr(const igraph_t *graph, 
					      const char *name,
					      igraph_vs_t vs,
					      igraph_strvector_t *value) {
  /* TODO: serialization */
  SEXP val, va;

  val=VECTOR_ELT(graph->attr, 2);  
  va=R_igraph_getListElement(val, name);
  if (va == R_NilValue) {
    IGRAPH_ERROR("No such attribute", IGRAPH_EINVAL);
  }
  
  if (igraph_vs_is_all(&vs)) {
    R_igraph_SEXP_to_strvector_copy(AS_CHARACTER(va), value);
  } else {
    igraph_vit_t it;
    long int i=0;
    IGRAPH_CHECK(igraph_vit_create(graph, vs, &it));
    IGRAPH_FINALLY(igraph_vit_destroy, &it);
    IGRAPH_CHECK(igraph_strvector_resize(value, IGRAPH_VIT_SIZE(it)));
    while (!IGRAPH_VIT_END(it)) {
      long int v=IGRAPH_VIT_GET(it);
      const char *str=CHAR(STRING_ELT(AS_CHARACTER(va), v));
      IGRAPH_CHECK(igraph_strvector_set(value, i, str));
      IGRAPH_VIT_NEXT(it);
      i++;
    }
    igraph_vit_destroy(&it);
    IGRAPH_FINALLY_CLEAN(1);
  }
  
  return 0;
}

int R_igraph_attribute_get_numeric_edge_attr(const igraph_t *graph,
					     const char *name,
					     igraph_es_t es,
					     igraph_vector_t *value) {
  /* TODO: serialization */
  SEXP eal=VECTOR_ELT(graph->attr, 3);
  SEXP ea=R_igraph_getListElement(eal, name);
  igraph_vector_t newvalue;

  if (ea == R_NilValue) {
    IGRAPH_ERROR("No such attribute", IGRAPH_EINVAL);
  }
  PROTECT(ea=AS_NUMERIC(ea));
  
  if (igraph_es_is_all(&es)) {    
    R_SEXP_to_vector_copy(AS_NUMERIC(ea), &newvalue);
    igraph_vector_destroy(value);
    *value=newvalue;
  } else {
    igraph_eit_t it;
    long int i=0;
    IGRAPH_CHECK(igraph_eit_create(graph, es, &it));
    IGRAPH_FINALLY(igraph_eit_destroy, &it);
    IGRAPH_CHECK(igraph_vector_resize(value, IGRAPH_EIT_SIZE(it)));
    while (!IGRAPH_EIT_END(it)) {
      long int e=IGRAPH_EIT_GET(it);
      VECTOR(*value)[i]=REAL(ea)[e];
      IGRAPH_EIT_NEXT(it);
      i++;
    }
    igraph_eit_destroy(&it);
    IGRAPH_FINALLY_CLEAN(1);
  }

  UNPROTECT(1);
  return 0;
}

int R_igraph_attribute_get_string_edge_attr(const igraph_t *graph,
					    const char *name,
					    igraph_es_t es,
					    igraph_strvector_t *value) {
  /* TODO: serialization */
  SEXP eal=VECTOR_ELT(graph->attr, 3);
  SEXP ea=R_igraph_getListElement(eal, name);
  
  if (ea == R_NilValue) {
    IGRAPH_ERROR("No such attribute", IGRAPH_EINVAL);
  }
  
  if (igraph_es_is_all(&es)) {
    R_igraph_SEXP_to_strvector_copy(AS_CHARACTER(ea), value);
  } else {
    igraph_eit_t it;
    long int i=0;
    IGRAPH_CHECK(igraph_eit_create(graph, es, &it));
    IGRAPH_FINALLY(igraph_eit_destroy, &it);
    IGRAPH_CHECK(igraph_strvector_resize(value, IGRAPH_EIT_SIZE(it)));
    while (!IGRAPH_EIT_END(it)) {
      long int e=IGRAPH_EIT_GET(it);
      const char *str=CHAR(STRING_ELT(AS_CHARACTER(ea), e));
      IGRAPH_CHECK(igraph_strvector_set(value, i, str));
      IGRAPH_EIT_NEXT(it);
      i++;
    }
    igraph_eit_destroy(&it);
    IGRAPH_FINALLY_CLEAN(1);
  }
  
  return 0;
}

igraph_attribute_table_t R_igraph_attribute_table={
  &R_igraph_attribute_init, &R_igraph_attribute_destroy,
  &R_igraph_attribute_copy, &R_igraph_attribute_add_vertices,
  &R_igraph_attribute_delete_vertices, &R_igraph_attribute_add_edges,
  &R_igraph_attribute_delete_edges, &R_igraph_attribute_permute_edges,
  &R_igraph_attribute_get_info,
  &R_igraph_attribute_has_attr, &R_igraph_attribute_gettype,
  &R_igraph_attribute_get_numeric_graph_attr,
  &R_igraph_attribute_get_string_graph_attr,
  &R_igraph_attribute_get_numeric_vertex_attr,
  &R_igraph_attribute_get_string_vertex_attr,
  &R_igraph_attribute_get_numeric_edge_attr,
  &R_igraph_attribute_get_string_edge_attr
};

igraph_attribute_table_t *R_igraph_attribute_oldtable;

/******************************************************
 * things to do before and after                      *
 * calling an interface function                      *
 *****************************************************/

igraph_error_handler_t *R_igraph_oldhandler;

void R_igraph_myhandler (const char *reason, const char *file,
			 int line, int igraph_errno) {
  IGRAPH_FINALLY_FREE();
  error("At %s:%i : %s, %s", file, line, reason, 
	igraph_strerror(igraph_errno));
}

igraph_interruption_handler_t *R_igraph_oldinterrupt;
igraph_progress_handler_t *R_igraph_oldprogress;

extern int R_interrupts_pending;

int R_igraph_interrupt_handler(void *data) {
#if  ( defined(HAVE_AQUA) || defined(Win32) )
  /* TODO!!!: proper interrupt handling for these systems, 
     right now memory is not deallocated!!! */
  R_CheckUserInterrupt();
#else
  if (R_interrupts_pending) {
    IGRAPH_FINALLY_FREE();
    R_CheckUserInterrupt();
  }
#endif
  return 0;
}

int R_igraph_progress_handler(const char *message, igraph_real_t percent,
			      void * data) {
  static igraph_real_t last=0;
  if (percent == 0) {
    int i, l=strlen(message);
    last=0;
    fprintf(stderr, 
	    "                    : --------------------------------------------------|\r");
    for (i=0; i<l && i<20; i++) {
      fputc(message[i], stderr);
    }
    for (; i<20; i++) {
      fputc(' ', stderr);
    }
    fputc(':', stderr); fputc(' ', stderr);
  } 
  while (last < percent) {
    fputc('*', stderr);
    last+=2;
  }
  return 0;
}

R_INLINE void R_igraph_before() {
  R_igraph_oldhandler=igraph_set_error_handler(R_igraph_myhandler);
  R_igraph_oldinterrupt=
    igraph_set_interruption_handler(R_igraph_interrupt_handler);
  R_igraph_attribute_oldtable=
    igraph_i_set_attribute_table(&R_igraph_attribute_table);
}

R_INLINE void R_igraph_before2(SEXP verbose, const char *str) {
  R_igraph_oldhandler=igraph_set_error_handler(R_igraph_myhandler);
  R_igraph_oldinterrupt=
    igraph_set_interruption_handler(R_igraph_interrupt_handler);
  R_igraph_attribute_oldtable=
    igraph_i_set_attribute_table(&R_igraph_attribute_table);
  if (LOGICAL(verbose)[0]) {
    R_igraph_oldprogress=igraph_set_progress_handler(R_igraph_progress_handler);
  }
}  

R_INLINE void R_igraph_after() {
  igraph_set_error_handler(R_igraph_oldhandler);
  igraph_set_interruption_handler(R_igraph_oldinterrupt);
  igraph_i_set_attribute_table(R_igraph_attribute_oldtable);
}

R_INLINE void R_igraph_after2(SEXP verbose) {
  igraph_set_error_handler(R_igraph_oldhandler);
  igraph_set_interruption_handler(R_igraph_oldinterrupt);
  igraph_i_set_attribute_table(R_igraph_attribute_oldtable);
  if (LOGICAL(verbose)[0]) {
    igraph_set_progress_handler(R_igraph_oldprogress);
    fputc('\n', stderr);
  }
}

/******************************************************
 * functions to convert igraph objects to SEXP
 *****************************************************/

SEXP R_igraph_vector_to_SEXP(igraph_vector_t *v) {
  SEXP result;
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(v)));
  igraph_vector_copy_to(v, REAL(result));
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_0orvector_to_SEXP(igraph_vector_t *v) {
  SEXP result;
  if (v) {
    PROTECT(result=R_igraph_vector_to_SEXP(v));
  } else {
    PROTECT(result=R_NilValue);
  }
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_matrix_to_SEXP(igraph_matrix_t *m) {

  SEXP result, dim; 
  
  PROTECT(result=NEW_NUMERIC(igraph_matrix_size(m)));
  igraph_matrix_copy_to(m, REAL(result));
  PROTECT(dim=NEW_INTEGER(2));
  INTEGER(dim)[0]=igraph_matrix_nrow(m);
  INTEGER(dim)[1]=igraph_matrix_ncol(m);
  SET_DIM(result, dim);

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_0ormatrix_to_SEXP(igraph_matrix_t *m) {
  SEXP result;
  if (m) {
    PROTECT(result=R_igraph_matrix_to_SEXP(m));
  } else {
    PROTECT(result=R_NilValue);
  }
  UNPROTECT(1);
  return result;
}


SEXP R_igraph_array3_to_SEXP(igraph_array3_t *a) {
  SEXP result, dim;
  
  PROTECT(result=NEW_NUMERIC(igraph_array3_size(a)));
  igraph_vector_copy_to(&a->data, REAL(result));
  PROTECT(dim=NEW_INTEGER(3));
  INTEGER(dim)[0]=igraph_array3_n(a, 1);
  INTEGER(dim)[1]=igraph_array3_n(a, 2);
  INTEGER(dim)[2]=igraph_array3_n(a, 3);
  SET_DIM(result, dim);
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_0orarray3_to_SEXP(igraph_array3_t *a) {
  SEXP result;
  if (a) {
    PROTECT(result=R_igraph_array3_to_SEXP(a));
  } else {
    PROTECT(result=R_NilValue);
  }
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_strvector_to_SEXP(const igraph_strvector_t *m) {
  SEXP result;
  long int i;
  char *str;
  long int len;
  
  len=igraph_strvector_size(m);
  PROTECT(result=NEW_CHARACTER(len));
  for (i=0; i<len; i++) {
    igraph_strvector_get(m, i, &str);
    SET_STRING_ELT(result, i, CREATE_STRING_VECTOR(str));
  }
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_to_SEXP(igraph_t *graph) {
  
  SEXP result;
  long int no_of_nodes=igraph_vcount(graph);
  long int no_of_edges=igraph_ecount(graph);
  
  PROTECT(result=NEW_LIST(9));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(1));
  SET_VECTOR_ELT(result, 1, NEW_LOGICAL(1));
  SET_VECTOR_ELT(result, 2, NEW_NUMERIC(no_of_edges));
  SET_VECTOR_ELT(result, 3, NEW_NUMERIC(no_of_edges));
  SET_VECTOR_ELT(result, 4, NEW_NUMERIC(no_of_edges));
  SET_VECTOR_ELT(result, 5, NEW_NUMERIC(no_of_edges));
  SET_VECTOR_ELT(result, 6, NEW_NUMERIC(no_of_nodes+1));
  SET_VECTOR_ELT(result, 7, NEW_NUMERIC(no_of_nodes+1));

  REAL(VECTOR_ELT(result, 0))[0]=no_of_nodes;
  LOGICAL(VECTOR_ELT(result, 1))[0]=graph->directed;
  memcpy(REAL(VECTOR_ELT(result, 2)), graph->from.stor_begin, 
	 sizeof(igraph_real_t)*no_of_edges);
  memcpy(REAL(VECTOR_ELT(result, 3)), graph->to.stor_begin, 
	 sizeof(igraph_real_t)*no_of_edges);
  memcpy(REAL(VECTOR_ELT(result, 4)), graph->oi.stor_begin, 
	 sizeof(igraph_real_t)*no_of_edges);
  memcpy(REAL(VECTOR_ELT(result, 5)), graph->ii.stor_begin, 
	 sizeof(igraph_real_t)*no_of_edges);
  memcpy(REAL(VECTOR_ELT(result, 6)), graph->os.stor_begin, 
	 sizeof(igraph_real_t)*(no_of_nodes+1));
  memcpy(REAL(VECTOR_ELT(result, 7)), graph->is.stor_begin, 
	 sizeof(igraph_real_t)*(no_of_nodes+1));
  
  SET_CLASS(result, ScalarString(CREATE_STRING_VECTOR("igraph")));

  /* Attributes */
  SET_VECTOR_ELT(result, 8, graph->attr);
  REAL(VECTOR_ELT(graph->attr, 0))[0] += 1;
  
  UNPROTECT(1);
  return result;
}

int R_igraph_SEXP_to_strvector(SEXP rval, igraph_strvector_t *sv) {
  long int i;
  sv->len=GET_LENGTH(rval);
  sv->data=(char**) R_alloc(sv->len, sizeof(char*));
  for (i=0; i<sv->len; i++) {
    sv->data[i]=(char*) CHAR(STRING_ELT(rval, i));
  }

  return 0;
}

int R_igraph_SEXP_to_strvector_copy(SEXP rval, igraph_strvector_t *sv) {
  long int i;
  igraph_strvector_init(sv, GET_LENGTH(rval));
  for (i=0; i<sv->len; i++) {
    igraph_strvector_set(sv, i, CHAR(STRING_ELT(rval, i)));
  }
  
  return 0;
}

int R_SEXP_to_vector(SEXP sv, igraph_vector_t *v) {
  v->stor_begin=REAL(sv);
  v->stor_end=v->stor_begin+GET_LENGTH(sv);
  v->end=v->stor_end;
  return 0;
}

int R_SEXP_to_vector_copy(SEXP sv, igraph_vector_t *v) {
  return igraph_vector_init_copy(v, REAL(sv), GET_LENGTH(sv));  
}

int R_SEXP_to_matrix(SEXP pakl, igraph_matrix_t *akl) {
  R_SEXP_to_vector(pakl, &akl->data);
  akl->nrow=INTEGER(GET_DIM(pakl))[0];
  akl->ncol=INTEGER(GET_DIM(pakl))[1];

  return 0;
}

int R_SEXP_to_igraph_matrix_copy(SEXP pakl, igraph_matrix_t *akl) {
  igraph_vector_init_copy(&akl->data, REAL(pakl), GET_LENGTH(pakl));
  akl->nrow=INTEGER(GET_DIM(pakl))[0];
  akl->ncol=INTEGER(GET_DIM(pakl))[1];

  return 0;
}

int R_igraph_SEXP_to_array3(SEXP rval, igraph_array3_t *a) {
  R_SEXP_to_vector(rval, &a->data);
  a->n1=INTEGER(GET_DIM(rval))[0];
  a->n2=INTEGER(GET_DIM(rval))[1];
  a->n3=INTEGER(GET_DIM(rval))[2];
  a->n1n2=(a->n1) * (a->n2);

  return 0;
}

int R_igraph_SEXP_to_array3_copy(SEXP rval, igraph_array3_t *a) {
  igraph_vector_init_copy(&a->data, REAL(rval), GET_LENGTH(rval));
  a->n1=INTEGER(GET_DIM(rval))[0];
  a->n2=INTEGER(GET_DIM(rval))[1];
  a->n3=INTEGER(GET_DIM(rval))[2];
  a->n1n2=(a->n1) * (a->n2);

  return 0;
}

int R_SEXP_to_igraph(SEXP graph, igraph_t *res) {
  
  res->n=REAL(VECTOR_ELT(graph, 0))[0];
  res->directed=LOGICAL(VECTOR_ELT(graph, 1))[0];
  R_SEXP_to_vector(VECTOR_ELT(graph, 2), &res->from);
  R_SEXP_to_vector(VECTOR_ELT(graph, 3), &res->to);
  R_SEXP_to_vector(VECTOR_ELT(graph, 4), &res->oi);
  R_SEXP_to_vector(VECTOR_ELT(graph, 5), &res->ii);
  R_SEXP_to_vector(VECTOR_ELT(graph, 6), &res->os);
  R_SEXP_to_vector(VECTOR_ELT(graph, 7), &res->is);
  
  /* attributes */
  REAL(VECTOR_ELT(VECTOR_ELT(graph, 8), 0))[0] = 1; /* R objects refcount */
  REAL(VECTOR_ELT(VECTOR_ELT(graph, 8), 0))[1] = 0; /* igraph_t objects */
  res->attr=VECTOR_ELT(graph, 8);
  
  return 0;
}

int R_SEXP_to_igraph_copy(SEXP graph, igraph_t *res) {
  
  res->n=REAL(VECTOR_ELT(graph, 0))[0];
  res->directed=LOGICAL(VECTOR_ELT(graph, 1))[0];
  igraph_vector_init_copy(&res->from, REAL(VECTOR_ELT(graph, 2)), 
		   GET_LENGTH(VECTOR_ELT(graph, 2)));
  igraph_vector_init_copy(&res->to, REAL(VECTOR_ELT(graph, 3)), 
		   GET_LENGTH(VECTOR_ELT(graph, 3)));
  igraph_vector_init_copy(&res->oi, REAL(VECTOR_ELT(graph, 4)), 
		   GET_LENGTH(VECTOR_ELT(graph, 4)));
  igraph_vector_init_copy(&res->ii, REAL(VECTOR_ELT(graph, 5)), 
		   GET_LENGTH(VECTOR_ELT(graph, 5)));
  igraph_vector_init_copy(&res->os, REAL(VECTOR_ELT(graph, 6)), 
		   GET_LENGTH(VECTOR_ELT(graph, 6)));
  igraph_vector_init_copy(&res->is, REAL(VECTOR_ELT(graph, 7)),
		   GET_LENGTH(VECTOR_ELT(graph, 7)));

  /* attributes */
  REAL(VECTOR_ELT(VECTOR_ELT(graph, 8), 0))[0] = 1; /* R objects */
  REAL(VECTOR_ELT(VECTOR_ELT(graph, 8), 0))[1] = 1; /* igraph_t objects */
  PROTECT(res->attr=VECTOR_ELT(graph, 8));  

  return 0;
}

/* 
 * We have only vector type
 */

int R_SEXP_to_igraph_vs(SEXP rit, igraph_t *graph, igraph_vs_t *it) {
  
  igraph_vector_t *tmpv=(igraph_vector_t*)R_alloc(1,sizeof(igraph_vector_t));
  igraph_vs_vector(it, igraph_vector_view(tmpv, REAL(rit), 
					  GET_LENGTH(rit)));
  return 0;
}

/* 
 * We have only vector type
 */

int R_SEXP_to_igraph_es(SEXP rit, igraph_t *graph, igraph_es_t *it) {

  igraph_vector_t *tmpv=(igraph_vector_t*)R_alloc(1,sizeof(igraph_vector_t));
  igraph_es_vector(it, igraph_vector_view(tmpv, REAL(rit),
					  GET_LENGTH(rit)));
  return 0;
}

/*******************************************************************/

SEXP R_igraph_add_edges(SEXP graph, SEXP edges) {
  
  igraph_vector_t v;			/* do NOT destroy! */
  igraph_t g;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_vector(edges, &v);
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_add_edges(&g, &v, 0);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_add_vertices(SEXP graph, SEXP pnv) {
  
  igraph_integer_t nv;
  igraph_t g;
  SEXP result;
  
  R_igraph_before();
  
  nv=REAL(pnv)[0];
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_add_vertices(&g, nv, 0);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_vcount(SEXP graph) {
  
  igraph_t g;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=igraph_vcount(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_ecount(SEXP graph) {
  
  igraph_t g;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=igraph_ecount(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_neighbors(SEXP graph, SEXP pvid, SEXP pmode) {
  
  igraph_t g;
  igraph_vector_t neis;
  SEXP result;
  igraph_real_t vid;
  igraph_integer_t mode;
  
  R_igraph_before();
  
  igraph_vector_init(&neis, 0);
  vid=REAL(pvid)[0];
  mode=REAL(pmode)[0];
  R_SEXP_to_igraph(graph, &g);
  igraph_neighbors(&g, &neis, vid, mode);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&neis)));
  igraph_vector_copy_to(&neis, REAL(result));
  igraph_vector_destroy(&neis);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_delete_edges(SEXP graph, SEXP edges) {
  
  igraph_es_t es;
  igraph_t g;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  R_SEXP_to_igraph_es(edges, &g, &es);
  igraph_delete_edges(&g, es);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_delete_vertices(SEXP graph, SEXP vertices) {
  
  igraph_vs_t vs;
  igraph_t g;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  R_SEXP_to_igraph_vs(vertices, &g, &vs);
  igraph_delete_vertices(&g, vs);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  igraph_vs_destroy(&vs);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_is_directed(SEXP graph) {
  
  igraph_t g;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  PROTECT(result=NEW_LOGICAL(1));
  LOGICAL(result)[0]=igraph_is_directed(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_create(SEXP edges, SEXP pn, SEXP pdirected) {
  
  igraph_t g;
  igraph_vector_t v;
  igraph_integer_t n=REAL(pn)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_vector(edges, &v);
  igraph_create(&g, &v, n, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_degree(SEXP graph, SEXP vids, SEXP pmode, SEXP ploops) {
  
  igraph_t g;
  igraph_vs_t vs;
  igraph_vector_t res;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(vids, &g, &vs);
  igraph_vector_init(&res, 0);
  igraph_degree(&g, &res, vs, mode, loops);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vs);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_clusters(SEXP graph, SEXP pmode) {

  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_t membership;
  igraph_vector_t csize;
  SEXP result, names;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&membership, 0);
  igraph_vector_init(&csize, 0);
  igraph_clusters(&g, &membership, &csize, 0, mode);
  
  PROTECT(result=NEW_LIST(2));
  PROTECT(names=NEW_CHARACTER(2));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(igraph_vector_size(&membership)));
  SET_VECTOR_ELT(result, 1, NEW_NUMERIC(igraph_vector_size(&csize)));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("membership"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("csize"));
  SET_NAMES(result, names);
  igraph_vector_copy_to(&membership, REAL(VECTOR_ELT(result, 0)));
  igraph_vector_copy_to(&csize, REAL(VECTOR_ELT(result, 1)));
  
  igraph_vector_destroy(&membership);
  igraph_vector_destroy(&csize);

  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_is_connected(SEXP graph, SEXP pmode) {

  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_bool_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_is_connected(&g, &res, mode);
  
  PROTECT(result=NEW_LOGICAL(1));
  LOGICAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_diameter(SEXP graph, SEXP pdirected, SEXP punconnected) {
  
  igraph_t g;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t unconnected=LOGICAL(punconnected)[0];
  igraph_integer_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_diameter(&g, &res, 0, 0, 0, directed, unconnected);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_get_diameter(SEXP graph, SEXP pdirected, SEXP punconnected) {
  
  igraph_t g;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t unconnected=LOGICAL(punconnected)[0];
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_diameter(&g, 0, 0, 0, &res, directed, unconnected);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_farthest_points(SEXP graph, SEXP pdirected, SEXP punconnected) {
  
  igraph_t g;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t unconnected=LOGICAL(punconnected)[0];
  igraph_integer_t from, to, len;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_diameter(&g, &len, &from, &to, 0, directed, unconnected);
  
  PROTECT(result=NEW_NUMERIC(3));
  if (from < 0) {
    REAL(result)[0]=REAL(result)[1]=REAL(result)[2]=NA_REAL;
  } else {
    REAL(result)[0]=from;
    REAL(result)[1]=to;
    REAL(result)[2]=len;
  }
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_closeness(SEXP graph, SEXP pvids, SEXP pmode) {
  
  igraph_t g;
  igraph_vs_t vs;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_vector_init(&res, 0);
  igraph_closeness(&g, &res, vs, mode);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vs);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_subcomponent(SEXP graph, SEXP pvertex, SEXP pmode) {
  
  igraph_t g;
  igraph_real_t vertex=REAL(pvertex)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_subcomponent(&g, &res, vertex, mode);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_betweenness(SEXP graph, SEXP pvids, SEXP pdirected) {
  
  igraph_t g;
  igraph_vs_t vs;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_vector_init(&res, 0);
  igraph_betweenness(&g, &res, vs, directed);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vs);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_running_mean(SEXP pdata, SEXP pbinwidth) {
  
  igraph_vector_t data;
  igraph_integer_t binwidth=REAL(pbinwidth)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  R_SEXP_to_vector(pdata, &data);
  
  igraph_vector_init(&res, 0);
  igraph_running_mean(&data, &res, binwidth);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_cocitation(SEXP graph, SEXP pvids) {

  igraph_t g;
  igraph_vs_t vs;
  igraph_matrix_t m;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_matrix_init(&m, 0, 0);
  igraph_cocitation(&g, &m, vs);
  
  PROTECT(result=R_igraph_matrix_to_SEXP(&m));
  igraph_matrix_destroy(&m);
  igraph_vs_destroy(&vs);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_bibcoupling(SEXP graph, SEXP pvids) {

  igraph_t g;
  igraph_vs_t vs;
  igraph_matrix_t m;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_matrix_init(&m, 0, 0);
  igraph_bibcoupling(&g, &m, vs);
  
  PROTECT(result=R_igraph_matrix_to_SEXP(&m));
  igraph_matrix_destroy(&m);
  igraph_vs_destroy(&vs);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_growing_random_game(SEXP pn, SEXP pm, SEXP pdirected,
				  SEXP pcitation) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_integer_t m=REAL(pm)[0];
  igraph_bool_t citation=LOGICAL(pcitation)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();
  
  igraph_growing_random_game(&g, n, m, directed, citation);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_shortest_paths(SEXP graph, SEXP pvids, SEXP pmode) {
  
  igraph_t g;
  igraph_vs_t vs;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_matrix_init(&res, 0, 0);
  igraph_shortest_paths(&g, &res, vs, mode);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  igraph_vs_destroy(&vs);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_lattice(SEXP pdimvector, SEXP pnei, SEXP pdirected,
		      SEXP pmutual, SEXP pcircular) {
  
  igraph_t g;
  igraph_vector_t dimvector;
  igraph_integer_t nei=REAL(pnei)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t mutual=LOGICAL(pmutual)[0];
  igraph_bool_t circular=LOGICAL(pcircular)[0];  
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pdimvector, &dimvector);
  
  igraph_lattice(&g, &dimvector, nei, directed, mutual, circular);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_barabasi_game(SEXP pn, SEXP pm, SEXP poutseq,
			    SEXP poutpref, SEXP pdirected) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_integer_t m=REAL(pm)[0]; 
  igraph_vector_t outseq;
  igraph_bool_t outpref=LOGICAL(poutpref)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_vector(poutseq, &outseq);
  
  igraph_barabasi_game(&g, n, m, &outseq, outpref, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_nonlinear_barabasi_game(SEXP pn, SEXP ppower, SEXP pm,
				      SEXP poutseq, SEXP poutpref, 
				      SEXP pzeroappeal,
				      SEXP pdirected) {
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_real_t power=REAL(ppower)[0];
  igraph_integer_t m=REAL(pm)[0];
  igraph_vector_t outseq;
  igraph_bool_t outpref=LOGICAL(poutpref)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_real_t zeroappeal=REAL(pzeroappeal)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(poutseq, &outseq);
  
  igraph_nonlinear_barabasi_game(&g, n, power, m, &outseq, outpref, 
				 zeroappeal, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_recent_degree_game(SEXP pn, SEXP ppower, SEXP pwindow,
				 SEXP pm, SEXP poutseq, SEXP poutpref,
				 SEXP pzero_appeal,
				 SEXP pdirected) {
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_real_t power=REAL(ppower)[0];
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_integer_t m=REAL(pm)[0];
  igraph_vector_t outseq;
  igraph_bool_t outpref=LOGICAL(poutpref)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_real_t zero_appeal=REAL(pzero_appeal)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_vector(poutseq, &outseq);
  
  igraph_recent_degree_game(&g, n, power, window, m, &outseq, outpref, 
			    zero_appeal, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_layout_kamada_kawai(SEXP graph, SEXP pniter, SEXP pinitemp, 
				  SEXP pcoolexp, SEXP pkkconst, SEXP psigma, 
				  SEXP verbose) {
  
  igraph_t g;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_real_t initemp=REAL(pinitemp)[0];
  igraph_real_t coolexp=REAL(pcoolexp)[0];
  igraph_real_t kkconst=REAL(pkkconst)[0];
  igraph_real_t sigma=REAL(psigma)[0];
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before2(verbose, "KK layout");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_kamada_kawai(&g, &res, niter, sigma, 
			     initemp, coolexp, kkconst);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after2(verbose);
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_kamada_kawai_3d(SEXP graph, SEXP pniter, SEXP pinitemp,
				     SEXP pcoolexp, SEXP pkkconst, 
				     SEXP psigma, SEXP verbose) {
  igraph_t g;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_real_t initemp=REAL(pinitemp)[0];
  igraph_real_t coolexp=REAL(pcoolexp)[0];
  igraph_real_t kkconst=REAL(pkkconst)[0];
  igraph_real_t sigma=REAL(psigma)[0];
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before2(verbose, "3D KK layout");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_kamada_kawai_3d(&g, &res, niter, sigma, 
				initemp, coolexp, kkconst);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after2(verbose);
  
  UNPROTECT(1);
  return result;
}


SEXP R_igraph_layout_lgl(SEXP graph, SEXP pmaxiter, SEXP pmaxdelta,
			 SEXP parea, SEXP pcoolexp, SEXP prepulserad,
			 SEXP pcellsize, SEXP proot) {
  
  igraph_t g;
  igraph_matrix_t res;
  igraph_integer_t maxiter=REAL(pmaxiter)[0];
  igraph_real_t maxdelta=REAL(pmaxdelta)[0];
  igraph_real_t area=REAL(parea)[0];
  igraph_real_t coolexp=REAL(pcoolexp)[0];
  igraph_real_t repulserad=REAL(prepulserad)[0];
  igraph_real_t cellsize=REAL(pcellsize)[0];
  igraph_integer_t root=REAL(proot)[0];
  SEXP result;

  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_lgl(&g, &res, maxiter, maxdelta, area, coolexp, repulserad,
		    cellsize, root);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}  

SEXP R_igraph_layout_fruchterman_reingold_grid(SEXP graph, SEXP mat,
					       SEXP pniter,
					       SEXP pmaxdelta, SEXP parea,
					       SEXP pcoolexp, SEXP prepulserad,
					       SEXP pcellsize, SEXP puseseed,
					       SEXP verbose) {

  igraph_t g;
  igraph_matrix_t res;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_real_t maxdelta=REAL(pmaxdelta)[0];
  igraph_real_t area=REAL(parea)[0];
  igraph_real_t coolexp=REAL(pcoolexp)[0];
  igraph_real_t repulserad=REAL(prepulserad)[0];
  igraph_real_t cellsize=REAL(pcellsize)[0];
  igraph_bool_t use_seed=LOGICAL(puseseed)[0];
  SEXP result;
  
  R_igraph_before2(verbose, "Grid-FR layout");

  R_SEXP_to_igraph(graph, &g);
  if (use_seed) {
    R_SEXP_to_igraph_matrix_copy(mat, &res);
  } else {
    igraph_matrix_init(&res, 0, 0);
  }
  igraph_layout_grid_fruchterman_reingold(&g, &res, niter, maxdelta, area,
					  coolexp, repulserad, cellsize, 
					  use_seed);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after2(verbose);
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_minimum_spanning_tree_unweighted(SEXP graph) {
  
  igraph_t g;
  igraph_t mst;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_minimum_spanning_tree_unweighted(&g, &mst);
  PROTECT(result=R_igraph_to_SEXP(&mst));
  igraph_destroy(&mst);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_minimum_spanning_tree_prim(SEXP graph, SEXP pweights) {
  
  igraph_t g;
  igraph_t mst;
  igraph_vector_t weights;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_vector(pweights, &weights);
  
  R_SEXP_to_igraph(graph, &g);
  igraph_minimum_spanning_tree_prim(&g, &mst, &weights);
  PROTECT(result=R_igraph_to_SEXP(&mst));
  igraph_destroy(&mst);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_edge_betweenness(SEXP graph, SEXP pdirected) {
  
  igraph_t g;
  igraph_vector_t res;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_edge_betweenness(&g, &res, directed);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_id(SEXP graph,
				  SEXP pst, SEXP pmaxind, SEXP psd,
				  SEXP pno, SEXP pcites, SEXP pdebug, 
				  SEXP pdebugdeg) {
  
  igraph_t g;
  igraph_matrix_t ak, sd, no;
  igraph_vector_t cites, debug;
  igraph_matrix_t *ppsd=&sd, *ppno=&no;
  igraph_vector_t *ppcites=&cites, *ppdebug=&debug;
  igraph_vector_t st;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_bool_t lno=LOGICAL(pno)[0];
  igraph_bool_t lcites=LOGICAL(pcites)[0];
  igraph_bool_t ldebug=LOGICAL(pdebug)[0];
  igraph_integer_t maxind=REAL(pmaxind)[0];
  igraph_integer_t debugdeg=REAL(pdebugdeg)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&ak, 0, 0);
  if (lsd) {
    igraph_matrix_init(&sd, 0, 0);
  } else {
    ppsd=0;
  }
  if (lno) {
    igraph_matrix_init(&no, 0, 0);
  } else {
    ppno=0;
  }
  if (lcites) { igraph_vector_init(&cites,0); } else { ppcites=0; }
  if (ldebug) { igraph_vector_init(&debug,0); } else { ppdebug=0; }
  igraph_measure_dynamics_id(&g, &ak, ppsd, ppno, ppcites, ppdebug, 
			     debugdeg, &st, maxind);
  PROTECT(result=NEW_LIST(5));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&ak));
  igraph_matrix_destroy(&ak);
  if (lsd) { 
    SET_VECTOR_ELT(result, 1, R_igraph_matrix_to_SEXP(&sd));
    igraph_matrix_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  if (lno) {
    SET_VECTOR_ELT(result, 2, R_igraph_matrix_to_SEXP(&no));
    igraph_matrix_destroy(&no);
  } else {
    SET_VECTOR_ELT(result, 2, R_NilValue);
  }
  if (lcites) {    
    SET_VECTOR_ELT(result, 3, NEW_NUMERIC(igraph_vector_size(&cites)));
    igraph_vector_copy_to(&cites, REAL(VECTOR_ELT(result, 3)));
    igraph_vector_destroy(&cites);
  } else {
    SET_VECTOR_ELT(result, 3, R_NilValue);
  }
  if (ldebug) {
    SET_VECTOR_ELT(result, 4, NEW_NUMERIC(igraph_vector_size(&debug)));
    igraph_vector_copy_to(&debug, REAL(VECTOR_ELT(result, 4)));
    igraph_vector_destroy(&debug);
  } else {
    SET_VECTOR_ELT(result, 4, R_NilValue);
  }
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_id_st(SEXP graph, SEXP pak) {
  
  igraph_t g;
  igraph_matrix_t ak;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pak, &ak);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_id_st(&g, &res, &ak);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_id_expected(SEXP graph, SEXP pak, 
					   SEXP pst, SEXP pmaxind, SEXP pwhich) {
  igraph_t g;
  igraph_vector_t ak, st;
  igraph_integer_t maxind=REAL(pmaxind)[0];
  igraph_vector_t err;
  igraph_integer_t which=REAL(pwhich)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pak, &ak);
  R_SEXP_to_vector(pst, &st);
  igraph_vector_init(&err, 0);
  if (which==1) {
    igraph_measure_dynamics_id_expected(&g, &err, &ak, &st, maxind);
  } else {
    igraph_measure_dynamics_id_expected2(&g, &err, &ak, &st, maxind);
  }
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&err)));
  igraph_vector_copy_to(&err, REAL(result));
  igraph_vector_destroy(&err);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_measure_dynamics_idwindow(SEXP graph,
					SEXP pst, SEXP pwindow, SEXP pmaxind,
					SEXP psd) {
  igraph_t g;
  igraph_matrix_t ak, sd;
  igraph_matrix_t *ppsd=&sd;
  igraph_vector_t st;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_integer_t maxind=REAL(pmaxind)[0];
  igraph_real_t window=REAL(pwindow)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&ak, 0, 0);
  if (lsd) {
    igraph_matrix_init(&sd, 0, 0);
  } else {
    ppsd=0;
  }
  igraph_measure_dynamics_idwindow(&g, &ak, ppsd,
				   &st, maxind, window);
  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&ak));
  igraph_matrix_destroy(&ak);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, R_igraph_matrix_to_SEXP(&sd));
    igraph_matrix_destroy(&sd);
  }

  R_igraph_after();

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_idwindow_st(SEXP graph, SEXP pak, 
					   SEXP pwindow) {
  igraph_t g;
  igraph_matrix_t ak;
  igraph_vector_t res;
  igraph_integer_t window=REAL(pwindow)[0];
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pak, &ak);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_idwindow_st(&g, &res, &ak, window);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_idage(SEXP graph,
				     SEXP pst, SEXP pagebins,
				     SEXP pmaxind, SEXP psd, SEXP pno,
				     SEXP pcites,
				     SEXP ptime_window) {

  igraph_t g;
  igraph_matrix_t akl, sd, no, cites;
  igraph_matrix_t *ppsd=&sd, *ppno=&no, *ppcites=&cites;
  igraph_vector_t st;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_bool_t lno=LOGICAL(pno)[0];
  igraph_bool_t lcites=LOGICAL(pcites)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_integer_t maxind=REAL(pmaxind)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pst, &st);

  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&akl, 0, 0);
  if (lsd) {
    igraph_matrix_init(&sd, 0, 0);
  } else {
    ppsd=0;
  }
  if (lno) {
    igraph_matrix_init(&no, 0, 0);
  } else {
    ppno=0;
  }
  if (lcites) { igraph_matrix_init(&cites,0,0); } else { ppcites=0; }
  if (ptime_window==R_NilValue) {
    igraph_measure_dynamics_idage(&g, &akl, ppsd, ppno, ppcites,
				  &st, agebins, maxind);
  } else {
    igraph_measure_dynamics_idwindowage(&g, &akl, ppsd,
					&st, agebins, maxind,
					REAL(ptime_window)[0]);
  }
    
  PROTECT(result=NEW_LIST(4));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&akl));
  igraph_matrix_destroy(&akl);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, R_igraph_matrix_to_SEXP(&sd));
    igraph_matrix_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  if (lno) {
    SET_VECTOR_ELT(result, 2, R_igraph_matrix_to_SEXP(&no));
    igraph_matrix_destroy(&no);
  } else {
    SET_VECTOR_ELT(result, 2, R_NilValue);
  }
  if (lcites) {
    SET_VECTOR_ELT(result, 3, R_igraph_matrix_to_SEXP(&cites));
    igraph_matrix_destroy(&cites);
  } else {
    SET_VECTOR_ELT(result, 3, R_NilValue);
  }

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_idage_expected(SEXP graph, SEXP pakl,
					      SEXP pst, SEXP pmaxind) {
  igraph_t g;
  igraph_matrix_t akl;
  igraph_vector_t st;
  igraph_integer_t maxind=REAL(pmaxind)[0];
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pakl, &akl);
  R_SEXP_to_vector(pst, &st);
  igraph_matrix_init(&res, 0, 0);
  igraph_measure_dynamics_idage_expected(&g, &res, &akl, &st, maxind);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_idage_st(SEXP graph, SEXP pakl, 
					SEXP ptime_window) {
  
  igraph_t g;
  igraph_matrix_t akl;
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pakl, &akl);
  igraph_vector_init(&res, 0);
  if (ptime_window==R_NilValue) {    
    igraph_measure_dynamics_idage_st(&g, &res, &akl);
  } else {
    igraph_measure_dynamics_idwindowage_st(&g, &res, &akl, 
					   REAL(ptime_window)[0]);
  }
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  
  igraph_vector_destroy(&res);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_citedcat_id_age(SEXP graph,
					       SEXP pst, SEXP pcats, 
					       SEXP pcatno, SEXP pagebins,
					       SEXP pmaxind, SEXP psd) {
  igraph_t g;
  igraph_array3_t adkl, sd;
  igraph_vector_t st;
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_vector_t cats;
  igraph_integer_t catno=REAL(pcatno)[0];
  igraph_integer_t maxind=REAL(pmaxind)[0];
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_array3_t *ppsd=&sd;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_igraph(graph, &g);
  
  igraph_array3_init(&adkl, 0, 0, 0);
  if (lsd) {    
    igraph_array3_init(&sd, 0, 0, 0);
  } else {
    ppsd=0;
  }
  igraph_measure_dynamics_citedcat_id_age(&g, &adkl, ppsd, 
					  &st, &cats, catno,
					  agebins, maxind);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, R_igraph_array3_to_SEXP(&adkl));
  igraph_array3_destroy(&adkl);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, R_igraph_array3_to_SEXP(&sd));
    igraph_array3_destroy(&sd);
  } 
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
} 

SEXP R_igraph_measure_dynamics_citedcat_id_age_st(SEXP graph, SEXP padkl,
						  SEXP pcats, SEXP pcatno) {
  igraph_t g;
  igraph_array3_t adkl;
  igraph_vector_t cats;
  igraph_integer_t catno=REAL(pcatno)[0];
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_igraph_SEXP_to_array3(padkl, &adkl);
  R_SEXP_to_vector(pcats, &cats);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_citedcat_id_age_st(&g, &res, &adkl, &cats, catno);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_citingcat_id_age(SEXP graph,
						SEXP pst, SEXP pcats, 
						SEXP pcatno, SEXP pagebins,
						SEXP pmaxind, SEXP psd) {
  igraph_t g;
  igraph_array3_t adkl, sd;
  igraph_vector_t st;
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_vector_t cats;
  igraph_integer_t catno=REAL(pcatno)[0];
  igraph_integer_t maxind=REAL(pmaxind)[0];
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_array3_t *ppsd=&sd;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_igraph(graph, &g);
  
  igraph_array3_init(&adkl, 0, 0, 0);
  if (lsd) {
    igraph_array3_init(&sd, 0, 0, 0);
  } else {
    ppsd=0;
  }
  igraph_measure_dynamics_citingcat_id_age(&g, &adkl, ppsd, 
					   &st, &cats, catno,
					   agebins, maxind);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, R_igraph_array3_to_SEXP(&adkl));
  igraph_array3_destroy(&adkl);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, R_igraph_array3_to_SEXP(&sd));
    igraph_array3_destroy(&sd);
  }
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
} 

SEXP R_igraph_measure_dynamics_citingcat_id_age_st(SEXP graph, SEXP padkl,
						   SEXP pcats, SEXP pcatno) {
  igraph_t g;
  igraph_array3_t adkl;
  igraph_vector_t cats;
  igraph_integer_t catno=REAL(pcatno)[0];
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_igraph_SEXP_to_array3(padkl, &adkl);
  R_SEXP_to_vector(pcats, &cats);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_citingcat_id_age_st(&g, &res, &adkl, &cats, catno);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_measure_dynamics_d_d(SEXP graph, SEXP pntime, SEXP petime,
				   SEXP pevents, SEXP pst, SEXP pmaxdeg, 
				   SEXP psd) {
  igraph_t g;
  igraph_vector_t ntime, etime;
  igraph_integer_t events=REAL(pevents)[0];
  igraph_matrix_t akk, sd, *ppsd=0;
  igraph_vector_t st;
  igraph_integer_t maxdeg=REAL(pmaxdeg)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pntime, &ntime);
  R_SEXP_to_vector(petime, &etime);
  igraph_matrix_init(&akk, 0, 0);
  if (LOGICAL(psd)[0]) {
    igraph_matrix_init(&sd, 0, 0);
    ppsd=&sd;
  }
  R_SEXP_to_vector(pst, &st);
  igraph_measure_dynamics_d_d(&g, &ntime, &etime, events, &akk, 
			      ppsd, &st, maxdeg);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&akk));
  igraph_matrix_destroy(&akk);
  if (LOGICAL(psd)[0]) {
    SET_VECTOR_ELT(result, 1, R_igraph_matrix_to_SEXP(&sd));
    igraph_matrix_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
} 

SEXP R_igraph_measure_dynamics_d_d_st(SEXP graph, SEXP pntime, SEXP petime,
				      SEXP pakk, SEXP pevents, 
				      SEXP pmaxtotaldeg) {
  igraph_t g;
  igraph_vector_t ntime, etime;
  igraph_matrix_t akk;
  igraph_integer_t events=REAL(pevents)[0];
  igraph_integer_t maxtotaldeg=REAL(pmaxtotaldeg)[0];
  igraph_vector_t st;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pntime, &ntime);
  R_SEXP_to_vector(petime, &etime);
  R_SEXP_to_matrix(pakk, &akk);
  igraph_vector_init(&st, 0);
  igraph_measure_dynamics_d_d_st(&g, &ntime, &etime, &akk,
				 events, maxtotaldeg, &st);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&st)));
  igraph_vector_copy_to(&st, REAL(result));
  igraph_vector_destroy(&st);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_lastcit(SEXP graph, SEXP pst,
				       SEXP pagebins, SEXP psd, SEXP pno) {
  igraph_t g;
  igraph_vector_t al, sd, no;
  igraph_vector_t *ppsd=&sd, *ppno=&no;
  igraph_vector_t st;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_bool_t lno=LOGICAL(pno)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&al, 0);
  if (lsd) {
    igraph_vector_init(&sd, 0);
  } else {
    ppsd=0;
  }
  if (lno) {
    igraph_vector_init(&no, 0);
  } else {
    ppno=0;
  }
  igraph_measure_dynamics_lastcit(&g, &al, ppsd, ppno, &st, agebins);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(igraph_vector_size(&al)));
  igraph_vector_copy_to(&al, REAL(VECTOR_ELT(result, 0)));
  igraph_vector_destroy(&al);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, NEW_NUMERIC(igraph_vector_size(&sd)));
    igraph_vector_copy_to(&sd, REAL(VECTOR_ELT(result, 1)));
    igraph_vector_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  if (lno) {
    SET_VECTOR_ELT(result, 2, NEW_NUMERIC(igraph_vector_size(&no)));
    igraph_vector_copy_to(&no, REAL(VECTOR_ELT(result, 2)));
    igraph_vector_destroy(&no);
  } else {
    SET_VECTOR_ELT(result, 2, R_NilValue);
  }
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_lastcit_st(SEXP graph, SEXP pal) {
  
  igraph_t g;
  igraph_vector_t al;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pal, &al);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_lastcit_st(&g, &res, &al);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_age(SEXP graph, SEXP pst, SEXP pagebins,
				   SEXP psd, SEXP pno) {
  igraph_t g;
  igraph_vector_t al, sd, no;
  igraph_vector_t *ppsd=&sd, *ppno=&no;
  igraph_vector_t st;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_bool_t lno=LOGICAL(pno)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&al, 0);
  if (lsd) {
    igraph_vector_init(&sd, 0);
  } else {
    ppsd=0;
  }
  if (lno) {
    igraph_vector_init(&no, 0);
  } else {
    ppno=0;
  }
  igraph_measure_dynamics_age(&g, &al, ppsd, ppno, &st, agebins);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(igraph_vector_size(&al)));
  igraph_vector_copy_to(&al, REAL(VECTOR_ELT(result, 0)));
  igraph_vector_destroy(&al);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, NEW_NUMERIC(igraph_vector_size(&sd)));
    igraph_vector_copy_to(&sd, REAL(VECTOR_ELT(result, 1)));
    igraph_vector_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  if (lno) {
    SET_VECTOR_ELT(result, 2, NEW_NUMERIC(igraph_vector_size(&no)));
    igraph_vector_copy_to(&no, REAL(VECTOR_ELT(result, 2)));
    igraph_vector_destroy(&no);
  } else {
    SET_VECTOR_ELT(result, 2, R_NilValue);
  }
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_age_st(SEXP graph, SEXP pal) {
  
  igraph_t g;
  igraph_vector_t al;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pal, &al);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_age_st(&g, &res, &al);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_citedcat(SEXP graph, SEXP pcats,
					SEXP pnocats, SEXP pst, SEXP psd, SEXP pno) {
  igraph_t g;
  igraph_vector_t al, sd, no;
  igraph_vector_t *ppsd=&sd, *ppno=&no;
  igraph_vector_t st;
  igraph_vector_t cats;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_bool_t lno=LOGICAL(pno)[0];
  igraph_integer_t nocats=REAL(pnocats)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&al, 0);
  if (lsd) {
    igraph_vector_init(&sd,0);
  } else {
    ppsd=0;
  }
  if (lno) {
    igraph_vector_init(&no, 0); 
  } else {
    ppno=0;
  }
  igraph_measure_dynamics_citedcat(&g, &cats, nocats, &al, ppsd, ppno, &st);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(igraph_vector_size(&al)));
  igraph_vector_copy_to(&al, REAL(VECTOR_ELT(result, 0)));
  igraph_vector_destroy(&al);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, NEW_NUMERIC(igraph_vector_size(&sd)));
    igraph_vector_copy_to(&sd, REAL(VECTOR_ELT(result, 1)));
    igraph_vector_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  if (lno) {
    SET_VECTOR_ELT(result, 2, NEW_NUMERIC(igraph_vector_size(&no)));
    igraph_vector_copy_to(&no, REAL(VECTOR_ELT(result, 2)));
    igraph_vector_destroy(&no);
  } else {
    SET_VECTOR_ELT(result, 2, R_NilValue);
  }
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_citedcat_st(SEXP graph, SEXP pal, SEXP pcats,
					   SEXP pnocats) {
  igraph_t g;
  igraph_vector_t al, cats;
  igraph_integer_t nocats=REAL(pnocats)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pal, &al);
  R_SEXP_to_vector(pcats, &cats);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_citedcat_st(&g, &res, &al, &cats, nocats);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_citingcat_citedcat(SEXP graph, SEXP pcats,
						  SEXP pnocats, SEXP pst,
						  SEXP psd, SEXP pno) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_integer_t nocats=REAL(pnocats)[0];
  igraph_vector_t st;
  igraph_matrix_t agd, sd, no;
  igraph_matrix_t *ppsd=&sd, *ppno=&no;
  igraph_bool_t lsd=LOGICAL(psd)[0];
  igraph_bool_t lno=LOGICAL(pno)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_vector(pst, &st);
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&agd, 0, 0);
  if (lsd) { igraph_matrix_init(&sd, 0, 0); } else { ppsd=0; }
  if (lno) { igraph_matrix_init(&no, 0, 0); } else { ppno=0; }
  igraph_measure_dynamics_citingcat_citedcat(&g, &agd, ppsd, 
					     ppno, &st, &cats, nocats);
  PROTECT(result=NEW_LIST(3));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&agd));
  igraph_matrix_destroy(&agd);
  if (lsd) {
    SET_VECTOR_ELT(result, 1, R_igraph_matrix_to_SEXP(&sd));
    igraph_matrix_destroy(&sd);
  } else {
    SET_VECTOR_ELT(result, 1, R_NilValue);
  }
  if (lno) {
    SET_VECTOR_ELT(result, 2, R_igraph_matrix_to_SEXP(&no));
    igraph_matrix_destroy(&no);
  } else {
    SET_VECTOR_ELT(result, 2, R_NilValue);
  }
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_measure_dynamics_citingcat_citedcat_st(SEXP graph, 
						     SEXP pagd, SEXP pcats,
						     SEXP pnocats) {
  igraph_t g;
  igraph_matrix_t agd;
  igraph_vector_t cats;
  igraph_integer_t nocats=REAL(pnocats)[0];
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pagd, &agd);
  R_SEXP_to_vector(pcats, &cats);
  igraph_vector_init(&res, 0);
  igraph_measure_dynamics_citingcat_citedcat_st(&g, &res, &agd, &cats, nocats);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_get_shortest_paths(SEXP graph, SEXP pfrom, SEXP pto, 
				 SEXP pmode, SEXP pno) {

  igraph_t g;
  igraph_integer_t from=REAL(pfrom)[0];
  igraph_vs_t to;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_t *vects;
  long int i;
  igraph_vector_ptr_t ptrvec;
  SEXP result;
  
  long int no=REAL(pno)[0];
  
  R_igraph_before();  

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pto, &g, &to);

  igraph_vector_ptr_init(&ptrvec, no);
  vects=(igraph_vector_t*) R_alloc(GET_LENGTH(pto), sizeof(igraph_vector_t));
  for (i=0; i<no; i++) {
    igraph_vector_init(&vects[i], 0);
    VECTOR(ptrvec)[i]=&vects[i];
  }
  igraph_get_shortest_paths(&g, &ptrvec, from, to, mode);
  PROTECT(result=NEW_LIST(no));
  for (i=0; i<no; i++) {
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(&vects[i])));
    igraph_vector_copy_to(&vects[i], REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(&vects[i]);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_are_connected(SEXP graph, SEXP pv1, SEXP pv2) {
  
  igraph_t g;
  igraph_integer_t v1=REAL(pv1)[0];
  igraph_integer_t v2=REAL(pv2)[0];
  igraph_bool_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  PROTECT(result=NEW_LOGICAL(1));
  igraph_are_connected(&g, v1, v2, &res);
  LOGICAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_graph_adjacency(SEXP adjmatrix, SEXP pmode) {
  
  igraph_t g;
  igraph_matrix_t adjm;
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_matrix(adjmatrix, &adjm);
  igraph_adjacency(&g, &adjm, mode);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_average_path_length(SEXP graph, SEXP pdirected, 
				  SEXP punconnected) {
  
  igraph_t g;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t unconnected=LOGICAL(punconnected)[0];
  igraph_real_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_average_path_length(&g, &res, directed, unconnected);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_star(SEXP pn, SEXP pmode, SEXP pcenter) {

  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_integer_t center=REAL(pcenter)[0];
  SEXP result;
  
  R_igraph_before();
  
  igraph_star(&g, n, mode, center);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_ring(SEXP pn, SEXP pdirected, SEXP pmutual, SEXP pcircular) {

  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t mutual=LOGICAL(pmutual)[0];
  igraph_bool_t circular=LOGICAL(pcircular)[0];
  SEXP result;
  
  R_igraph_before();
  
  igraph_ring(&g, n, directed, mutual, circular);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_tree(SEXP pn, SEXP pchildren, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_integer_t children=REAL(pchildren)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;

  R_igraph_before();
  
  igraph_tree(&g, n, children, mode);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_subgraph(SEXP graph, SEXP pvids) {
  
  igraph_t g;
  igraph_t sub;
  igraph_vs_t vs;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_subgraph(&g, &sub, vs);
  PROTECT(result=R_igraph_to_SEXP(&sub));
  igraph_destroy(&sub);
  igraph_vs_destroy(&vs);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_random(SEXP graph) {
  
  igraph_t g;
  igraph_matrix_t res;
  SEXP result=R_NilValue;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_random(&g, &res);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_circle(SEXP graph) {
  
  igraph_t g;
  igraph_matrix_t res;
  SEXP result=R_NilValue;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_circle(&g, &res);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_erdos_renyi_game(SEXP pn, SEXP ptype,
			       SEXP pporm, SEXP pdirected, SEXP ploops) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_integer_t type=REAL(ptype)[0];
  igraph_real_t porm=REAL(pporm)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;
  
  R_igraph_before();
  
  igraph_erdos_renyi_game(&g, type, n, porm, directed, loops);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_full(SEXP pn, SEXP pdirected, SEXP ploops) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;
  
  R_igraph_before();
  
  igraph_full(&g, n, directed, loops);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_random_sample(SEXP plow, SEXP phigh, SEXP plength) {
  
  igraph_vector_t res;
  igraph_integer_t low=REAL(plow)[0];
  igraph_integer_t high=REAL(phigh)[0];
  igraph_integer_t length=REAL(plength)[0];
  SEXP result;

  R_igraph_before();
  
  igraph_vector_init(&res, 0);
  igraph_random_sample(&res, low, high, length);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_get_edgelist(SEXP graph, SEXP pbycol) {
  
  igraph_t g;
  igraph_vector_t res;
  igraph_bool_t bycol=LOGICAL(pbycol)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_get_edgelist(&g, &res, bycol);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_get_adjacency(SEXP graph, SEXP ptype) {
  
  igraph_t g;
  igraph_matrix_t res;
  igraph_integer_t type=REAL(ptype)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_get_adjacency(&g, &res, type);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_simplify(SEXP graph, SEXP pmultiple, SEXP ploops) {
  
  igraph_t g;
  igraph_bool_t multiple=LOGICAL(pmultiple)[0];
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_simplify(&g, multiple, loops);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_fruchterman_reingold(SEXP graph, SEXP pniter, 
					  SEXP pmaxdelta, SEXP parea,
					  SEXP pcoolexp, SEXP prepulserad, 
					  SEXP verbose) {
  igraph_t g;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_real_t maxdelta=REAL(pmaxdelta)[0];
  igraph_real_t area=REAL(parea)[0];
  igraph_real_t coolexp=REAL(pcoolexp)[0];
  igraph_real_t repulserad=REAL(prepulserad)[0];
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before2(verbose, "FR layout");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_fruchterman_reingold(&g, &res, niter, maxdelta, area, 
				     coolexp, repulserad, 0);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after2(verbose);
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_fruchterman_reingold_3d(SEXP graph, SEXP pniter, 
					     SEXP pmaxdelta, SEXP parea,
					     SEXP pcoolexp, SEXP prepulserad,
					     SEXP verbose) {
  igraph_t g;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_real_t maxdelta=REAL(pmaxdelta)[0];
  igraph_real_t area=REAL(parea)[0];
  igraph_real_t coolexp=REAL(pcoolexp)[0];
  igraph_real_t repulserad=REAL(prepulserad)[0];
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before2(verbose, "3D FR layout");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_fruchterman_reingold_3d(&g, &res, niter, maxdelta, area, 
				     coolexp, repulserad, 0);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after2(verbose);
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_degree_sequence_game(SEXP pout_seq, SEXP pin_seq, 
				   SEXP pmethod) {
  igraph_t g;
  igraph_vector_t outseq;
  igraph_vector_t inseq;
  igraph_integer_t method=REAL(pmethod)[0];
  SEXP result;

  R_igraph_before();

  R_SEXP_to_vector(pout_seq, &outseq);
  R_SEXP_to_vector(pin_seq, &inseq);
  igraph_degree_sequence_game(&g, &outseq, &inseq, method);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_transitivity_undirected(SEXP graph) {
  
  igraph_t g;
  igraph_real_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_transitivity_undirected(&g, &res);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_transitivity_avglocal_undirected(SEXP graph) {
  
  igraph_t g;
  igraph_real_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_transitivity_avglocal_undirected(&g, &res);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_transitivity_local_undirected(SEXP graph, SEXP pvids) {
  
  igraph_t g;
  igraph_vs_t vs;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vs);
  igraph_vector_init(&res, 0);
  igraph_transitivity_local_undirected(&g, &res, vs);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vs);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_transitivity_local_undirected_all(SEXP graph) {
  
  igraph_t g;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_transitivity_local_undirected(&g, &res, igraph_vss_all());
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_read_graph_edgelist(SEXP pvfile, SEXP pn, SEXP pdirected) {
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  FILE *file;
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else 
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot read edgelist", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  igraph_read_graph_edgelist(&g, file, n, directed);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_read_graph_gml(SEXP pvfile) {

  igraph_t g;
  FILE *file;
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else 
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot read GML file", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  igraph_read_graph_gml(&g, file);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_read_graph_graphdb(SEXP pvfile, SEXP pdirected) {
  igraph_t g;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  FILE *file;
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "rb");
#else
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "rb");
#endif
  if (file==0) { igraph_error("Cannot read graphdb file", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  igraph_read_graph_graphdb(&g, file, directed);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_write_graph_edgelist(SEXP graph, SEXP file) {
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else
  stream=fopen(CHAR(STRING_ELT(file, 0)), "w");
#endif
  if (stream==0) { igraph_error("Cannot write edgelist", __FILE__, __LINE__,
				IGRAPH_EFILE); }
  igraph_write_graph_edgelist(&g, stream);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else 
  PROTECT(result=NEW_NUMERIC(0));
#endif
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_write_graph_gml(SEXP graph, SEXP file, SEXP pid, SEXP pcreator) {
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  igraph_vector_t id, *ppid=0;
  const char *creator=0;
  SEXP result;
  
  R_igraph_before();
  
  if (!isNull(pid)) { R_SEXP_to_vector(pid, &id); ppid=&id; }
  if (!isNull(pcreator)) { creator=CHAR(STRING_ELT(pcreator, 0)); } 
  R_SEXP_to_igraph(graph, &g);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else
  stream=fopen(CHAR(STRING_ELT(file, 0)), "w");
#endif
  if (stream==0) { igraph_error("Cannot write edgelist", __FILE__, __LINE__,
				IGRAPH_EFILE); }
  igraph_write_graph_gml(&g, stream, ppid, creator);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else 
  PROTECT(result=NEW_NUMERIC(0));
#endif
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_write_graph_pajek(SEXP graph, SEXP file) {
  
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else
  stream=fopen(CHAR(STRING_ELT(file, 0)), "wb");
#endif
  if (stream==0) { igraph_error("Cannot write oajek file", __FILE__, __LINE__,
				IGRAPH_EFILE); }
  igraph_write_graph_pajek(&g, stream);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else 
  PROTECT(result=NEW_NUMERIC(0));
#endif
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_read_graph_ncol(SEXP pvfile, SEXP ppredef,
			      SEXP pnames, SEXP pweights,
			      SEXP pdirected) {
  igraph_t g;
  igraph_bool_t names=LOGICAL(pnames)[0];
  igraph_bool_t weights=LOGICAL(pweights)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  FILE *file;
  igraph_strvector_t predef, *predefptr=0;  
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else 
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot read edgelist", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  if (GET_LENGTH(ppredef)>0) {
    R_igraph_SEXP_to_strvector(ppredef, &predef);
    predefptr=&predef;
  } 
  igraph_read_graph_ncol(&g, file, predefptr, names, weights, directed);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_write_graph_ncol(SEXP graph, SEXP file, SEXP pnames, 
			       SEXP pweights) {
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  const char *names, *weights;
  SEXP result;

  R_igraph_before();
  
  if (isNull(pnames)) {
    names=0; 
  } else {
    names=CHAR(STRING_ELT(pnames, 0));
  } 
  if (isNull(pweights)) {
    weights=0; 
  } else {
    weights=CHAR(STRING_ELT(pweights, 0));
  }   

  R_SEXP_to_igraph(graph, &g);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else 
  stream=fopen(CHAR(STRING_ELT(file,0)), "w");
#endif
  if (stream==0) { igraph_error("Cannot write .ncol file", __FILE__, __LINE__,
				IGRAPH_EFILE); }
  igraph_write_graph_ncol(&g, stream, names, weights);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else
  PROTECT(result=NEW_NUMERIC(0));
#endif
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_read_graph_lgl(SEXP pvfile, SEXP pnames, SEXP pweights) {
  igraph_t g;
  igraph_bool_t names=LOGICAL(pnames)[0];
  igraph_bool_t weights=LOGICAL(pweights)[0];
  FILE *file;
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else 
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot read edgelist", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  igraph_read_graph_lgl(&g, file, names, weights);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_write_graph_lgl(SEXP graph, SEXP file, SEXP pnames, 
			      SEXP pweights, SEXP pisolates) {
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  const char *names, *weights;
  igraph_bool_t isolates=LOGICAL(pisolates)[0];
  SEXP result;

  R_igraph_before();
  
  if (isNull(pnames)) {
    names=0; 
  } else {
    names=CHAR(STRING_ELT(pnames, 0));
  } 
  if (isNull(pweights)) {
    weights=0; 
  } else {
    weights=CHAR(STRING_ELT(pweights, 0));
  }   

  R_SEXP_to_igraph(graph, &g);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else
  stream=fopen(CHAR(STRING_ELT(file, 0)), "w");
#endif
  igraph_write_graph_lgl(&g, stream, names, weights, isolates);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else
  PROTECT(result=NEW_NUMERIC(0));
#endif
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_read_graph_pajek(SEXP pvfile) {
  igraph_t g;
  FILE *file;  
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot read Pajek file", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  igraph_read_graph_pajek(&g, file);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
} 

SEXP R_igraph_decompose(SEXP graph, SEXP pmode, SEXP pmaxcompno, 
			SEXP pminelements) {

  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_integer_t maxcompno=REAL(pmaxcompno)[0];
  igraph_integer_t minelements=REAL(pminelements)[0];
  igraph_vector_ptr_t comps;
  SEXP result;
  long int i;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&comps, 0);
  IGRAPH_FINALLY(igraph_vector_ptr_destroy, &comps);
  igraph_decompose(&g, &comps, mode, maxcompno, minelements);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&comps)));
  for (i=0; i<igraph_vector_ptr_size(&comps); i++) {
    SET_VECTOR_ELT(result, i, R_igraph_to_SEXP(VECTOR(comps)[i]));
    igraph_destroy(VECTOR(comps)[i]);
    igraph_free(VECTOR(comps)[i]);
  }
  igraph_vector_ptr_destroy(&comps);
  IGRAPH_FINALLY_CLEAN(1);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;  
}

SEXP R_igraph_atlas(SEXP pno) {
  
  int no=REAL(pno)[0];
  igraph_t g;
  SEXP result;
  
  R_igraph_before();

  igraph_atlas(&g, no);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_random_3d(SEXP graph) {
  
  igraph_t g;
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_random_3d(&g, &res);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);  
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_sphere(SEXP graph) {

  igraph_t g;
  igraph_matrix_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_sphere(&g, &res);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_isoclass_34(SEXP graph) {
  
  igraph_t g;
  int class;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_isoclass(&g, &class);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=class;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_isomorphic_34(SEXP graph1, SEXP graph2) {
  
  igraph_t g1, g2;
  igraph_bool_t res;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph1, &g1);
  R_SEXP_to_igraph(graph2, &g2);
  igraph_isomorphic(&g1, &g2, &res);
  PROTECT(result=NEW_LOGICAL(1));
  LOGICAL(result)[0]=(res>0);
    
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_callaway_traits_game(SEXP pnodes, SEXP ptypes, 
				  SEXP pepers, SEXP ptype_dist,
				  SEXP pmatrix, SEXP pdirected) {

  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t types=REAL(ptypes)[0];
  igraph_integer_t epers=REAL(pepers)[0];
  igraph_vector_t type_dist;
  igraph_matrix_t matrix;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result; 

  R_igraph_before();
  
  R_SEXP_to_vector(ptype_dist, &type_dist);
  R_SEXP_to_matrix(pmatrix, &matrix);
  igraph_callaway_traits_game(&g, nodes, types, epers, &type_dist,
			      &matrix, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_establishment_game(SEXP pnodes, SEXP ptypes, SEXP pk,
				 SEXP ptype_dist, SEXP pmatrix,
				 SEXP pdirected) {
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t types=REAL(ptypes)[0];
  igraph_integer_t k=REAL(pk)[0];
  igraph_vector_t type_dist;
  igraph_matrix_t matrix;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(ptype_dist, &type_dist);
  R_SEXP_to_matrix(pmatrix, &matrix);
  igraph_establishment_game(&g, nodes, types, k, &type_dist, &matrix,
			    directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
			    
SEXP R_igraph_motifs_randesu(SEXP graph, SEXP psize, SEXP pcutprob) {
  igraph_t g;
  igraph_integer_t size=REAL(psize)[0];
  igraph_vector_t cutprob;
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();

  R_SEXP_to_vector(pcutprob, &cutprob);
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_motifs_randesu(&g, &res, size, &cutprob);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_motifs_randesu_no(SEXP graph, SEXP psize, SEXP pcutprob) {
  igraph_t g;
  igraph_integer_t size=REAL(psize)[0];
  igraph_vector_t cutprob;
  igraph_integer_t res;
  SEXP result;

  R_igraph_before();

  R_SEXP_to_vector(pcutprob, &cutprob);
  R_SEXP_to_igraph(graph, &g);
  igraph_motifs_randesu_no(&g, &res, size, &cutprob);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_motifs_randesu_estimate(SEXP graph, SEXP psize, SEXP pcutprob,
				      SEXP psamplesize, SEXP psample) {
  igraph_t g;
  igraph_integer_t size=REAL(psize)[0];
  igraph_vector_t cutprob;
  igraph_integer_t samplesize=REAL(psamplesize)[0];
  igraph_vector_t sample;
  igraph_vector_t *sampleptr=0;
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_vector(pcutprob, &cutprob);
  if (GET_LENGTH(psample) != 0) {
    R_SEXP_to_vector(psample, &sample);
    sampleptr=&sample;
  }
  R_SEXP_to_igraph(graph, &g);
  igraph_motifs_randesu_estimate(&g, &res, size, &cutprob, samplesize,
				 sampleptr);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_get_all_shortest_paths(SEXP graph, SEXP pfrom, SEXP pto,
				     SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t from=REAL(pfrom)[0];
  igraph_vs_t to;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_ptr_t res;
  SEXP result;
  long int i;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pto, &g, &to);
  igraph_vector_ptr_init(&res, 0);
  igraph_get_all_shortest_paths(&g, &res, 0, from, to, mode);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&res)));
  for (i=0; i<igraph_vector_ptr_size(&res); i++) {
    long int len=igraph_vector_size(VECTOR(res)[i]);
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(len));
    igraph_vector_copy_to(VECTOR(res)[i], REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(VECTOR(res)[i]);
    igraph_free(VECTOR(res)[i]);
  }
  igraph_vector_ptr_destroy(&res);
  igraph_vs_destroy(&to);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_isoclass_create(SEXP psize, SEXP pnumber, SEXP pdirected) {

  igraph_t g;
  igraph_integer_t size=REAL(psize)[0];
  igraph_integer_t number=REAL(pnumber)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();

  igraph_isoclass_create(&g, size, number, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_layout_merge_dla(SEXP graphs, SEXP layouts, SEXP verbose) {

  igraph_vector_ptr_t graphvec;
  igraph_vector_ptr_t ptrvec;
  igraph_t *gras;
  igraph_matrix_t *mats;
  igraph_matrix_t res;
  long int i;
  SEXP result;
  
  R_igraph_before2(verbose, "Merge layouts");
  
  igraph_vector_ptr_init(&graphvec, GET_LENGTH(graphs));
  igraph_vector_ptr_init(&ptrvec, GET_LENGTH(layouts));
  gras=(igraph_t*)R_alloc(GET_LENGTH(graphs), sizeof(igraph_t));
  mats=(igraph_matrix_t*)R_alloc(GET_LENGTH(layouts),
				 sizeof(igraph_matrix_t));
  for (i=0; i<GET_LENGTH(graphs); i++) {
    R_SEXP_to_igraph(VECTOR_ELT(graphs, i), &gras[i]);
    VECTOR(graphvec)[i]=&gras[i];
  }
  for (i=0; i<GET_LENGTH(layouts); i++) {
    R_SEXP_to_matrix(VECTOR_ELT(layouts, i), &mats[i]);
    VECTOR(ptrvec)[i]=&mats[i];
  }
  igraph_matrix_init(&res, 0, 0);
  igraph_layout_merge_dla(&graphvec, &ptrvec, &res);
  igraph_vector_ptr_destroy(&graphvec);
  igraph_vector_ptr_destroy(&ptrvec);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after2(verbose);
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_disjoint_union(SEXP pgraphs) {
  
  igraph_vector_ptr_t ptrvec;
  igraph_t *graphs;
  igraph_t res;
  long int i;
  SEXP result;

  R_igraph_before();
  
  igraph_vector_ptr_init(&ptrvec, GET_LENGTH(pgraphs));
  graphs=(igraph_t *)R_alloc(GET_LENGTH(pgraphs),
			     sizeof(igraph_t));
  for (i=0; i<GET_LENGTH(pgraphs); i++) {
    R_SEXP_to_igraph(VECTOR_ELT(pgraphs, i), &graphs[i]);
    VECTOR(ptrvec)[i]=&graphs[i];
  }
  igraph_disjoint_union_many(&res, &ptrvec);
  igraph_vector_ptr_destroy(&ptrvec);
  PROTECT(result=R_igraph_to_SEXP(&res));
  igraph_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_union(SEXP pgraphs) {
  
  igraph_vector_ptr_t ptrvec;
  igraph_t *graphs;
  igraph_t res;
  long int i;
  SEXP result;

  R_igraph_before();
  
  igraph_vector_ptr_init(&ptrvec, GET_LENGTH(pgraphs));
  graphs=(igraph_t *)R_alloc(GET_LENGTH(pgraphs),
			     sizeof(igraph_t));
  for (i=0; i<GET_LENGTH(pgraphs); i++) {
    R_SEXP_to_igraph(VECTOR_ELT(pgraphs, i), &graphs[i]);
    VECTOR(ptrvec)[i]=&graphs[i];
  }
  igraph_union_many(&res, &ptrvec);
  igraph_vector_ptr_destroy(&ptrvec);
  PROTECT(result=R_igraph_to_SEXP(&res));
  igraph_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_intersection(SEXP pgraphs) {
  
  igraph_vector_ptr_t ptrvec;
  igraph_t *graphs;
  igraph_t res;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  igraph_vector_ptr_init(&ptrvec, GET_LENGTH(pgraphs));
  graphs=(igraph_t *)R_alloc(GET_LENGTH(pgraphs),
			     sizeof(igraph_t));
  for (i=0; i<GET_LENGTH(pgraphs); i++) {
    R_SEXP_to_igraph(VECTOR_ELT(pgraphs, i), &graphs[i]);
    VECTOR(ptrvec)[i]=&graphs[i];
  }
  igraph_intersection_many(&res, &ptrvec);
  igraph_vector_ptr_destroy(&ptrvec);
  PROTECT(result=R_igraph_to_SEXP(&res));
  igraph_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_difference(SEXP pleft, SEXP pright) {
  
  igraph_t left, right;
  igraph_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(pleft, &left);
  R_SEXP_to_igraph(pright, &right);
  igraph_difference(&res, &left, &right);
  PROTECT(result=R_igraph_to_SEXP(&res));
  igraph_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_complementer(SEXP pgraph, SEXP ploops) {
  
  igraph_t g;
  igraph_t res;
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(pgraph, &g);
  igraph_complementer(&res, &g, loops);
  PROTECT(result=R_igraph_to_SEXP(&res));
  igraph_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_compose(SEXP pleft, SEXP pright) {
  
  igraph_t left, right;
  igraph_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(pleft, &left);
  R_SEXP_to_igraph(pright, &right);
  igraph_compose(&res, &left, &right);
  PROTECT(result=R_igraph_to_SEXP(&res));
  igraph_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_barabasi_aging_game(SEXP pn, SEXP ppa_exp, SEXP paging_exp,
				  SEXP paging_bin, SEXP pm, SEXP pout_seq,
				  SEXP pout_pref, SEXP pzero_deg_appeal,
				  SEXP pzero_age_appeal, SEXP pdeg_coef,
				  SEXP page_coef,
				  SEXP pdirected) {
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_real_t pa_exp=REAL(ppa_exp)[0];
  igraph_real_t aging_exp=REAL(paging_exp)[0];
  igraph_integer_t aging_bin=REAL(paging_bin)[0];
  igraph_integer_t m=REAL(pm)[0];
  igraph_vector_t out_seq;
  igraph_bool_t out_pref=LOGICAL(pout_pref)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_real_t zero_deg_appeal=REAL(pzero_deg_appeal)[0];
  igraph_real_t zero_age_appeal=REAL(pzero_age_appeal)[0];
  igraph_real_t deg_coef=REAL(pdeg_coef)[0];
  igraph_real_t age_coef=REAL(page_coef)[0];  
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pout_seq, &out_seq);  
  
  igraph_barabasi_aging_game(&g, n, m, &out_seq, out_pref, pa_exp, aging_exp,
			     aging_bin, zero_deg_appeal, zero_age_appeal,
			     deg_coef, age_coef, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_recent_degree_aging_game(SEXP pn, SEXP ppa_exp, SEXP paging_exp,
				       SEXP paging_bin, SEXP pm, SEXP pout_seq,
				       SEXP pout_pref, SEXP pzero_appeal, 
				       SEXP pdirected,
				       SEXP ptime_window) {
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_real_t pa_exp=REAL(ppa_exp)[0];
  igraph_real_t aging_exp=REAL(paging_exp)[0];
  igraph_integer_t aging_bin=REAL(paging_bin)[0];
  igraph_integer_t m=REAL(pm)[0];
  igraph_vector_t out_seq;
  igraph_bool_t out_pref=LOGICAL(pout_pref)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_integer_t time_window=REAL(ptime_window)[0];
  igraph_real_t zero_appeal=REAL(pzero_appeal)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pout_seq, &out_seq);  
  
  igraph_recent_degree_aging_game(&g, n, m, &out_seq, out_pref, pa_exp, 
				  aging_exp, aging_bin, time_window, 
				  zero_appeal, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_get_edge(SEXP graph, SEXP peid) {
  
  igraph_t g;
  igraph_integer_t eid=REAL(peid)[0];
  igraph_integer_t from, to;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);  
  igraph_edge(&g, eid, &from, &to);
  PROTECT(result=NEW_NUMERIC(2));
  REAL(result)[0]=from;
  REAL(result)[1]=to;
  
  R_igraph_after();

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_edges(SEXP graph, SEXP eids) {
  igraph_t g;
  igraph_es_t es;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_es(eids, &g, &es);
  igraph_vector_init(&res, 0);
  igraph_edges(&g, es, &res);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_es_destroy(&es);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}  

SEXP R_igraph_constraint(SEXP graph, SEXP vids, SEXP pweights) {
  
  igraph_t g;
  igraph_vs_t vs;
  igraph_vector_t weights, *wptr=0;
  igraph_vector_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(vids, &g, &vs);
  if (GET_LENGTH(pweights) != 0) {
    R_SEXP_to_vector(pweights, &weights);
    wptr=&weights;
  } 
  igraph_vector_init(&res, 0);
  igraph_constraint(&g, &res, vs, wptr);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vs);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_es_path(SEXP graph, SEXP pp, SEXP pdir) {
  
  igraph_t g;
  igraph_vector_t p;
  igraph_bool_t dir=LOGICAL(pdir)[0];
  igraph_es_t es;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pp, &p);
  igraph_es_path(&es, &p, dir);
  igraph_vector_init(&res, 0);
  igraph_es_as_vector(&g, es, &res);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_es_destroy(&es);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_es_pairs(SEXP graph, SEXP pp, SEXP pdir) {

  igraph_t g;
  igraph_vector_t p;
  igraph_bool_t dir=LOGICAL(pdir)[0];
  igraph_es_t es;
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pp, &p);
  igraph_es_pairs(&es, &p, dir);
  igraph_vector_init(&res, 0);
  igraph_es_as_vector(&g, es, &res);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_es_destroy(&es);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_pagerank(SEXP graph, SEXP pvids, SEXP pdirected, 
		       SEXP pniter, SEXP peps, SEXP pdamping) {
  
  igraph_t g;
  igraph_vs_t vids;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_real_t eps=REAL(peps)[0];
  igraph_real_t damping=REAL(pdamping)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vids);
  igraph_vector_init(&res, 0);
  igraph_pagerank(&g, &res, vids, directed, niter, eps, damping);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vids);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_reciprocity(SEXP graph, SEXP pignore_loops) {
  
  igraph_t g;
  igraph_bool_t ignore_loops=LOGICAL(pignore_loops)[0];
  igraph_real_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_reciprocity(&g, &res, ignore_loops);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_layout_reingold_tilford(SEXP graph, SEXP proot, SEXP pcirc) {
  
  igraph_t g;
  igraph_integer_t root=REAL(proot)[0];
  igraph_matrix_t res;
  igraph_bool_t circ=LOGICAL(pcirc)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  if (!circ) {
    igraph_layout_reingold_tilford(&g, &res, root);
  } else {
    igraph_layout_reingold_tilford_circular(&g, &res, root);
  }
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_rewire(SEXP graph, SEXP pn, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_rewiring_t mode=REAL(pmode)[0];
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph_copy(graph, &g);
  igraph_rewire(&g, n, mode);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
  
SEXP R_igraph_to_directed(SEXP graph, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_to_directed(&g, mode);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_to_undirected(SEXP graph, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_to_undirected(&g, mode);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_read_graph_graphml(SEXP pvfile, SEXP pindex) {
  igraph_t g;
  int index=REAL(pindex)[0];
  FILE *file;
  SEXP result;

  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot open GraphML file", __FILE__, __LINE__,
			      IGRAPH_EFILE); }
  igraph_read_graph_graphml(&g, file, index);
  fclose(file);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_write_graph_graphml(SEXP graph, SEXP file) {
  
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  SEXP result;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else
  stream=fopen(CHAR(STRING_ELT(file, 0)), "w");
#endif
  if (stream==0) { igraph_error("Cannot write GraphML file", __FILE__, 
				__LINE__, IGRAPH_EFILE); }
  igraph_write_graph_graphml(&g, stream);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else
  PROTECT(result=NEW_NUMERIC(0));
#endif

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_vs_nei(SEXP graph, SEXP px, SEXP pv, SEXP pmode) {
  
  igraph_t g;
  igraph_vs_t v;
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;

  igraph_vit_t vv;
  igraph_vector_t neis;
  long int i;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pv, &g, &v);

  igraph_vector_init(&neis, 0);  
  igraph_vit_create(&g, v, &vv);
  PROTECT(result=NEW_LOGICAL(igraph_vcount(&g)));
  memset(LOGICAL(result), 0, sizeof(LOGICAL(result)[0])*igraph_vcount(&g));
  
  while (!IGRAPH_VIT_END(vv)) {
    igraph_neighbors(&g, &neis, IGRAPH_VIT_GET(vv), mode);
    for (i=0; i<igraph_vector_size(&neis); i++) {
      long int nei=VECTOR(neis)[i];
      LOGICAL(result)[nei]=1;
    }
    IGRAPH_VIT_NEXT(vv);
  }

  igraph_vit_destroy(&vv);
  igraph_vector_destroy(&neis);
  igraph_vs_destroy(&v);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_vs_adj(SEXP graph, SEXP px, SEXP pe, SEXP pmode) {
  
  igraph_t g;
  igraph_es_t e;
  int mode=REAL(pmode)[0];
  SEXP result;

  igraph_integer_t from, to;
  igraph_eit_t ee;

  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_es(pe, &g, &e);
  
  igraph_eit_create(&g, e, &ee);
  PROTECT(result=NEW_LOGICAL(igraph_vcount(&g)));
  memset(LOGICAL(result), 0, sizeof(LOGICAL(result)[0])*igraph_vcount(&g));

  while (!IGRAPH_EIT_END(ee)) {
    igraph_edge(&g, IGRAPH_EIT_GET(ee), &from, &to);
    if (mode & 1) { 
      LOGICAL(result)[ (long int)from]=1;
    }
    if (mode & 2) {
      LOGICAL(result)[ (long int)to]=1;
    }
    IGRAPH_EIT_NEXT(ee);
  }
  
  igraph_eit_destroy(&ee);
  igraph_es_destroy(&e);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_es_adj(SEXP graph, SEXP x, SEXP pv, SEXP pmode) {
  
  igraph_t g;
  igraph_vs_t v;
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;
  
  igraph_vector_t adje;
  igraph_vit_t vv;
  long int i;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pv, &g, &v);
  
  igraph_vit_create(&g, v, &vv);
  igraph_vector_init(&adje, 0);
  PROTECT(result=NEW_LOGICAL(igraph_ecount(&g)));
  memset(LOGICAL(result), 0, sizeof(LOGICAL(result)[0])*igraph_ecount(&g));
  
  while (!IGRAPH_VIT_END(vv)) {
    igraph_adjacent(&g, &adje, IGRAPH_VIT_GET(vv), mode);
    for (i=0; i<igraph_vector_size(&adje); i++) {
      long int edge=VECTOR(adje)[i];
      LOGICAL(result)[edge]=1;
    }
    IGRAPH_VIT_NEXT(vv);
  }

  igraph_vector_destroy(&adje);
  igraph_vit_destroy(&vv);
  igraph_vs_destroy(&v);

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_grg_game(SEXP pn, SEXP pradius, SEXP ptorus) {
  
  igraph_t g;
  igraph_integer_t n=REAL(pn)[0];
  igraph_real_t radius=REAL(pradius)[0];
  igraph_bool_t torus=LOGICAL(ptorus)[0];
  SEXP result;

  R_igraph_before();
  
  igraph_grg_game(&g, n, radius, torus);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_density(SEXP graph, SEXP ploops) {
  
  igraph_t g;
  igraph_bool_t loops=LOGICAL(ploops)[0];
  igraph_real_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_density(&g, &res, loops);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_maxflow(SEXP graph, SEXP psource, SEXP ptarget, 
		      SEXP pcapacity) {

  igraph_t g;
  igraph_integer_t source=REAL(psource)[0], target=REAL(ptarget)[0];
  igraph_vector_t capacity;
  igraph_real_t value;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcapacity, &capacity);
  igraph_maxflow_value(&g, &value, source, target, &capacity);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=value;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_read_graph_dimacs(SEXP pvfile, SEXP pdirected) {
  
  igraph_t g;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  FILE *file;
  igraph_integer_t source, target;
  igraph_vector_t cap;
  SEXP result;
  
  R_igraph_before();
  
#if HAVE_FMEMOPEN == 1
  file=fmemopen(RAW(pvfile), GET_LENGTH(pvfile), "r");
#else
  file=fopen(CHAR(STRING_ELT(pvfile, 0)), "r");
#endif
  if (file==0) { igraph_error("Cannot read edgelist", __FILE__, __LINE__,
			      IGRAPH_EFILE); 
  }
  igraph_vector_init(&cap, 0);
  igraph_read_graph_dimacs(&g, file, &source, &target, &cap, directed);
  fclose(file);
  PROTECT(result=NEW_LIST(4));
  SET_VECTOR_ELT(result, 0, R_igraph_to_SEXP(&g));
  SET_VECTOR_ELT(result, 1, NEW_NUMERIC(1));
  REAL(VECTOR_ELT(result, 1))[0]=source;
  SET_VECTOR_ELT(result, 2, NEW_NUMERIC(1));
  REAL(VECTOR_ELT(result, 2))[0]=target;
  SET_VECTOR_ELT(result, 3, NEW_NUMERIC(igraph_vector_size(&cap)));
  igraph_vector_copy_to(&cap, REAL(VECTOR_ELT(result,3)));
  igraph_destroy(&g);
  igraph_vector_destroy(&cap);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_write_graph_dimacs(SEXP graph, SEXP file,
				 SEXP psource, SEXP ptarget,
				 SEXP pcap) {
  
  igraph_t g;
  FILE *stream;
  char *bp;
  size_t size;
  igraph_integer_t source=REAL(psource)[0];
  igraph_integer_t target=REAL(ptarget)[0];
  igraph_vector_t cap;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcap, &cap);
#if HAVE_OPEN_MEMSTREAM == 1
  stream=open_memstream(&bp, &size);
#else
  stream=fopen(CHAR(STRING_ELT(file, 0)), "w");
#endif
  if (stream==0) { igraph_error("Cannot write edgelist", __FILE__, __LINE__,
				IGRAPH_EFILE); 
  }
  igraph_write_graph_dimacs(&g, stream, source, target, &cap);
  fclose(stream);
#if HAVE_OPEN_MEMSTREAM == 1
  PROTECT(result=allocVector(RAWSXP, size));
  memcpy(RAW(result), bp, sizeof(char)*size);
  free(bp);
#else
  PROTECT(result=NEW_NUMERIC(0));
#endif
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_mincut_value(SEXP graph, SEXP pcapacity) {
  
  igraph_t g;
  igraph_vector_t capacity;
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcapacity, &capacity);
  igraph_mincut_value(&g, &res, &capacity);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_st_vertex_connectivity(SEXP graph, SEXP psource, 
				     SEXP ptarget) {
  
  igraph_t g;
  igraph_integer_t source=REAL(psource)[0], target=REAL(ptarget)[0];
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_st_vertex_connectivity(&g, &res, source, target, 
				IGRAPH_VCONN_NEI_ERROR);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_vertex_connectivity(SEXP graph, SEXP pchecks) {
  
  igraph_t g;
  igraph_integer_t res;
  igraph_bool_t checks=LOGICAL(pchecks)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vertex_connectivity(&g, &res, checks);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;

  UNPROTECT(1);
  return result;
}

SEXP R_igraph_st_edge_connectivity(SEXP graph, SEXP psource, SEXP ptarget) {
  
  igraph_t g;
  igraph_integer_t source=REAL(psource)[0], target=REAL(ptarget)[0];
  igraph_real_t value;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_st_edge_connectivity(&g, &value, source, target);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=value;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_edge_connectivity(SEXP graph, SEXP pchecks) {
  
  igraph_t g;
  igraph_integer_t res;
  igraph_bool_t checks=LOGICAL(pchecks)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_edge_connectivity(&g, &res, checks);
  
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_st_mincut_value(SEXP graph, SEXP psource, SEXP ptarget,
			      SEXP pcapacity) {
  igraph_t g;
  igraph_integer_t source=REAL(psource)[0];
  igraph_integer_t target=REAL(ptarget)[0];
  igraph_vector_t capacity;
  igraph_real_t res;
  SEXP result;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcapacity, &capacity);
  igraph_st_mincut_value(&g, &res, source, target, &capacity);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_edge_disjoint_paths(SEXP graph, SEXP psource, SEXP ptarget) {
  
  igraph_t g;
  igraph_integer_t source=REAL(psource)[0];
  igraph_integer_t target=REAL(ptarget)[0];
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_edge_disjoint_paths(&g, &res, source, target);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_vertex_disjoint_paths(SEXP graph, SEXP psource, SEXP ptarget) {
  
  igraph_t g;
  igraph_integer_t source=REAL(psource)[0];
  igraph_integer_t target=REAL(ptarget)[0];
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vertex_disjoint_paths(&g, &res, source, target);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;

  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_adhesion(SEXP graph, SEXP pchecks) {
  
  igraph_t g;
  igraph_integer_t res;
  igraph_bool_t checks=LOGICAL(pchecks)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_adhesion(&g, &res, checks);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_cohesion(SEXP graph, SEXP pchecks) {
  
  igraph_t g;
  igraph_integer_t res;
  igraph_bool_t checks=LOGICAL(pchecks)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_cohesion(&g, &res, checks);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_spinglass_community(SEXP graph, SEXP pweights,
				  SEXP pspins, SEXP pparupdate,
				  SEXP pstarttemp, SEXP pstoptemp,
				  SEXP pcoolfact, SEXP pupdate_rule,
				  SEXP pgamma) {
  igraph_t g;
  igraph_vector_t weights;
  igraph_integer_t spins=REAL(pspins)[0];
  igraph_bool_t parupdate=LOGICAL(pparupdate)[0];
  igraph_real_t starttemp=REAL(pstarttemp)[0];
  igraph_real_t stoptemp=REAL(pstoptemp)[0];
  igraph_real_t coolfact=REAL(pcoolfact)[0];
  igraph_spincomm_update_t update_rule=REAL(pupdate_rule)[0];
  igraph_real_t gamma=REAL(pgamma)[0];
  igraph_real_t modularity;
  igraph_real_t temperature;
  igraph_vector_t membership;
  igraph_vector_t csize;
  SEXP result, names;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pweights, &weights);
  igraph_vector_init(&membership, 0);
  igraph_vector_init(&csize, 0);
  igraph_community_spinglass(&g, &weights, 
			     &modularity, &temperature, 
			     &membership, &csize,
			     spins, parupdate, starttemp, stoptemp,
			     coolfact, update_rule, gamma);
  
  PROTECT(result=NEW_LIST(4));
  PROTECT(names=NEW_CHARACTER(4));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(igraph_vector_size(&membership)));
  SET_VECTOR_ELT(result, 1, NEW_NUMERIC(igraph_vector_size(&csize)));
  SET_VECTOR_ELT(result, 2, NEW_NUMERIC(1));
  SET_VECTOR_ELT(result, 3, NEW_NUMERIC(1));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("membership"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("csize"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("modularity"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("temperature"));
  SET_NAMES(result, names);
  igraph_vector_copy_to(&membership, REAL(VECTOR_ELT(result, 0)));
  igraph_vector_copy_to(&csize, REAL(VECTOR_ELT(result, 1)));
  REAL(VECTOR_ELT(result, 2))[0]=modularity;
  REAL(VECTOR_ELT(result, 3))[0]=temperature;
  
  igraph_vector_destroy(&membership);
  igraph_vector_destroy(&csize);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_spinglass_my_community(SEXP graph, SEXP pweights,
				     SEXP pvertex, SEXP pspins,
				     SEXP pupdate_rule, SEXP pgamma) {
  igraph_t g;
  igraph_vector_t weights;
  igraph_integer_t vertex=REAL(pvertex)[0];
  igraph_integer_t spins=REAL(pspins)[0];
  igraph_spincomm_update_t update_rule=REAL(pupdate_rule)[0];
  igraph_real_t gamma=REAL(pgamma)[0];
  igraph_vector_t community;
  igraph_real_t cohesion;
  igraph_real_t adhesion;
  igraph_integer_t inner_links;
  igraph_integer_t outer_links;
  SEXP result, names;

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pweights, &weights);
  igraph_vector_init(&community, 0);
  igraph_community_spinglass_single(&g, &weights, vertex, &community,
				    &cohesion, &adhesion, &inner_links,
				    &outer_links, spins, update_rule, gamma);
  
  PROTECT(result=NEW_LIST(5));
  PROTECT(names=NEW_CHARACTER(5));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(igraph_vector_size(&community)));
  SET_VECTOR_ELT(result, 1, NEW_NUMERIC(1));
  SET_VECTOR_ELT(result, 2, NEW_NUMERIC(1));
  SET_VECTOR_ELT(result, 3, NEW_NUMERIC(1));
  SET_VECTOR_ELT(result, 4, NEW_NUMERIC(1));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("community"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("cohesion"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("adhesion"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("inner.links"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("outer.links"));
  SET_NAMES(result, names);
  igraph_vector_copy_to(&community, REAL(VECTOR_ELT(result, 0)));
  REAL(VECTOR_ELT(result, 1))[0] = cohesion;
  REAL(VECTOR_ELT(result, 2))[0] = adhesion;
  REAL(VECTOR_ELT(result, 3))[0] = inner_links;
  REAL(VECTOR_ELT(result, 4))[0] = outer_links;
  
  igraph_vector_destroy(&community);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_extended_chordal_ring(SEXP pnodes, SEXP pw) {

  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_matrix_t w;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_matrix(pw, &w);
  igraph_extended_chordal_ring(&g, nodes, &w);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_no_clusters(SEXP graph, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_integer_t res;
  SEXP result;

  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  igraph_clusters(&g, 0, 0, &res, mode);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_neighborhood_size(SEXP graph, SEXP pvids, SEXP porder, 
				SEXP pmode) {
  igraph_t g;
  igraph_vs_t vids;
  igraph_integer_t order=REAL(porder)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vids);
  igraph_vector_init(&res, 0);
  igraph_neighborhood_size(&g, &res, vids, order, mode);
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  igraph_vs_destroy(&vids);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_neighborhood(SEXP graph, SEXP pvids, SEXP porder, 
			   SEXP pmode) {
  igraph_t g;
  igraph_vs_t vids;
  igraph_integer_t order=REAL(porder)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_ptr_t res;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vids);
  igraph_vector_ptr_init(&res, 0);
  igraph_neighborhood(&g, &res, vids, order, mode);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&res)));
  for (i=0; i<igraph_vector_ptr_size(&res); i++) {
    igraph_vector_t *v=VECTOR(res)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(v)));
    igraph_vector_copy_to(v, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(v);
    igraph_free(v);
  }
  igraph_vector_ptr_destroy(&res);
  igraph_vs_destroy(&vids);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_neighborhood_graphs(SEXP graph, SEXP pvids, SEXP porder, 
				  SEXP pmode) {
  igraph_t g;
  igraph_vs_t vids;
  igraph_integer_t order=REAL(porder)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_ptr_t res;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_igraph_vs(pvids, &g, &vids);
  igraph_vector_ptr_init(&res, 0);
  igraph_neighborhood_graphs(&g, &res, vids, order, mode);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&res)));
  for (i=0; i<igraph_vector_ptr_size(&res); i++) {
    igraph_t *g=VECTOR(res)[i];
    SET_VECTOR_ELT(result, i, R_igraph_to_SEXP(g));
    igraph_destroy(g);
    igraph_free(g);
  }
  igraph_vector_ptr_destroy(&res);
  igraph_vs_destroy(&vids);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_preference_game(SEXP pnodes, SEXP ptypes,
			      SEXP ptype_dist, SEXP pmatrix,
			      SEXP pdirected, SEXP ploops) {
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t types=REAL(ptypes)[0];
  igraph_vector_t type_dist;
  igraph_matrix_t matrix;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(ptype_dist, &type_dist);
  R_SEXP_to_matrix(pmatrix, &matrix);
  igraph_preference_game(&g, nodes, types, &type_dist, &matrix, 0,
			 directed, loops);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_asymmetric_preference_game(SEXP pnodes, SEXP ptypes,
					 SEXP ptype_dist_matrix, SEXP pmatrix,
					 SEXP ploops) {
  
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t types=REAL(ptypes)[0];
  igraph_matrix_t type_dist_matrix;
  igraph_matrix_t matrix;
  igraph_bool_t loops=LOGICAL(ploops)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_matrix(ptype_dist_matrix, &type_dist_matrix);
  R_SEXP_to_matrix(pmatrix, &matrix);
  igraph_asymmetric_preference_game(&g, nodes, types, &type_dist_matrix,
				    &matrix, 0, 0, loops);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_laplacian(SEXP graph, SEXP pnormalized) {
  
  igraph_t g;
  igraph_matrix_t res;
  igraph_bool_t normalized=LOGICAL(pnormalized)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&res, 0, 0);
  igraph_laplacian(&g, &res, normalized);
  PROTECT(result=R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_connect_neighborhood(SEXP graph, SEXP porder, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t order=REAL(porder)[0];
  igraph_integer_t mode=REAL(pmode)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_connect_neighborhood(&g, order, mode);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_rewire_edges(SEXP graph, SEXP pprob) {
  
  igraph_t g;
  igraph_real_t prob=REAL(pprob)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph_copy(graph, &g);
  igraph_rewire_edges(&g, prob);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_watts_strogatz_game(SEXP pdim, SEXP psize, SEXP pnei, SEXP pp) {
  
  igraph_t g;
  igraph_integer_t dim=REAL(pdim)[0];
  igraph_integer_t size=REAL(psize)[0];
  igraph_integer_t nei=REAL(pnei)[0];
  igraph_real_t p=REAL(pp)[0];
  SEXP result;
  
  R_igraph_before();
  
  igraph_watts_strogatz_game(&g, dim, size, nei, p);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_coreness(SEXP graph, SEXP pmode) {
  
  igraph_t g;
  igraph_integer_t mode=REAL(pmode)[0];
  igraph_vector_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_coreness(&g, &res, mode);
  
  PROTECT(result=NEW_NUMERIC(igraph_vector_size(&res)));
  igraph_vector_copy_to(&res, REAL(result));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_cliques(SEXP graph, SEXP pminsize, SEXP pmaxsize) {
  
  igraph_t g;
  igraph_vector_ptr_t ptrvec;
  igraph_integer_t minsize=REAL(pminsize)[0];
  igraph_integer_t maxsize=REAL(pmaxsize)[0];
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&ptrvec, 0);
  igraph_cliques(&g, &ptrvec, minsize, maxsize);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&ptrvec)));
  for (i=0; i<igraph_vector_ptr_size(&ptrvec); i++) {
    igraph_vector_t *vec=VECTOR(ptrvec)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(vec)));
    igraph_vector_copy_to(vec, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(vec);
    igraph_free(vec);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_largest_cliques(SEXP graph) {
  
  igraph_t g;
  igraph_vector_ptr_t ptrvec;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&ptrvec,0);
  igraph_largest_cliques(&g, &ptrvec);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&ptrvec)));
  for (i=0; i<igraph_vector_ptr_size(&ptrvec); i++) {
    igraph_vector_t *vec=VECTOR(ptrvec)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(vec)));
    igraph_vector_copy_to(vec, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(vec);
    igraph_free(vec);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_maximal_cliques(SEXP graph) {
  
  igraph_t g;
  igraph_vector_ptr_t ptrvec;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&ptrvec,0);
  igraph_maximal_cliques(&g, &ptrvec);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&ptrvec)));
  for (i=0; i<igraph_vector_ptr_size(&ptrvec); i++) {
    igraph_vector_t *vec=VECTOR(ptrvec)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(vec)));
    igraph_vector_copy_to(vec, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(vec);
    igraph_free(vec);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_clique_number(SEXP graph) {
  
  igraph_t g;
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_clique_number(&g, &res);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_independent_vertex_sets(SEXP graph, 
				      SEXP pminsize, SEXP pmaxsize) {
  igraph_t g;
  igraph_vector_ptr_t ptrvec;
  igraph_integer_t minsize=REAL(pminsize)[0];
  igraph_integer_t maxsize=REAL(pmaxsize)[0];
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&ptrvec, 0);
  igraph_independent_vertex_sets(&g, &ptrvec, minsize, maxsize);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&ptrvec)));
  for (i=0; i<igraph_vector_ptr_size(&ptrvec); i++) {
    igraph_vector_t *vec=VECTOR(ptrvec)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(vec)));
    igraph_vector_copy_to(vec, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(vec);
    igraph_free(vec);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_largest_independent_vertex_sets(SEXP graph) {
  igraph_t g;
  igraph_vector_ptr_t ptrvec;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&ptrvec,0);
  igraph_largest_independent_vertex_sets(&g, &ptrvec);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&ptrvec)));
  for (i=0; i<igraph_vector_ptr_size(&ptrvec); i++) {
    igraph_vector_t *vec=VECTOR(ptrvec)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(vec)));
    igraph_vector_copy_to(vec, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(vec);
    igraph_free(vec);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_maximal_independent_vertex_sets(SEXP graph) {
  igraph_t g;
  igraph_vector_ptr_t ptrvec;
  long int i;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_ptr_init(&ptrvec,0);
  igraph_maximal_independent_vertex_sets(&g, &ptrvec);
  PROTECT(result=NEW_LIST(igraph_vector_ptr_size(&ptrvec)));
  for (i=0; i<igraph_vector_ptr_size(&ptrvec); i++) {
    igraph_vector_t *vec=VECTOR(ptrvec)[i];
    SET_VECTOR_ELT(result, i, NEW_NUMERIC(igraph_vector_size(vec)));
    igraph_vector_copy_to(vec, REAL(VECTOR_ELT(result, i)));
    igraph_vector_destroy(vec);
    igraph_free(vec);
  }
  igraph_vector_ptr_destroy(&ptrvec);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_independence_number(SEXP graph) {
  igraph_t g;
  igraph_integer_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_independence_number(&g, &res);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_empty(SEXP n, SEXP directed) {
  SEXP graph;
  igraph_t c_graph;
  igraph_integer_t c_n;
  igraph_bool_t c_directed;
  
  R_igraph_before();

  c_n=REAL(n)[0];
  c_directed=LOGICAL(directed)[0];

  igraph_empty(&c_graph, c_n, c_directed);

  PROTECT(graph=R_igraph_to_SEXP(&c_graph));

  igraph_destroy(&c_graph);

  R_igraph_after();

  UNPROTECT(1);
  return graph;
}

SEXP R_igraph_lastcit_game(SEXP pnodes, SEXP pedges, SEXP pagebins,
			   SEXP ppreference, SEXP pdirected) {
  
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t edges=REAL(pedges)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_vector_t preference;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(ppreference, &preference);
  igraph_lastcit_game(&g, nodes, edges, agebins, &preference, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_cited_type_game(SEXP pnodes, SEXP pedges, SEXP ptypes,
			      SEXP ppref, SEXP pdirected) {
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t edges=REAL(pedges)[0];
  igraph_vector_t types, pref;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(ptypes, &types);
  R_SEXP_to_vector(ppref, &pref);
  igraph_cited_type_game(&g, nodes, &types, &pref, edges, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
			      

SEXP R_igraph_citing_cited_type_game(SEXP pnodes, SEXP ptypes, SEXP ppref,
				     SEXP pedges, SEXP pdirected) {
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_integer_t edges=REAL(pedges)[0];
  igraph_vector_t types;
  igraph_matrix_t pref;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(ptypes, &types);
  R_SEXP_to_matrix(ppref, &pref);
  igraph_citing_cited_type_game(&g, nodes, &types, &pref, edges, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_isomorphic_vf2(SEXP graph1, SEXP graph2) {
  
  igraph_t g1, g2;
  igraph_bool_t iso;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph1, &g1);
  R_SEXP_to_igraph(graph2, &g2);
  igraph_isomorphic_vf2(&g1, &g2, &iso);
  PROTECT(result=NEW_LOGICAL(1));
  LOGICAL(result)[0]=iso;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_walktrap_community(SEXP graph, SEXP pweights,
				 SEXP psteps, SEXP pmerges, 
				 SEXP pmodularity) {
  igraph_t g;
  igraph_vector_t weights, *ppweights=0;
  igraph_integer_t steps=REAL(psteps)[0];
  igraph_matrix_t merges, *ppmerges=0;
  igraph_vector_t modularity, *ppmodularity=0;
  SEXP result, names;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  if (!isNull(pweights)) {
    ppweights=&weights;
    R_SEXP_to_vector(pweights, ppweights);
  }        
  if (LOGICAL(pmerges)[0]) {
    ppmerges=&merges;
    igraph_matrix_init(ppmerges, 0, 0);
  }
  if (LOGICAL(pmodularity)[0]) {
    ppmodularity=&modularity;
    igraph_vector_init(ppmodularity, 0);
  }

  igraph_community_walktrap(&g, ppweights, steps, ppmerges, ppmodularity);

  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, R_igraph_0ormatrix_to_SEXP(ppmerges));
  if (ppmerges) { igraph_matrix_destroy(ppmerges); }
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppmodularity));
  if (ppmodularity) { igraph_vector_destroy(ppmodularity); }
  PROTECT(names=NEW_CHARACTER(2));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("merges"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("modularity"));
  SET_NAMES(result, names);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
} 

SEXP R_igraph_topological_sorting(SEXP graph, SEXP pneimode) {
  igraph_t g;
  igraph_vector_t res;
  igraph_integer_t mode=REAL(pneimode)[0];
  SEXP result;  

  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  igraph_topological_sorting(&g, &res, mode);
  
  PROTECT(result=R_igraph_vector_to_SEXP(&res));
  igraph_vector_destroy(&res);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_community_edge_betweenness(SEXP graph, SEXP pdirected, 
					 SEXP peb, SEXP pmerges, SEXP pbridges) {
  igraph_t g;
  igraph_vector_t res;
  igraph_vector_t eb, *ppeb=0;
  igraph_matrix_t merges, *ppmerges=0;
  igraph_vector_t bridges, *ppbridges=0;
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result, names;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&res, 0);
  if (LOGICAL(peb)[0]) {
    ppeb=&eb;
    igraph_vector_init(&eb, 0);
  }
  if (LOGICAL(pmerges)[0]) {
    ppmerges=&merges;
    igraph_matrix_init(&merges, 0, 0);
  }
  if (LOGICAL(pbridges)[0]) {
    ppbridges=&bridges;
    igraph_vector_init(&bridges, 0);
  }
  igraph_community_edge_betweenness(&g, &res, ppeb, ppmerges, ppbridges, 
				    directed);
  
  PROTECT(result=NEW_LIST(4));
  SET_VECTOR_ELT(result, 0, R_igraph_vector_to_SEXP(&res));
  igraph_vector_destroy(&res);
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppeb));
  if (ppeb) { igraph_vector_destroy(ppeb); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppmerges));
  if (ppmerges) { igraph_matrix_destroy(ppmerges); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orvector_to_SEXP(ppbridges));
  if (ppbridges) { igraph_vector_destroy(ppbridges); }
  PROTECT(names=NEW_CHARACTER(4));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("removed.edges"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("edge.betweenness"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("merges"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("bridges"));
  SET_NAMES(result, names);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_community_eb_get_merges(SEXP graph, 
				      SEXP pedges) {
  
  igraph_t g;
  igraph_vector_t edges;
  igraph_vector_t bridges;
  igraph_matrix_t res;
  SEXP result, names;
  
  R_igraph_before();
 
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pedges, &edges);
  igraph_matrix_init(&res, 0, 0);
  igraph_vector_init(&bridges, 0);
  igraph_community_eb_get_merges(&g, &edges, &res, &bridges);
  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&res));
  igraph_matrix_destroy(&res);    
  SET_VECTOR_ELT(result, 1, R_igraph_vector_to_SEXP(&bridges));
  igraph_vector_destroy(&bridges);
  PROTECT(names=NEW_CHARACTER(2));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("merges"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("bridges"));  
  SET_NAMES(result, names);

  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_community_fastgreedy(SEXP graph, SEXP pmerges, SEXP pmodularity) {
  
  igraph_t g;
  igraph_matrix_t merges, *ppmerges=0;
  igraph_vector_t modularity, *ppmodularity=0;
  SEXP result, names;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  if (LOGICAL(pmerges)[0]) {
    ppmerges=&merges;
    igraph_matrix_init(&merges, 0, 0);
  }
  if (LOGICAL(pmodularity)[0]) {
    ppmodularity=&modularity;
    igraph_vector_init(&modularity, 0);
  }
  igraph_community_fastgreedy(&g, ppmerges, ppmodularity);
  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, R_igraph_0ormatrix_to_SEXP(ppmerges));
  if (ppmerges) { igraph_matrix_destroy(ppmerges); }
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppmodularity));
  if (ppmodularity) { igraph_vector_destroy(ppmodularity); }
  PROTECT(names=NEW_CHARACTER(2));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("merges"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("modularity"));
  SET_NAMES(result, names);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
} 

SEXP R_igraph_community_to_membership(SEXP graph, SEXP pmerges, 
				      SEXP psteps, SEXP pmembership,
				      SEXP pcsize) {
  igraph_t g;
  igraph_matrix_t merges;
  igraph_integer_t steps=REAL(psteps)[0];
  igraph_vector_t membership, *ppmembership=0;
  igraph_vector_t csize, *ppcsize=0;
  SEXP result, names;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pmerges, &merges);
  if (LOGICAL(pmembership)[0]) {
    ppmembership=&membership;
    igraph_vector_init(ppmembership, 0);
  } 
  if (LOGICAL(pcsize)[0]) {
    ppcsize=&csize;
    igraph_vector_init(ppcsize, 0);
  }
  igraph_community_to_membership(&g, &merges, steps, ppmembership, ppcsize);
  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, R_igraph_0orvector_to_SEXP(ppmembership));
  if (ppmembership) { igraph_vector_destroy(ppmembership); }
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppcsize)); 
  if (ppcsize) { igraph_vector_destroy(ppcsize); }
  PROTECT(names=NEW_CHARACTER(2));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("membership"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("csize"));
  SET_NAMES(result, names);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_modularity(SEXP graph, SEXP pmembership) {
  
  igraph_t g;
  igraph_vector_t membership;
  igraph_real_t res;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pmembership, &membership);
  igraph_modularity(&g, &membership, &res);
  PROTECT(result=NEW_NUMERIC(1));
  REAL(result)[0]=res;
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_community_leading_eigenvector(SEXP graph, SEXP psteps, SEXP pnaive) {
  
  igraph_t g;
  long int steps=REAL(psteps)[0];
  igraph_matrix_t merges;
  igraph_vector_t membership;
  SEXP result, names;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&merges, 0, 0);
  igraph_vector_init(&membership, 0);
  if (LOGICAL(pnaive)[0]) {
    igraph_community_leading_eigenvector_naive(&g, &merges, &membership, steps);
  } else {
    igraph_community_leading_eigenvector(&g, &merges, &membership, steps);
  }
  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&merges));
  igraph_matrix_destroy(&merges);
  SET_VECTOR_ELT(result, 1, R_igraph_vector_to_SEXP(&membership));
  igraph_vector_destroy(&membership);
  PROTECT(names=NEW_CHARACTER(2));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("merges"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("membership"));
  SET_NAMES(result, names);
  
  R_igraph_after();
  
  UNPROTECT(2);
  return result;
} 

SEXP R_igraph_community_leading_eigenvector_step(SEXP graph, 
						 SEXP pmembership,
						 SEXP pcommunity,
						 SEXP peigenvector) {

  igraph_t g;
  igraph_vector_t membership;
  igraph_integer_t community=REAL(pcommunity)[0];
  igraph_bool_t split;
  igraph_vector_t eigenvector, *ppeigenvector=0;
  igraph_real_t eigenvalue;
  SEXP result, names;

  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector_copy(pmembership, &membership);
  if (LOGICAL(peigenvector)[0]) {
    ppeigenvector=&eigenvector;
    igraph_vector_init(ppeigenvector, 0);
  }
  igraph_community_leading_eigenvector_step(&g, &membership, community, 
					    &split, ppeigenvector, &eigenvalue);
  PROTECT(result=NEW_LIST(4));
  SET_VECTOR_ELT(result, 0, R_igraph_vector_to_SEXP(&membership));
  igraph_vector_destroy(&membership);
  SET_VECTOR_ELT(result, 1, NEW_LOGICAL(1));
  LOGICAL(VECTOR_ELT(result, 1))[0]=split;
  SET_VECTOR_ELT(result, 2, R_igraph_0orvector_to_SEXP(ppeigenvector));
  if (ppeigenvector) { igraph_vector_destroy(ppeigenvector); }
  SET_VECTOR_ELT(result, 3, NEW_NUMERIC(1));
  REAL(VECTOR_ELT(result, 3))[0]=eigenvalue;  
  PROTECT(names=NEW_CHARACTER(4));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("membership"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("split"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("eigenvector"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("eigenvalue"));
  SET_NAMES(result, names);

  R_igraph_after();

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_girth(SEXP graph, SEXP pcircle) {
  
  igraph_t g;
  igraph_vector_t circle, *ppcircle=0;
  igraph_integer_t girth;
  SEXP result, names;
  
  R_igraph_before();

  R_SEXP_to_igraph(graph, &g);
  if (LOGICAL(pcircle)[0]) { igraph_vector_init(&circle, 0); ppcircle=&circle; }

  igraph_girth(&g, &girth, ppcircle);

  PROTECT(result=NEW_LIST(2));
  SET_VECTOR_ELT(result, 0, NEW_NUMERIC(1));
  REAL(VECTOR_ELT(result, 0))[0]=girth;
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppcircle));
  if (ppcircle) { igraph_vector_destroy(ppcircle); }
  PROTECT(names=NEW_CHARACTER(2));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("girth"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("circle"));
  SET_NAMES(result, names);

  R_igraph_after();
  
  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_d(SEXP graph, SEXP pniter, SEXP psd, SEXP pnorm,
			SEXP pcites, SEXP pexpected, SEXP perror, SEXP pdebug,
			SEXP verbose) {
  igraph_t g;
  igraph_vector_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_bool_t sd=LOGICAL(psd)[0];
  igraph_bool_t norm=LOGICAL(pnorm)[0];
  igraph_bool_t cites=LOGICAL(pcites)[0];
  igraph_bool_t expected=LOGICAL(pexpected)[0];
  igraph_vector_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_vector_t vsd, vnorm, vcites, vexpected;
  igraph_vector_t *pvsd=0, *pvnorm=0, *pvcites=0, *pvexpected=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver d ");

  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&kernel, 0);
  if (sd) { igraph_vector_init(&vsd, 0); pvsd=&vsd; }
  if (norm) { igraph_vector_init(&vnorm, 0); pvnorm=&vnorm; }
  if (cites) { igraph_vector_init(&vcites, 0); pvcites=&vcites; }
  if (expected) { igraph_vector_init(&vexpected, 0); pvexpected=&vexpected; }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_vector(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }

  igraph_revolver_d(&g, niter, &kernel, pvsd, pvnorm, pvcites, pvexpected,
		   pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_vector_to_SEXP(&kernel));
  igraph_vector_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(pvsd));
  if (pvsd) { igraph_vector_destroy(pvsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orvector_to_SEXP(pvnorm));
  if (pvnorm) { igraph_vector_destroy(pvnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orvector_to_SEXP(pvcites));
  if (pvcites) { igraph_vector_destroy(pvcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orvector_to_SEXP(pvexpected));
  if (pvexpected) { igraph_vector_destroy(pvexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_d(SEXP graph, SEXP pkernel) {

  igraph_t g;
  igraph_vector_t kernel;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pkernel, &kernel);
  igraph_revolver_error2_d(&g, &kernel, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}   

SEXP R_igraph_revolver_ad(SEXP graph, SEXP pniter, SEXP pagebins,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_bool_t sd=LOGICAL(psd)[0];
  igraph_bool_t norm=LOGICAL(pnorm)[0];
  igraph_bool_t cites=LOGICAL(pcites)[0];
  igraph_bool_t expected=LOGICAL(pexpected)[0];
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_matrix_t vsd, vnorm, vcites, vexpected;
  igraph_matrix_t *pvsd=0, *pvnorm=0, *pvcites=0, *pvexpected=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver ad ");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&kernel, 0, 0);
  if (sd) { igraph_matrix_init(&vsd, 0, 0); pvsd=&vsd; }
  if (norm) { igraph_matrix_init(&vnorm, 0, 0); pvnorm=&vnorm; }
  if (cites) { igraph_matrix_init(&vcites, 0, 0); pvcites=&vcites; }
  if (expected) { igraph_matrix_init(&vexpected, 0, 0); pvexpected=&vexpected; }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }

  igraph_revolver_ad(&g, niter, agebins, &kernel, pvsd, pvnorm, pvcites,
		     pvexpected, pplogprob, pplognull, pplogmax, 
		     ppdebug, ppdebugres);

  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(pvsd));
  if (pvsd) { igraph_matrix_destroy(pvsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(pvnorm));
  if (pvnorm) { igraph_matrix_destroy(pvnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(pvcites));
  if (pvcites) { igraph_matrix_destroy(pvcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(pvexpected));
  if (pvexpected) { igraph_matrix_destroy(pvexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
  
}

SEXP R_igraph_revolver_error2_ad(SEXP graph, SEXP pkernel) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_ad(&g, &kernel, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}   

SEXP R_igraph_revolver_ade(SEXP graph, SEXP pcats, SEXP pniter, SEXP pagebins,
			  SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			  SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_array3_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_array3_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver ade ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_array3_init(&kernel, 0, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_array3_init(&vsd, 0, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_array3_init(&vnorm, 0, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_array3_init(&vcites, 0, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_array3_init(&vexpected, 0, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_ade(&g, niter, agebins, &cats, &kernel, ppsd, ppnorm,
		     ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		     ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_array3_to_SEXP(&kernel));
  igraph_array3_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orarray3_to_SEXP(ppsd));
  if (ppsd) { igraph_array3_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orarray3_to_SEXP(ppnorm));
  if (ppnorm) { igraph_array3_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orarray3_to_SEXP(ppcites));
  if (ppcites) { igraph_array3_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orarray3_to_SEXP(ppexpected));
  if (ppexpected) { igraph_array3_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_ade(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_array3_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_igraph_SEXP_to_array3(pkernel, &kernel);
  igraph_revolver_error2_ade(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}   
  
SEXP R_igraph_revolver_e(SEXP graph, SEXP pcats, SEXP pniter, 
			SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_vector_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_vector_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_vector_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver e ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_vector_init(&kernel, 0);
  if (LOGICAL(psd)[0]) { igraph_vector_init(&vsd, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_vector_init(&vnorm, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_vector_init(&vcites, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_vector_init(&vexpected, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_vector(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_e(&g, niter, &cats, &kernel, ppsd, ppnorm, ppcites, ppexpected,
		   pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_vector_to_SEXP(&kernel));
  igraph_vector_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppsd));
  if (ppsd) { igraph_vector_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orvector_to_SEXP(ppnorm));
  if (ppnorm) { igraph_vector_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orvector_to_SEXP(ppcites));
  if (ppcites) { igraph_vector_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orvector_to_SEXP(ppexpected));
  if (ppexpected) { igraph_vector_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_e(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_vector_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_vector(pkernel, &kernel);
  igraph_revolver_error2_e(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}   
  
SEXP R_igraph_revolver_de(SEXP graph, SEXP pcats, SEXP pniter,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver de ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_de(&g, niter, &cats, &kernel, ppsd, ppnorm, ppcites,
		    ppexpected, pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}
  
SEXP R_igraph_revolver_error2_de(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_de(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_l(SEXP graph, SEXP pniter, SEXP pagebins,
			SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_vector_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_vector_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver l ");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&kernel, 0);
  if (LOGICAL(psd)[0]) { igraph_vector_init(&vsd, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_vector_init(&vnorm, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_vector_init(&vcites, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_vector_init(&vexpected, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_vector(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_l(&g, niter, agebins, &kernel, ppsd, ppnorm, 
		   ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		   ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_vector_to_SEXP(&kernel));
  igraph_vector_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(ppsd));
  if (ppsd) { igraph_vector_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orvector_to_SEXP(ppnorm));
  if (ppnorm) { igraph_vector_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orvector_to_SEXP(ppcites));
  if (ppcites) { igraph_vector_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orvector_to_SEXP(ppexpected));
  if (ppexpected) { igraph_vector_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_l(SEXP graph, SEXP pkernel) {

  igraph_t g;
  igraph_vector_t kernel;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pkernel, &kernel);
  igraph_revolver_error2_l(&g, &kernel, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_dl(SEXP graph, SEXP pniter, SEXP pagebins,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver dl ");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_dl(&g, niter, agebins, &kernel, ppsd, ppnorm, 
		   ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		   ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}
  
SEXP R_igraph_revolver_error2_dl(SEXP graph, SEXP pkernel) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_dl(&g, &kernel, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_el(SEXP graph, SEXP pcats, SEXP pniter, SEXP pagebins,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver el ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_el(&g, niter, &cats, agebins, &kernel, ppsd, ppnorm, 
		   ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		   ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_el(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_el(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_r(SEXP graph, SEXP pniter, SEXP pwindow,
			SEXP psd, SEXP pnorm,
			SEXP pcites, SEXP pexpected, SEXP perror, SEXP pdebug,
			SEXP verbose) {
  igraph_t g;
  igraph_vector_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_bool_t sd=LOGICAL(psd)[0];
  igraph_bool_t norm=LOGICAL(pnorm)[0];
  igraph_bool_t cites=LOGICAL(pcites)[0];
  igraph_bool_t expected=LOGICAL(pexpected)[0];
  igraph_vector_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_vector_t vsd, vnorm, vcites, vexpected;
  igraph_vector_t *pvsd=0, *pvnorm=0, *pvcites=0, *pvexpected=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver r ");

  R_SEXP_to_igraph(graph, &g);
  igraph_vector_init(&kernel, 0);
  if (sd) { igraph_vector_init(&vsd, 0); pvsd=&vsd; }
  if (norm) { igraph_vector_init(&vnorm, 0); pvnorm=&vnorm; }
  if (cites) { igraph_vector_init(&vcites, 0); pvcites=&vcites; }
  if (expected) { igraph_vector_init(&vexpected, 0); pvexpected=&vexpected; }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_vector(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }

  igraph_revolver_r(&g, niter, window, &kernel, pvsd, pvnorm, pvcites, pvexpected,
		   pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_vector_to_SEXP(&kernel));
  igraph_vector_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orvector_to_SEXP(pvsd));
  if (pvsd) { igraph_vector_destroy(pvsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orvector_to_SEXP(pvnorm));
  if (pvnorm) { igraph_vector_destroy(pvnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orvector_to_SEXP(pvcites));
  if (pvcites) { igraph_vector_destroy(pvcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orvector_to_SEXP(pvexpected));
  if (pvexpected) { igraph_vector_destroy(pvexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_r(SEXP graph, SEXP pkernel, SEXP pwindow) {

  igraph_t g;
  igraph_vector_t kernel;
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pkernel, &kernel);
  igraph_revolver_error2_r(&g, &kernel, window, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_ar(SEXP graph, SEXP pniter, SEXP pagebins, SEXP pwindow,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_matrix_t kernel;  
  igraph_integer_t niter=REAL(pniter)[0];  
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_bool_t sd=LOGICAL(psd)[0];
  igraph_bool_t norm=LOGICAL(pnorm)[0];
  igraph_bool_t cites=LOGICAL(pcites)[0];
  igraph_bool_t expected=LOGICAL(pexpected)[0];
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_matrix_t vsd, vnorm, vcites, vexpected;
  igraph_matrix_t *pvsd=0, *pvnorm=0, *pvcites=0, *pvexpected=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver ar ");
  
  R_SEXP_to_igraph(graph, &g);
  igraph_matrix_init(&kernel, 0, 0);
  if (sd) { igraph_matrix_init(&vsd, 0, 0); pvsd=&vsd; }
  if (norm) { igraph_matrix_init(&vnorm, 0, 0); pvnorm=&vnorm; }
  if (cites) { igraph_matrix_init(&vcites, 0, 0); pvcites=&vcites; }
  if (expected) { igraph_matrix_init(&vexpected, 0, 0); pvexpected=&vexpected; }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }

  igraph_revolver_ar(&g, niter, agebins, window, &kernel, pvsd, pvnorm, pvcites,
		    pvexpected, pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);

  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(pvsd));
  if (pvsd) { igraph_matrix_destroy(pvsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(pvnorm));
  if (pvnorm) { igraph_matrix_destroy(pvnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(pvcites));
  if (pvcites) { igraph_matrix_destroy(pvcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(pvexpected));
  if (pvexpected) { igraph_matrix_destroy(pvexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
  
}

SEXP R_igraph_revolver_error2_ar(SEXP graph, SEXP pkernel, SEXP pwindow) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_ar(&g, &kernel, window, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_di(SEXP graph, SEXP pcats, SEXP pniter,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver di ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_di(&g, niter, &cats, &kernel, ppsd, ppnorm, ppcites,
		    ppexpected, pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_di(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_di(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_adi(SEXP graph, SEXP pcats, SEXP pniter, SEXP pagebins,
			  SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			  SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_array3_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_array3_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver adi ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_array3_init(&kernel, 0, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_array3_init(&vsd, 0, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_array3_init(&vnorm, 0, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_array3_init(&vcites, 0, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_array3_init(&vexpected, 0, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_adi(&g, niter, agebins, &cats, &kernel, ppsd, ppnorm,
		     ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		     ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_array3_to_SEXP(&kernel));
  igraph_array3_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orarray3_to_SEXP(ppsd));
  if (ppsd) { igraph_array3_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orarray3_to_SEXP(ppnorm));
  if (ppnorm) { igraph_array3_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orarray3_to_SEXP(ppcites));
  if (ppcites) { igraph_array3_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orarray3_to_SEXP(ppexpected));
  if (ppexpected) { igraph_array3_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_adi(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_array3_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_igraph_SEXP_to_array3(pkernel, &kernel);
  igraph_revolver_error2_adi(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_il(SEXP graph, SEXP pcats, SEXP pniter, SEXP pagebins,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver il ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_il(&g, niter, agebins, &cats, &kernel, ppsd, ppnorm, 
		    ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		    ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_il(SEXP graph, SEXP pkernel, SEXP pcats) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_vector_t cats;
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_il(&g, &kernel, &cats, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_ir(SEXP graph, SEXP pcats, SEXP pwindow, SEXP pniter,
			 SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			 SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_matrix_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver di ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_ir(&g, niter, window, &cats, &kernel, ppsd, ppnorm, ppcites,
		    ppexpected, pplogprob, pplognull, pplogmax, ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_ir(SEXP graph, SEXP pkernel, SEXP pcats, 
				 SEXP pwindow) {

  igraph_t g;
  igraph_matrix_t kernel;
  igraph_vector_t cats;
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_SEXP_to_matrix(pkernel, &kernel);
  igraph_revolver_error2_ir(&g, &kernel, &cats, window, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_air(SEXP graph, SEXP pcats, SEXP pwindow, 
			  SEXP pniter, SEXP pagebins,
			  SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			  SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t cats;
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_array3_t kernel;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_integer_t agebins=REAL(pagebins)[0];
  igraph_array3_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0, rlogmax, *pplogmax=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revolver air ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  igraph_array3_init(&kernel, 0, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_array3_init(&vsd, 0, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_array3_init(&vnorm, 0, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_array3_init(&vcites, 0, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_array3_init(&vexpected, 0, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; pplogmax=&rlogmax; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_air(&g, niter, window, agebins, &cats, &kernel, ppsd, ppnorm,
		     ppcites, ppexpected, pplogprob, pplognull, pplogmax, ppdebug,
		     ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_array3_to_SEXP(&kernel));
  igraph_array3_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0orarray3_to_SEXP(ppsd));
  if (ppsd) { igraph_array3_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0orarray3_to_SEXP(ppnorm));
  if (ppnorm) { igraph_array3_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0orarray3_to_SEXP(ppcites));
  if (ppcites) { igraph_array3_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0orarray3_to_SEXP(ppexpected));
  if (ppexpected) { igraph_array3_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(3));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
    REAL(VECTOR_ELT(result, 6))[2]=*pplogmax;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_revolver_error2_air(SEXP graph, SEXP pkernel, SEXP pcats, 
				  SEXP pwindow) {

  igraph_t g;
  igraph_array3_t kernel;
  igraph_vector_t cats;
  igraph_integer_t window=REAL(pwindow)[0];
  igraph_real_t logprob, lognull;
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pcats, &cats);
  R_igraph_SEXP_to_array3(pkernel, &kernel);
  igraph_revolver_error2_air(&g, &kernel, &cats, window, &logprob, &lognull);
  PROTECT(result=NEW_NUMERIC(2));  
  REAL(result)[0]=logprob;
  REAL(result)[1]=lognull;
   
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}

SEXP R_igraph_revolver_d_d(SEXP graph, SEXP pniter, SEXP pvtime, SEXP petime,
			   SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			   SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t vtime, etime;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_matrix_t kernel;
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revover d-d ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pvtime, &vtime);
  R_SEXP_to_vector(petime, &etime);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_d_d(&g, niter, &vtime, &etime, &kernel, ppsd, ppnorm,
		      ppcites, ppexpected, pplogprob, pplognull,
		      ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(2));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}

SEXP R_igraph_evolver_d(SEXP pnodes, SEXP pkernel, SEXP poutseq, 
			SEXP poutdist, SEXP pm, SEXP pdirected) {
  
  igraph_t g;
  igraph_integer_t nodes=REAL(pnodes)[0];
  igraph_vector_t kernel;
  igraph_vector_t voutseq, *ppoutseq=0;
  igraph_vector_t voutdist, *ppoutdist=0;
  igraph_integer_t m=REAL(pm)[0];
  igraph_bool_t directed=LOGICAL(pdirected)[0];
  SEXP result;
  
  R_igraph_before();
  
  R_SEXP_to_vector(pkernel, &kernel);
  if (!isNull(poutseq)) {
    R_SEXP_to_vector(poutseq, &voutseq);
    ppoutseq=&voutseq;
  }
  if (!isNull(poutdist)) {
    R_SEXP_to_vector(poutdist, &voutdist);
    ppoutdist=&voutdist;
  }
  
  igraph_evolver_d(&g, nodes, &kernel, ppoutseq, ppoutdist, m, directed);
  PROTECT(result=R_igraph_to_SEXP(&g));
  igraph_destroy(&g);
  
  R_igraph_after();
  
  UNPROTECT(1);
  return result;
}
		   
SEXP R_igraph_revolver_p_p(SEXP graph, SEXP pniter, SEXP pvtime, SEXP petime,
			   SEXP pauthors, SEXP peventsizes,
			   SEXP psd, SEXP pnorm, SEXP pcites, SEXP pexpected,
			   SEXP perror, SEXP pdebug, SEXP verbose) {
  igraph_t g;
  igraph_vector_t vtime, etime;
  igraph_vector_t authors, eventsizes;
  igraph_integer_t niter=REAL(pniter)[0];
  igraph_matrix_t kernel;
  igraph_matrix_t vsd, *ppsd=0, vnorm, *ppnorm=0, vcites, *ppcites=0,
    vexpected, *ppexpected=0;
  igraph_matrix_t debug, *ppdebug=0;
  igraph_vector_ptr_t debugres, *ppdebugres=0;
  igraph_real_t rlogprob, rlognull, *pplogprob=0, *pplognull=0;
  SEXP result, names;
  
  R_igraph_before2(verbose, "Revover d-d ");
  
  R_SEXP_to_igraph(graph, &g);
  R_SEXP_to_vector(pvtime, &vtime);
  R_SEXP_to_vector(petime, &etime);
  R_SEXP_to_vector(pauthors, &authors);
  R_SEXP_to_vector(peventsizes, &eventsizes);
  igraph_matrix_init(&kernel, 0, 0);
  if (LOGICAL(psd)[0]) { igraph_matrix_init(&vsd, 0, 0); ppsd=&vsd; }
  if (LOGICAL(pnorm)[0]) { igraph_matrix_init(&vnorm, 0, 0); ppnorm=&vnorm; }
  if (LOGICAL(pcites)[0]) { igraph_matrix_init(&vcites, 0, 0); ppcites=&vcites; }
  if (LOGICAL(pexpected)[0]) { 
    igraph_matrix_init(&vexpected, 0, 0); 
    ppexpected=&vexpected; 
  }
  if (LOGICAL(perror)[0]) { pplogprob=&rlogprob; pplognull=&rlognull; }
  if (!isNull(pdebug) && GET_LENGTH(pdebug)!=0) {
    R_SEXP_to_matrix(pdebug, &debug); ppdebug=&debug; 
    igraph_vector_ptr_init(&debugres, 0); ppdebugres=&debugres;
  }
  
  igraph_revolver_p_p(&g, niter, &vtime, &etime, &authors, &eventsizes,
		      &kernel, ppsd, ppnorm,
		      ppcites, ppexpected, pplogprob, pplognull,
		      ppdebug, ppdebugres);
  
  PROTECT(result=NEW_LIST(7));
  SET_VECTOR_ELT(result, 0, R_igraph_matrix_to_SEXP(&kernel));
  igraph_matrix_destroy(&kernel);
  SET_VECTOR_ELT(result, 1, R_igraph_0ormatrix_to_SEXP(ppsd));
  if (ppsd) { igraph_matrix_destroy(ppsd); }
  SET_VECTOR_ELT(result, 2, R_igraph_0ormatrix_to_SEXP(ppnorm));
  if (ppnorm) { igraph_matrix_destroy(ppnorm); }
  SET_VECTOR_ELT(result, 3, R_igraph_0ormatrix_to_SEXP(ppcites));
  if (ppcites) { igraph_matrix_destroy(ppcites); }
  SET_VECTOR_ELT(result, 4, R_igraph_0ormatrix_to_SEXP(ppexpected));
  if (ppexpected) { igraph_matrix_destroy(ppexpected); }
  if (!isNull(pdebug) && GET_LENGTH(pdebug) != 0) {
    /* TODO */
  } else {
    SET_VECTOR_ELT(result, 5, R_NilValue);
  }
  if (pplogprob) {
    SET_VECTOR_ELT(result, 6, NEW_NUMERIC(2));
    REAL(VECTOR_ELT(result, 6))[0]=*pplogprob;
    REAL(VECTOR_ELT(result, 6))[1]=*pplognull;
  } else {
    SET_VECTOR_ELT(result, 6, R_NilValue);
  }
  PROTECT(names=NEW_CHARACTER(7));
  SET_STRING_ELT(names, 0, CREATE_STRING_VECTOR("kernel"));
  SET_STRING_ELT(names, 1, CREATE_STRING_VECTOR("sd"));
  SET_STRING_ELT(names, 2, CREATE_STRING_VECTOR("norm"));
  SET_STRING_ELT(names, 3, CREATE_STRING_VECTOR("cites"));
  SET_STRING_ELT(names, 4, CREATE_STRING_VECTOR("expected"));
  SET_STRING_ELT(names, 5, CREATE_STRING_VECTOR("debug"));
  SET_STRING_ELT(names, 6, CREATE_STRING_VECTOR("error"));
  SET_NAMES(result, names);
  
  R_igraph_after2(verbose);  

  UNPROTECT(2);
  return result;
}		   
			 
/***********************************************/
/* THE REST IS GENERATED BY inger.py           */
/***********************************************/
