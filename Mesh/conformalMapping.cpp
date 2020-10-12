//crosses computed on edges with crouzeix raviart
//H computed on triangular nodes

//We have to think by GFaces

//map triangles to edges
//Map edges to triangles
//map line to edge ??
//map edge to line ??


//cross class
// -- edge associated to cross
// -- get four directions
// -- get theta (regarding the local basis)
// -- get 3 euler angles (normal fixed. It has to be manifold normal)
// -- lifting to a given direction
//Edge enriched
// -- local basis
// -- ??

//Get mesh faces and lines
//Compute gaussian curvature
//Compute geodesic curvature
//Cutting mesh along feature lines
// -- Keep correspondances to old mesh
//Creating cut graph and cutting mesh along it
// -- Keep correspondances to old mesh
//Creating smooth arbitrary 3D basis
// -- work on mesh cut along feature lines and cutgraph
//Compute H
//Create lifting of local basis on a triangle, to get phi, gamma and psi (same as in 3D, avoiding locks etc)
//Compute crossfield
//Compute overall lifting
//Compute Potentials

#include "GModel.h"
#include "GFace.h"
#include "GEdge.h"
#include "MEdge.h"
#include "MVertex.h"
#include "MTriangle.h"
#include "MLine.h"
#include <queue>
#include <limits>
#include "GmshMessage.h"

#include "conformalMapping.h"
//FOR DBG VIEWS ONLY
#include "gmsh.h"

static void getModelFaces(GModel *gm, std::set<GFace *> &f)
{
  if(f.size()>0){
    Msg::Warning("Faces set not empty. Clearing it.");
    f.clear();
  }
  for(GModel::fiter it = gm->firstFace(); it != gm->lastFace(); ++it) {
    GFace *gf = *it;
    f.insert(gf);
  }
}

static void getModelEdges(GModel *gm, std::set<GEdge *> &e)
{
  if(e.size()>0){
    Msg::Warning("Edges set not empty. Clearing it.");
    e.clear();
  }
  for(GModel::eiter it = gm->firstEdge(); it != gm->lastEdge(); ++it) {
    GEdge *ge = *it;
    e.insert(ge);
  }
}

static void getFaceEdges(GFace *gf, std::set<GEdge *> &e)
{
  if(e.size()>0){
    Msg::Warning("Edges set not empty. Clearing it.");
    e.clear();
  }
  for (GEdge* ge: gf->edges()) {
    e.insert(ge);
  }
  for (GEdge* ge: gf->embeddedEdges()) {
    e.insert(ge);
  }
}

static void addFaceTriangles(GFace *gf, std::set<MTriangle *, MElementPtrLessThan> &t)
{
  for(std::vector<MTriangle *>::const_iterator it = gf->triangles.begin(); it != gf->triangles.end(); ++it) {
    MTriangle *mt = *it;
    t.insert(mt);
  }
}

static void addEdgeLines(GEdge *ge, std::set<MLine *, MLinePtrLessThan> &l)
{
  for(std::vector<MLine *>::const_iterator it = ge->lines.begin(); it != ge->lines.end(); ++it) {
    MLine *ml = *it;
    l.insert(ml);
  }
}

static void addEdgeLinesEntities(GEdge *ge, std::map<MLine *, GEdge *, MLinePtrLessThan> &linesEntities)
{
  for(std::vector<MLine *>::const_iterator it = ge->lines.begin(); it != ge->lines.end(); ++it) {
    MLine *ml = *it;
    linesEntities[ml]=ge;
  }
}

static void getAllLinesAndTriangles(GModel *gm, std::set<MLine *, MLinePtrLessThan> &l, std::map<MLine *, GEdge *, MLinePtrLessThan> &linesEntities, std::set<MTriangle *, MElementPtrLessThan> &t){
  if(l.size()>0){
    Msg::Warning("Lines set not empty. Clearing it.");
    l.clear();
  }
  if(t.size()>0){
    Msg::Warning("Triangles set not empty. Clearing it");
    t.clear();
  }
  std::set<GFace *> f;
  std::set<GEdge *> e;
  getModelFaces(gm, f);
  getModelEdges(gm, e);
  for(GEdge *ge: e){
    addEdgeLines(ge, l);
    addEdgeLinesEntities(ge,linesEntities);
  }
  for(GFace *gf: f)
    addFaceTriangles(gf, t);
}

