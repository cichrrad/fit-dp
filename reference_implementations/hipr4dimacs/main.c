/* Maximum flow - highest lavel push-relabel algorithm */
/* COPYRIGHT C 1995 -- 2008 by Andrew V. Goldberg, avg@alum.mit.edu */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "values.h"
#include "types.h"          /* type definitions */
#include "parser.h"         /* parser */
#include "timer.h"          /* timing routine */

//#define GLOB_UPDT_FREQ 0.6667
#define GLOB_UPDT_FREQ 0.23

// how far above the last excess to update
#define STOP_H 0

#define INIT_W 4.0

#define WHITE 0
#define GREY 1
#define BLACK 2

/* global variables */

long   n;                    /* number of nodes */
long  nLeft;                 /* after deleting by gap and global update */
long   m;                    /* number of arcs */
long   mInp;                 /* input num. arcs with parallel arcs */
long   nMin;                 /* smallest node id */
node   *nodes;               /* array of nodes */
arc    *arcs;                /* array of arcs */
bucket *buckets;             /* array of buckets */
cType  *cap;                 /* array of capacities */
node   *source;              /* source node pointer */
node   *sink;                /* sink node pointer */
//node   **stack;              /* for DFS */
//node   **sBot, **sTop;       /* stack pointers */
long   dMax;                 /* maximum label */
long   aMax;                 /* maximum actie node label */
long   eMax;                 /* maximum guaranteed correct bucket */
double flow;                 /* flow value */
long pushCnt  = 0;           /* number of pushes */
//long pushCntI = 0;
long relabelCnt   = 0;       /* number of relabels */
long upScanCnt=0;
long updateCnt    = 0;       /* number of updates */
long gapCnt   = 0;           /* number of gaps */
long gNodeCnt = 0;           /* number of nodes after gap */  
float t, t2;                 /* for saving times */
float t3;
node   *sentinelNode;        /* end of the node list marker */
double workSinceUpdate=0;      /* the number of arc scans since last update */
double globUpdtFreq;          /* global update frequency */
double globUpdtFreqOrig;
double updThresh;
long scanN=0;

/* macros */

#define forAllNodes(i) for ( i = nodes; i != sentinelNode; i++ )
#define forAllArcs(i,a) for (a = i->first, stop__A = (i+1)->first; a != stop__A; a++)

#define nNode( i ) ( (i) - nodes + nMin )
#define nArc( a )  ( ( a == NULL )? -1 : (a) - arcs )

#define isCurrent(i) ((i->current->resCap > 0) && (i->current->head->d < i->d))

#define min(a, b) (((a) < (b)) ? a : b)

#define max(a, b) (((a) > (b)) ? a : b)

#define sReset() (sTop = sBot)

#define sPush(i) {*sTop = i; sTop++;}

#define sPop(i) {sTop--; i = *sTop;}

/* 
   bucket macros:
   bucket's active node list is singly-linked
     operations aAdd, aRemove (from the front)
   bucket's inactive list is doubly-linked
     operations iAdd, iDelete (from arbitrary position)
*/

//long i_dist;
//node *i_next, *i_prev;


void aAdd(bucket *l, node *i)
{
  i->bNext = l->firstActive;
  i->bNext->bPrev = i;
  i->bPrev = sentinelNode;
  l->firstActive = i;
  if (i->d > aMax) { 
    aMax = i->d;
    if (i->d > dMax)
      dMax = i->d;
  }
}

void aDelete(bucket *l, node *i)
{
  if (l->firstActive == i) {
    l->firstActive = i->bNext;
    i->bNext->bPrev = sentinelNode;
  }
  else {
    i->bPrev->bNext = i->bNext;
    i->bNext->bPrev = i->bPrev;
  }
}


void iAdd(bucket *l, node *i)
{
  i->bNext = l->firstInactive;
  i->bPrev = sentinelNode;
  i->bNext->bPrev = i;
  l->firstInactive = i;
  if (i->d > dMax)
    dMax = i->d;
}

void iDelete(bucket *l, node *i)
{
  if (l->firstInactive == i) {
    l->firstInactive = i->bNext;
    i->bNext->bPrev = sentinelNode;
  }
  else {
    i->bPrev->bNext = i->bNext;
    i->bNext->bPrev = i->bPrev;
  }
}

