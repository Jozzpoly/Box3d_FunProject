#!/usr/bin/env python3
import base64, json, math, struct
from pathlib import Path

ASSET = Path("assets/source/OneSided_Steering_Suspension_Rig.gltf")
gltf = json.loads(ASSET.read_text(encoding="utf-8"))
uri = gltf["buffers"][0]["uri"]
raw = base64.b64decode(uri.split(",", 1)[1])

FMT = {5123: ("H", 2), 5126: ("f", 4)}
NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

def accessor(i):
    a = gltf["accessors"][i]
    bv = gltf["bufferViews"][a["bufferView"]]
    fmt, size = FMT[a["componentType"]]
    n = NCOMP[a["type"]]
    stride = bv.get("byteStride", size * n)
    start = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    out = []
    for k in range(a["count"]):
        off = start + k * stride
        out.append(struct.unpack_from("<" + fmt * n, raw, off))
    return out

prim = gltf["meshes"][0]["primitives"][0]
pos = accessor(prim["attributes"]["POSITION"])
joints = accessor(prim["attributes"]["JOINTS_0"])
weights = accessor(prim["attributes"]["WEIGHTS_0"])
indices = [v[0] for v in accessor(prim["indices"])]
skin = gltf["skins"][0]
slot_to_node = skin["joints"]
nodes = gltf["nodes"]

per = {}
for vi, (p, js, ws) in enumerate(zip(pos, joints, weights)):
    nz = [(int(j), float(w)) for j, w in zip(js, ws) if abs(w) > 1e-7]
    if len(nz) != 1 or abs(nz[0][1] - 1.0) > 1e-6:
        raise SystemExit(f"non-rigid vertex {vi}: joints={js} weights={ws}")
    slot = nz[0][0]
    d = per.setdefault(slot, {"vertices": [], "triangles": 0, "crossTriangles": 0})
    d["vertices"].append((vi, p))

cross = []
for ti in range(0, len(indices), 3):
    tri = indices[ti:ti+3]
    slots = [int(next(j for j, w in zip(joints[v], weights[v]) if abs(w) > 1e-7)) for v in tri]
    uniq = sorted(set(slots))
    if len(uniq) == 1:
        per[uniq[0]]["triangles"] += 1
    else:
        cross.append({"triangle": ti // 3, "indices": tri, "slots": slots})
        for s in uniq:
            per.setdefault(s, {"vertices": [], "triangles": 0, "crossTriangles": 0})["crossTriangles"] += 1

report = {
    "asset": str(ASSET),
    "generator": gltf["asset"].get("generator"),
    "vertexCount": len(pos),
    "triangleCount": len(indices) // 3,
    "crossBoneTriangleCount": len(cross),
    "usedJointSlots": sorted(per),
    "skinJointSlotToNode": [
        {"slot": slot, "node": node, "name": nodes[node].get("name"), "translation": nodes[node].get("translation", [0,0,0])}
        for slot, node in enumerate(slot_to_node)
    ],
    "parts": [],
    "crossBoneTrianglesPreview": cross[:20],
}

for slot in sorted(per):
    d = per[slot]
    pts = [p for _, p in d["vertices"]]
    mn = [min(p[k] for p in pts) for k in range(3)] if pts else [None]*3
    mx = [max(p[k] for p in pts) for k in range(3)] if pts else [None]*3
    node = slot_to_node[slot]
    report["parts"].append({
        "slot": slot,
        "node": node,
        "name": nodes[node].get("name"),
        "nodeTranslation": nodes[node].get("translation", [0,0,0]),
        "vertexCount": len(pts),
        "pureTriangleCount": d["triangles"],
        "crossTriangleMembership": d["crossTriangles"],
        "bboxMeshLocal": {"min": mn, "max": mx, "extent": [mx[k]-mn[k] for k in range(3)] if pts else [None]*3},
    })

print("JV_CORE_DONOR_FRAGMENT_REPORT " + json.dumps(report, separators=(",", ":")))
