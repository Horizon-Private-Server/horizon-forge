# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####
# <pep8 compliant>

bl_info = {
    "name": "OBJ Batch Export",
    "author": "p2or, brockmann, trippeljojo",
    "version": (0, 3),
    "blender": (3, 1, 0),
    "location": "File > Import-Export",
    "description": "Export multiple OBJ files, their UVs and Materials",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export"}


import bpy
import os
from bpy_extras.io_utils import ExportHelper

from bpy.props import (BoolProperty,
                       IntProperty,
                       FloatProperty,
                       StringProperty,
                       EnumProperty,
                       CollectionProperty
                       )


class WM_OT_batchExportObjs(bpy.types.Operator, ExportHelper):
    """Batch export the scene to separate obj files"""
    bl_idname = "export_scene.batch_obj"
    bl_label = "Batch export OBJ's"
    bl_options = {'PRESET', 'UNDO'}

    # ExportHelper mixin class uses this
    filename_ext = ".obj"

    filter_glob = StringProperty(
            default="*.obj;*.mtl",
            options={'HIDDEN'},)
    
    # Object Properties
    axis_forward: EnumProperty(
            name="Axis Forward",
            items=(('X', "X", "Positive X Axis"),
                   ('Y', "Y", "Positive Y Axis"),
                   ('Z', "Z", "Positive Z Axis"),
                   ('NEGATIVE_X', "-X", "Negative X Axis"),
                   ('NEGATIVE_Y', "-Y", "Negative Y Axis"),
                   ('NEGATIVE_Z', "-Z (Default)", "Negative Z Axis"),),
            default='NEGATIVE_Z')
            
    axis_up: EnumProperty(
            name="Axis Up",
            items=(('X', "X Up", "Positive X Axis"),
                   ('Y', "Y Up (Default)", "Positive Y Axis"),
                   ('Z', "Z Up", "Positive Z Axis"),
                   ('NEGATIVE_X', "-X Up", "Negative X Axis"),
                   ('NEGATIVE_Y', "-Y Up", "Negative Y Axis"),
                   ('NEGATIVE_Z', "-Z Up", "Negative Z Axis"),),
            default='Y')

    scale_factor: FloatProperty(
            name="Scale",
            min=0.01, max=1000.0,
            default=1.0,)
    
    selection_only: BoolProperty(
            name="Selection Only",
            description="Export selected objects only",
            default=True,)
            
    eval_mode: EnumProperty(
            name="Evaluation Mode",
            items=(('DAG_EVAL_VIEWPORT', "Viewport (Default)", "Objects as they appear in the Viewport"),
                   ('DAG_EVAL_RENDER', "Render", "Objects as they appear in Render"),),
            default='DAG_EVAL_VIEWPORT')
    
    # Geometry Export
    write_uvs: BoolProperty(
            name="Include UVs",
            description="Write out the active UV coordinates",
            default=True)
            
    write_normals: BoolProperty(
            name="Write Normals",
            description="Export one normal per vertex and per face, to represent flat faces and sharp edges",
            default=True)

    write_materials: BoolProperty(
            name="Write Materials",
            description="Write out the MTL file",
            default=True)
            
    triangulate_faces: BoolProperty(
            name="Triangulate Faces",
            description="Convert all faces to triangles",
            default=False)

    write_nurbs: BoolProperty(
            name="Write Nurbs",
            description="Write nurbs curves as OBJ nurbs rather than "
                        "converting to geometry",
            default=False)
    
    # Grouping
    group_by_object: BoolProperty(
            name="Objects as OBJ Groups ",
            description="",
            default=False)
    
    group_by_material: BoolProperty(
            name="Material Groups",
            description="",
            default=False)
    
    group_by_vertex: BoolProperty(
            name="Polygroups",
            description="",
            default=False)
            
    smoothing_groups: BoolProperty(
            name="Smooth Groups",
            description="Write sharp edges as smooth groups",
            default=False)

    smoothing_group_bitflags: BoolProperty(
            name="Bitflag Smooth Groups",
            description="Same as 'Smooth Groups', but generate smooth groups IDs as bitflags "
                        "(produces at most 32 different smooth groups, usually much less)",
            default=False)

    def execute(self, context):                
        # Get the current folder
        folder_path = os.path.dirname(self.filepath)
        
        # Get all objects selected in the viewport
        viewport_selection = candidates = context.selected_objects
        if self.selection_only == False:
            candidates = [o for o in context.scene.objects]

        # Deselect all objects
        bpy.ops.object.select_all(action='DESELECT')
        
        for obj in [o for o in candidates if o.type == 'MESH']:
            obj.select_set(True)
            
            file_path = os.path.join(folder_path, "{}.obj".format(obj.name))
            bpy.ops.wm.obj_export(
                    filepath=file_path,
                    export_animation=False,
                    # Object Properties
                    forward_axis=self.axis_forward,
                    up_axis=self.axis_up,
                    scaling_factor=self.scale_factor,
                    export_selected_objects=True,
                    export_eval_mode=self.eval_mode,
                    # Geometry Export
                    export_uv=self.write_uvs,
                    export_normals=self.write_normals,
                    export_materials=self.write_materials,
                    export_triangulated_mesh=self.triangulate_faces,
                    export_curves_as_nurbs=self.write_nurbs,
                    # Grouping
                    export_object_groups=self.group_by_object,
                    export_material_groups = self.group_by_material,
                    export_vertex_groups = self.group_by_vertex,
                    export_smooth_groups=self.smoothing_groups,
                    smooth_group_bitflags=self.smoothing_group_bitflags
            )
            obj.select_set(False)

        # Restore Viewport Selection
        for obj in viewport_selection:
            obj.select_set(True)
            
        return {'FINISHED'}


def menu_func_import(self, context):
    self.layout.operator(WM_OT_batchExportObjs.bl_idname, text="Wavefront Batch (.obj)")

def register():
    bpy.utils.register_class(WM_OT_batchExportObjs)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_import)

def unregister():
    bpy.utils.unregister_class(WM_OT_batchExportObjs)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_import)


if __name__ == "__main__":
    register()

    # test call
    #bpy.ops.export_scene.batch_obj('INVOKE_DEFAULT')