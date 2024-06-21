import bpy
import sys
import math
import os
import bmesh
from mathutils import Matrix

C = bpy.context

# enter object mode
if bpy.context.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

#Deselect all
bpy.ops.object.select_all(action='DESELECT')

argv = sys.argv
try:
    argv = argv[argv.index("--") + 1:]  # get all args after "--"
except ValueError:
    # test value
    argv = [ "M:/VS/wrench/bin/RelWithDebInfo/games/uya_scus_973_53/moby_classes/unsorted/6889/mesh.dae" ]

export_filepath = argv[0] if argv is not None and len(argv) > 0 else None
additional_imports = argv[1:]
area_threshold = 32*32
len_threshold = 32

print(export_filepath)

# imports
if additional_imports is not None and len(additional_imports) > 0:
    for additional_import in additional_imports:
        print(additional_import)
        ext = os.path.splitext(additional_import)[1]

        # import file
        if ext == ".dae":
            bpy.ops.wm.collada_import(filepath = additional_import, 
                                  auto_connect = False, 
                                  find_chains = False, 
                                  fix_orientation = False) 
            
        elif ext == ".blend":
            bpy.ops.wm.open_mainfile(filepath = additional_import)
            
        elif ext == ".glb" or ext == ".gltf":
            bpy.ops.import_scene.gltf(filepath = additional_import)

idx = 0
all_objects = [x for x in C.scene.objects]
for obj in all_objects:
    obj.name = str(idx)
    idx += 1

# create root
emptyMesh = bpy.data.meshes.new('emptyMesh')
root = bpy.data.objects.new("collision", emptyMesh)
root.location = (0,0,0)
C.collection.objects.link(root)
C.view_layer.objects.active = root
root.select_set(state=True)

# recurse hierarchy and find objs with negative scale
# mark them for a normal flip
objs_flip = {}
def recurse_find_flipped_objs(ob, levels=10):
    def recurse(ob, scale, parent, depth):
        if depth > levels: 
            return
        
        if (scale.x*scale.y*scale.z) < 0:
            objs_flip[ob] = True
        
        for child in ob.children:
            recurse(child, scale * child.scale, ob,  depth + 1)
            
    scale = ob.scale
    recurse(ob, scale, ob.parent, 0)

for ob in all_objects:
    if ob.parent is None:
        recurse_find_flipped_objs(ob, levels=100)

for ob in all_objects:
    # flip normal if product of object scale is negative
    normal_flip = False
    if ob in objs_flip:
        normal_flip = objs_flip[ob]

    if ob.type == 'MESH':
        copy = ob.copy()
        copy.data = ob.data.copy()
        C.collection.objects.link(copy)
        
        if normal_flip:
            for p in copy.data.polygons:
                p.flip()
        
        copy.select_set(state=True)
        ob.select_set(state=False)
    else:
        ob.select_set(state=True)

# merge into single mesh
C.view_layer.objects.active = root
bpy.ops.object.join()
bpy.ops.object.select_all(action='DESELECT')
C.view_layer.objects.active = root
root.select_set(state=True)

# subdivide as necessary
if True:
    bm = bmesh.new()
    bm.from_mesh(root.data)
    bm.edges.ensure_lookup_table()
    
    # subdivide large faces until none left
    while True:
        faces = bm.faces
        edges = []
        for e in range(0, len(bm.edges)):
            edge = bm.edges[e]
            edge_len = edge.calc_length()
            if edge_len > len_threshold:
                if not edge in edges:
                    edges.append(edge)
                    
        # for f in range(0, len(bm.faces)):
        #     face = faces[f]
        #     area = face.calc_area()
        #     if area < area_threshold:
        #         continue
            
        #     for e in range(0, len(face.edges)):
        #         edge = face.edges[e]
        #         if not edge in edges:
        #             edges.append(edge)
                    
        if len(edges) == 0:
            break
        
        # subdivide
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=1, use_grid_fill=True)
        bm.edges.ensure_lookup_table()
        
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bm.to_mesh(root.data)
    

# merge and rename materials to expected collision materials
idx = 1
mats = bpy.data.materials[:]
for mat in mats:
    if mat.name.startswith('col_'):
        expected_name = mat.name[:]
        if mat.name[-3:].isnumeric():
            expected_name = mat.name[:-4]
        mat.name = expected_name + '.' + str(idx).zfill(3)
        idx += 1

mat_list = [x.material.name for x in root.material_slots]
remove_slots = []
for s in root.material_slots:
    if s.material.name[-3:].isnumeric() and s.material.name.startswith('col_'):
        expected_name = s.material.name[:-4]

        # the last 3 characters are numbers
        # that indicates it might be a duplicate of another material
        # but this is pure guesswork, so expect errors to happen!
        if expected_name in mat_list:

            # there is a material without the numeric extension so use it
            # this again is just guessing that we're having identical node trees here

            # get the material index of the 'clean' material
            index_clean = mat_list.index(expected_name)
            index_wrong = mat_list.index(s.material.name)

            # get the faces which are assigned to the 'wrong' material
            faces = [x for x in root.data.polygons if x.material_index == index_wrong]

            for f in faces:
                f.material_index = index_clean

            remove_slots.append(s.name)
        else:
            index = mat_list.index(s.material.name)

            print(f'renaming {s.material.name} => {expected_name}')
            s.material.name = expected_name
            mat_list[index] = expected_name
            print(f'renamed {s.material.name}')

# now remove all empty material slots:
for s in remove_slots:
    if s in [x.name for x in root.material_slots]:
        print('removing slot %s' % s)
        root.active_material_index = [x.material.name for x in root.material_slots].index(s)
        bpy.ops.object.material_slot_remove()

if export_filepath:
    bpy.ops.wm.collada_export(filepath=export_filepath, check_existing=False, selected=True, triangulate=False)

#bpy.ops.wm.save_as_mainfile(filepath='C:/Users/dna11/OneDrive/Desktop/test.blend')

bpy.data.objects.remove(root)

# success
exit(1)
