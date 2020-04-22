// Gmsh - Copyright (C) 1997-2020 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// issues on https://gitlab.onelab.info/gmsh/gmsh/issues.

#ifndef GMSH_EDGE_H
#define GMSH_EDGE_H

#include "GEdge.h"

class Curve;

class gmshEdge : public GEdge {
protected:
  Curve *c;

public:
  gmshEdge(GModel *model, Curve *edge, GVertex *v1, GVertex *v2);
  virtual ~gmshEdge() {}
  virtual Range<double> parBounds(int i) const;
  virtual GeomType geomType() const;
  virtual bool haveParametrization();
  virtual GPoint point(double p) const;
  virtual SVector3 firstDer(double par) const;
  virtual SVector3 secondDer(double par) const;
  ModelType getNativeType() const { return GmshModel; }
  void *getNativePtr() const { return c; }
  virtual std::string getAdditionalInfoString(bool multline = false);
  virtual int minimumMeshSegments() const;
  virtual int minimumDrawSegments() const;
  virtual void resetMeshAttributes();
  virtual SPoint2 reparamOnFace(const GFace *face, double epar, int dir) const;
  virtual void writeGEO(FILE *fp);
  void discretize(double tol, std::vector<SPoint3> &dpts,
                  std::vector<double> &ts);
  virtual bool degenerate(int dim) const;
  void resetNativePtr(Curve *edge, GVertex *v1, GVertex *v2);
  virtual double prescribedMeshSizeAtParam(double u);
};

#endif