static void getFaceLinesAndTriangles(GFace *gf, std::set<MLine *, MLinePtrLessThan> &l,std::set<MTriangle *, MElementPtrLessThan> &t){
  if(l.size()>0){
    Msg::Warning("Lines set not empty. Clearing it.");
    l.clear();
  }
  if(t.size()>0){
    Msg::Warning("Triangles set not empty. Clearing it");
    t.clear();
  }
  for(GEdge* ge: gf->edges())
    addEdgeLines(ge, l);
  for(GEdge* ge: gf->embeddedEdges())
    addEdgeLines(ge, l);
  addFaceTriangles(gf, t);
}

MyMesh::MyMesh(GModel *gm){
  getAllLinesAndTriangles(gm, lines, linesEntities, triangles);
  for(MTriangle *t: triangles){
    SVector3 v10(t->getVertex(1)->x() - t->getVertex(0)->x(),
		 t->getVertex(1)->y() - t->getVertex(0)->y(),
		 t->getVertex(1)->z() - t->getVertex(0)->z());
    SVector3 v20(t->getVertex(2)->x() - t->getVertex(0)->x(),
		 t->getVertex(2)->y() - t->getVertex(0)->y(),
		 t->getVertex(2)->z() - t->getVertex(0)->z());
    SVector3 normal_to_triangle = crossprod(v20, v10);
    normal_to_triangle.normalize();
    for(int k=0;k<3;k++){
      MEdge edgeK=t->getEdge(k);
      std::pair<std::set<MEdge, MEdgeLessThan>::iterator,bool> insertData;
      insertData=edges.insert(edgeK);
      triangleToEdges[t].push_back(&(*insertData.first));
      edgeToTriangles[&(*insertData.first)].push_back(t);
      normals[&(*insertData.first)]+=normal_to_triangle;
      isFeatureEdge[&(*insertData.first)]=false;
      MVertex *vK=t->getVertex(k);
      normalsVertex[vK]+=normal_to_triangle;
    }
  }
  for(std::map<const MEdge *, SVector3>::value_type& kv: normals)
    kv.second.normalize();
  for(std::map<MVertex *, SVector3>::value_type& kv: normalsVertex)
    kv.second.normalize();
  
  for(MLine *l: lines){
    std::set<MEdge, MEdgeLessThan>::iterator it=edges.find(l->getEdge(0));
    if(it==edges.end()){
      Msg::Error("Cannot find feature edge in mesh edges");
      return;
    }
    else{
      featureDiscreteEdges.insert(&(*it));
      isFeatureEdge[&(*it)]=true;
      featureDiscreteEdgesEntities[&(*it)]=linesEntities[l];
    }
  }
  for(const MEdge *e: featureDiscreteEdges){
    featureVertices.insert(e->getVertex(0));
    featureVertices.insert(e->getVertex(1));
    featureVertexToEdges[e->getVertex(0)].insert(e);
    featureVertexToEdges[e->getVertex(1)].insert(e);
  }
  getSingularities(gm);
}

void MyMesh::getSingularities(GModel *gm){
  std::map<int, std::vector<GEntity *> > groups[4];
  gm->getPhysicalGroups(groups);
  for(std::map<int, std::vector<GEntity *> >::iterator it = groups[0].begin();
      it != groups[0].end(); ++it) {
    std::string name = gm->getPhysicalName(0, it->first);
    if(name == "SINGULARITY_OF_INDEX_THREE") {
      for(size_t j = 0; j < it->second.size(); j++) {
        if(!it->second[j]->mesh_vertices.empty()){
	  singularities.insert(it->second[j]->mesh_vertices[0]);
	  singIndices[it->second[j]->mesh_vertices[0]] = -1;
	}
      }
    }
    else if(name == "SINGULARITY_OF_INDEX_FIVE") {
      for(size_t j = 0; j < it->second.size(); j++) {
        if(!it->second[j]->mesh_vertices.empty()){
	  singularities.insert(it->second[j]->mesh_vertices[0]);
	  singIndices[it->second[j]->mesh_vertices[0]] = 1;
	}
      }
    }
    else if(name == "SINGULARITY_OF_INDEX_SIX") {
      for(size_t j = 0; j < it->second.size(); j++) {
        if(!it->second[j]->mesh_vertices.empty()){
	  singularities.insert(it->second[j]->mesh_vertices[0]);
	  singIndices[it->second[j]->mesh_vertices[0]] = 2;
	}
      }
    }
    else if(name == "SINGULARITY_OF_INDEX_EIGHT") {
      for(size_t j = 0; j < it->second.size(); j++) {
        if(!it->second[j]->mesh_vertices.empty()){
	  singularities.insert(it->second[j]->mesh_vertices[0]);
	  singIndices[it->second[j]->mesh_vertices[0]] = 4;
	}
      }
    }
    else if(name == "SINGULARITY_OF_INDEX_TWO") {
      for(size_t j = 0; j < it->second.size(); j++) {
        if(!it->second[j]->mesh_vertices.empty()){
	  singularities.insert(it->second[j]->mesh_vertices[0]);
	  singIndices[it->second[j]->mesh_vertices[0]] = -2;
	}
      }
    }
  }
  return;
}