/* allocate datastructures, initialize related variables */

int allocDS( )

{
  bucket *l;

  buckets = (bucket*) calloc ( n+2, sizeof (bucket) );
  if ( buckets == NULL ) return ( 1 );

  sentinelNode = nodes + n;
  sentinelNode->first = arcs + 2*m;

  for (l = buckets; l <= buckets + n; l++) {
    l->firstActive = sentinelNode;
    l->firstInactive = sentinelNode;
  }

  return (0);

} /* end of allocate */


void printGraph()
{
  node *i;
  arc *a;
  arc *stop__A;

  printf("n %ld mInp %ld m %ld sent %i\n", n, mInp, m, sentinelNode - nodes + 1);

  forAllNodes(i) {
    printf("node %d\n", i-nodes+1);
    forAllArcs(i, a) {
      printf("  arc %d (%d %d) res cap %ld cap %ld rev %d\n", a - arcs, 
	     i-nodes+1, a->head-nodes+1, 
	     a->resCap, cap[a-arcs],
	     a->rev - arcs);
    }
  }


}

// return 0 if s or t have no adjacent arcs
int init()

{
  node *i;
  arc *stop__A;

  bucket *l;
  arc *a;
  cType delta;

  nLeft = n;
  updThresh = n + (long) pow(m, 0.75) - (long) (INIT_W * (double) n);

  // initialize excesses

  forAllNodes(i) {
    i->excess = 0;
    i->current = i->first;
    i->d = 1;
  }

  // iterate over all arcs
  for (a = arcs, stop__A = arcs + m; a != stop__A; a++) {
    a->resCap = cap[a-arcs];
  }

  if ((source->first == NULL) || (sink->first == NULL))
    return 0;
  /*
  for (l = buckets; l <= buckets + n; l++) {
    l->firstActive = sentinelNode;
    l->firstInactive = sentinelNode;
  }
  */

  source->excess = 0;
  // saturate source arcs
  forAllArcs(source,a) {
    if (a->head != source) {
      pushCnt++;
      //      pushCntI++;
      delta = a->resCap;
      a->resCap -= delta;
      (a->rev)->resCap += delta;
      a->head->excess += delta;
    }
  }

  /*  setup labels and buckets */
  l = buckets + 1;
  
  aMax = 0;
  dMax = 0;
  eMax = 0;

  source->d = n;
  
  sink->d = 0;
 
  forAllNodes(i) {
    if (i == sink) {
      iAdd(buckets,i);
      continue;
    }

    if (i->d < n) {
      l = buckets + i->d;
      if (i->excess > 0) {
	aAdd(l,i);
      }
      else {
	iAdd(l,i);
      }
    }
  }
  return (1);
} /* end of init */

void checkDs()

{
  bucket *l;
  node *i;

  for (l = buckets ; l < buckets + dMax; l++) {
    for (i = l->firstActive; i != sentinelNode; i = i->bNext)
      assert(i->d == l - buckets);
    for (i = l->firstInactive; i != sentinelNode; i = i->bNext)
      assert(i->d == l - buckets);
  }
}

/* global update via breadth first search backward */
/* assume sink is the only node at level 0 */

void globalUpdate ()

