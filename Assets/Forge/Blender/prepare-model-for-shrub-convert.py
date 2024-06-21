import bpy
import sys
import os
import bmesh
from mathutils import Matrix

def add(ob, face, u_off, v_off):
    for vert_idx, loop_idx in zip(face.vertices, face.loop_indices):
        uv_coords = ob.data.uv_layers.active.data[loop_idx].uv
        uv_coords[0] += u_off
        uv_coords[1] += v_off
        ob.data.uv_layers.active.data[loop_idx].uv = uv_coords

def avg(ob, face):
    uAvg = 0
    vAvg = 0
    count = 0
    
    for vert_idx, loop_idx in zip(face.vertices, face.loop_indices):
        uv_coords = ob.data.uv_layers.active.data[loop_idx].uv
        uAvg += uv_coords[0]
        vAvg += uv_coords[1]
        count += 1
    
    
    if count > 0:
        return (uAvg / count, vAvg / count)
    
    return 0

def triangulate_object(obj):
    me = obj.data
    # Get a BMesh representation
    bm = bmesh.new()
    bm.from_mesh(me)

    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    # V2.79 : bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method=0, ngon_method=0)

    # Finish up, write the bmesh back to the mesh
    bm.to_mesh(me)
    bm.free()

def apply_transfrom(ob, use_location=False, use_rotation=False, use_scale=False):
    mb = ob.matrix_basis
    I = Matrix()
    loc, rot, scale = mb.decompose()

    # rotation
    T = Matrix.Translation(loc)
    #R = rot.to_matrix().to_4x4()
    R = mb.to_3x3().normalized().to_4x4()
    S = Matrix.Diagonal(scale).to_4x4()

    transform = [I, I, I]
    basis = [T, R, S]

    def swap(i):
        transform[i], basis[i] = basis[i], transform[i]

    if use_location:
        swap(0)
    if use_rotation:
        swap(1)
    if use_scale:
        swap(2)
        
    M = transform[0] @ transform[1] @ transform[2]
    if hasattr(ob.data, "transform"):
        ob.data.transform(M)
    
    for c in ob.children:
        c.matrix_local = M @ c.matrix_local
        
    ob.matrix_basis = basis[0] @ basis[1] @ basis[2]
    print(ob.name)

def apply_all_transforms_in_hierarchy(ob, levels=10):
    def recurse(ob, parent, depth):
        if depth > levels: 
            return
        
        apply_transfrom(ob, use_location=True, use_rotation=True, use_scale=True)

        for child in ob.children:
            recurse(child, ob,  depth + 1)
    recurse(ob, ob.parent, 0)
    
argv = sys.argv
try:
    argv = argv[argv.index("--") + 1:]  # get all args after "--"
except ValueError:
    # test value
    argv = [ "", "C:/Users/dna11/AppData/Local/Temp/horizon-forge/shrub-converter/shrub.glb", "de_dust2.001;de_dust2.002" ]

in_filepath = argv[0]
out_filepath = argv[1]
objs_to_export = argv[2]
cleanup = False
obj_names = objs_to_export.split(';') if objs_to_export else []
ext = os.path.splitext(in_filepath)[1]

print(out_filepath)
print(objs_to_export)

# reset scene
bpy.ops.wm.read_factory_settings(use_empty=True)

# import file
if ext == ".dae":
    bpy.ops.wm.collada_import(filepath = in_filepath, 
                          auto_connect = False, 
                          find_chains = False, 
                          fix_orientation = False) 
    
elif ext == ".blend":
    bpy.ops.wm.open_mainfile(filepath = in_filepath)
    
elif ext == ".glb" or ext == ".gltf":
    bpy.ops.import_scene.gltf(filepath = in_filepath) 

# enter object mode
if bpy.context.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

#Deselect all
bpy.ops.object.select_all(action='DESELECT')

# apply all modifiers
for obj in bpy.data.objects:
    print(f'{obj.name} {obj.type}')
    if obj.type == 'MESH':
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj

bpy.ops.object.convert(target='MESH')
#for obj in bpy.context.scene.objects:
#    triangulate_object(obj)

