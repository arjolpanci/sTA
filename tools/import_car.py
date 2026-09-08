#!/usr/bin/env python3
"""Turn a downloaded car GLB into one this engine can draw.

Models from asset sites are built for offline renderers: a few hundred thousand
triangles, forty-odd materials, and a full PBR stack of metallic/roughness,
occlusion, clearcoat and transmission maps. The renderer here reads exactly two
of those maps, so everything else is weight. This script:

  1. keeps only the base colour and normal textures, dropping the rest along
     with the material extensions that reference them;
  2. bakes KHR_texture_transform into the UVs, since the shader samples plain
     UVs and would otherwise mis-map those materials;
  3. resizes what is left to a texture budget;
  4. simplifies the mesh with meshoptimizer (gltfpack).

Orientation is checked, not assumed: the engine drives vehicles along +Z, and
the script fails loudly if the model's long axis is not Z.

Usage:
  python3 tools/import_car.py import/cars/thing.glb --name thing \\
      [--triangles 12000] [--textures 512] [--source URL] [--license NAME]
"""

import argparse
import hashlib
import io
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "resources/models/cars"
MANIFEST = ROOT / "resources/models/manifest.json"

COMPONENT = {5120: "i1", 5121: "u1", 5122: "i2", 5123: "u2", 5125: "u4", 5126: "f4"}
COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def read_glb(path):
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise SystemExit(f"{path} is not a binary glTF (.glb)")
    offset, chunks = 12, {}
    while offset < len(data):
        length, kind = struct.unpack_from("<I4s", data, offset)
        offset += 8
        chunks[kind.decode().strip("\x00 ")] = data[offset:offset + length]
        offset += length
    return json.loads(chunks["JSON"]), bytearray(chunks["BIN"])


def write_glb(path, gltf, binary):
    def pad(block, filler):
        return block + filler * (-len(block) % 4)
    # Views were appended, so the declared buffer length is stale.
    gltf["buffers"][0]["byteLength"] = len(binary)
    json_chunk = pad(json.dumps(gltf, separators=(",", ":")).encode(), b" ")
    bin_chunk = pad(bytes(binary), b"\0")
    with open(path, "wb") as handle:
        handle.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)))
        handle.write(struct.pack("<I4s", len(json_chunk), b"JSON"))
        handle.write(json_chunk)
        handle.write(struct.pack("<I4s", len(bin_chunk), b"BIN\0"))
        handle.write(bin_chunk)


def view_bytes(gltf, binary, index):
    view = gltf["bufferViews"][index]
    start = view.get("byteOffset", 0)
    return bytes(binary[start:start + view["byteLength"]])


