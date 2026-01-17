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

# force material ids to col_XX format
for obj in bpy.data.objects:
    for num, m in list(enumerate(obj.material_slots)):
        if m.material:
            mat_name = m.material.name

            # material_0, material_1, etc
            if mat_name == 'hero_group_collision':
                print ("Renaming material ", mat_name, " to col_100")
                m.material.name = 'col_100'

# merge into single mesh
bpy.ops.object.select_all(action='SELECT')
root = bpy.data.objects[0]
C.view_layer.objects.active = root
bpy.ops.object.join()
bpy.ops.object.select_all(action='DESELECT')
C.view_layer.objects.active = root
root.select_set(state=True)

# Export
if out_ext == ".blend":
    bpy.ops.wm.save_as_mainfile(filepath = out_filepath)
elif out_ext == ".glb":
    bpy.ops.export_scene.gltf(filepath = out_filepath)
else:
    bpy.ops.export_scene.fbx(filepath = out_filepath, axis_forward='Y', axis_up='Z', apply_scale_options='FBX_SCALE_ALL')

# success
print("FORGE SCRIPT COMPLETE", flush=True)
sys.exit(1)