{

  node  *i, *j;       /* node pointers */
  arc   *a;           /* current arc pointers  */
  bucket *l, *jL;          /* bucket */
  long curDist, jD;
  long state;
  arc *stop__A;
  //  excessType remEx;
  bucket B, *remNodes;
  long aMaxOld, dMaxOld;
  int tType=0; // reason for termination
  long remN=0;
#ifdef LASY_UPDATE
  node *lastN;
#endif

  remNodes = &B;
  remNodes->firstActive = sentinelNode;
  remNodes->firstInactive = sentinelNode;

  updateCnt++;
  scanN = 0;

  /* breadth first search */

  //  remEx = 0;
  aMaxOld = aMax;
  dMaxOld = dMax;

#ifdef OLD_UPDATE
  eMax = 0;
#endif

  for (curDist = eMax; 1; curDist++) {

    state = 0;
    l = buckets + curDist;
    jD = curDist + 1;
    jL = l + 1;

    // first case
    if ((l->firstActive == sentinelNode) && 
	(l->firstInactive == sentinelNode)) {
      // stop after scanning all reachable vertices
      tType = 1;
      break;
    }
#ifdef LASY_UPDATE
    if ((curDist > aMaxOld + STOP_H) && 
	(remNodes->firstActive == sentinelNode)) {
      // all nodes with excess have current distances
      tType = 2;
      break;
    }
#endif
    // clean up the next level
#ifdef LASY_UPDATE
    lastN = NULL;
#endif
    for (j = jL->firstInactive; j != sentinelNode; j = j->bNext) {
      j->d = n;
#ifdef LASY_UPDATE
      lastN = j;
#endif
    }

#ifdef LASY_UPDATE
    // concatenate and clean
    if (jL->firstInactive != sentinelNode) {
      // lastN points to the last real node
      lastN->bNext = remNodes->firstInactive;
      lastN->bNext->bPrev = lastN;
      remNodes->firstInactive = jL->firstInactive;
      jL->firstInactive = sentinelNode;
    }
#else
    jL->firstInactive = sentinelNode;
#endif

#ifdef LASY_UPDATE
    lastN = NULL;
#endif
    for (j = jL->firstActive; j != sentinelNode; j = j->bNext) {
      j->d = n;
#ifdef LASY_UPDATE
      lastN = j;
#endif
    }

#ifdef LASY_UPDATE
    // concatenate and clean
    if (jL->firstActive != sentinelNode) {
      // lastN points to the last real node
      lastN->bNext = remNodes->firstActive;
      lastN->bNext->bPrev = lastN;
      remNodes->firstActive = jL->firstActive;
      jL->firstActive = sentinelNode;
    }
#else
    jL->firstActive = sentinelNode;
#endif

    i = NULL;
    while (1) {

      switch (state) {
      case 0: 
	i = l->firstInactive;
	state = 1;
	break;
      case 1:
	i = i->bNext;
	break;
      case 2:
	i = l->firstActive;
	state = 3;
	break;
      case 3:
	i = i->bNext;
	break;
      default: 
	assert(0);
	break;
      }

      assert(i != NULL);

      if (i == sentinelNode) {
	if (state == 1) {
	  state = 2;
	  continue;
	}
	else {
	  assert(state == 3);
	  break;
	}
      }

      scanN++;
      upScanCnt++;
      /* scanning arcs incident to node i */
      forAllArcs(i,a) {
	if (a->rev->resCap > 0 ) {
	  j = a->head;
	  if (j->d == n) {
	    if (j->excess > 0) {
	      aDelete(remNodes, j);
	      j->d = jD;
	      aAdd(jL,j);
	    }
	    else {
	      iDelete(remNodes, j);
	      j->d = jD;
	      iAdd(jL,j);
	    }
	    j->current = j->first;
	  }
	}
      } /* node i is scanned */ 
    }
  }

  if (tType == 1) {
    // all reachable nodes scanned
    // clean up higher levels containing unreachable nodes
    for (l = buckets + curDist + 1; l <= buckets + dMaxOld; l++) {
      for (i = l->firstInactive; i != sentinelNode; i = i->bNext) {
	i->d = n;
	nLeft--;
      }
      l->firstInactive = sentinelNode;
      
      for (i = l->firstActive; i != sentinelNode; i = i->bNext) {
	remN++;
	i->d = n;
	nLeft--;
      }
      l->firstActive = sentinelNode;

      aMax = dMax = curDist;
    }
  }
#ifdef LASY_UPDATE
  else {
    // stopped because all nodes with excess have current distances
    
    assert(tType == 2);
    assert(remNodes->firstActive == sentinelNode);
    
    jD = curDist + 1;
    jL = buckets + jD;
    // concervatively update removed node d's
    if (remNodes->firstInactive != sentinelNode) {
      lastN = NULL;
      for (j = remNodes->firstInactive; j != sentinelNode; j = j->bNext) {
	remN++;
	lastN = j;
	j->d = jD;
	j->current = j->first;
      }
      
      // place nodes on remNodes list into jL
      lastN->bNext = jL->firstInactive;
      lastN->bNext->bPrev = lastN;
      jL->firstInactive = remNodes->firstInactive;
    }
    
    if (dMaxOld > curDist)
      dMax = dMaxOld;
    else
      dMax = curDist;
  }
#endif
  
  eMax = curDist;
  aMax = curDist;

  workSinceUpdate = 0;

  updThresh = 1 +
    ((double) n)/100.0 + 
    ((double) nLeft)/20.0 +
    scanN;
  /*
  updThresh = ((double) n)/100.0 + 
    (double) (nLeft + 1) *
    (
     pow(4.0, (((float) (scanN + 1))/(float) (nLeft + 1)))
     );
  */
  //  printf(">>> tr %f    sc %ld    left %ld\n", (float) updThresh, scanN, nLeft);

} /* end of global update */

