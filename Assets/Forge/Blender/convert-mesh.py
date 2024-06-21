import sys
import bpy
import os
import time

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
ob = C.object
me = ob.data
uvlayer = me.uv_layers.active

# Object Mode
bpy.ops.object.mode_set(mode='OBJECT')

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
exit(1)