MyMesh::MyMesh(MyMesh &originalMesh){
  for(MTriangle *t: originalMesh.triangles){
    triangles.insert(t);
  }
  for(MLine *l: originalMesh.lines){
    lines.insert(l);
  }
  for(auto &kv: originalMesh.linesEntities){
    linesEntities[kv.first]=kv.second;
  }
  for(MTriangle *t: triangles)
    for(int k=0;k<3;k++){
      MEdge edgeK=t->getEdge(k);
      std::pair<std::set<MEdge, MEdgeLessThan>::iterator,bool> insertData;
      insertData=edges.insert(edgeK);
      triangleToEdges[t].push_back(&(*insertData.first));
      edgeToTriangles[&(*insertData.first)].push_back(t);
      isFeatureEdge[&(*insertData.first)]=false;
    }
  for(MLine *l: lines){
    std::set<MEdge, MEdgeLessThan>::iterator it=edges.find(l->getEdge(0));
    if(it==edges.end()){
      Msg::Error("Cannot find feature edge in mesh edges");
      return;
    }
    else{
      featureDiscreteEdges.insert(&(*it));
      featureDiscreteEdgesEntities[&(*it)]=linesEntities[l];
      isFeatureEdge[&(*it)]=true;
    }
  }
  for(const MEdge *e: featureDiscreteEdges){
    featureVertices.insert(e->getVertex(0));
    featureVertices.insert(e->getVertex(1));
    featureVertexToEdges[e->getVertex(0)].insert(e);
    featureVertexToEdges[e->getVertex(1)].insert(e);
  }
  //TODO need smart gestion of singularities (everything can change when cutting
  for(MVertex *v:originalMesh.singularities){
    singularities.insert(v);
    singIndices[v]=originalMesh.singIndices[v];
  }
}

void MyMesh::updateNormals(){
  normals.clear();
  normalsVertex.clear();
  for(MTriangle *t: triangles){
    SVector3 v10(t->getVertex(1)->x() - t->getVertex(0)->x(),
		 t->getVertex(1)->y() - t->getVertex(0)->y(),
		 t->getVertex(1)->z() - t->getVertex(0)->z());
    SVector3 v20(t->getVertex(2)->x() - t->getVertex(0)->x(),
		 t->getVertex(2)->y() - t->getVertex(0)->y(),
		 t->getVertex(2)->z() - t->getVertex(0)->z());
    SVector3 normal_to_triangle = crossprod(v20, v10);
    normal_to_triangle.normalize();
    for(int k=0;k<3;k++){
      MEdge edgeK=t->getEdge(k);
      std::pair<std::set<MEdge, MEdgeLessThan>::iterator,bool> insertData;
      insertData=edges.insert(edgeK);
      if(insertData.second){
	Msg::Error("Reached an edge not listed while updating normals");
	return;
      }
      normals[&(*insertData.first)]+=normal_to_triangle;
      MVertex *vK=t->getVertex(k);
      normalsVertex[vK]+=normal_to_triangle;
    }
  }
  for(std::map<const MEdge *, SVector3>::value_type& kv: normals)
    kv.second.normalize();
  for(std::map<MVertex *, SVector3>::value_type& kv: normalsVertex)
    kv.second.normalize();
}

void MyMesh::updateEdges(){
  triangleToEdges.clear();
  for(MTriangle *t: triangles)
    for(int k=0;k<3;k++){
      MEdge edgeK=t->getEdge(k);
      std::pair<std::set<MEdge, MEdgeLessThan>::iterator,bool> insertData;
      insertData=edges.insert(edgeK);
      triangleToEdges[t].push_back(&(*insertData.first));
      edgeToTriangles[&(*insertData.first)].push_back(t);
      if(insertData.second)
	isFeatureEdge[&(*insertData.first)]=false;
    }
}

