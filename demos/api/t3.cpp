#include <cmath>
#include <gmsh.h>

// this reimplements gmsh/tutorial/t3.geo

int main(int argc, char **argv)
{
  gmshInitialize(argc, argv);
  gmshOptionSetNumber("General.Terminal", 1);

  gmshModelCreate("t3");

  // copy/paste from t1.cpp
  double lc = 1e-2;
  int o;
  gmshModelGeoAddPoint(1, 0, 0, 0, o, lc);
  gmshModelGeoAddPoint(2, .1, 0,  0, o, lc);
  gmshModelGeoAddPoint(3, .1, .3, 0, o, lc);
  gmshModelGeoAddPoint(4, 0,  .3, 0, o, lc);

  gmshModelGeoAddLine(1, 1, 2, o);
  gmshModelGeoAddLine(2, 3, 2, o);
  gmshModelGeoAddLine(3, 3, 4, o);
  gmshModelGeoAddLine(4, 4, 1, o);

  gmshModelGeoAddLineLoop(1, {4, 1, -2, 3}, o);
  gmshModelGeoAddPlaneSurface(1, {1}, o);
  gmshModelAddPhysicalGroup(0, 1, {1, 2});
  gmshModelAddPhysicalGroup(1, 2, {1, 2});
  gmshModelAddPhysicalGroup(2, 6, {1});
  gmshModelSetPhysicalName(2, 6, "My surface");
  // end copy/paste

  double h = 0.1, angle = 90.;

  std::vector<std::pair<int, int> > ov;
  gmshModelGeoExtrude({{2,1}}, 0, 0, h, ov, {8,2}, {0.5,1});
  gmshModelGeoRevolve({{2,28}}, -0.1,0,0.1, 0,1,0, -M_PI/2, ov, {7});
  gmshModelGeoTwist({{2,50}}, 0,0.15,0.25, -2*h,0,0, 1,0,0, angle*M_PI/180.,
                    ov, {10}, {}, true);
  gmshModelAddPhysicalGroup(3, 101, {1, 2, ov[1].second});

  gmshModelGeoSynchronize();
  gmshModelMesh(3);
  gmshExport("t3.msh");
  gmshFinalize();

  return 0;
}