def read_accessor(gltf, binary, index):
    accessor = gltf["accessors"][index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    width = COMPONENTS[accessor["type"]]
    dtype = np.dtype("<" + COMPONENT[accessor["componentType"]])
    stride = view.get("byteStride") or width * dtype.itemsize
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    raw = np.frombuffer(bytes(binary), np.uint8, count=accessor["count"] * stride, offset=start)
    return np.ascontiguousarray(raw.reshape(accessor["count"], stride)[:, : width * dtype.itemsize]).view(dtype).reshape(accessor["count"], width)


def append_view(gltf, binary, blob):
    binary.extend(b"\0" * (-len(binary) % 4))
    offset = len(binary)
    binary.extend(blob)
    gltf["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob)})
    return len(gltf["bufferViews"]) - 1


def drop_nodes(gltf, names):
    """Detach named subtrees - such as an interior nobody can see through opaque glass."""
    if not names:
        return 0
    roots = [i for i, node in enumerate(gltf["nodes"]) if node.get("name") in names]
    if not roots:
        raise SystemExit(f"No node named {names}; check the model's node names")
    detached, stack = 0, list(roots)
    while stack:
        node = gltf["nodes"][stack.pop()]
        stack.extend(node.get("children", []))
        detached += node.pop("mesh", None) is not None
    for node in gltf["nodes"]:
        children = [c for c in node.get("children", []) if c not in roots]
        if "children" in node:
            node["children"] = children
            if not children:
                node.pop("children")
    for scene in gltf.get("scenes", []):
        scene["nodes"] = [n for n in scene["nodes"] if n not in roots]
    return detached


def drop_invisible_layers(gltf):
    """Remove geometry that only exists to be blended.

    Car models stack a transparent "coat" mesh over the paint to carry
    clearcoat, and glass panes over the interior. With no alpha blending in the
    renderer those layers draw as flat opaque shells - the coat covered this
    Porsche's paint in grey blotches. Anything with a near-zero alpha factor
    goes; anything merely tinted (glass at 0.8) stays and is marked opaque,
    since it has no alpha channel to test against anyway.
    """
    invisible = set()
    for index, material in enumerate(gltf.get("materials", [])):
        alpha = material.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])[3]
        # A coat carries its transparency in KHR_materials_transmission rather
        # than in alphaMode, so the alpha factor alone decides this.
        if alpha < 0.35:
            invisible.add(index)
        else:
            material.pop("alphaMode", None)
            material.pop("alphaCutoff", None)
    removed = 0
    for mesh in gltf["meshes"]:
        kept = [p for p in mesh["primitives"] if p.get("material") not in invisible]
        removed += len(mesh["primitives"]) - len(kept)
        mesh["primitives"] = kept
    # Mesh indices are referenced by nodes, so emptied meshes are detached
    # rather than removed; gltfpack drops what nothing points at.
    empty = {i for i, mesh in enumerate(gltf["meshes"]) if not mesh["primitives"]}
    for node in gltf["nodes"]:
        if node.get("mesh") in empty:
            node.pop("mesh")
    return removed


def strip_materials(gltf):
    """Reduce every material to base colour + normal, and count what went."""
    dropped = 0
    for material in gltf.get("materials", []):
        pbr = material.setdefault("pbrMetallicRoughness", {})
        for key in ("metallicRoughnessTexture",):
            dropped += key in pbr
            pbr.pop(key, None)
        for key in ("occlusionTexture", "emissiveTexture", "emissiveFactor"):
            dropped += key in material
            material.pop(key, None)
        # Clearcoat, transmission, specular, ior: all beyond a Blinn-Phong pass.
        dropped += len(material.pop("extensions", {}) or {}) > 0
    for key in ("extensionsUsed", "extensionsRequired"):
        gltf[key] = [e for e in gltf.get(key, []) if e == "KHR_texture_transform"]
    return dropped


def bake_texture_transforms(gltf, binary):
    """Fold KHR_texture_transform into the UVs it applies to.

    The shader samples plain UVs, so a material carrying a transform would map
    its texture wrongly. Each primitive gets its own rewritten UV accessor,
    which also sidesteps two primitives sharing one accessor under different
    transforms.
    """
    baked = 0
    for mesh in gltf["meshes"]:
        for primitive in mesh["primitives"]:
            if "material" not in primitive or "TEXCOORD_0" not in primitive["attributes"]:
                continue
            material = gltf["materials"][primitive["material"]]
            transforms = []
            for reference in (material.get("pbrMetallicRoughness", {}).get("baseColorTexture"),
                              material.get("normalTexture")):
                extension = (reference or {}).get("extensions", {}).get("KHR_texture_transform")
                if extension:
                    transforms.append((reference, extension))
            if not transforms:
                continue
            # One UV stream, so all of this material's maps must agree on it.
            first = transforms[0][1]
            if any(extension != first for _, extension in transforms):
                raise SystemExit("Material maps disagree on KHR_texture_transform; cannot bake")

            uv = read_accessor(gltf, binary, primitive["attributes"]["TEXCOORD_0"]).astype("<f4")
            scale = np.array(first.get("scale", [1, 1]), dtype="f4")
            offset = np.array(first.get("offset", [0, 0]), dtype="f4")
            rotation = float(first.get("rotation", 0))
            if rotation:
                cos, sin = np.cos(rotation), np.sin(rotation)
                uv = uv @ np.array([[cos, -sin], [sin, cos]], dtype="f4")
            uv = uv * scale + offset

            view = append_view(gltf, binary, uv.astype("<f4").tobytes())
            gltf["accessors"].append({"bufferView": view, "componentType": 5126,
                                      "count": len(uv), "type": "VEC2"})
            primitive["attributes"]["TEXCOORD_0"] = len(gltf["accessors"]) - 1
            for reference, _ in transforms:
                reference["extensions"].pop("KHR_texture_transform")
                if not reference["extensions"]:
                    reference.pop("extensions")
            baked += 1
    for key in ("extensionsUsed", "extensionsRequired"):
        gltf[key] = [e for e in gltf.get(key, []) if e != "KHR_texture_transform"]
        if not gltf[key]:
            gltf.pop(key)
    return baked


