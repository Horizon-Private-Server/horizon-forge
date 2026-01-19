import sys
import bpy
import os
import time
import bmesh

def add(obj, face, u_off, v_off):
    for vert_idx, loop_idx in zip(face.vertices, face.loop_indices):
        uv_coords = obj.data.uv_layers.active.data[loop_idx].uv
        uv_coords[0] += u_off
        uv_coords[1] += v_off
        obj.data.uv_layers.active.data[loop_idx].uv = uv_coords

def avg(obj, face):
    uAvg = 0
    vAvg = 0
    count = 0
    
    for vert_idx, loop_idx in zip(face.vertices, face.loop_indices):
        uv_coords = obj.data.uv_layers.active.data[loop_idx].uv
        uAvg += uv_coords[0]
        vAvg += uv_coords[1]
        count += 1
    
    if count > 0:
        return (uAvg / count, vAvg / count)
    
    return 0


def create_merged(objs):
    bpy.ops.object.select_all(action='DESELECT')
    
    # create root
    otherMesh = bpy.data.meshes.new('merged')
    otherRoot = bpy.data.objects.new("merged", otherMesh)
    otherRoot.location = (0,0,0)
    C.collection.objects.link(otherRoot)
    C.view_layer.objects.active = otherRoot
    otherRoot.select_set(state=True)
    
    # merge objs into root
    for merge_obj in objs:
        copy = merge_obj.copy()
        copy.data = merge_obj.data.copy()
        C.collection.objects.link(copy)
        copy.select_set(state=True)
        merge_obj.select_set(state=False)
        
    # merge
    C.view_layer.objects.active = otherRoot
    bpy.ops.object.join()
    
    # remove materials from merged
    for s in [x.name for x in otherRoot.material_slots]:
        print('removing slot %s' % s)
        otherRoot.active_material_index = 0
        bpy.ops.object.material_slot_remove()
        
    # recalculate normals
    bpy.ops.object.select_all(action='DESELECT')
    otherRoot.select_set(True)
    bpy.context.view_layer.objects.active = otherRoot
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.remove_doubles(threshold = 0.001)
    bpy.ops.mesh.normals_tools(mode='RESET')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.editmode_toggle()
    
    return otherRoot

argv = sys.argv
try:
    argv = argv[argv.index("--") + 1:]  # get all args after "--"
except ValueError:
    # test value
    argv = [ "M:/Unity/horizon-forge/levels/New Map/assets/shrub/0002_20B8/shrub.bin.glb"
    , "M:/Unity/horizon-forge/Assets/Maps/New Map/Shrub/8376/8376.fbx" ]

print(argv)

in_filepath = argv[0]
out_filepath = argv[1]
fix_normals = argv[2] == "1" if argv is not None and len(argv) > 2 else False
ext = os.path.splitext(in_filepath)[1]

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

C = bpy.context
obj = C.object
me = obj.data
uvlayer = me.uv_layers.active

# Object Mode
bpy.ops.object.mode_set(mode='OBJECT')

# todo: new fix normals
if fix_normals and False:
    all_objects = [x for x in C.scene.objects]
    all_objects_merged = create_merged(all_objects)

    for obj in all_objects:
        modDT = obj.modifiers.new("DataTransfer", type="DATA_TRANSFER")
        modDT.object = all_objects_merged
        modDT.use_loop_data = True
        modDT.data_types_loops = {'CUSTOM_NORMAL'}
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.modifier_apply(modifier=modDT.name)

        # recalculate normals
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        #bpy.ops.mesh.remove_doubles(threshold = 0.001)
        #bpy.ops.mesh.normals_tools(mode='RESET')
        #bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.object.editmode_toggle()
        
    bpy.data.objects.remove(all_objects_merged)

# old fix normals
if fix_normals:
    all_objects = [x for x in C.scene.objects]
    for obj in all_objects:
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        # go edit mode
        bpy.ops.object.mode_set(mode='EDIT')
        # select all faces
        bpy.ops.mesh.select_all(action='SELECT')
        # merge
        bpy.ops.mesh.remove_doubles(threshold = 0.001)
        # reset normals
        bpy.ops.mesh.normals_tools(mode='RESET')
        # recalculate outside normals 
        bpy.ops.mesh.normals_make_consistent(inside=False)
        # go object mode again
        bpy.ops.object.editmode_toggle()

    # Object Mode
    bpy.ops.object.mode_set(mode='OBJECT')

# fix uvs
if True:
    all_objects = [x for x in C.scene.objects]
    for obj in all_objects:
        me = obj.data

        # cycle all loops
        if obj.data is not None and obj.data.uv_layers.active is not None and len(obj.data.uv_layers.active.data) > 0:
            for face in obj.data.polygons:
                uAvg, vAvg = avg(obj, face)
                while uAvg > 1 or uAvg < 0 or vAvg > 1 or vAvg < 0:
                    if uAvg > 1:
                        add(obj, face, -1, 0)
                    elif uAvg < 0:
                        add(obj, face, 1, 0)
                    
                    if vAvg > 1:
                        add(obj, face, 0, -1)
                    elif vAvg < 0:
                        add(obj, face, 0, 1)
                        
                    uAvg, vAvg = avg(obj, face)

# Convert material names to "0","1","2",etc
# for obj in bpy.data.objects:
#     for num, m in list(enumerate(obj.material_slots)):
#         if m.material:
#             mat_name = m.material.name

#             # material_0, material_1, etc
#             if mat_name.find("material_") == 0:
#                 new_name = mat_name[9:]
#                 print ("Renaming material ", mat_name, " to ", new_name)
#                 m.material.name = new_name

# Export
out_ext = os.path.splitext(out_filepath)[1]
if out_ext == ".blend":
    bpy.ops.wm.save_as_mainfile(filepath = out_filepath)
elif out_ext == ".glb":
    bpy.ops.export_scene.gltf(filepath = out_filepath)
elif out_ext == ".fbx":
    bpy.ops.export_scene.fbx(filepath = out_filepath, axis_forward='Y', axis_up='Z', apply_scale_options='FBX_SCALE_ALL')
else:
    raise RuntimeError(f'Unsupported export extension {out_ext}')

# success
print("FORGE SCRIPT COMPLETE", flush=True)
sys.exit(1)