/* second stage -- preflow to flow */
void stageTwo ( )
/*
   do dsf in the reverse flow graph from nodes with excess
   cancel cycles if found
   return excess flow in topological order
*/

/*
   i->d is used for dfs labels 
   i->bNext is used for topological order list
   buckets[i-nodes].firstActive is used for DSF tree
*/

{
  node *i, *j, *tos, *bos, *restart, *r;
  arc *a;
  cType delta;

  /* assume self-loops have been removed
 
  // deal with self-loops

  forAllNodes(i) {
    forAllArcs(i,a)
    if ( a -> head == i ) {
      a -> resCap = cap[a - arcs];
    }
  }
  */

  /* initialize */
  tos = bos = NULL;
  forAllNodes(i) {
    i -> d = WHITE;
    //    buckets[i-nodes].firstActive = NULL;
    buckets[i-nodes].firstActive = sentinelNode;
    i->current = i->first;
  }

  /* eliminate flow cycles, topologicaly order vertices */
  forAllNodes(i)
    if (( i -> d == WHITE ) && ( i -> excess > 0 ) &&
	( i != source ) && ( i != sink )) {
      r = i;
      r -> d = GREY;
      do {
	for ( ; i->current != (i+1)->first; i->current++) {
	  a = i -> current;
	  if (cap[a - arcs] < a->resCap) { 
	  // a->rev has positive flow
	    j = a -> head;
	    if ( j -> d == WHITE ) {
	      /* start scanning j */
	      j -> d = GREY;
	      buckets[j-nodes].firstActive = i;
	      i = j;
	      break;
	    }
	    else
	      if (j->d == GREY) {
		/* find minimum flow on the cycle */
		delta = a->resCap - cap[a-arcs];
		while ( 1 ) {
		  delta = min(delta, 
			      j->current->resCap - cap[j->current - arcs]);
		  if ( j == i )
		    break;
		  else
		    j = j -> current -> head;
		}

		/* remove delta flow units */
		j = i;
		while ( 1 ) {
		  a = j -> current;
		  a -> resCap -= delta;
		  a -> rev -> resCap += delta;
		  j = a -> head;
		  if ( j == i )
		    break;
		}
	  
		/* backup DFS to the first saturated arc */
		restart = i;
		for ( j = i -> current -> head; j != i; j = a -> head ) {
		  a = j -> current;
		  if (( j -> d == WHITE ) || 
		      (a->resCap == cap[a - arcs])) {
		    j -> current -> head -> d = WHITE;
		    if ( j -> d != WHITE )
		      restart = j;
		  }
		}
	  
		if ( restart != i ) {
		  i = restart;
		  i->current++;
		  break;
		}
	      }
	  }
	}

	if (i->current == (i+1)->first) {
	  /* scan of i complete */
	  i -> d = BLACK;
	  if ( i != source ) {
	    if ( bos == NULL ) {
	      bos = i;
	      tos = i;
	    }
	    else {
	      i -> bNext = tos;
	      tos = i;
	    }
	  }

	  if ( i != r ) {
	    i = buckets[i-nodes].firstActive;
	    i->current++;
	  }
	  else
	    break;
	}
      } while ( 1 );
    }


  /* return excesses */
  /* note that sink is not on the stack */
  if ( bos != NULL ) {
    for ( i = tos; i != bos; i = i -> bNext ) {
      a = i -> first;
      while ( i -> excess > 0 ) {
	if (cap[a - arcs] < a->resCap) {
	  // a->rev has positive flow
	  if (a->resCap - cap[a - arcs] < i->excess)
	    delta = a->resCap - cap[a - arcs];
	  else
	    delta = i->excess;
	  a -> resCap -= delta;
	  a -> rev -> resCap += delta;
	  i -> excess -= delta;
	  a -> head -> excess += delta;
	}
	a++;
      }
    }
    /* now do the bottom */
    i = bos;
    a = i -> first;
    while ( i -> excess > 0 ) {
      if (cap[a - arcs] < a->resCap) {
	  // a->rev has positive flow
	if (a->resCap - cap[a - arcs] < i->excess)
	  delta = a->resCap - cap[a - arcs];
	else
	  delta = i->excess;
	a -> resCap -= delta;
	a -> rev -> resCap += delta;
	i -> excess -= delta;
	a -> head -> excess += delta;
      }
      a++;
    }
  }
}


