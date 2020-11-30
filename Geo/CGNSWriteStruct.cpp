// Gmsh - Copyright (C) 1997-2020 C. Geuzaine, J.-F. Remacle
//
// See the LICENSE.txt file for license information. Please report all
// issues on https://gitlab.onelab.info/gmsh/gmsh/issues.
//

#include <math.h>
#include <sstream>
#include "GModel.h"
#include "Context.h"
#include "CGNSCommon.h"
#include "CGNSConventions.h"

// The structured CGNS writer follows ICEM-like conventions, which are
// significantly different from those used in the unstructured CGNS writer:
//
// - structured 3D (resp. 2D) meshes contain one zone per volume (resp.
//   surface) and no zones are created for lower dimensional entities; the
//   unstructured writer creates a single zone per mesh partition (i.e. a single
//   zone for non-partitioned meshes)
//
// - all structured volumes (3D) or surfaces (2D) are saved; if physical groups
//   are defined, the unstructured writer only saves entities belonging to
//   physical groups, unless Mesh.SaveAll is set
//
// - interfaces between structured zones are 1-to-1, with point ranges; the
//   unstructured writer creates interfaces (between partitions) defined by
//   point lists
//
// - elementary entity tags are encoded in the zone name; the unstructured
//   writer writes elementary entity information as boundary conditions and
//   family names
//
// - physical groups for the highest dimensional entities are directly encoded
//   at the begining of the zone name; the unstructured writer saves physical
//   names in families
//
// - boundary conditions are created for physical groups of lower dimensions

#if defined(HAVE_LIBCGNS)

static std::string dimName(int dim)
{
  if(dim == 3) return "Volume";
  else if(dim == 2) return "Surface";
  else if(dim == 1) return "Curve";
  else return "Point";
}

static std::string getZoneName(GEntity *ge)
{
  std::ostringstream sstream;
  for(auto t: ge->physicals) {
    std::string n = ge->model()->getPhysicalName(ge->dim(), t);
    if(n.empty())
      sstream << "Physical" << dimName(ge->dim()) << t << "_";
    else
      sstream << n << "_";
  }
  sstream << dimName(ge->dim()) << ge->tag();
  return sstream.str();
}

static std::string getInterfaceName(GEntity *ge1, GEntity *ge2)
{
  std::ostringstream sstream;
  sstream << "Interface_" << dimName(ge1->dim()) << ge1->tag()
          << "_" << dimName(ge2->dim()) << ge2->tag();
  return sstream.str();
}

static void computeTransform2D(const std::vector<cgsize_t> &pointRange,
                               const std::vector<cgsize_t> &pointDonorRange,
                               int transform[2])
{
  // This routine was written with the financial aid of of Indian Institute of
  // Technology Hyderabad - BRNS sponsored project in 2018, under the guidance
  // of Prof. Vinayak Eswaran <eswar@iith.ac.in>

  if(pointRange.size() != 4 || pointDonorRange.size() != 4) {
    Msg::Error("Invalid point ranges to compute tranform - using default");
    transform[0] = 1;
    transform[1] = 2;
    return;
  }

  int a1 = pointDonorRange[2] - pointDonorRange[0];
  int a2 = pointDonorRange[3] - pointDonorRange[1];
  int b1 = pointRange[2] - pointRange[0];
  int b2 = pointRange[3] - pointRange[1];

  // In the interface face, if one index is varying, the other should remain
  // constant and hence one of a1, a2, b1, b2 should be zero (in 2D): (Index2 -
  // Begin2) = T.(Index1 - Begin1); (Index1 - Begin1) = Transpose[T].(Index2 -
  // Begin2)
  int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
  if(b2 == 0) {
    x1 = a1 / b1;
    y1 = a2 / b1;
    if(x1 == 0) {
      y2 = 0;
      if(y1 < 0) x2 = 1;
      else x2 = -1;
    }
    else if(y1 == 0) {
      x2 = 0;
      if(x1 > 0) y2 = 1;
      else y2 = -1;
    }
  }
  else if(b1 == 0) {
    x2 = a1 / b2;
    y2 = a2 / b2;
    if(x2 == 0) {
      y1 = 0;
      if(y2 > 0) x1 = 1;
      else x1 = -1;
    }
    else if(y2 == 0) {
      x1 = 0;
      if(x2 < 0) y1 = 1;
      else y1 = -1;
    }
  }
  else {
    Msg::Warning("Could not find transform matrix for zone - using default");
    transform[0] = 1;
    transform[1] = 2;
    return;
  }

  if(x1 == 1) transform[0] = 1;
  else if(x1 == -1) transform[0] = -1;
  if(x2 == 1) transform[1] = 1;
  else if(x2 == -1) transform[1] = -1;

  if(y1 == 1) transform[0] = 2;
  else if(y1 == -1) transform[0] = -2;
  if(y2 == 1) transform[1] = 2;
  else if(y2 == -1) transform[1] = -2;
}

