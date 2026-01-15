import bpy
import sys
import os
import math

# 获取命令行参数
argv = sys.argv
argv = argv[argv.index("--") + 1:]  # 获取 -- 之后的参数

if len(argv) < 2:
    print("Usage: blender -b -P vrml_to_png.py -- input.wrl output.png")
    sys.exit(1)

input_file = argv[0]
output_file = argv[1]

print(f"Converting {input_file} to {output_file}")

# 清除默认场景
bpy.ops.wm.read_factory_settings(use_empty=True)

# 导入VRML文件
try:
    bpy.ops.import_scene.x3d(filepath=input_file)
except Exception as e:
    print(f"Error importing VRML: {e}")
    sys.exit(1)

# 获取所有导入的物体并计算边界框
imported_objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
if not imported_objects:
    print("No mesh objects found in the scene")
    sys.exit(1)

# 计算场景边界
min_coords = [float('inf')] * 3
max_coords = [float('-inf')] * 3
for obj in imported_objects:
    for v in obj.bound_box:
        world_v = obj.matrix_world @ bpy.types.Object.bl_rna.properties['bound_box'].fixed_type.Vector(v)
        for i in range(3):
            min_coords[i] = min(min_coords[i], world_v[i])
            max_coords[i] = max(max_coords[i], world_v[i])

center = [(min_coords[i] + max_coords[i]) / 2 for i in range(3)]
size = max(max_coords[i] - min_coords[i] for i in range(3))

# 创建相机（俯视图）
bpy.ops.object.camera_add(location=(center[0], center[1], max_coords[2] + size * 2))
camera = bpy.context.active_object
camera.rotation_euler = (0, 0, 0)  # 俯视
camera.data.type = 'ORTHO'
camera.data.ortho_scale = size * 1.5
bpy.context.scene.camera = camera

# 创建光源
bpy.ops.object.light_add(type='SUN', location=(center[0], center[1], max_coords[2] + size * 3))
light = bpy.context.active_object
light.data.energy = 3

# 设置渲染参数
bpy.context.scene.render.engine = 'BLENDER_EEVEE_NEXT' if hasattr(bpy.types, 'BLENDER_EEVEE_NEXT') else 'BLENDER_EEVEE'
bpy.context.scene.render.resolution_x = 1920
bpy.context.scene.render.resolution_y = 1080
bpy.context.scene.render.filepath = output_file
bpy.context.scene.render.image_settings.file_format = 'PNG'

# 渲染
bpy.ops.render.render(write_still=True)

print(f"Successfully saved to {output_file}")