/* gap relabeling */

void gap(bucket *emptyB)

{

  bucket *l;
  node  *i; 
  long  r;           /* index of the bucket before l  */

  //  printf(">>> gap %d\n", emptyB - buckets);
  gapCnt++;
  r = (emptyB - buckets) - 1;

  /* set labels of nodes beyond the gap to "infinity" */
  for (l = emptyB; l <= buckets + dMax; l ++) {
    for (i = l -> firstActive; i != sentinelNode; i = i -> bNext) {
      i -> d = n;
      gNodeCnt++;
      nLeft--;
    }
    l -> firstActive = sentinelNode;

    for (i = l -> firstInactive; i != sentinelNode; i = i -> bNext) {
      i -> d = n;
      gNodeCnt++;
      nLeft--;
    }
    l -> firstInactive = sentinelNode;
  }

  dMax = r;
  aMax = r;
  if (r < eMax)
    eMax = r;

}

/*--- relabelling node i */
#ifdef FULL_RELABEL
// find the best label
void relabel (node *i)

{

  node  *j;
  long  minD;     /* minimum d of a node reachable from i */
  arc   *minA;    /* an arc which leads to the node with minimal d */
  arc   *a;
  arc *stop__A;
  bucket *l;

  //  printf(">>>relabeling %d (%ld)\n", i-nodes+1, i->d);

  relabelCnt++;
  workSinceUpdate++;

  if (eMax > i->d)
    eMax = i->d;

  // check for gap
  l = buckets + i->d;
  if (((i->bNext == sentinelNode) && (i->bPrev == sentinelNode)) &&
      (((i->excess > 0) && 
	(l->firstInactive == sentinelNode)) ||
       ((i->excess == 0) && (l->firstActive == sentinelNode)))
      ) {
    gap(l);
    return;
  }

  minD = n;
  minA = NULL;

  /* find the minimum */
  forAllArcs(i,a) {
    if (a -> resCap > 0) {
      j = a -> head;
      if (j->d < minD) {
	minD = j->d;
	minA = a;
      }
    }
  }

  if (minD < n)
    minD++;
  else
    minD = n;

  if (i->excess > 0) {
    aDelete(l,i);
  }
  else {
    iDelete(l,i);
  }
    
  if (minD < n) {
    i->current = minA;
    assert(i->d < minD);
    i->d = minD;
    l = buckets + i->d;
    if (i->excess > 0) {
      aAdd(l, i);
    }
    else {
      iAdd(l, i);
    }
  }
  else {
    i->d = n;
    nLeft--;
  }

} /* end of relabel */
#else
// quick relabel: add one to i->d
void relabel(node *i)

{

  bucket *l;

  //  printf(">>>relabeling %d (%ld)\n", i-nodes+1, i->d);

  relabelCnt++;
  workSinceUpdate++;

  if (eMax > i->d)
    eMax = i->d;

  // check for gap
  l = buckets + i->d;
  if (((i->bNext == sentinelNode) && (i->bPrev == sentinelNode)) &&
      (((i->excess > 0) && 
	(l->firstInactive == sentinelNode)) ||
       ((i->excess == 0) && (l->firstActive == sentinelNode)))
      ) {
    gap(l);
    return;
  }

  if (i->excess > 0) {
    aDelete(l,i);
  }
  else {
    iDelete(l,i);
  }

  i->d++;
  i->current = i->first;
  l++;
  if (i->excess > 0) {
    aAdd(l, i);
  }
  else {
    iAdd(l, i);
  }
} /* end of relabel */
#endif