def prune_and_resize(gltf, binary, limit):
    """Keep only the images the surviving geometry needs, at the size budget."""
    # Materials orphaned by a detached subtree still name their textures, so
    # the set that matters is the one the remaining primitives point at.
    live = {p["material"] for mesh in gltf["meshes"] for p in mesh["primitives"] if "material" in p}
    used = []
    def keep(reference):
        if reference is None:
            return
        if reference["index"] not in used:
            used.append(reference["index"])
        reference["index"] = used.index(reference["index"])

    for index, material in enumerate(gltf.get("materials", [])):
        if index not in live:
            material.pop("normalTexture", None)
            material.get("pbrMetallicRoughness", {}).pop("baseColorTexture", None)
            continue
        keep(material.get("pbrMetallicRoughness", {}).get("baseColorTexture"))
        keep(material.get("normalTexture"))
    textures = [gltf["textures"][index] for index in used]
    sources = []
    for texture in textures:
        if texture["source"] not in sources:
            sources.append(texture["source"])
        texture["source"] = sources.index(texture["source"])

    images = []
    for index in sources:
        image = gltf["images"][index]
        raw = view_bytes(gltf, binary, image["bufferView"]) if "bufferView" in image else None
        if raw is None:
            raise SystemExit("External image URIs are not supported; use a self-contained .glb")
        picture = Image.open(io.BytesIO(raw))
        if max(picture.size) > limit:
            scale = limit / max(picture.size)
            picture = picture.resize((max(1, int(picture.width * scale)), max(1, int(picture.height * scale))), Image.LANCZOS)
        buffer = io.BytesIO()
        if picture.mode in ("RGBA", "LA", "P"):
            picture.convert("RGBA").save(buffer, format="PNG", optimize=True)
            mime = "image/png"
        else:
            picture.convert("RGB").save(buffer, format="JPEG", quality=88, optimize=True)
            mime = "image/jpeg"
        images.append({"bufferView": append_view(gltf, binary, buffer.getvalue()), "mimeType": mime})

    before, after = len(gltf["images"]), len(images)
    gltf["images"], gltf["textures"] = images, textures
    return before, after