void MyMesh::_computeDarbouxFrameOnFeatureVertices(){ //darboux frame with geodesic normal poiting inside the domain
  MVertexPtrEqual isVertTheSame;
  MEdgeEqual isEdgeTheSame;
  if(normals.size()==0||normalsVertex.size()==0)
    updateNormals();
  for(MVertex *v: featureVertices){
    if(featureVertexToEdges[v].size()!=2){
      Msg::Error("Can't run darboux frame on a mesh not splitted along feature edges");
      return;
    }

    std::set<const MEdge *>::iterator itSetEdge=featureVertexToEdges[v].begin();
    const MEdge *e1=*(itSetEdge);
    itSetEdge++;
    const MEdge *e2=*(itSetEdge);
    MVertex *v1=e1->getVertex(0);
    if(isVertTheSame(v1,v))
      v1=e1->getVertex(1);
    MVertex *v2=e2->getVertex(0);
    if(isVertTheSame(v2,v))
      v2=e2->getVertex(1);
    SVector3 v01(v1->x() - v->x(),
		 v1->y() - v->y(),
		 v1->z() - v->z());
    v01.normalize();
    SVector3 v02(v2->x() - v->x(),
		 v2->y() - v->y(),
		 v2->z() - v->z());
    v02.normalize();
    SVector3 normalV = normalsVertex[v];
    SVector3 t01 = crossprod(normalV, v01);
    t01.normalize();
    double cosV02=dot(v02,v01);
    double sinV02=dot(v02,t01);
    if(edgeToTriangles[e1].size()!=1){
      Msg::Error("Can't run darboux frame on a mesh not splitted along feature edges");
      return;
    }
    MTriangle *tri = *(edgeToTriangles[e1].begin());
    const MEdge *eRef=NULL;	
    for(const MEdge* eT: triangleToEdges[tri]){
      if(!isEdgeTheSame(*eT,*e1)){
	if(isVertTheSame(eT->getVertex(0),v)||isVertTheSame(eT->getVertex(1),v)){
	  eRef=eT;
	}
      }
    }
    if(eRef==NULL){
      Msg::Error("Can't run darboux frame. Not finding reference edge for orientability");
      return;      
    }
    MVertex *vRef=eRef->getVertex(0);
    if(isVertTheSame(vRef,v))
      vRef=eRef->getVertex(1);
    SVector3 vectRef(vRef->x() - v->x(),
		     vRef->y() - v->y(),
		     vRef->z() - v->z());
    vectRef.normalize();
    double cosRef=dot(vectRef,v01);;
    double sinRef=dot(vectRef,t01);
    double angleRef=atan2(sinRef,cosRef);
    double angleV02=atan2(sinV02,cosV02);
    if(angleRef>0){
      if(angleV02<=0+1e-10)
	angleV02+=2*M_PI;
    }
    else
      if(angleV02>=0-1e-10)
	angleV02-=2*M_PI;
	
    double angleInnerGeodesicNormal=angleV02/2.0;
    SVector3 innerGeodesicNormal = cos(angleInnerGeodesicNormal)*v01 + sin(angleInnerGeodesicNormal)*t01;
    innerGeodesicNormal.normalize();
    SVector3 T = crossprod(innerGeodesicNormal, normalV);
    T.normalize();
    _darbouxFrameVertices[v].push_back(T);
    _darbouxFrameVertices[v].push_back(innerGeodesicNormal);
    _darbouxFrameVertices[v].push_back(normalV);
  }
  return;
}

void MyMesh::_computeGeodesicCurv(){
  
  return;
}

ConformalMapping::ConformalMapping(GModel *gm): _currentMesh(NULL), _gm(gm), _initialMesh(NULL), _featureCutMesh(NULL), _cutGraphCutMesh(NULL)
{
  _initialMesh = new MyMesh(gm);
  _currentMesh=_initialMesh;
  _getFeatureVertAndSing();
  // _initialMesh->viewNormals();
  //cut mesh on feature lines here
  Msg::Info("Cutting mesh on feature lines");
  _cutMeshOnFeatureLines();
  _currentMesh=_featureCutMesh;
  Msg::Info("Creating cut graph");
  _createCutGraph();
  _currentMesh->computeGeoCharac();
  // _featureCutMesh->computeGeoCharac();
  //Solve H here
  //create cutgraph and cut mesh along it
  // _cutMeshOnCutGraph();
  //solve crosses
  //parametrization
}