// return 1 if found a current arc out of i or 0 otherwise
int makeCurrent(node *i)

{
  arc *a;
  arc *stopA;

  // assume i->current is not current
  assert((i->current->resCap == 0) || (i->current->head->d >= i->d));
  //  assert(i->current < (i+1)->first);

  for (a = (i->current)+1, stopA = (i+1)->first; a != stopA; a++) {
    if ((a->resCap > 0) && (a->head->d < i->d)) {
      i->current = a;
      return(1);
    }
  }
   
  return(0);

}

// push flow from i until no excess or gap
void discharge(node *i)

{
  node  *j;                 /* sucsessor of i */
  bucket *lj;               /* j's bucket */
  bucket *l;                /* i's bucket */
  arc   *a;                 /* current arc (i,j) */
  cType  delta;
  arc *stopA;

  while (1) {
    /* scanning arcs outgoing from  i  */
    for (a = i->current, stopA = (i+1)->first; a != stopA; a++) {
      j = a->head;
      if ((a->resCap > 0) && (j->d < i->d)) {
	pushCnt++;
	
	delta = min(a->resCap, i->excess);
	
	if (j != sink) {
	  lj = buckets + j->d;
	  
	  if (j->excess == 0) {
	    /* remove j from inactive list */
	    iDelete(lj,j);
	    /* add j to active list */
	    aAdd(lj,j);
	  }
	}
	
	a->resCap -= delta;
	a->rev->resCap += delta;
	j->excess += delta;
	i->excess -= delta;
	
	assert(j->excess > 0);
	
	if (i->excess == 0) {
	  l = buckets + i->d;
	  aDelete(l, i);
	  iAdd(l, i);
	  i->current = a;
	  break;
	}
      }
    }

    if (i->excess > 0) {
      relabel(i);
      if (i->d == n) //gap
	return;
    }
    else
      break;
  }
}

/* first stage  -- maximum preflow*/
void stageOne ( )

{

  node *i;
  bucket *l;

#if defined(INIT_UPDATE)
  globalUpdate ();
#else
  workSinceUpdate = 0;
  //  workSinceUpdate =
  //   (long) (INIT_W * (double) n);
#endif

  //  printf(">>> init %ld n %ld updThresh %ld\n", workSinceUpdate, n, updThresh);
  //  workSinceUpdate = 0;

  /* main loop */
  while (aMax >= 0) {
    l = buckets + aMax;
    i = l->firstActive;

    if (i == sentinelNode)
      aMax--;
    else {
      assert (i->d == aMax);
      assert(i->excess > 0);

      discharge(i);

      /* is it time for global update? */
      if (workSinceUpdate * globUpdtFreq > updThresh) {
	globalUpdate ();
      }
    }
  } /* end of the main loop */
    
  flow = sink->excess;

} 


int main(int argc, char *argv[])


