import numpy as np
import gmsh

# number of points to trianguate
N = 100

# visualize the mesh?
visu = True

gmsh.initialize()

points = np.random.standard_normal(2 * N)
tris = gmsh.model.mesh.triangulate(points)

if visu:
    surf = gmsh.model.addDiscreteEntity(2)
    p3 = np.hstack((np.reshape(points, (-1,2)), np.zeros((N,1)))).flatten()
    gmsh.model.mesh.addNodes(2, surf, range(1, N+1), p3)
    gmsh.model.mesh.addElementsByType(surf, 2, [], tris)
    gmsh.fltk.run()

gmsh.finalize()