void ConformalMapping::_computeDistancesToBndAndSing(){
  typedef std::pair<double, MVertex*> weightedVertex;
  std::map<MVertex *, std::set<weightedVertex>> weightedConnectivity;
  for(const MEdge &e: _currentMesh->edges){
    double length=e.length();
    weightedConnectivity[e.getVertex(0)].insert(weightedVertex(length,e.getVertex(1)));
    weightedConnectivity[e.getVertex(1)].insert(weightedVertex(length,e.getVertex(0)));
    _distanceToFeatureAndSing[e.getVertex(0)]=std::numeric_limits<double>::max();
    _distanceToFeatureAndSing[e.getVertex(1)]=std::numeric_limits<double>::max();
  }
  std::priority_queue<weightedVertex,std::vector<weightedVertex>, std::greater<weightedVertex>> priorityQueue;
  for(MVertex *v: _currentMesh->featureVertices){
    _distanceToFeatureAndSing[v]=0.0;
    priorityQueue.push(weightedVertex(0.0,v));
  }
  for(MVertex *v: _currentMesh->singularities){
    _distanceToFeatureAndSing[v]=0.0;
    priorityQueue.push(weightedVertex(0.0,v));
  }
  while(!priorityQueue.empty()){
    MVertex *v=priorityQueue.top().second;
    double distV=_distanceToFeatureAndSing[v];
    priorityQueue.pop();
    for(const weightedVertex &wV: weightedConnectivity[v]){
      double dist=_distanceToFeatureAndSing[wV.second];
      if(dist>distV+wV.first){
	_distanceToFeatureAndSing[wV.second]=distV+wV.first;
	priorityQueue.push(weightedVertex(_distanceToFeatureAndSing[wV.second],wV.second));
      }
    }
  }
  _viewScalarVertex(_distanceToFeatureAndSing,"distances"); //DBG
}

void ConformalMapping::_createCutGraph(){
  Msg::Info("Computing distances");
  _computeDistancesToBndAndSing();
  std::set<const MEdge *> edgeTree = _createEdgeTree();
  _trimEdgeTree(edgeTree);
  _viewEdges(edgeTree,"cutgraphTrimmed");  
}

std::set<const MEdge *> ConformalMapping::_createEdgeTree(){
  typedef std::pair<double,MTriangle *> weightedTri;
  std::set<weightedTri> weightedTriangles;
  std::map<MTriangle *, bool, MElementPtrLessThan> trianglePassed;
  std::map<MTriangle *, double, MElementPtrLessThan> triangleWeight;
  std::priority_queue<weightedTri,std::vector<weightedTri>, std::less<weightedTri>> pQueue;
  // std::priority_queue<weightedTri,std::vector<weightedTri>, std::greater<weightedTri>> pQueue;
  std::set<const MEdge *> edgeTree;
  for(MTriangle *t: _currentMesh->triangles){
    trianglePassed[t]=false;
    triangleWeight[t]=0.0;
    for(int k=0;k<3;k++){
      MVertex *v=t->getVertex(k);
      triangleWeight[t]+=_distanceToFeatureAndSing[v];
    }
    triangleWeight[t]/=3.0;
  }
  for(const MEdge &e: _currentMesh->edges){
    edgeTree.insert(&e);
  }
  //Put random triangle as passed (or well chosen one
  MTriangle *triangleLeft=NULL;
  for(auto &kv: trianglePassed){
    if(!kv.second){
      triangleLeft=kv.first;
      break;
    }
  }
  pQueue.push(weightedTri(triangleWeight[triangleLeft],triangleLeft));
  trianglePassed[triangleLeft]=true;
  while(triangleLeft){
    for(const MEdge *e: _currentMesh->triangleToEdges[triangleLeft]){
      if(!_currentMesh->isFeatureEdge[e]){
	for(MTriangle *t: _currentMesh->edgeToTriangles[e]){
	  if(!trianglePassed[t]){
	    pQueue.push(weightedTri(triangleWeight[t],t));
	    trianglePassed[t]=true;
	    edgeTree.erase(e);
	  }
	}
      }
    }
    if(!pQueue.empty()){
      triangleLeft = pQueue.top().second;
      pQueue.pop();
    }
    else{
      triangleLeft=NULL;
      for(auto &kv: trianglePassed){
	if(!kv.second){
	  triangleLeft=kv.first;
	  break;
	}
      }
    }
  }
  return edgeTree;
}

