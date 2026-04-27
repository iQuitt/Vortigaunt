# Sketchfab Animation Importer for Blender
# Version: 1.0.0
# Author: Vortigaunt Project

bl_info = {
    "name": "Sketchfab Animation Importer",
    "author": "Vortigaunt Project",
    "version": (1, 0, 0),
    "blender": (3, 0, 0),
    "location": "File > Import > Sketchfab Animation",
    "description": "Import OBJ models with animations from Sketchfab Ripper",
    "category": "Import-Export",
}

import bpy
import json
import os
import math
from bpy.props import StringProperty
from bpy_extras.io_utils import ImportHelper
from mathutils import Quaternion, Vector, Matrix


class IMPORT_OT_sketchfab_animation(bpy.types.Operator, ImportHelper):
    """Import Sketchfab Ripper Output (animations.json + OBJ files)"""
    bl_idname = "import_scene.sketchfab_animation"
    bl_label = "Import Sketchfab Animation"
    bl_options = {'REGISTER', 'UNDO'}

    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'},
    )

    def execute(self, context):
        return self.import_sketchfab(context, self.filepath)

    def import_sketchfab(self, context, json_path):
        # Get directory containing the JSON file
        base_dir = os.path.dirname(json_path)
        
        # Load animations.json
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                anim_data = json.load(f)
        except Exception as e:
            self.report({'ERROR'}, f"Failed to load JSON: {str(e)}")
            return {'CANCELLED'}
        
        self.report({'INFO'}, f"Loaded animation data from {json_path}")
        
        # Import all OBJ files in the same directory
        obj_files = [f for f in os.listdir(base_dir) if f.endswith('.obj')]
        imported_objects = {}
        
        for obj_file in obj_files:
            obj_path = os.path.join(base_dir, obj_file)
            try:
                # Import OBJ with forward Y, up Z (convert from Y-up to Z-up)
                bpy.ops.wm.obj_import(
                    filepath=obj_path,
                    forward_axis='Y',
                    up_axis='Z'
                )
                
                # Get the newly imported object(s)
                for obj in context.selected_objects:
                    # Clean up name for matching
                    clean_name = obj.name.replace('.', '_').replace(' ', '_')
                    imported_objects[clean_name] = obj
                    imported_objects[obj.name] = obj
                    
                    # Also store with original file name (without extension)
                    base_name = os.path.splitext(obj_file)[0]
                    imported_objects[base_name] = obj
                    
                    # Store by prefix (e.g., "Box001" from "Box001_Material__154_0")
                    prefix = obj.name.split('_')[0]
                    if prefix not in imported_objects:
                        imported_objects[prefix] = obj
                    
                self.report({'INFO'}, f"Imported: {obj_file}")
            except Exception as e:
                self.report({'WARNING'}, f"Failed to import {obj_file}: {str(e)}")
        
        if not imported_objects:
            self.report({'ERROR'}, "No OBJ files imported")
            return {'CANCELLED'}
        
        self.report({'INFO'}, f"Imported objects: {list(imported_objects.keys())}")
        
        # Apply animations
        self.apply_animations(context, anim_data, imported_objects)
        
        return {'FINISHED'}
    
    def apply_animations(self, context, anim_data, imported_objects):
        """Apply animations from the JSON data to imported objects"""
        
        # Get animation data
        instance_anims = anim_data.get('instanceAnimations', {})
        
        if not instance_anims:
            self.report({'WARNING'}, "No animations found in JSON")
            return
        
        # Set frame rate (30 FPS based on timing)
        context.scene.render.fps = 30
        
        for anim_id, anim_info in instance_anims.items():
            name = anim_info.get('name', anim_id)
            duration = anim_info.get('duration', 1.0)
            channels = anim_info.get('channels', [])
            
            self.report({'INFO'}, f"Processing animation: {name}, duration: {duration}s")
            
            # Set scene frame range
            context.scene.frame_start = 0
            context.scene.frame_end = int(duration * context.scene.render.fps)
            
            for channel in channels:
                target = channel.get('target', '')
                target_name = channel.get('targetName', '')
                anim_type = channel.get('animationType', '')
                times = channel.get('times', [])
                keys = channel.get('keys', [])
                
                self.report({'INFO'}, f"  Channel: {anim_type} -> {target}")
                
                # Find target object
                target_obj = self.find_target_object(target, imported_objects)
                
                if not target_obj:
                    # Try to find by partial name match
                    for obj_name, obj in imported_objects.items():
                        if target.split('_')[0] in obj_name:
                            target_obj = obj
                            break
                
                if not target_obj:
                    self.report({'WARNING'}, f"  Target object not found: {target}")
                    continue
                
                self.report({'INFO'}, f"  Found target: {target_obj.name}")
                
                # Apply animation based on type
                if anim_type == 'rotate':
                    self.apply_rotation_animation(context, target_obj, times, keys)
                elif anim_type == 'translate':
                    self.apply_translation_animation(context, target_obj, times, keys)
                elif anim_type == 'scale':
                    self.apply_scale_animation(context, target_obj, times, keys)
    
    def create_armature(self, context, hierarchy, imported_objects):
        """Create armature from hierarchy"""
        if not hierarchy:
            return None
            
        bpy.ops.object.armature_add(enter_editmode=True, align='WORLD')
        amt_obj = context.object
        amt_obj.name = "Sketchfab_Skeleton"
        amt = amt_obj.data
        amt.name = "Sketchfab_Rig"
        
        # Clear default bone
        bpy.ops.armature.select_all(action='SELECT')
        bpy.ops.armature.delete()
        
        # Build bones recursively
        # We need to process in two passes: 
        # 1. Create all bones and hierarchy
        # 2. Set matrices (matrices in Sketchfab are often local or world, need to check)
        # Sketchfab OSGJS _matrix is usually local to parent.
        
        created_bones = {}
        
        def process_node(node, parent_bone_name=None):
            node_name = node.get('name', 'Node')
            # Sanitize name
            clean_name = node_name.replace('.', '_').replace(' ', '_')
            
            bone = amt.edit_bones.new(clean_name)
            bone.tail = (0, 0.1, 0) # Dummy tail
            
            created_bones[node.get('instanceID')] = bone.name
            created_bones[node_name] = bone.name
            
            if parent_bone_name:
                parent_bone = amt.edit_bones.get(parent_bone_name)
                if parent_bone:
                    bone.parent = parent_bone
            
            # Set transform if matrix is present
            matrix_data = node.get('matrix')
            if matrix_data:
                # Convert list to Matrix
                # OSGJS is Column-Major? or Row-Major? 
                # Usually standard GL is Column-Major. Blender is Row-Major.
                # But mathutils.Matrix takes nested lists (rows). 
                # If data is flat array of 16, we need to inspect.
                # Assuming flat list, column-major (standard OpenGL/WebGL)
                m_list = matrix_data
                mat = Matrix((
                    (m_list[0], m_list[4], m_list[8], m_list[12]),
                    (m_list[1], m_list[5], m_list[9], m_list[13]),
                    (m_list[2], m_list[6], m_list[10], m_list[14]),
                    (m_list[3], m_list[7], m_list[11], m_list[15])
                ))
                
                # We are in Edit Mode. Edit Bones use Head/Tail/Roll relative to parent? No, in Edit mode they are World Space mostly (or Armature Space).
                # But here we are building the rig.
                # It's better to store the matrix and apply it in Pose Mode OR set the Head/Tail in Edit Mode.
                # Setting Head/Tail in Edit Mode from a Matrix is tricky because Matrix includes Rotation/Scale/Translation.
                # Let's try to set the matrix in Object Mode (Pose Mode) for the REST POSE.
                # Or, simpler: Just build hierarchy in Edit Mode, then exit to Pose Mode and apply the Rest Pose matrices?
                # Actually, if we want the mesh to bind, the Edit Mode bones must match the mesh rest state.
                # If imported OBJs are already transformed (or not), we must match that.
                # Usually Sketchfab models (OBJs) are baked world space or local?
                # If disjoint OBJs, they are likely World Space if extracted via `saveAs`.
                # If so, the rig needs to align.
                pass
                
            for child in node.get('children', []):
                process_node(child, bone.name)

        process_node(hierarchy)
        
        bpy.ops.object.mode_set(mode='OBJECT')
        
        # Now apply the transforms (Binding Pose)
        # We need to traverse again and set the PoseBone matrices, then apply as Rest Pose?
        
        # Important: The axis conversion (Y-up to Z-up) needs to be applied to the Root Bone or the Armature Object.
        amt_obj.rotation_euler = (math.radians(90), 0, 0) # Fix Z-up
        
        return amt_obj

    def apply_animations(self, context, anim_data, imported_objects):
        """Apply animations from the JSON data to imported objects"""
        
        # Check for hierarchy
        hierarchy = anim_data.get('hierarchy', None)
        armature = None
        if hierarchy:
            self.report({'INFO'}, "Hierarchy found, building armature...")
            armature = self.create_armature(context, hierarchy, imported_objects)
        
        # Get animation data
        instance_anims = anim_data.get('instanceAnimations', {})
        
        if not instance_anims:
            self.report({'WARNING'}, "No animations found in JSON")
            return
        
        # Set frame rate (30 FPS based on timing)
        context.scene.render.fps = 30
        
        for anim_id, anim_info in instance_anims.items():
            name = anim_info.get('name', anim_id)
            duration = anim_info.get('duration', 1.0)
            channels = anim_info.get('channels', [])
            
            self.report({'INFO'}, f"Processing animation: {name}, duration: {duration}s")
            
            # Set scene frame range
            context.scene.frame_start = 0
            context.scene.frame_end = int(duration * context.scene.render.fps)
            
            for channel in channels:
                target = channel.get('target', '')
                target_name = channel.get('targetName', '')
                target_node_name = channel.get('targetNodeName', '')
                anim_type = channel.get('animationType', '')
                times = channel.get('times', [])
                keys = channel.get('keys', [])
                
                # self.report({'INFO'}, f"  Channel: {anim_type} -> {target}")
                
                # Determine target: Object or Bone
                target_obj = None
                is_bone = False
                
                if armature:
                    # Look for bone
                    # Try targetNodeName (most reliable from my extraction update)
                    if target_node_name:
                         # Sanitize
                        clean_bone_name = target_node_name.replace('.', '_').replace(' ', '_')
                        if clean_bone_name in armature.pose.bones:
                            target_obj = armature.pose.bones[clean_bone_name]
                            is_bone = True
                    
                    if not target_obj and target_name:
                         clean_bone_name = target_name.replace('.', '_').replace(' ', '_')
                         if clean_bone_name in armature.pose.bones:
                            target_obj = armature.pose.bones[clean_bone_name]
                            is_bone = True
                
                if not target_obj:
                    # Fallback to Mesh Objects
                    target_obj = self.find_target_object(target, imported_objects)
                    if not target_obj and target_node_name:
                         target_obj = self.find_target_object(target_node_name, imported_objects)

                if not target_obj:
                    # self.report({'WARNING'}, f"  Target not found: {target} / {target_node_name}")
                    continue
                
                # Apply animation based on type
                if anim_type == 'rotate':
                    self.apply_rotation_animation(context, target_obj, times, keys, is_bone)
                elif anim_type == 'translate':
                    self.apply_translation_animation(context, target_obj, times, keys, is_bone)
                elif anim_type == 'scale':
                    self.apply_scale_animation(context, target_obj, times, keys, is_bone)
    
    def find_target_object(self, target, imported_objects):
        """Find target object by name with various matching strategies"""
        if not target: return None
        # Direct match
        if target in imported_objects:
            return imported_objects[target]
        
        # Try with underscores
        clean_target = target.replace('.', '_').replace(' ', '_')
        if clean_target in imported_objects:
            return imported_objects[clean_target]
        
        # Try matching by prefix (e.g., "Box001" matches "Box001_Material__154_0")
        # But be careful not to match wrong things
        for name, obj in imported_objects.items():
            if name.startswith(target) or name.startswith(clean_target):
                 return obj
        
        return None
    
    def apply_rotation_animation(self, context, obj, times, keys, is_bone=False):
        """Apply rotation animation using quaternions"""
        fps = context.scene.render.fps
        
        if len(keys) % 4 != 0: return
        num_keyframes = len(keys) // 4
        
        # Set rotation mode
        if not is_bone:
            obj.rotation_mode = 'QUATERNION'
        else:
            obj.rotation_mode = 'QUATERNION' # PoseBone
        
        # Y-up to Z-up conversion quaternion (90 degrees around X axis)
        convert_quat = Quaternion((0.7071068, 0.7071068, 0, 0))  # 90 deg around X
        
        for i in range(min(len(times), num_keyframes)):
            frame = int(times[i] * fps)
            
            qx = keys[i * 4 + 0]
            qy = keys[i * 4 + 1]
            qz = keys[i * 4 + 2]
            qw = keys[i * 4 + 3]
            
            # Sketchfab: x, y, z, w
            # Blender: w, x, y, z
            src_quat = Quaternion((qw, qx, qy, qz))
            
            if not is_bone:
                # Apply axis conversion for Objects
                quat = convert_quat @ src_quat @ convert_quat.inverted()
                obj.rotation_quaternion = quat
                obj.keyframe_insert(data_path="rotation_quaternion", frame=frame)
            else:
                # Bones in the armature are already in the armature space
                # If Armature is rotated 90 deg, local bone rotations might need adjustment or not.
                # Usually animation data is Local space relative to parent keyframes.
                # If so, we just apply them as is (swapping Y/Z maybe?)
                
                # Let's try direct application first, assuming data is local
                # If "Ref" frame is different, we might need more math.
                # But for now, try direct mapping. Note: Blender Bone Y axis is "Length" axis.
                # Sketchfab/OSGJS usually Z or Y?
                
                obj.rotation_quaternion = src_quat
                obj.keyframe_insert(data_path="rotation_quaternion", frame=frame)

        # self.report({'INFO'}, f"  Applied rotation to {obj.name}")

    
    def apply_translation_animation(self, context, obj, times, keys, is_bone=False):
        """Apply translation animation"""
        fps = context.scene.render.fps
        
        if len(keys) % 3 != 0: return
        num_keyframes = len(keys) // 3
        
        # Y-up to Z-up conversion quaternion
        convert_quat = Quaternion((0.7071068, 0.7071068, 0, 0))
        
        for i in range(min(len(times), num_keyframes)):
            frame = int(times[i] * fps)
            
            x = keys[i * 3 + 0]
            y = keys[i * 3 + 1]
            z = keys[i * 3 + 2]
            
            vec = Vector((x, y, z))
            
            if not is_bone:
                # Fix Axis for Translation!
                # Rotate the vector by the conversion quaternion
                vec = convert_quat @ vec
                obj.location = vec
                obj.keyframe_insert(data_path="location", frame=frame)
            else:
                # Bone translation (Local)
                obj.location = vec
                obj.keyframe_insert(data_path="location", frame=frame)
        
        # self.report({'INFO'}, f"  Applied translation to {obj.name}")
    
    def apply_scale_animation(self, context, obj, times, keys, is_bone=False):
        """Apply scale animation"""
        fps = context.scene.render.fps
        
        if len(keys) % 3 != 0: return
        num_keyframes = len(keys) // 3
        
        for i in range(min(len(times), num_keyframes)):
            frame = int(times[i] * fps)
            
            sx = keys[i * 3 + 0]
            sy = keys[i * 3 + 1]
            sz = keys[i * 3 + 2]
            
            # Swapping Y/Z scale? Usually scale is uniform so doesn't matter, but if non-uniform...
            # If we rotated space, we swap axes.
            
            obj.scale = Vector((sx, sy, sz))
            obj.keyframe_insert(data_path="scale", frame=frame)
        
        # self.report({'INFO'}, f"  Applied scale to {obj.name}")


def menu_func_import(self, context):
    self.layout.operator(IMPORT_OT_sketchfab_animation.bl_idname, text="Sketchfab Animation (.json)")


def register():
    bpy.utils.register_class(IMPORT_OT_sketchfab_animation)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(IMPORT_OT_sketchfab_animation)
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)


if __name__ == "__main__":
    register()