static bool findRange2D(GFace *gf, GEdge *ge,
                        int &ibeg, int &jbeg, int &iend, int &jend)
{
  GVertex *gv1 = ge->getBeginVertex(), *gv2 = ge->getEndVertex();
  if(!gv1 || !gv1->getNumMeshVertices() || !gv2 || !gv2->getNumMeshVertices())
    return false;
  MVertex *v1 = gv1->getMeshVertex(0), *v2 = gv2->getMeshVertex(0);
  std::vector<std::vector<MVertex *> > &v = gf->transfinite_vertices;
  int imax = v.size(), jmax = v[0].size();
  ibeg = iend = jbeg = jend = -1;
  for(int i = 0; i < imax; i++) {
    for(int j = 0; j < jmax; j++) {
      if(i == 0 || j == 0 || i == imax - 1 || j == jmax - 1) {
        if(v[i][j] == v1) { ibeg = i + 1; jbeg = j + 1; }
        else if(v[i][j] == v2) { iend = i + 1; jend = j + 1; }
      }
    }
  }
  if(ibeg > 0 && iend > 0 && jbeg > 0 && jend > 0) return true;
  return false;
}

static int writeInterface2D(int cgIndexFile, int cgIndexBase, int cgIndexZone,
                            GFace *gf, GEdge *ge, GFace *gf2)
{
  std::vector<cgsize_t> pointRange, pointDonorRange;
  int ibeg, iend, jbeg, jend;
  if(findRange2D(gf, ge, ibeg, jbeg, iend, jend))
    pointRange = {ibeg, jbeg, iend, jend};
  if(findRange2D(gf2, ge, ibeg, jbeg, iend, jend))
    pointDonorRange = {ibeg, jbeg, iend, jend};
  if(pointRange.size() && pointDonorRange.size()) {
    int transform[2];
    computeTransform2D(pointRange, pointDonorRange, transform);
    int cgIndexConn;
    if(cg_1to1_write(cgIndexFile, cgIndexBase, cgIndexZone,
                     getInterfaceName(gf, gf2).c_str(),
                     getZoneName(gf2).c_str(), &pointRange[0],
                     &pointDonorRange[0], transform, &cgIndexConn)) {
      return cgnsError(__FILE__, __LINE__, cgIndexFile);
    }
  }
  else {
    Msg::Warning("Could not identify interface between surfaces %d and %d, "
                 "on curve %d", gf->tag(), gf2->tag(), ge->tag());
  }
  return 1;
}

static int writeBC2D(int cgIndexFile, int cgIndexBase, int cgIndexZone,
                     GFace *gf, GEdge *ge)
{
  int ibeg, iend, jbeg, jend;
  if(findRange2D(gf, ge, ibeg, jbeg, iend, jend)) {
    std::vector<cgsize_t> pointRange = {ibeg, jbeg, iend, jend};
    int cgIndexBoco = 0;
    if(cg_boco_write(cgIndexFile, cgIndexBase, cgIndexZone,
                     getZoneName(ge).c_str(), BCWallViscous, PointRange,
                     2, &pointRange[0], &cgIndexBoco)) {
      return cgnsError(__FILE__, __LINE__, cgIndexFile);
    }
  }
  else{
    Msg::Warning("Could not identify boundary condition on curve %d in "
                 "surface %d", ge->tag(), gf->tag());
  }
  return 1;
}

