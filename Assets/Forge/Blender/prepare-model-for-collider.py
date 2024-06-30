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
    , "M:/Unity/horizon-forge/Assets/Maps/New Map/Shrub/8376/8376.fbx", "col_2f" ]

print(argv)

in_filepath = argv[0]
out_filepath = argv[1]
default_mat_name = argv[2]
out_ext = os.path.splitext(out_filepath)[1]
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

# merge vertices by distance
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
    #bpy.ops.mesh.normals_tools(mode='RESET')
    # recalculate outside normals 
    #bpy.ops.mesh.normals_make_consistent(inside=False)
    # go object mode again
    bpy.ops.object.editmode_toggle()

# Object Mode
bpy.ops.object.mode_set(mode='OBJECT')


# all_objects = [x for x in C.scene.objects]
# for obj in all_objects:
#     bpy.ops.object.select_all(action='DESELECT')
#     obj.select_set(True)
#     bpy.context.view_layer.objects.active = obj
#     # go edit mode
#     bpy.ops.object.mode_set(mode='EDIT')
#     # select al faces
#     bpy.ops.mesh.select_all(action='SELECT')
#     # recalculate outside normals 
#     bpy.ops.mesh.normals_make_consistent(inside=False)
#     # go object mode again
#     bpy.ops.object.editmode_toggle()

# force material ids to col_XX format
for obj in bpy.data.objects:
    for num, m in list(enumerate(obj.material_slots)):
        if m.material:
            mat_name = m.material.name

            # material_0, material_1, etc
            if mat_name.find("col_") < 0:
                print ("Renaming material ", mat_name, " to ", default_mat_name)
                m.material.name = default_mat_name

# Export
if out_ext == ".blend":
    bpy.ops.wm.save_as_mainfile(filepath = out_filepath)
elif out_ext == ".glb":
    bpy.ops.export_scene.gltf(filepath = out_filepath)
else:
    bpy.ops.export_scene.fbx(filepath = out_filepath, axis_forward='Y', axis_up='Z', apply_scale_options='FBX_SCALE_ALL')

# success
exit(1)