#
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.remove_doubles(threshold = 0.001)
bpy.ops.object.mode_set(mode='OBJECT')

#Deselect all
bpy.ops.object.select_all(action='DESELECT')

# apply all transforms
root_objs = (o for o in bpy.data.objects if not o.parent)
for obj in root_objs:
    apply_all_transforms_in_hierarchy(obj)

# iterate every object and move uv shapes to (0,0)
if len(obj_names) == 0 and bpy.data.objects != []:
    for ob in bpy.data.objects:
        if len(obj_names) > 0 and ob.name not in obj_names:
            continue
        
        me = ob.data

        # Or just cycle all loops
        if ob.type == 'MESH' and ob.data is not None and ob.data.uv_layers.active is not None and len(ob.data.uv_layers.active.data) > 0:
            for face in ob.data.polygons:
                uAvg, vAvg = avg(ob, face)
                while uAvg > 1 or uAvg < 0 or vAvg > 1 or vAvg < 0:
                    if uAvg > 1:
                        add(ob, face, -1, 0)
                    elif uAvg < 0:
                        add(ob, face, 1, 0)
                    
                    if vAvg > 1:
                        add(ob, face, 0, -1)
                    elif vAvg < 0:
                        add(ob, face, 0, 1)
                        
                    uAvg, vAvg = avg(ob, face)
                    
                    #ob.data.uv_layers.active.data[loop_idx].uv = uv_coords

# split every mesh by material
if len(obj_names) == 0 and bpy.data.objects != []:

    if bpy.context.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    # select all
    bpy.ops.object.select_all(action='DESELECT')
    for ob in bpy.data.objects:
        if ob.type == 'MESH' and not ob.name.endswith('_collider'):
            ob.select_set(True)
            bpy.context.view_layer.objects.active = ob

    # join all
    bpy.ops.object.join()

    # split by material
    objs = list(bpy.data.objects)
    for ob in objs:
        bpy.ops.object.select_all(action='DESELECT')
        if not ob.name.endswith('_collider'):
            ob.select_set(True)
                
            # enter edit mode
            if bpy.context.mode != 'EDIT':
                bpy.ops.object.mode_set(mode='EDIT')
                
            bpy.ops.mesh.separate(type='MATERIAL')
                
            # exit edit mode
            bpy.ops.object.mode_set(mode='OBJECT')
          
    # rename by idx
    rename_map = {}
    objs = list(bpy.data.objects)
    idx = 0
    for ob in objs:
        if ob.type == 'MESH' and not ob.name.endswith('_collider'):
            rename_map[ob.name] = str(idx)
            ob.name = str(idx)
            ob.data.name = str(idx)
            idx += 1
    
    # rename colliders
    objs = list(bpy.data.objects)
    idx = 0
    for ob in objs:
        if ob.name.endswith('_collider'):
            name = ob.name[:-9]
            if name in rename_map:
                ob.data.name = ob.name = rename_map[name] + '_collider'

                
# export as glb
bpy.ops.object.select_all(action='DESELECT')
if out_filepath:
    if objs_to_export:
        obj_names = objs_to_export.split(';')
        active_object = None
        for ob in bpy.data.objects:
            print(ob.name)
            if ob.name in obj_names:
                if not active_object:
                    active_object = ob
                ob.select_set(True)

        bpy.context.view_layer.objects.active = active_object
        bpy.ops.object.join()
        if cleanup:
          bpy.ops.object.mode_set(mode='EDIT')
          bpy.ops.mesh.select_all(action='SELECT')
          bpy.ops.mesh.remove_doubles(threshold = 0.0001)
          bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
        active_object.name = "shrub"
        active_object.data.name = "shrub"
        active_object.select_set(True)
        bpy.context.view_layer.objects.active = active_object
        print(active_object)
        # export selection
        bpy.ops.export_scene.gltf(
                    filepath=out_filepath,
                    use_selection=True,
                    )
    else:
        # export scene
        bpy.ops.export_scene.gltf(
                    filepath=out_filepath,
                    use_selection=False,
                    )

# success
exit(1)