static int writeZonesStruct2D(int cgIndexFile, int cgIndexBase,
                              std::vector<GFace *> &faces,
                              double scalingFactor)
{
  for(auto gf : faces) {
    cgsize_t imax = gf->transfinite_vertices.size();
    cgsize_t jmax = gf->transfinite_vertices[0].size();

    // write zone
    int cgIndexZone = 0;
    cgsize_t cgZoneSize[6] = {imax, jmax, imax - 1, jmax - 1, 0, 0};
    std::string zoneName = cgnsString(getZoneName(gf));
    if(cg_zone_write(cgIndexFile, cgIndexBase, zoneName.c_str(), cgZoneSize,
                     Structured, &cgIndexZone) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write grid
    int cgIndexGrid = 0;
    if(cg_grid_write(cgIndexFile, cgIndexBase, cgIndexZone, "GridCoordinates",
                     &cgIndexGrid) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write x coordinates
    int cgIndexCoord = 0;
    std::vector<double> data(cgZoneSize[0] * cgZoneSize[1]);
    for(cgsize_t j = 0; j < jmax; j++) {
      for(cgsize_t i = 0; i < imax; i++) {
        MVertex *v = gf->transfinite_vertices[i][j];
        data[cgZoneSize[0] * j + i] = v->x() * scalingFactor;
      }
    }
    if(cg_coord_write(cgIndexFile, cgIndexBase, cgIndexZone, RealDouble,
                      "CoordinateX", &data[0], &cgIndexCoord) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write y coordinates
    for(cgsize_t j = 0; j < jmax; j++) {
      for(cgsize_t i = 0; i < imax; i++) {
        MVertex *v = gf->transfinite_vertices[i][j];
        data[cgZoneSize[0] * j + i] = v->y() * scalingFactor;
      }
    }
    if(cg_coord_write(cgIndexFile, cgIndexBase, cgIndexZone, RealDouble,
                      "CoordinateY", &data[0], &cgIndexCoord) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write z coordinates
    for(cgsize_t j = 0; j < jmax; j++) {
      for(cgsize_t i = 0; i < imax; i++) {
        MVertex *v = gf->transfinite_vertices[i][j];
        data[cgZoneSize[0] * j + i] = v->z() * scalingFactor;
      }
    }
    if(cg_coord_write(cgIndexFile, cgIndexBase, cgIndexZone, RealDouble,
                      "CoordinateZ", &data[0], &cgIndexCoord) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    for(auto ge: gf->edges()) {
      // write interface data
      for(auto gf2: ge->faces()) {
        if(gf2 != gf)
          writeInterface2D(cgIndexFile, cgIndexBase, cgIndexZone, gf, ge, gf2);
      }
      // write boundary conditions
      if(!ge->physicals.empty())
        writeBC2D(cgIndexFile, cgIndexBase, cgIndexZone, gf, ge);
    }
  }
  return 1;
}

static void computeTransform3D(const std::vector<cgsize_t> &pointRange,
                               const std::vector<cgsize_t> &pointDonorRange,
                               int transform[3])
{
  // This routine was written with the financial aid of of Indian Institute of
  // Technology Hyderabad - BRNS sponsored project in 2018, under the guidance
  // of Prof. Vinayak Eswaran <eswar@iith.ac.in>

  if(pointRange.size() != 6 || pointDonorRange.size() != 6) {
    Msg::Error("Invalid point ranges to compute tranform - using default");
    transform[0] = 1;
    transform[1] = 2;
    transform[2] = 3;
    return;
  }

  int a1 = pointDonorRange[3] - pointDonorRange[0];
  int a2 = pointDonorRange[4] - pointDonorRange[1];
  int a3 = pointDonorRange[5] - pointDonorRange[2];
  int b1 = pointRange[3] - pointRange[0];
  int b2 = pointRange[4] - pointRange[1];
  int b3 = pointRange[5] - pointRange[2];

  // In the interface Face, if one index is varying, other should remain
  // constant and hence one of a1, a2, b1, b2 should be zero (for 2D) (Index2 -
  // Begin2) = T.(Index1 - Begin1) (Index1 - Begin1) = Transpose[T].(Index2 -
  // Begin2)

  int x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0, x3 = 0, y3 = 0, z3 = 0;
  if((b1 == a1) && (b1 != 0)) { x1 = 1; }
  if((b1 == -a1) && (b1 != 0)) { x1 = -1; }
  if((b1 == a2) && (b1 != 0)) { x2 = 1; }
  if((b1 == -a2) && (b1 != 0)) { x2 = -1; }
  if((b1 == a3) && (b1 != 0)) { x3 = 1; }
  if((b1 == -a3) && (b1 != 0)) { x3 = -1; }

  if((b2 == a1) && (b2 != 0)) { y1 = 1; }
  if((b2 == -a1) && (b2 != 0)) { y1 = -1; }
  if((b2 == a2) && (b2 != 0)) { y2 = 1; }
  if((b2 == -a2) && (b2 != 0)) { y2 = -1; }
  if((b2 == a3) && (b2 != 0)) { y3 = 1; }
  if((b2 == -a3) && (b2 != 0)) { y3 = -1; }

  if((b3 == a1) && (b3 != 0)) { z1 = 1; }
  if((b3 == -a1) && (b3 != 0)) { z1 = -1; }
  if((b3 == a2) && (b3 != 0)) { z2 = 1; }
  if((b3 == -a2) && (b3 != 0)) { z2 = -1; }
  if((b3 == a3) && (b3 != 0)) { z3 = 1; }
  if((b3 == -a3) && (b3 != 0)) { z3 = -1; }

  if((b1 == a2) && (b1 == a3) && (a1 == 0)) {
    if(b2 == b1) {
      x2 = 0;
      x3 = 1;
      y2 = 1;
      y3 = 0;
    }
    if(b3 == b1) {
      x2 = 1;
      x3 = 0;
      z2 = 0;
      z3 = 1;
    }
  }
  if((b1 == a3) && (b1 == a1) && (a2 == 0)) {
    if(b2 == b1) {
      x1 = 1;
      x3 = 0;
      y1 = 0;
      y3 = 1;
    }
    if(b3 == b1) {
      x1 = 1;
      x3 = 0;
      z1 = 0;
      z3 = 1;
    }
  }
  if((b1 == a1) && (b1 == a2) && (a3 == 0)) {
    if(b2 == b1) {
      x1 = 1;
      x2 = 0;
      y1 = 0;
      y2 = 1;
    }
    if(b3 == b1) {
      x1 = 1;
      x2 = 0;
      z1 = 0;
      z2 = 1;
    }
  }
  if((b1 == -a2) && (b1 == -a3) && (a1 == 0)) {
    if(b2 == b1) {
      x2 = 0;
      x3 = -1;
      y2 = -1;
      y3 = 0;
    }
    if(b3 == b1) {
      x2 = -1;
      x3 = 0;
      z2 = 0;
      z3 = -1;
    }
  }
  if((b1 == -a3) && (b1 == -a1) && (a2 == 0)) {
    if(b2 == b1) {
      x1 = -1;
      x3 = 0;
      y1 = 0;
      y3 = -1;
    }
    if(b3 == b1) {
      x1 = -1;
      x3 = 0;
      z1 = 0;
      z3 = -1;
    }
  }
  if((b1 == -a1) && (b1 == -a2) && (a3 == 0)) {
    if(b2 == b1) {
      x1 = -1;
      x2 = 0;
      y1 = 0;
      y2 = -1;
    }
    if(b3 == b1) {
      x1 = -1;
      x2 = 0;
      z1 = 0;
      z2 = -1;
    }
  }

  if((b2 == a2) && (b2 == a3) && (a1 == 0)) {
    if(b1 == b2) {
      x2 = 1;
      x3 = 0;
      y2 = 0;
      y3 = 1;
    }
    if(b3 == b2) {
      y2 = 1;
      y3 = 0;
      z2 = 0;
      z3 = 1;
    }
  }
  if((b2 == a3) && (b2 == a1) && (a2 == 0)) {
    if(b1 == b2) {
      x1 = 1;
      x3 = 0;
      y1 = 0;
      y3 = 1;
    }
    if(b3 == b2) {
      y1 = 1;
      y3 = 0;
      z1 = 0;
      z3 = 1;
    }
  }
  if((b2 == a1) && (b2 == a2) && (a3 == 0)) {
    if(b1 == b2) {
      x1 = 1;
      x2 = 0;
      y1 = 0;
      y2 = 1;
    }
    if(b3 == b2) {
      y1 = 1;
      y2 = 0;
      z1 = 0;
      z2 = 1;
    }
  }
  if((b2 == -a2) && (b2 == -a3) && (a1 == 0)) {
    if(b1 == b2) {
      x2 = -1;
      x3 = 0;
      y2 = 0;
      y3 = -1;
    }
    if(b3 == b2) {
      y2 = -1;
      y3 = 0;
      z2 = 0;
      z3 = -1;
    }
  }
  if((b2 == -a3) && (b2 == -a1) && (a2 == 0)) {
    if(b1 == b2) {
      x1 = -1;
      x3 = 0;
      y1 = 0;
      y3 = -1;
    }
    if(b3 == b2) {
      y1 = -1;
      y3 = 0;
      z1 = 0;
      z3 = -1;
    }
  }
  if((b2 == -a1) && (b2 == -a2) && (a3 == 0)) {
    if(b1 == b2) {
      x1 = -1;
      x2 = 0;
      y1 = 0;
      y2 = -1;
    }
    if(b3 == b2) {
      y1 = -1;
      y2 = 0;
      z1 = 0;
      z2 = -1;
    }
  }

  if((b3 == a2) && (b3 == a3) && (a1 == 0)) {
    if(b1 == b3) {
      x2 = 1;
      x3 = 0;
      z2 = 0;
      z3 = 1;
    }
    if(b2 == b3) {
      y2 = 1;
      y3 = 0;
      z2 = 0;
      z3 = 1;
    }
  }
  if((b3 == a3) && (b3 == a1) && (a2 == 0)) {
    if(b1 == b3) {
      x1 = 1;
      x3 = 0;
      z1 = 0;
      z3 = 1;
    }
    if(b2 == b3) {
      y1 = 1;
      y3 = 0;
      z1 = 0;
      z3 = 1;
    }
  }
  if((b3 == a1) && (b3 == a2) && (a3 == 0)) {
    if(b1 == b3) {
      x1 = 1;
      x2 = 0;
      z1 = 0;
      z2 = 1;
    }
    if(b2 == b3) {
      y1 = 1;
      y2 = 0;
      z1 = 0;
      z2 = 1;
    }
  }

  if((b1 == 0) && (a1 == 0)) {
    x1 = 1;
    if(((pointRange[0] == 1) && (pointDonorRange[0] == 1)) ||
       ((pointRange[0] != 1) && (pointDonorRange[0] != 1)))
      x1 = -1;
  }
  if((b1 == 0) && (a2 == 0)) {
    x2 = 1;
    if(((pointRange[0] == 1) && (pointDonorRange[1] == 1)) ||
       ((pointRange[0] != 1) && (pointDonorRange[1] != 1)))
      x2 = -1;
  }
  if((b1 == 0) && (a3 == 0)) {
    x3 = 1;
    if(((pointRange[0] == 1) && (pointDonorRange[2] == 1)) ||
       ((pointRange[0] != 1) && (pointDonorRange[2] != 1)))
      x3 = -1;
  }
  if((b2 == 0) && (a1 == 0)) {
    y1 = 1;
    if(((pointRange[1] == 1) && (pointDonorRange[0] == 1)) ||
       ((pointRange[1] != 1) && (pointDonorRange[0] != 1)))
      y1 = -1;
  }
  if((b2 == 0) && (a2 == 0)) {
    y2 = 1;
    if(((pointRange[1] == 1) && (pointDonorRange[1] == 1)) ||
       ((pointRange[1] != 1) && (pointDonorRange[1] != 1)))
      y2 = -1;
  }
  if((b2 == 0) && (a3 == 0)) {
    y3 = 1;
    if(((pointRange[1] == 1) && (pointDonorRange[2] == 1)) ||
       ((pointRange[1] != 1) && (pointDonorRange[2] != 1)))
      y3 = -1;
  }
  if((b3 == 0) && (a1 == 0)) {
    z1 = 1;
    if(((pointRange[2] == 1) && (pointDonorRange[0] == 1)) ||
       ((pointRange[2] != 1) && (pointDonorRange[0] != 1)))
      z1 = -1;
  }
  if((b3 == 0) && (a2 == 0)) {
    z2 = 1;
    if(((pointRange[2] == 1) && (pointDonorRange[1] == 1)) ||
       ((pointRange[2] != 1) && (pointDonorRange[1] != 1)))
      z2 = -1;
  }
  if((b3 == 0) && (a3 == 0)) {
    z3 = 1;
    if(((pointRange[2] == 1) && (pointDonorRange[2] == 1)) ||
       ((pointRange[2] != 1) && (pointDonorRange[2] != 1)))
      z3 = -1;
  }

  int det_A = x1 * y2 * z3 + x2 * y3 * z1 + x3 * y1 * z2 -
    x1 * y3 * z2 - x2 * y1 * z3 - x3 * y2 * z1;

  if(!((det_A == 1) || (det_A == -1))) {
    Msg::Warning("Could not find transformation matrix (determinant = %i "
                 "!= +/-1) - using default", det_A);
    transform[0] = 1;
    transform[1] = 2;
    transform[1] = 3;
    return;
  }

  if((x1 == 1) && (x2 == 0) && (x3 == 0)) {
    transform[0] = 1;
  }
  if((x1 == -1) && (x2 == 0) && (x3 == 0)) {
    transform[0] = -1;
  }
  if((x1 == 0) && (x2 == 1) && (x3 == 0)) {
    transform[0] = 2;
  }
  if((x1 == 0) && (x2 == -1) && (x3 == 0)) {
    transform[0] = -2;
  }
  if((x1 == 0) && (x2 == 0) && (x3 == 1)) {
    transform[0] = 3;
  }
  if((x1 == 0) && (x2 == 0) && (x3 == -1)) {
    transform[0] = -3;
  }

  if((y1 == 1) && (y2 == 0) && (y3 == 0)) {
    transform[1] = 1;
  }
  if((y1 == -1) && (y2 == 0) && (y3 == 0)) {
    transform[1] = -1;
  }
  if((y1 == 0) && (y2 == 1) && (y3 == 0)) {
    transform[1] = 2;
  }
  if((y1 == 0) && (y2 == -1) && (y3 == 0)) {
    transform[1] = -2;
  }
  if((y1 == 0) && (y2 == 0) && (y3 == 1)) {
    transform[1] = 3;
  }
  if((y1 == 0) && (y2 == 0) && (y3 == -1)) {
    transform[1] = -3;
  }

  if((z1 == 1) && (z2 == 0) && (z3 == 0)) {
    transform[2] = 1;
  }
  if((z1 == -1) && (z2 == 0) && (z3 == 0)) {
    transform[2] = -1;
  }
  if((z1 == 0) && (z2 == 1) && (z3 == 0)) {
    transform[2] = 2;
  }
  if((z1 == 0) && (z2 == -1) && (z3 == 0)) {
    transform[2] = -2;
  }
  if((z1 == 0) && (z2 == 0) && (z3 == 1)) {
    transform[2] = 3;
  }
  if((z1 == 0) && (z2 == 0) && (z3 == -1)) {
    transform[2] = -3;
  }
}

static bool findRange3D(GRegion *gr, GFace *gf,
                        int &ibeg, int &jbeg, int &kbeg,
                        int &iend, int &jend, int &kend)
{
  if(gf->transfinite_vertices.empty() || gf->transfinite_vertices[0].empty())
    return false;
  MVertex *v1 = gf->transfinite_vertices.front().front();
  MVertex *v2 = gf->transfinite_vertices.back().back();

  std::vector<std::vector<std::vector<MVertex *> > > &v = gr->transfinite_vertices;
  int imax = v.size(), jmax = v[0].size(), kmax = v[0][0].size();
  ibeg = iend = jbeg = jend = kbeg = kend = -1;
  for(int i = 0; i < imax; i++) {
    for(int j = 0; j < jmax; j++) {
      for(int k = 0; k < kmax; k++) {
        if(i == 0 || j == 0 || k == 0 ||
           i == imax - 1 || j == jmax - 1 || k == kmax - 1) {
          if(v[i][j][k] == v1) { ibeg = i + 1; jbeg = j + 1; kbeg = k + 1; }
          else if(v[i][j][k] == v2) { iend = i + 1; jend = j + 1; kend = k + 1; }
        }
      }
    }
  }
  if(ibeg > 0 && iend > 0 && jbeg > 0 && jend > 0 && kbeg > 0 && kend > 0)
    return true;
  return false;
}

static int writeInterface3D(int cgIndexFile, int cgIndexBase, int cgIndexZone,
                            GRegion *gr, GFace *gf, GRegion *gr2)
{
  std::vector<cgsize_t> pointRange, pointDonorRange;
  int ibeg, iend, jbeg, jend, kbeg, kend;
  if(findRange3D(gr, gf, ibeg, jbeg, kbeg, iend, jend, kend))
    pointRange = {ibeg, jbeg, kbeg, iend, jend, kend};
  if(findRange3D(gr2, gf, ibeg, jbeg, kbeg, iend, jend, kend))
    pointDonorRange = {ibeg, jbeg, kbeg, iend, jend, kend};
  if(pointRange.size() && pointDonorRange.size()) {
    int transform[3];
    computeTransform3D(pointRange, pointDonorRange, transform);
    int cgIndexConn;
    if(cg_1to1_write(cgIndexFile, cgIndexBase, cgIndexZone,
                     getInterfaceName(gr, gr2).c_str(),
                     getZoneName(gr2).c_str(), &pointRange[0],
                     &pointDonorRange[0], transform, &cgIndexConn)) {
      return cgnsError(__FILE__, __LINE__, cgIndexFile);
    }
  }
  else {
    Msg::Warning("Could not identify interface between volumes %d and %d, "
                 "on surface %d", gr->tag(), gr2->tag(), gf->tag());
  }
  return 1;
}

static int writeBC3D(int cgIndexFile, int cgIndexBase, int cgIndexZone,
                     GRegion *gr, GFace *gf)
{
  int ibeg, iend, jbeg, jend, kbeg, kend;
  if(findRange3D(gr, gf, ibeg, jbeg, kbeg, iend, jend, kend)) {
    std::vector<cgsize_t> pointRange = {ibeg, jbeg, kbeg, iend, jend, kend};
    int cgIndexBoco = 0;
    if(cg_boco_write(cgIndexFile, cgIndexBase, cgIndexZone,
                     getZoneName(gf).c_str(), BCWallViscous, PointRange,
                     3, &pointRange[0], &cgIndexBoco)) {
      return cgnsError(__FILE__, __LINE__, cgIndexFile);
    }
  }
  else{
    Msg::Warning("Could not identify boundary condition on surface %d in "
                 "volume %d", gf->tag(), gr->tag());
  }
  return 1;
}

static int writeZonesStruct3D(int cgIndexFile, int cgIndexBase,
                              std::vector<GRegion *> &regions,
                              double scalingFactor)
{
  for(auto gr : regions) {
    cgsize_t imax = gr->transfinite_vertices.size();
    cgsize_t jmax = gr->transfinite_vertices[0].size();
    cgsize_t kmax = gr->transfinite_vertices[0][0].size();

    // write zone
    int cgIndexZone = 0;
    cgsize_t cgZoneSize[9] = {imax, jmax, kmax, imax - 1, jmax - 1, kmax - 1,
                              0, 0, 0};
    std::string zoneName = cgnsString("Volume_" + std::to_string(gr->tag()));
    if(cg_zone_write(cgIndexFile, cgIndexBase, zoneName.c_str(), cgZoneSize,
                     Structured, &cgIndexZone) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write grid
    int cgIndexGrid = 0;
    if(cg_grid_write(cgIndexFile, cgIndexBase, cgIndexZone, "GridCoordinates",
                     &cgIndexGrid) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write x coordinates
    int cgIndexCoord = 0;
    std::vector<double> data(imax * jmax * kmax);
    for(cgsize_t k = 0; k < kmax; k++) {
      for(cgsize_t j = 0; j < jmax; j++) {
        for(cgsize_t i = 0; i < imax; i++) {
          MVertex *v = gr->transfinite_vertices[i][j][k];
          data[imax * jmax * k + imax * j + i] = v->x() * scalingFactor;
        }
      }
    }
    if(cg_coord_write(cgIndexFile, cgIndexBase, cgIndexZone, RealDouble,
                      "CoordinateX", &data[0], &cgIndexCoord) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write y coordinates
    for(cgsize_t k = 0; k < kmax; k++) {
      for(cgsize_t j = 0; j < jmax; j++) {
        for(cgsize_t i = 0; i < imax; i++) {
          MVertex *v = gr->transfinite_vertices[i][j][k];
          data[imax * jmax * k + imax * j + i] = v->y() * scalingFactor;
        }
      }
    }
    if(cg_coord_write(cgIndexFile, cgIndexBase, cgIndexZone, RealDouble,
                      "CoordinateY", &data[0], &cgIndexCoord) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    // write z coordinates
    for(cgsize_t k = 0; k < kmax; k++) {
      for(cgsize_t j = 0; j < jmax; j++) {
        for(cgsize_t i = 0; i < imax; i++) {
          MVertex *v = gr->transfinite_vertices[i][j][k];
          data[imax * jmax * k + imax * j + i] = v->z() * scalingFactor;
        }
      }
    }
    if(cg_coord_write(cgIndexFile, cgIndexBase, cgIndexZone, RealDouble,
                      "CoordinateZ", &data[0], &cgIndexCoord) != CG_OK)
      return cgnsError(__FILE__, __LINE__, cgIndexFile);

    for(auto gf: gr->faces()) {
      // write interface data
      for(auto gr2: gf->regions()) {
        if(gr2 != gr)
          writeInterface3D(cgIndexFile, cgIndexBase, cgIndexZone, gr, gf, gr2);
      }
      // write boundary conditions
      if(!gf->physicals.empty())
        writeBC3D(cgIndexFile, cgIndexBase, cgIndexZone, gr, gf);
    }

  }
  return 1;
}

int writeZonesStruct(GModel *model, double scalingFactor, int cgIndexFile,
                     int cgIndexBase)
{
  int meshDim = -1;

  std::vector<GFace *> faces;
  for(GModel::fiter it = model->firstFace(); it != model->lastFace(); ++it) {
    if((*it)->transfinite_vertices.size() &&
       (*it)->transfinite_vertices[0].size()) {
      meshDim = 2;
      faces.push_back(*it);
    }
  }

  std::vector<GRegion *> regions;
  for(GModel::riter it = model->firstRegion(); it != model->lastRegion();
      ++it) {
    if((*it)->transfinite_vertices.size() &&
       (*it)->transfinite_vertices[0].size() &&
       (*it)->transfinite_vertices[0][0].size()) {
      meshDim = 3;
      regions.push_back(*it);
    }
  }

  if(meshDim < 0 || (faces.empty() && regions.empty())) {
    Msg::Warning("No structured meshes to save");
    return 0;
  }

  if(meshDim == 2) {
    if(writeZonesStruct2D(cgIndexFile, cgIndexBase, faces, scalingFactor))
      return 1;
  }
  else if(meshDim == 3) {
    if(writeZonesStruct3D(cgIndexFile, cgIndexBase, regions, scalingFactor))
      return 1;
  }

  return 0;
}

#endif // HAVE_LIBCGNS
