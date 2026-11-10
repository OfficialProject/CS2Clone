"""Convert Source 2 Viewer's texture-free Mirage export into a local playtest map."""
import argparse
import hashlib
import json
import math
import re
import struct
import subprocess
from pathlib import Path
from urllib.parse import unquote

import numpy as np

SOURCE_UNIT = 5.8 / 250
GLTF_SCALE = SOURCE_UNIT / 0.0254


def accessor(document, buffers, index):
    item = document["accessors"][index]
    if "sparse" in item:
        raise ValueError("Sparse accessors are unsupported")
    view = document["bufferViews"][item["bufferView"]]
    dtype = np.dtype({5121: "u1", 5123: "<u2", 5125: "<u4", 5126: "<f4"}[item["componentType"]])
    width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[item["type"]]
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    return np.ndarray((item["count"], width), dtype, buffers[view["buffer"]], start,
                      strides=(view.get("byteStride", width * dtype.itemsize), dtype.itemsize))


def transform(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=float).reshape(4, 4, order="F")
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    rotation = np.array([
        [1-2*y*y-2*z*z, 2*x*y-2*z*w, 2*x*z+2*y*w],
        [2*x*y+2*z*w, 1-2*x*x-2*z*z, 2*y*z-2*x*w],
        [2*x*z-2*y*w, 2*y*z+2*x*w, 1-2*x*x-2*y*y],
    ])
    result = np.eye(4)
    result[:3, :3] = rotation @ np.diag(node.get("scale", [1, 1, 1]))
    result[:3, 3] = node.get("translation", [0, 0, 0])
    return result


def triangles(path, mode):
    document = json.loads(path.read_text(encoding="utf-8"))
    buffers = [(path.parent / unquote(item["uri"])).read_bytes() for item in document.get("buffers", [])]
    pieces = []
    excluded = []

    def visit(index, parent):
        node = document["nodes"][index]
        matrix = parent @ transform(node)
        name = node.get("name", "")
        extras = node.get("extras", {})
        tags = set(extras.get("InteractAs", []))
        include = True
        if mode == "visual":
            include = not any(word in name.lower() for word in ("blocklight", "mesh_overlay"))
        elif mode == "entities":
            include = name in ("func_brush", "func_breakable", "prop_dynamic", "prop_dynamic_override")
        elif tags:
            include = bool(tags & {"playerclip", "ladder", "window", "passbullets"})
        if "mesh" in node and include:
            for primitive in document["meshes"][node["mesh"]]["primitives"]:
                if primitive.get("mode", 4) != 4:
                    raise ValueError(f"Unsupported primitive in {name}")
                positions = accessor(document, buffers, primitive["attributes"]["POSITION"])
                points = (positions @ matrix[:3, :3].T + matrix[:3, 3]) * GLTF_SCALE
                indices = (accessor(document, buffers, primitive["indices"]).reshape(-1)
                           if "indices" in primitive else np.arange(len(points)))
                faces = points[indices].reshape(-1, 3, 3)
                if np.linalg.det(matrix[:3, :3]) < 0:
                    faces = faces[:, [0, 2, 1]]
                area = np.linalg.norm(np.cross(faces[:, 1] - faces[:, 0],
                                               faces[:, 2] - faces[:, 0]), axis=1)
                pieces.append(faces[area > 1e-8])
        elif "mesh" in node:
            excluded.append(name)
        for child in node.get("children", []):
            visit(child, matrix)

    for index in document["scenes"][document.get("scene", 0)]["nodes"]:
        visit(index, np.eye(4))
    result = np.concatenate(pieces).astype("<f4") if pieces else np.empty((0, 3, 3), dtype="<f4")
    if not np.isfinite(result).all():
        raise ValueError("Nonfinite map geometry")
    return result, excluded


def spawns(path):
    result = []
    for entity in re.split(r"====\d+====", path.read_text(encoding="utf-8-sig")):
        team = re.search(r'classname\s+"info_player_(terrorist|counterterrorist)"', entity)
        origin = re.search(r"\borigin\s+\[([^]]+)\]", entity)
        angles = re.search(r"\bangles\s+\[([^]]+)\]", entity)
        if team and origin and angles:
            x, y, z = (float(value) for value in origin[1].split(","))
            yaw = float(angles[1].split(",")[1])
            result.append({"team": team[1], "position": [y*SOURCE_UNIT, z*SOURCE_UNIT + 0.02, x*SOURCE_UNIT],
                           "yaw": math.pi - math.radians(yaw)})
    if not result:
        raise ValueError("No player spawns found in exported entities")
    return result


def convert(export, entities, output, vpk):
    visual, excluded_visual = triangles(export / "de_mirage.gltf", "visual")
    world, excluded_world = triangles(export / "world_physics_physics.gltf", "collision")
    props, excluded_entities = triangles(export / "de_mirage_physics.gltf", "entities")
    collision = np.concatenate([world, props])
    locations = spawns(entities)
    first = next(item for item in locations if item["team"] == "terrorist")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("xb") as handle:
        handle.write(struct.pack("<8sII4f", b"CS2MAP01", len(visual), len(collision),
                                 *first["position"], first["yaw"]))
        handle.write(visual.tobytes())
        handle.write(collision.tobytes())
    with vpk.open("rb") as handle:
        digest = hashlib.file_digest(handle, "sha256").hexdigest()
    report = dict(format=1, input=vpk.name, sha256=digest, exporter="Source 2 Viewer 20.1",
                  source_unit=SOURCE_UNIT, visual_triangles=len(visual), collision_triangles=len(collision),
                  spawns=locations, excluded_visual=excluded_visual,
                  excluded_world=excluded_world, excluded_entities=excluded_entities)
    output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("sha256", "visual_triangles", "collision_triangles")}))


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("vpk", type=Path)
    cli.add_argument("--viewer-cli", type=Path, required=True)
    cli.add_argument("--work", type=Path, required=True, help="New export directory")
    cli.add_argument("--out", type=Path, required=True)
    args = cli.parse_args()
    if args.out.exists():
        cli.error("output already exists")
    args.work.mkdir(parents=True, exist_ok=False)
    viewer = str(args.viewer_cli.resolve())
    vpk = str(args.vpk.resolve())
    for resource, target in (
        ("maps/de_mirage.vmap_c", "de_mirage.gltf"),
        ("maps/de_mirage/world_physics.vmdl_c", "world_physics.gltf"),
        ("maps/de_mirage/entities/default_ents.vents_c", "entities.txt"),
    ):
        command = [viewer, "-i", vpk, "-f", resource, "-o", str(args.work / target), "-d"]
        if target.endswith(".gltf"):
            command += ["--gltf_export_format", "gltf"]
        subprocess.run(command, check=True)
        if not (args.work / target).is_file():
            raise RuntimeError(f"Exporter did not create {target}")
    convert(args.work, args.work / "entities.txt", args.out, args.vpk)


if __name__ == "__main__":
    main()