def bounds(gltf, binary):
    def matrix(node):
        if "matrix" in node:
            return np.array(node["matrix"]).reshape(4, 4).T
        result = np.eye(4)
        if "scale" in node:
            result = np.diag(list(node["scale"]) + [1.0]) @ result
        if "rotation" in node:
            x, y, z, w = node["rotation"]
            spin = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                             [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                             [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
            spin4 = np.eye(4); spin4[:3, :3] = spin
            result = spin4 @ result
        if "translation" in node:
            move = np.eye(4); move[:3, 3] = node["translation"]
            result = move @ result
        return result

    parent = {}
    for index, node in enumerate(gltf["nodes"]):
        for child in node.get("children", []):
            parent[child] = index
    def world(index):
        result, walk = matrix(gltf["nodes"][index]), index
        while walk in parent:
            walk = parent[walk]
            result = matrix(gltf["nodes"][walk]) @ result
        return result

    low, high = np.full(3, np.inf), np.full(3, -np.inf)
    for index, node in enumerate(gltf["nodes"]):
        if "mesh" not in node:
            continue
        placement = world(index)
        for primitive in gltf["meshes"][node["mesh"]]["primitives"]:
            accessor = gltf["accessors"][primitive["attributes"]["POSITION"]]
            corners = np.array([[x, y, z, 1] for x in (accessor["min"][0], accessor["max"][0])
                                for y in (accessor["min"][1], accessor["max"][1])
                                for z in (accessor["min"][2], accessor["max"][2])])
            placed = (placement @ corners.T).T[:, :3]
            low, high = np.minimum(low, placed.min(0)), np.maximum(high, placed.max(0))
    return low, high


def triangles(gltf):
    return sum(gltf["accessors"][p["indices"]]["count"] // 3
               for mesh in gltf["meshes"] for p in mesh["primitives"] if "indices" in p)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--name", required=True, help="model name under resources/models/cars")
    parser.add_argument("--triangles", type=int, default=12000)
    parser.add_argument("--textures", type=int, default=512)
    parser.add_argument("--drop-node", action="append", default=[],
                        help="detach a named node and its children, e.g. an interior nothing can see")
    parser.add_argument("--source", default="", help="where it came from, recorded in the manifest")
    parser.add_argument("--license", default="unknown - user supplied")
    parser.add_argument("--gltfpack", default=shutil.which("gltfpack") or "npx --yes gltfpack")
    arguments = parser.parse_args()

    gltf, binary = read_glb(arguments.input)
    source_triangles = triangles(gltf)
    low, high = bounds(gltf, binary)
    size = high - low
    print(f"{arguments.input.name}: {source_triangles:,} triangles, {len(gltf['materials'])} materials, "
          f"{len(gltf.get('images', []))} images, {size[0]:.2f} x {size[1]:.2f} x {size[2]:.2f} m")
    # The engine drives vehicles along +Z and scales them to a car footprint.
    if size[2] < max(size[0], size[1]):
        raise SystemExit("Long axis is not Z: rotate the model so the car faces +Z before importing")

    detached = drop_nodes(gltf, arguments.drop_node)
    invisible = drop_invisible_layers(gltf)
    baked = bake_texture_transforms(gltf, binary)
    dropped = strip_materials(gltf)
    before, after = prune_and_resize(gltf, binary, arguments.textures)
    if arguments.drop_node:
        print(f"  detached {detached} mesh node(s) under {', '.join(arguments.drop_node)}")
    print(f"  dropped {invisible} blended layer(s), baked {baked} texture transform(s), "
          f"dropped {dropped} unused map(s), {before} images -> {after} at <= {arguments.textures}px")

    work = arguments.input.with_suffix(".prepared.glb")
    write_glb(work, gltf, binary)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    output = OUT_DIR / f"{arguments.name}.glb"
    ratio = min(1.0, arguments.triangles / max(source_triangles, 1))
    subprocess.run([*arguments.gltfpack.split(), "-i", str(work), "-o", str(output),
                    "-si", f"{ratio:.6f}", "-se", "0.15", "-sp",
                    "-vpf", "-vtf", "-vnf", "-noq"], check=True, capture_output=True, text=True)
    work.unlink()

    packed, _ = read_glb(output)
    print(f"  -> {output.relative_to(ROOT)}: {triangles(packed):,} triangles, "
          f"{len(packed['materials'])} materials, {output.stat().st_size/1e6:.1f} MB")

    records = [r for r in json.loads(MANIFEST.read_text())
               if (ROOT / "resources/models" / r["file"]).exists() and r["file"] != f"cars/{arguments.name}.glb"]
    records.append({"file": f"cars/{arguments.name}.glb",
                    "source": arguments.source or arguments.input.name,
                    "author": "", "license": arguments.license, "sha256": sha256(output)})
    MANIFEST.write_text(json.dumps(sorted(records, key=lambda r: r["file"]), indent=2) + "\n")


if __name__ == "__main__":
    sys.exit(main())