{
#if (defined(PRINT_FLOW) || defined(CHECK_SOLUTION))
  node *i;
  arc *a;
#endif

#ifdef PRINT_FLOW
  long ni, na;
#endif
#ifdef PRINT_CUT
  node *j;
#endif
  int  cc;
#ifdef CHECK_SOLUTION
  long long sum;
  bucket *l;
#endif
  arc *stop__A;

  int nonTriv;


  if (argc > 2) {
    printf("Usage: %s [update frequency]\n", argv[0]);
    exit(1);
  }

  if (argc < 2)
    globUpdtFreq = GLOB_UPDT_FREQ;
  else
    globUpdtFreq = (float) atof(argv[1]);
  globUpdtFreqOrig = globUpdtFreq;

  printf("c\nc HI_PR version 4.0 \n");

  parse( &n, &m, &mInp, &nodes, &arcs, &cap, &source, &sink, &nMin );

  printf("c nodes:       %10ld\nc arcs:        %10ld (%ld)\nc\n", n, mInp, m*2);
  printf("c (freq %.4f)\nc \nc\n",
	 globUpdtFreq);

  cc = allocDS();
  if ( cc ) { fprintf ( stderr, "Allocation error\n"); exit ( 1 ); }

  //  printGraph();

  t = timer();
  t3 = t;
  t2 = t;

  nonTriv = init();
  t3 = timer() - t3;
  
  if (nonTriv)
    stageOne();
  else
    flow = 0;
    
  t2 = timer() - t2;

  printf ("c flow:      %12.01f\n", flow);

#ifndef CUT_ONLY
  stageTwo ( );

  t = timer() - t;

  printf ("c time:        %10.2f\n", t);

#endif

  printf ("c init tm:     %10.2f\n", t3);
  printf ("c cut tm:      %10.2f\n", t2);

  //  printGraph();
#ifdef CHECK_SOLUTION

  /* check if you have a flow (pseudoflow) */
  /* check arc flows */
  if (nonTriv) {
    forAllNodes(i) {
      forAllArcs(i,a) {
	if ((a->resCap + a->rev->resCap != 
	     cap[a-arcs] + cap[a->rev - arcs]) 
	    || (a->resCap < 0)
	    || (a->rev->resCap < 0)) {
	  printf("ERROR: bad arc flow\n");
	  //	  exit(2);
	}
      }
    }
    
    /* check conservation */
    forAllNodes(i)
      if ((i != source) && (i != sink)) {
#ifdef CUT_ONLY
	if (i->excess < 0) {
	  printf("ERROR: nonzero node excess\n");
	  //	exit(2);
	}
#else
	if (i->excess != 0) {
	  printf("ERROR: nonzero node excess\n");
	  //	exit(2);
	}
#endif

	sum = 0;
	forAllArcs(i,a) {
	  sum += ((long long) cap[a - arcs]) - 
	    ((long long) a->resCap);
	}

	if (i->excess != sum) {
	  printf("ERROR: conservation constraint violated\n");
	  //	exit(2);
	}
      }
    
    /* check if mincut is saturated */
    aMax = dMax = 0;
    forAllNodes(i) {
      i->d = n;
    }
    for (l = buckets; l <= buckets + n; l++) {
      l->firstActive = sentinelNode;
      l->firstInactive = sentinelNode;
    }
    sink->d = 0;
    iAdd(buckets, sink);
  
    long saveUp = upScanCnt;
    globalUpdate();
    updateCnt--; // this one does not count
    upScanCnt = saveUp;
    if (source->d < n) {
      printf("ERROR: the solution is not optimal\n");
      //    exit(2);
    }
    
    printf("c\nc Solution checks (feasible and optimal)\nc\n");
  }
  else
    printf("c\nc trivial cut, zero flow\nc\n");
#endif

  //  printf(">>> init pushes: %ld\n", pushCntI);

#ifdef PRINT_STAT
    printf ("c relabels:    %10ld\n", relabelCnt);
    printf ("c upd. scans:  %10ld\n", upScanCnt);
    printf ("c scans/n:     %10.2f\n", 
	    ((float) (relabelCnt + upScanCnt)) / ((float) n));
    printf ("c pushes:      %10ld\n", pushCnt);
    printf ("c updates:     %10ld\n", updateCnt);
    printf ("c gaps:        %10ld\n", gapCnt);
    printf ("c gap nodes:   %10ld\n", gNodeCnt);
    printf ("c\n");
#endif

#ifdef PRINT_FLOW
    printf ("c flow values\n");
    forAllNodes(i) {
      ni = nNode(i);
      forAllArcs(i,a) {
	na = nArc(a);
	if ( cap[na] > 0 )
	  printf ( "f %7ld %7ld %12ld\n",
		  ni, nNode( a -> head ), cap[na] - ( a -> resCap )
		  );
      }
    }
    printf("c\n");
#endif

#ifdef PRINT_CUT
  globalUpdate();
  printf ("c nodes on the sink side\n");
  forAllNodes(j)
    if (j->d < n)
      printf("c %ld\n", nNode(j));

#endif

exit(0);

}
