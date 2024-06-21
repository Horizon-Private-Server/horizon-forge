import math
import bpy
import json
import mathutils
import time
import sys
import bmesh


argv = sys.argv
try:
    argv = argv[argv.index("--") + 1:]  # get all args after "--"
except ValueError:
    # test value
    argv = [
        'M:/Unity/deadlocked-srp/Assets/Resources/DL/Sarathos/0AE0.fbx',
        'M:/Unity/deadlocked-srp/Assets/Resources/DL/Sarathos/0AE0-out.fbx'
    ]
 
print(argv)

# reset scene
bpy.ops.wm.read_factory_settings(use_empty=True)

# import file
bpy.ops.import_scene.fbx( filepath = argv[0] )

for obj in bpy.context.selected_objects:

    # If object type is mesh and mode is set to object
    if obj.type == 'MESH' and obj.mode == 'OBJECT':
        
        print(obj.name)
        bpy.context.view_layer.objects.active = obj
        
        # Edit Mode
        bpy.ops.object.mode_set(mode='EDIT')
        # Seperate by material
        bpy.ops.mesh.duplicate()
        bpy.ops.mesh.flip_normals()
        
        #bpy.ops.mesh.separate(type='MATERIAL')
        # Object Mode
        bpy.ops.object.mode_set(mode='OBJECT')


# export to fbx
bpy.ops.export_scene.fbx(filepath = argv[1], apply_scale_options='FBX_SCALE_NONE', bake_space_transform=False, object_types={'MESH'})

# success
exit(1)