void ConformalMapping::_trimEdgeTree(std::set<const MEdge *> &edgeTree){//TODO: can be tremendously accelerated (killing all trees with root on boundary/sing and no sing as leave right away, then trimming the one left). Option 2: trimming can actually be in a 4 loop, we store falgs for deleted edges and voila.
  std::map<MVertex *, int> multVertex;
  for(const MEdge *e: edgeTree){
    multVertex[e->getVertex(0)]++;
    multVertex[e->getVertex(1)]++;
  }
  for(const MEdge *e: _currentMesh->featureDiscreteEdges){
    multVertex[e->getVertex(0)]++;
    multVertex[e->getVertex(1)]++;
  }
  for(MVertex *v: _currentMesh->singularities){//TODO: we have to think about what to do with singularities which are on a boundary
    multVertex[v]++;
  }
  std::map<MVertex *, std::vector<MVertex*>> connectivity;
  std::map<MVertex *, std::vector<const MEdge*>> edgesConnect;
  for(const MEdge *e: edgeTree){
    connectivity[e->getVertex(0)].push_back(e->getVertex(1));
    connectivity[e->getVertex(1)].push_back(e->getVertex(0));
    edgesConnect[e->getVertex(0)].push_back(e);
    edgesConnect[e->getVertex(1)].push_back(e);
  }
  MVertex *vLeaf=NULL;
  const MEdge *eLeaf=NULL;
  for(auto &kv: connectivity){
    if(kv.second.size()==1){
      if(_currentMesh->singularities.find(kv.first)==_currentMesh->singularities.end()){
	vLeaf = kv.first;
	eLeaf = edgesConnect[vLeaf][0];
	break;
      }
    }
  }
  while(vLeaf){
    MVertex *nextLeaf=connectivity[vLeaf][0];
    connectivity[vLeaf].pop_back();
    edgesConnect[vLeaf].pop_back();
    edgeTree.erase(eLeaf);
    std::vector<const MEdge*>::iterator itEdgeConn= edgesConnect[nextLeaf].begin();
    for(std::vector<MVertex*>::iterator itVertConn = connectivity[nextLeaf].begin(); itVertConn != connectivity[nextLeaf].end() ; itVertConn++){
      if(*itVertConn!=vLeaf){
	itEdgeConn++;
      }
      else{
	connectivity[nextLeaf].erase(itVertConn);
	edgesConnect[nextLeaf].erase(itEdgeConn);
	break;
      }
    }
    if(connectivity[nextLeaf].size()==1 && _currentMesh->singularities.find(nextLeaf)==_currentMesh->singularities.end()){
      vLeaf=nextLeaf;
      eLeaf=edgesConnect[vLeaf][0];
    }
    else{
      vLeaf=NULL;
      eLeaf=NULL;
      for(auto &kv: connectivity){
	if(kv.second.size()==1){
	  if(_currentMesh->singularities.find(kv.first)==_currentMesh->singularities.end()){
	    vLeaf = kv.first;
	    eLeaf = edgesConnect[vLeaf][0];
	    break;
	  }
	}
      }
    }
  }
}

