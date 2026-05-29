#!/usr/bin/env python3
"""Inspect or apply Elden material bindings inside Blender."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def parse_args(argv: list[str]) -> argparse.Namespace:
    if "--" in argv:
        argv = argv[argv.index("--") + 1 :]
    parser = argparse.ArgumentParser(description="Blender FBX material binding helper.")
    parser.add_argument("--input", required=True, help="Input FBX path.")
    parser.add_argument("--output", help="Output FBX path after applying bindings.")
    parser.add_argument("--bindings", help="Material binding manifest JSON.")
    parser.add_argument("--inspect-json", help="Write material/image summary JSON.")
    parser.add_argument("--dry-run", action="store_true", help="Apply nothing and only report.")
    parser.add_argument("--embed-textures", action="store_true", help="Embed textures into exported FBX.")
    parser.add_argument(
        "--connect-packed-mask",
        action="store_true",
        help="Connect mask red/green channels to Metallic/Roughness.",
    )
    return parser.parse_args(argv)


def enable_fbx_importer(bpy) -> None:
    try:
        bpy.ops.preferences.addon_enable(module="io_scene_fbx")
    except Exception:
        pass


def import_fbx(bpy, path: str) -> None:
    enable_fbx_importer(bpy)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    bpy.ops.import_scene.fbx(filepath=path, use_anim=True)


def image_path(bpy, image) -> str:
    return bpy.path.abspath(image.filepath).replace("\\", "/") if image and image.filepath else ""


def material_images(bpy, mat) -> list[dict]:
    images = []
    if mat.use_nodes and mat.node_tree:
        for node in mat.node_tree.nodes:
            if node.bl_idname == "ShaderNodeTexImage" and getattr(node, "image", None):
                images.append(
                    {
                        "node": node.name,
                        "image": node.image.name,
                        "filepath": image_path(bpy, node.image),
                    }
                )
    return images


def write_inspect_json(bpy, path: str, input_fbx: str) -> None:
    data = {
        "schema": "winters.elden.blender_material_inspect.v1",
        "input": input_fbx.replace("\\", "/"),
        "objects": [
            {"name": obj.name, "type": obj.type}
            for obj in bpy.data.objects
        ],
        "materials": [
            {
                "name": mat.name,
                "useNodes": bool(mat.use_nodes),
                "images": material_images(bpy, mat),
            }
            for mat in bpy.data.materials
        ],
        "actions": [
            {
                "name": action.name,
                "frames": [float(action.frame_range[0]), float(action.frame_range[1])],
                "fcurves": len(action.fcurves),
            }
            for action in bpy.data.actions
        ],
    }
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def load_bindings(path: str) -> dict[str, dict]:
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    records = data.get("materials", [])
    result = {}
    for item in records:
        if not isinstance(item, dict):
            continue
        name = item.get("materialName")
        if name:
            result[str(name)] = item
    return result


def find_input(bsdf, names: tuple[str, ...]):
    for name in names:
        if name in bsdf.inputs:
            return bsdf.inputs[name]
    return None


def new_image_node(bpy, mat, image_path_value: str, label: str, non_color: bool):
    nodes = mat.node_tree.nodes
    node = nodes.new(type="ShaderNodeTexImage")
    node.label = label
    node.name = f"Winters {label}"
    image = bpy.data.images.load(image_path_value, check_existing=True)
    if non_color:
        image.colorspace_settings.name = "Non-Color"
    node.image = image
    return node


def reset_principled_tree(bpy, mat):
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    out = nodes.new(type="ShaderNodeOutputMaterial")
    out.location = (500, 0)
    bsdf = nodes.new(type="ShaderNodeBsdfPrincipled")
    bsdf.location = (240, 0)
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return bsdf


def apply_one_material(bpy, mat, record: dict, connect_packed_mask: bool) -> bool:
    bindings = record.get("bindings", {})
    if not isinstance(bindings, dict) or not bindings:
        return False

    bsdf = reset_principled_tree(bpy, mat)
    links = mat.node_tree.links

    for role, texture_path in bindings.items():
        if not texture_path or not Path(texture_path).exists():
            continue
        mat[f"winters_{role}_texture"] = texture_path

    if albedo := bindings.get("albedo"):
        if Path(albedo).exists():
            tex = new_image_node(bpy, mat, albedo, "Albedo", False)
            tex.location = (-520, 120)
            if inp := find_input(bsdf, ("Base Color",)):
                links.new(tex.outputs["Color"], inp)
            if "Alpha" in tex.outputs and (alpha_in := find_input(bsdf, ("Alpha",))):
                links.new(tex.outputs["Alpha"], alpha_in)
                mat.blend_method = "BLEND"

    if normal := bindings.get("normal"):
        if Path(normal).exists():
            tex = new_image_node(bpy, mat, normal, "Normal", True)
            tex.location = (-520, -160)
            normal_map = mat.node_tree.nodes.new(type="ShaderNodeNormalMap")
            normal_map.location = (-160, -160)
            links.new(tex.outputs["Color"], normal_map.inputs["Color"])
            if inp := find_input(bsdf, ("Normal",)):
                links.new(normal_map.outputs["Normal"], inp)

    if emissive := bindings.get("emissive"):
        if Path(emissive).exists():
            tex = new_image_node(bpy, mat, emissive, "Emissive", False)
            tex.location = (-520, -420)
            if inp := find_input(bsdf, ("Emission Color", "Emission")):
                links.new(tex.outputs["Color"], inp)
            if strength := find_input(bsdf, ("Emission Strength",)):
                strength.default_value = 1.0

    if mask := bindings.get("mask"):
        if Path(mask).exists():
            tex = new_image_node(bpy, mat, mask, "Mask", True)
            tex.location = (-520, -620)
            if connect_packed_mask:
                separate = mat.node_tree.nodes.new(type="ShaderNodeSeparateColor")
                separate.location = (-260, -620)
                links.new(tex.outputs["Color"], separate.inputs["Color"])
                if metallic := find_input(bsdf, ("Metallic",)):
                    links.new(separate.outputs["Red"], metallic)
                if roughness := find_input(bsdf, ("Roughness",)):
                    links.new(separate.outputs["Green"], roughness)

    if roughness := bindings.get("roughness"):
        if Path(roughness).exists() and (inp := find_input(bsdf, ("Roughness",))):
            tex = new_image_node(bpy, mat, roughness, "Roughness", True)
            tex.location = (-520, -820)
            links.new(tex.outputs["Color"], inp)

    mat["winters_binding_source"] = record.get("source", "")
    mat["winters_binding_token"] = record.get("token", "")
    return True


def apply_bindings(bpy, bindings_path: str, dry_run: bool, connect_packed_mask: bool) -> dict:
    records = load_bindings(bindings_path)
    applied = []
    unresolved = []
    for mat in bpy.data.materials:
        record = records.get(mat.name)
        if not record:
            unresolved.append(mat.name)
            continue
        if dry_run:
            if record.get("bindings"):
                applied.append(mat.name)
            else:
                unresolved.append(mat.name)
            continue
        if apply_one_material(bpy, mat, record, connect_packed_mask):
            applied.append(mat.name)
        else:
            unresolved.append(mat.name)
    return {"applied": applied, "unresolved": unresolved}


def export_fbx(bpy, path: str, embed_textures: bool) -> None:
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(
        filepath=str(out),
        object_types={"ARMATURE", "MESH"},
        axis_forward="-Z",
        axis_up="Y",
        apply_scale_options="FBX_SCALE_ALL",
        add_leaf_bones=False,
        bake_anim=False,
        path_mode="COPY" if embed_textures else "AUTO",
        embed_textures=embed_textures,
    )


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    import bpy

    import_fbx(bpy, args.input)

    if args.inspect_json:
        write_inspect_json(bpy, args.inspect_json, args.input)

    if args.bindings:
        result = apply_bindings(bpy, args.bindings, args.dry_run, args.connect_packed_mask)
        print(json.dumps(result, ensure_ascii=False, indent=2))

    if args.output and not args.dry_run:
        export_fbx(bpy, args.output, args.embed_textures)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
