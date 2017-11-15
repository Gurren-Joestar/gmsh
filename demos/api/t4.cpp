#include <math.h>
#include <gmsh.h>

// this reimplements gmsh/tutorial/t4.geo

double hypoth(double a, double b){ return sqrt(a * a + b * b); }

int main(int argc, char **argv)
{
  gmshInitialize(argc, argv);
  gmshOptionSetNumber("General.Terminal", 1);

  gmshModelCreate("t4");

  double cm = 1e-02;
  double e1 = 4.5 * cm, e2 = 6 * cm / 2, e3 =  5 * cm / 2;
  double h1 = 5 * cm, h2 = 10 * cm, h3 = 5 * cm, h4 = 2 * cm, h5 = 4.5 * cm;
  double R1 = 1 * cm, R2 = 1.5 * cm, r = 1 * cm;
  double Lc1 = 0.01;
  double Lc2 = 0.003;

  double ccos = (-h5*R1 + e2 * hypot(h5, hypot(e2, R1))) / (h5*h5 + e2*e2);
  double ssin = sqrt(1 - ccos*ccos);

  int o;
  gmshModelGeoAddPoint(1, -e1-e2, 0    , 0, o, Lc1);
  gmshModelGeoAddPoint(2, -e1-e2, h1   , 0, o, Lc1);
  gmshModelGeoAddPoint(3, -e3-r , h1   , 0, o, Lc2);
  gmshModelGeoAddPoint(4, -e3-r , h1+r , 0, o, Lc2);
  gmshModelGeoAddPoint(5, -e3   , h1+r , 0, o, Lc2);
  gmshModelGeoAddPoint(6, -e3   , h1+h2, 0, o, Lc1);
  gmshModelGeoAddPoint(7,  e3   , h1+h2, 0, o, Lc1);
  gmshModelGeoAddPoint(8,  e3   , h1+r , 0, o, Lc2);
  gmshModelGeoAddPoint(9,  e3+r , h1+r , 0, o, Lc2);
  gmshModelGeoAddPoint(10, e3+r , h1   , 0, o, Lc2);
  gmshModelGeoAddPoint(11, e1+e2, h1   , 0, o, Lc1);
  gmshModelGeoAddPoint(12, e1+e2, 0    , 0, o, Lc1);
  gmshModelGeoAddPoint(13, e2   , 0    , 0, o, Lc1);

  gmshModelGeoAddPoint(14,  R1 / ssin, h5+R1*ccos, 0, o, Lc2);
  gmshModelGeoAddPoint(15,  0        , h5        , 0, o, Lc2);
  gmshModelGeoAddPoint(16, -R1 / ssin, h5+R1*ccos, 0, o, Lc2);
  gmshModelGeoAddPoint(17, -e2       , 0.0       , 0, o, Lc1);

  gmshModelGeoAddPoint(18, -R2 , h1+h3   , 0, o, Lc2);
  gmshModelGeoAddPoint(19, -R2 , h1+h3+h4, 0, o, Lc2);
  gmshModelGeoAddPoint(20,  0  , h1+h3+h4, 0, o, Lc2);
  gmshModelGeoAddPoint(21,  R2 , h1+h3+h4, 0, o, Lc2);
  gmshModelGeoAddPoint(22,  R2 , h1+h3   , 0, o, Lc2);
  gmshModelGeoAddPoint(23,  0  , h1+h3   , 0, o, Lc2);

  gmshModelGeoAddPoint(24,  0, h1+h3+h4+R2, 0, o, Lc2);
  gmshModelGeoAddPoint(25,  0, h1+h3-R2,    0, o, Lc2);

  gmshModelGeoAddLine(1, 1 , 17, o);
  gmshModelGeoAddLine(2, 17, 16, o);

  gmshModelGeoAddCircleArc(3, 14,15,16, o);
  gmshModelGeoAddLine(4, 14,13, o);
  gmshModelGeoAddLine(5, 13,12, o);
  gmshModelGeoAddLine(6, 12,11, o);
  gmshModelGeoAddLine(7, 11,10, o);
  gmshModelGeoAddCircleArc(8, 8,9,10, o);
  gmshModelGeoAddLine(9, 8,7, o);
  gmshModelGeoAddLine(10, 7,6, o);
  gmshModelGeoAddLine(11, 6,5, o);
  gmshModelGeoAddCircleArc(12, 3,4,5, o);
  gmshModelGeoAddLine(13, 3,2, o);
  gmshModelGeoAddLine(14, 2,1, o);
  gmshModelGeoAddLine(15, 18,19, o);
  gmshModelGeoAddCircleArc(16, 21,20,24, o);
  gmshModelGeoAddCircleArc(17, 24,20,19, o);
  gmshModelGeoAddCircleArc(18, 18,23,25, o);
  gmshModelGeoAddCircleArc(19, 25,23,22, o);
  gmshModelGeoAddLine(20, 21,22, o);

  gmshModelGeoAddLineLoop(21, {17,-15,18,19,-20,16}, o);
  gmshModelGeoAddPlaneSurface(22, {21}, o);
  gmshModelGeoAddLineLoop(23, {11,-12,13,14,1,2,-3,4,5,6,7,-8,9,10}, o);
  gmshModelGeoAddPlaneSurface(24, {23,21}, o);

  // FIXME: this will be implemented through the gmshPost or gmshView API
  /*
  View "comments" {
    T2(10, -10, 0){ StrCat("Created on ", Today, " with Gmsh") };
    T3(0, 0.11, 0, TextAttributes("Align", "Center", "Font", "Helvetica")){ "Hole" };
    T3(0, 0.09, 0, TextAttributes("Align", "Center")){ "file://image.png@0.01x0" };
    T3(-0.01, 0.09, 0, 0){ "file://image.png@0.01x0,0,0,1,0,1,0" };
    T3(0, 0.12, 0, TextAttributes("Align", "Center")){ "file://image.png@0.01x0#" };
    T2(350, -7, 0){ "file://image.png@20x0" };
  };
  */
  gmshModelGeoSynchronize();

  gmshModelMesh(2);

  gmshExport("t4.msh");

  gmshFinalize();
  return 0;
}
