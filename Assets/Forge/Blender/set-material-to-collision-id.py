import bpy
from random import random

mat = None
for m in bpy.data.materials:
    if m.name == 'col_f':
        mat = m
        
        
if mat is not None:
    
    for ob in bpy.context.selected_objects:
        if ob.type == 'MESH':
            me = ob.data
            
            # Can't assign materials in editmode
            bpy.ops.object.mode_set(mode='OBJECT')

            me.materials.clear()
            me.materials.append(mat)

            i = 0
            for poly in me.polygons:
                if poly.select:
                    poly.material_index = 0
                    i += 1
        