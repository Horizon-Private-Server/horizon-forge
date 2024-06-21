import bpy

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

if bpy.context.selected_objects != []:
    for ob in bpy.context.selected_objects: 
        me = ob.data

        # Or just cycle all loops
        if ob.data is not None and ob.data.uv_layers.active is not None and len(ob.data.uv_layers.active.data) > 0:
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
            
print('done')

# success
exit(1)