void ConformalMapping::_cutMeshOnFeatureLines(){
  _featureCutMesh = new MyMesh(*_initialMesh);
  std::map<MVertex *, int> multVert;
  for(MTriangle *t: _featureCutMesh->triangles){
    for(int k=0;k<3;k++){
      multVert[t->getVertex(k)]=0;
      _featureToInitMeshVertex[t->getVertex(k)]=t->getVertex(k);
    }
  }
  for(MLine *l: _featureCutMesh->lines){
    for(int k=0;k<2;k++){
      multVert[l->getVertex(k)]++;
    }
  }
  // _featureCutMesh->viewMult(multVert);
  //Delete all edges and lines
  _featureCutMesh->featureVertices.clear();
  _featureCutMesh->triangleToEdges.clear();
  _featureCutMesh->normals.clear();
  _featureCutMesh->edges.clear();
  _featureCutMesh->lines.clear();
  _featureCutMesh->linesEntities.clear();
  _featureCutMesh->isFeatureEdge.clear();
  _featureCutMesh->featureDiscreteEdges.clear();
  _featureCutMesh->featureDiscreteEdgesEntities.clear();
  _featureCutMesh->edgeToTriangles.clear();
  _featureCutMesh->featureVertexToEdges.clear();


  //Cut the mesh
  //loop on tri
  std::map<MVertex *, int> multVertexProcessed;
  std::map<MTriangle *, std::vector<MVertex *>> vertexProcessedPerTriangle;
  std::set<MTriangle *, MElementPtrLessThan> trianglesQueue;
  for(const MEdge *fe: _initialMesh->featureDiscreteEdges){
    for(int k=0; k<2;k++){
      MVertex *currentVertex;
      currentVertex = fe->getVertex(k);
      for(MTriangle *t:_initialMesh->edgeToTriangles[fe]){
	MVertex *newVertex=currentVertex;
	if(multVertexProcessed[currentVertex]>0){
	  GEntity *geVertex = currentVertex->onWhat();
	  // //DBG
	  // double fact = (1.0+0.05*multVertexProcessed[currentVertex]);
	  // newVertex = new MVertex(currentVertex->x()*fact,currentVertex->y()*fact,currentVertex->z()*fact,geVertex,0);
	  // //
	  newVertex = new MVertex(currentVertex->x(),currentVertex->y(),currentVertex->z(),geVertex,0);
	  geVertex->mesh_vertices.push_back(newVertex);
	}
	trianglesQueue.clear();
	std::vector<MVertex *>::iterator vertexProcessed=std::find(vertexProcessedPerTriangle[t].begin(),vertexProcessedPerTriangle[t].end(),currentVertex);
	if(vertexProcessed==vertexProcessedPerTriangle[t].end()){
	  trianglesQueue.insert(t);
	  _featureToInitMeshVertex[newVertex]=currentVertex;
	}
	while(trianglesQueue.size()>0){
	  std::set<MTriangle *, MElementPtrLessThan>::iterator it=trianglesQueue.begin();
	  MTriangle *currentTri=*it;
	  for(int l=0;l<3;l++){
	    MVertexPtrEqual isVertTheSame;
	    if(isVertTheSame(currentTri->getVertex(l),currentVertex)){
	      currentTri->setVertex(l,newVertex);
	      // Getting other triangles to add to queue
	      for(int m=0;m<2;m++){
	      	const MEdge *ptrEdgeM=_initialMesh->triangleToEdges[currentTri][(l+2+m)%3];
	      	if(!_initialMesh->isFeatureEdge[ptrEdgeM]){
	      	  for(MTriangle *triCandidate: _initialMesh->edgeToTriangles[ptrEdgeM]){
	      	    std::vector<MVertex *>::iterator itProcessed=std::find(vertexProcessedPerTriangle[triCandidate].begin(),vertexProcessedPerTriangle[triCandidate].end(),currentVertex);
	      	    MElementPtrEqual isTriTheSame;
	      	    if((!isTriTheSame(triCandidate, currentTri))&&(itProcessed==vertexProcessedPerTriangle[triCandidate].end())){
	      	      trianglesQueue.insert(triCandidate);
	      	    }
	      	  }
	      	}
	      }
	    }
	  }
	  vertexProcessedPerTriangle[currentTri].push_back(currentVertex);
	  trianglesQueue.erase(it);
	}
	multVertexProcessed[currentVertex]++;
      }
    }
    //add new feature MLine and MEdge if some where created
    for(MTriangle *t:_initialMesh->edgeToTriangles[fe]){
      MEdgeEqual isEdgeTheSame;
      for(int k=0;k<3;k++){
    	MEdge newEdgeK=t->getEdge(k);
	if(_featureToInitMeshVertex[newEdgeK.getVertex(0)]!=NULL&&_featureToInitMeshVertex[newEdgeK.getVertex(1)]!=NULL){
	  MEdge edgeK(_featureToInitMeshVertex[newEdgeK.getVertex(0)],_featureToInitMeshVertex[newEdgeK.getVertex(1)]);
	  if(isEdgeTheSame(edgeK,*fe)){
	    std::pair<std::set<MEdge, MEdgeLessThan>::iterator,bool> insertData;
	    insertData=_featureCutMesh->edges.insert(newEdgeK);
	    if(insertData.second){
	      _featureCutMesh->isFeatureEdge[&(*insertData.first)]=true;
	      _featureCutMesh->featureDiscreteEdges.insert(&(*insertData.first));
	      MLine *newLine;
	      newLine = new MLine((*(insertData.first)).getVertex(0),(*(insertData.first)).getVertex(1));
	      _featureCutMesh->lines.insert(newLine);
	    
	      _featureCutMesh->linesEntities[newLine]=_initialMesh->featureDiscreteEdgesEntities[fe];
	      _featureCutMesh->featureDiscreteEdgesEntities[&(*insertData.first)]=_initialMesh->featureDiscreteEdgesEntities[fe];
	      _featureCutMesh->featureDiscreteEdgesEntities[&(*insertData.first)]->addLine(newLine); //TODO has to be removed in the end
	    }
	  }
	}
      }
    }
  }
  _featureCutMesh->updateEdges();
  _featureCutMesh->updateNormals();
  for(const MEdge *e: _featureCutMesh->featureDiscreteEdges){
    _featureCutMesh->featureVertices.insert(e->getVertex(0));
    _featureCutMesh->featureVertices.insert(e->getVertex(1));
    _featureCutMesh->featureVertexToEdges[e->getVertex(0)].insert(e);
    _featureCutMesh->featureVertexToEdges[e->getVertex(1)].insert(e);
  }
}

