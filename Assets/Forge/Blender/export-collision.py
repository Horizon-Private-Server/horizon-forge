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

# remove loose vertices
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.delete_loose(use_verts=True, use_edges=True, use_faces=False)
bpy.ops.object.editmode_toggle()

# remove duplicate vertices
#bpy.ops.object.mode_set(mode='EDIT')
#bpy.ops.mesh.select_all(action='SELECT')
#bpy.ops.mesh.remove_doubles(threshold = 0.001)
#bpy.ops.mesh.normals_tools(mode='RESET')
#bpy.ops.mesh.normals_make_consistent(inside=False)
#bpy.ops.object.editmode_toggle()

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
        
    # Select n-gons (faces with more than 4 verts)
    ngon_faces = [f for f in bm.faces if len(f.verts) > 4]
    tri_result = bmesh.ops.triangulate(bm, faces=ngon_faces, quad_method='BEAUTY', ngon_method='BEAUTY')
    new_tris = [f for f in tri_result.get('faces', []) if f.is_valid]
    bmesh.ops.join_triangles(bm, faces=new_tris, angle_face_threshold=0.01, angle_shape_threshold=0.785)
    #bmesh.update_edit_mesh(obj.data)
    bm.to_mesh(root.data)
    

# merge and rename materials to expected collision materials
idx = 1
mats = bpy.data.materials[:]
for mat in mats:
    if mat.name.startswith('col_'):
        parts = mat.name.split('.')
        lastpart = parts[len(parts)-1]
        expected_name = mat.name[:]
        if lastpart.isnumeric():
            expected_name = mat.name[:-(len(lastpart)+1)]
        mat.name = expected_name + '.' + str(idx).zfill(5)
        idx += 1

mat_list = [x.material.name for x in root.material_slots]
remove_slots = []
for s in root.material_slots:
    parts = s.material.name.split('.')
    lastpart = parts[len(parts)-1]
    if lastpart.isnumeric() and s.material.name.startswith('col_'):
        expected_name = s.material.name[:-(len(lastpart)+1)]

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

# convert tris to quads
# not used anymore because the subdivider uses quads
#bpy.ops.object.mode_set(mode='EDIT')
#bpy.ops.mesh.select_all(action='SELECT')
#bpy.ops.mesh.tris_convert_to_quads()
#bpy.ops.object.mode_set(mode='OBJECT')

# split col_100 into its own mesh 'hero_group_collision'
obj = bpy.context.active_object
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='DESELECT')
bpy.ops.object.mode_set(mode='OBJECT')

# Select faces with target material
has_selection = False
for p in obj.data.polygons:
    if obj.data.materials[p.material_index].name == "col_100":
        p.select = True
        has_selection = True

if has_selection:
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.separate(type='SELECTED')
    bpy.ops.object.mode_set(mode='OBJECT')

def split_by_distance(obj, idx):
    if not obj or obj.type != 'MESH':
        raise Exception("Select a mesh object.")

    # Ensure we're in object mode
    bpy.ops.object.mode_set(mode='OBJECT')

    # Create a BMesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.verts.ensure_lookup_table()
    bm.faces.ensure_lookup_table()

    # Transform to world space so we compare properly
    world_matrix = obj.matrix_world

    # grab first vertex as sphere center
    sphere_center = world_matrix @ bm.faces[0].verts[0].co
    sphere_radius = 8.0

    # Determine which vertices are inside the sphere
    verts_in_sphere = set()
    for v in bm.verts:
        world_pos = world_matrix @ v.co
        if (world_pos - sphere_center).length <= sphere_radius:
            verts_in_sphere.add(v)

    # Find all faces that have all their vertices inside the sphere
    print(f"found {len(verts_in_sphere)} verts in sphere")
    faces_in_sphere = [f for f in bm.faces if any(v in verts_in_sphere for v in f.verts)]

    if not faces_in_sphere:
        raise Exception("No faces found within sphere region.")

    # Create a new bmesh for extracted region
    bm_new = bmesh.new()
    vert_map = {}

    for f in faces_in_sphere:
        new_verts = []
        for v in f.verts:
            if v not in vert_map:
                vert_map[v] = bm_new.verts.new(v.co)
            new_verts.append(vert_map[v])
        try:
            bm_new.faces.new(new_verts)
        except ValueError:
            # face might already exist
            pass

    # Output to new mesh and object
    bm_new.normal_update()
    new_mesh = bpy.data.meshes.new(f"{obj.name}_{idx}")
    bm_new.to_mesh(new_mesh)
    bm_new.free()

    new_obj = bpy.data.objects.new(new_mesh.name, new_mesh)
    bpy.context.collection.objects.link(new_obj)
    new_obj.matrix_world = obj.matrix_world
    new_obj.data.materials.clear()
    new_obj.data.materials.append(obj.data.materials.get("col_100"))
    new_obj.select_set(True)

    # --- Remove those faces from the original ---
    for f in faces_in_sphere:
        bm.faces.remove(f)

    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()

    print(f"✅ Created new object '{new_obj.name}' containing {len(faces_in_sphere)} faces inside sphere.")

# rename hero mesh
hero_group_name = "hero_group_collision"
hero_group = bpy.data.objects.get("collision.001")
if hero_group:
    hero_group.name = hero_group_name
    hero_group.data.name = hero_group_name

    # split by distance
    idx = 0
    while len(hero_group.data.polygons) > 0:
        split_by_distance(hero_group, idx)
        idx += 1

    # remove empty
    bpy.data.objects.remove(hero_group, do_unlink=True)

    # rename all 
    #for obj in bpy.data.objects:
    #    if obj.name.startswith(hero_group_name):
    #        obj.name = hero_group_name
    #        obj.data.name = hero_group_name

# make hero group triangles
#C.view_layer.objects.active = hero_group
#bpy.ops.object.mode_set(mode='EDIT')
#bpy.ops.mesh.select_all(action='SELECT')
#bpy.ops.mesh.quads_convert_to_tris()
#bpy.ops.object.mode_set(mode='OBJECT')

# select all
#bpy.ops.object.mode_set(mode='OBJECT')
#bpy.ops.mesh.select_all(action='SELECT')

# export
if export_filepath:
    bpy.ops.wm.collada_export(filepath=export_filepath, check_existing=False, selected=True, triangulate=False)

#bpy.ops.wm.save_as_mainfile(filepath='C:/Users/dna11/OneDrive/Desktop/test.blend')
#bpy.data.objects.remove(root)

# success
print("FORGE SCRIPT COMPLETE")
sys.exit(1)
