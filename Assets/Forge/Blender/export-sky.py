import sys
import bpy
import os
import time
import bmesh

argv = sys.argv
try:
    argv = argv[argv.index("--") + 1:]  # get all args after "--"
except ValueError:
    # test value
    argv = []

print(argv)

in_filepath = argv[0]
out_filepath = argv[1]
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
