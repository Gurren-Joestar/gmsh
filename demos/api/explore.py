import gmsh
import sys

if len(sys.argv) < 2:
    print "Usage: " + sys.argv[0] + " file.msh [options]"
    exit(0)

gmsh.initialize()
gmsh.option.setNumber("General.Terminal", 1)
gmsh.open(sys.argv[1])

# get all elementary entities in the model
entities = gmsh.model.getEntities()

for e in entities:
    # get the mesh vertices for each elementary entity
    vertexTags, vertexCoords, vertexParams = gmsh.model.mesh.getVertices(e[0], e[1])
    # get the mesh elements for each elementary entity
    elemTypes, elemTags, elemVertexTags = gmsh.model.mesh.getElements(e[0], e[1])
    # report some statistics
    numElem = sum(len(i) for i in elemTags)
    print str(len(vertexTags)) + " mesh vertices and " + str(numElem),\
          "mesh elements on entity " + str(e)
    for t in elemTypes:
        name, dim, order, numv, parv = gmsh.model.mesh.getElementProperties(t)
        print " - Element type: " + name + ", order " + str(order)
        print "   with " + str(numv) + " vertices in param coord: ", parv

# all mesh vertex coordinates
vertexTags, vertexCoords, _ = gmsh.model.mesh.getVertices()
x = dict(zip(vertexTags, vertexCoords[0::3]))
y = dict(zip(vertexTags, vertexCoords[1::3]))
z = dict(zip(vertexTags, vertexCoords[2::3]))

gmsh.finalize()