ConformalMapping::~ConformalMapping(){
  if(_initialMesh)
    delete _initialMesh;
  if(_featureCutMesh)
    delete _featureCutMesh;
  if(_cutGraphCutMesh)
    delete _cutGraphCutMesh;
}

void MyMesh::viewNormals(){ //for DBG only
#if defined(HAVE_POST)

  std::vector<double> datalist;
  for(const MEdge &e: edges){
    SPoint3 b=e.barycenter();
    for (size_t lv = 0; lv < 3; ++lv){
      datalist.push_back(b[lv]);
    }
    for (size_t lv = 0; lv < 3; ++lv){
      datalist.push_back(normals[&e][lv]);
    }
  }
  gmsh::initialize();
  Msg::Debug("create view '%s'","normals");
  int dataListViewTag = gmsh::view::add("normals");
  gmsh::view::addListData(dataListViewTag, "VP", datalist.size()/(3+3), datalist);
#else 
  Msg::Error("Cannot create view without POST module");
  return;
#endif
}

void MyMesh::viewDarbouxFrame(){ //for DBG only
#if defined(HAVE_POST)

  std::vector<double> datalist;
  for(const auto &kv: _darbouxFrameVertices){
    MVertex *v=kv.first;
    std::vector<SVector3> darbouxFrame = kv.second;
    for (size_t lv = 0; lv < 1; ++lv){
      datalist.push_back(v->x());
      datalist.push_back(v->y());
      datalist.push_back(v->z());
      datalist.push_back(darbouxFrame[lv][0]);
      datalist.push_back(darbouxFrame[lv][1]);
      datalist.push_back(darbouxFrame[lv][2]);
    }
  }
  gmsh::initialize();
  Msg::Debug("create view '%s'","darboux frames");
  int dataListViewTag = gmsh::view::add("darboux frames");
  gmsh::view::addListData(dataListViewTag, "VP", datalist.size()/(3+3), datalist);
#else 
  Msg::Error("Cannot create view without POST module");
  return;
#endif
}

void MyMesh::viewMult(std::map<MVertex *, int> &multVert){ //for DBG only
#if defined(HAVE_POST)

  std::vector<double> datalist;
  for(const auto& kv: multVert){
    datalist.push_back(kv.first->x());
    datalist.push_back(kv.first->y());
    datalist.push_back(kv.first->z());
    datalist.push_back(kv.second);
  }
  gmsh::initialize();
  Msg::Debug("create view '%s'","mult");
  int dataListViewTag = gmsh::view::add("mult");
  gmsh::view::addListData(dataListViewTag, "SP", datalist.size()/(3+1), datalist);
#else 
  Msg::Error("Cannot create view without POST module");
  return;
#endif
}

void ConformalMapping::_viewScalarVertex(std::map<MVertex *, double, MVertexPtrLessThan> &scalar, const std::string& viewName){ //for DBG only
#if defined(HAVE_POST)

  std::vector<double> datalist;
  for(const auto& kv: scalar){
    datalist.push_back(kv.first->x());
    datalist.push_back(kv.first->y());
    datalist.push_back(kv.first->z());
    datalist.push_back(kv.second);
  }
  gmsh::initialize();
  Msg::Debug("create view '%s'",viewName);
  int dataListViewTag = gmsh::view::add(viewName);
  gmsh::view::addListData(dataListViewTag, "SP", datalist.size()/(3+1), datalist);
#else 
  Msg::Error("Cannot create view without POST module");
  return;
#endif
}

void ConformalMapping::_viewEdges(std::set<const MEdge*> &edges, const std::string& viewName){ //for DBG only
#if defined(HAVE_POST)

  std::vector<double> datalist;
  std::cout << "numbers of edges : " << edges.size() << std::endl;
  for(const MEdge *e: edges){
    datalist.push_back(e->getVertex(0)->x());
    datalist.push_back(e->getVertex(1)->x());
    datalist.push_back(e->getVertex(0)->y());
    datalist.push_back(e->getVertex(1)->y());
    datalist.push_back(e->getVertex(0)->z());
    datalist.push_back(e->getVertex(1)->z());
    datalist.push_back(1.0);
    datalist.push_back(1.0);
  }
  gmsh::initialize();
  Msg::Debug("create view '%s'",viewName);
  int dataListViewTag = gmsh::view::add(viewName);
  // gmsh::view::addListData(dataListViewTag, "SL", datalist.size()/(3+3+2), datalist);
  gmsh::view::addListData(dataListViewTag, "SL", edges.size(), datalist);
#else 
  Msg::Error("Cannot create view without POST module");
  return;
#endif
}
