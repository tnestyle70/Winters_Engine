"""Strip armatures from an FBX and re-export meshes only (bind pose).

Used for >512-bone characters (Margit, Tree Guard) that the runtime cannot
skin yet: produces a static-display FBX that WintersAssetConverter cooks into
a boneless stride-48 .wmesh.

Usage:
  blender --factory-startup --background --python blender_strip_armature.py -- \
      --input <in.fbx> --output <out.fbx>
"""

import argparse
import sys

import bpy

argv = sys.argv
argv = argv[argv.index("--") + 1 :] if "--" in argv else []
parser = argparse.ArgumentParser()
parser.add_argument("--input", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args(argv)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete()

bpy.ops.import_scene.fbx(filepath=args.input)

# Keep world transforms while unparenting meshes from the armature.
for obj in [o for o in bpy.data.objects if o.type == "MESH"]:
    matrix = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = matrix
    for modifier in list(obj.modifiers):
        if modifier.type == "ARMATURE":
            obj.modifiers.remove(modifier)

for obj in [o for o in bpy.data.objects if o.type == "ARMATURE"]:
    bpy.data.objects.remove(obj, do_unlink=True)

meshes = [o for o in bpy.data.objects if o.type == "MESH"]
if not meshes:
    raise RuntimeError("no mesh objects after armature strip")

bpy.ops.object.select_all(action="DESELECT")
for obj in meshes:
    obj.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]

bpy.ops.export_scene.fbx(
    filepath=args.output,
    use_selection=True,
    object_types={"MESH"},
    add_leaf_bones=False,
    bake_anim=False,
)
print(f"STRIPPED_FBX={args.output} meshes={len(meshes)}")
