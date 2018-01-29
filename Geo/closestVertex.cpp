// Gmsh - Copyright (C) 1997-2017 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// bugs and problems to the public mailing list <gmsh@onelab.info>.

#include "closestVertex.h"
#include "GEntity.h"
#include "GEdge.h"
#include "GFace.h"
#include <vector>

closestVertexFinder :: closestVertexFinder (GEntity *ge,bool closure):
  nbVtcs(0) {
  
#if defined(HAVE_ANN)
  
  
  std::set<MVertex*> vtcs;
  ge->addVerticesInSet(vtcs,closure);
  
  nbVtcs = vtcs.size();
  vertex = new MVertex*[nbVtcs];
  index = new ANNidx[1];
  dist = new ANNdist[1];
  
  vCoord  = annAllocPts(nbVtcs, 3);
  
  int k = 0;
  std::set<MVertex*>::iterator vIter=vtcs.begin();
  for (;vIter!=vtcs.end();++vIter,++k) {
    MVertex* mv = *vIter;
    vCoord[k][0] = mv->x();
    vCoord[k][1] = mv->y();
    vCoord[k][2] = mv->z();
    vertex[k] = mv;
  }
  kdtree = new ANNkd_tree(vCoord,nbVtcs, 3);
#else
  Msg::Fatal("Gmsh should be compiled using ANN");
#endif
}

closestVertexFinder :: ~closestVertexFinder ()
{
#if defined(HAVE_ANN)
  if(kdtree) delete kdtree;
  if(vCoord) annDeallocPts(vCoord);
  if(vertex) delete[] vertex;
  delete[]index;
  delete[]dist;
#endif
}

MVertex* closestVertexFinder ::operator() (const SPoint3& p)
{
#if defined(HAVE_ANN)
  double xyz[3] = {p.x(),p.y(),p.z()};
  kdtree->annkSearch(xyz, 1, index, dist);
  return vertex[index[0]];
#else
  return NULL;
#endif
}


MVertex* closestVertexFinder ::operator() (const SPoint3& p,
                                           const std::vector<double>& tfo)
{
#if defined(HAVE_ANN)
  double ori[4] = {p.x(),p.y(),p.z(),1};
  double xyz[4] = {0,0,0,0};
  if (tfo.size() == 16) {
    int idx=0;
    // std::cout << "Transforming ";
    for (int i=0;i<4;i++) for (int j=0;j<4;j++) xyz[i] += tfo[idx++] * ori[j];
    
    /// std::cout << "(" << ori[0] << "," << ori[1] << "," << ori[2] << ") -> ";
    /// std::cout << "(" << xyz[0] << "," << xyz[1] << "," << xyz[2] << ")" << std::endl;
  }
  else std::memcpy(xyz,ori,3*sizeof(double));
  kdtree->annkSearch(xyz, 1, index, dist);
  return vertex[index[0]];
#else
  return NULL;
#endif
}


