#!/usr/bin/env python3
# [EN] Usage: python3 scripts/visualization/export_step_scan_3d.py --overlay-root results/step_size_scan/B115T_3deg/step_size_compare.root --gdml detector_geometry.gdml --event 0 --out results/step_size_scan/exports_3d / [CN] 用法: python3 scripts/visualization/export_step_scan_3d.py --overlay-root results/step_size_scan/B115T_3deg/step_size_compare.root --gdml detector_geometry.gdml --event 0 --out results/step_size_scan/exports_3d

import argparse
import datetime as dt
import json
import os
import sys
from array import array
from pathlib import Path
from typing import Dict, List, Sequence, Set, Tuple

import vtk


def sanitize_root_pythonpath() -> None:
    # [EN] Remove incompatible ROOT python path entries to avoid mixed-ROOT crashes. / [CN] 移除不兼容ROOT路径，避免多版本混用崩溃。
    env = os.environ.get("PYTHONPATH", "")
    if env:
        kept = [p for p in env.split(":") if p and "/software/root/lib" not in p]
        os.environ["PYTHONPATH"] = ":".join(kept)

    sys.path[:] = [p for p in sys.path if "/software/root/lib" not in p]


def import_root():
    sanitize_root_pythonpath()
    import ROOT  # type: ignore

    ROOT.gROOT.SetBatch(True)
    return ROOT


def write_polydata(path: Path, poly: vtk.vtkPolyData) -> None:
    writer = vtk.vtkXMLPolyDataWriter()
    writer.SetFileName(str(path))
    writer.SetInputData(poly)
    if writer.Write() != 1:
        raise RuntimeError(f"Failed to write VTP: {path}")


def load_traj_records(tree, event_id: int) -> Tuple[List[Dict], Set[int], int]:
    records: List[Dict] = []
    events: Set[int] = set()
    skipped = 0

    for entry in tree:
        eid = int(entry.event_id)
        events.add(eid)
        if eid != event_id:
            continue

        xs = [float(v) for v in entry.x]
        ys = [float(v) for v in entry.y]
        zs = [float(v) for v in entry.z]
        if len(xs) != len(ys) or len(xs) != len(zs) or len(xs) < 2:
            skipped += 1
            continue

        records.append(
            {
                "event_id": eid,
                "particle_id": int(entry.particle_id),
                "step_size_mm": float(entry.step_size),
                "x": xs,
                "y": ys,
                "z": zs,
            }
        )

    return records, events, skipped


def load_hit_records(tree, event_id: int) -> Tuple[List[Dict], Set[int]]:
    records: List[Dict] = []
    events: Set[int] = set()

    for entry in tree:
        eid = int(entry.event_id)
        events.add(eid)
        if eid != event_id:
            continue

        records.append(
            {
                "event_id": eid,
                "step_size_mm": float(entry.step_size),
                "x": float(entry.hit_x),
                "y": float(entry.hit_y),
                "z": float(entry.hit_z),
            }
        )

    return records, events


def build_trajectory_polydata(records: Sequence[Dict], tube_radius: float) -> Tuple[vtk.vtkPolyData, Dict]:
    points = vtk.vtkPoints()
    lines = vtk.vtkCellArray()

    point_track_id = vtk.vtkIntArray()
    point_track_id.SetName("track_id")

    point_index = vtk.vtkIntArray()
    point_index.SetName("point_index")

    cell_track_id = vtk.vtkIntArray()
    cell_track_id.SetName("track_id")

    cell_step_size = vtk.vtkDoubleArray()
    cell_step_size.SetName("step_size_mm")

    cell_particle_id = vtk.vtkIntArray()
    cell_particle_id.SetName("particle_id")

    cell_event_id = vtk.vtkIntArray()
    cell_event_id.SetName("event_id")

    step_sizes: Set[float] = set()
    total_points = 0
    total_tracks = 0

    for track_id, rec in enumerate(records):
        xs = rec["x"]
        ys = rec["y"]
        zs = rec["z"]
        n = len(xs)
        if n < 2:
            continue

        polyline = vtk.vtkPolyLine()
        polyline.GetPointIds().SetNumberOfIds(n)

        for i in range(n):
            pid = points.InsertNextPoint(xs[i], ys[i], zs[i])
            polyline.GetPointIds().SetId(i, pid)
            point_track_id.InsertNextValue(track_id)
            point_index.InsertNextValue(i)

        lines.InsertNextCell(polyline)
        cell_track_id.InsertNextValue(track_id)
        cell_step_size.InsertNextValue(rec["step_size_mm"])
        cell_particle_id.InsertNextValue(rec["particle_id"])
        cell_event_id.InsertNextValue(rec["event_id"])
        step_sizes.add(rec["step_size_mm"])

        total_points += n
        total_tracks += 1

    poly = vtk.vtkPolyData()
    poly.SetPoints(points)
    poly.SetLines(lines)
    poly.GetPointData().AddArray(point_track_id)
    poly.GetPointData().AddArray(point_index)
    poly.GetCellData().AddArray(cell_track_id)
    poly.GetCellData().AddArray(cell_step_size)
    poly.GetCellData().AddArray(cell_particle_id)
    poly.GetCellData().AddArray(cell_event_id)

    out_poly = poly
    if tube_radius > 0:
        # [EN] Convert lines to tubes for 3D viewing in generic tools. / [CN] 将线轨迹转为管道，便于通用3D工具观察。
        tube = vtk.vtkTubeFilter()
        tube.SetInputData(poly)
        tube.SetRadius(tube_radius)
        tube.SetNumberOfSides(12)
        tube.CappingOn()
        tube.Update()
        out_poly = vtk.vtkPolyData()
        out_poly.ShallowCopy(tube.GetOutput())

    stats = {
        "track_count": total_tracks,
        "point_count": total_points,
        "step_sizes_mm": sorted(step_sizes),
    }
    return out_poly, stats


def build_hits_polydata(records: Sequence[Dict], hit_size: float) -> Tuple[vtk.vtkPolyData, Dict]:
    points = vtk.vtkPoints()
    verts = vtk.vtkCellArray()

    arr_step_size = vtk.vtkDoubleArray()
    arr_step_size.SetName("step_size_mm")

    arr_event_id = vtk.vtkIntArray()
    arr_event_id.SetName("event_id")

    arr_hit_index = vtk.vtkIntArray()
    arr_hit_index.SetName("hit_index")

    arr_hit_size = vtk.vtkDoubleArray()
    arr_hit_size.SetName("hit_size")

    step_sizes: Set[float] = set()
    for i, rec in enumerate(records):
        pid = points.InsertNextPoint(rec["x"], rec["y"], rec["z"])
        verts.InsertNextCell(1)
        verts.InsertCellPoint(pid)

        arr_step_size.InsertNextValue(rec["step_size_mm"])
        arr_event_id.InsertNextValue(rec["event_id"])
        arr_hit_index.InsertNextValue(i)
        arr_hit_size.InsertNextValue(hit_size)
        step_sizes.add(rec["step_size_mm"])

    poly = vtk.vtkPolyData()
    poly.SetPoints(points)
    poly.SetVerts(verts)
    poly.GetPointData().AddArray(arr_step_size)
    poly.GetPointData().AddArray(arr_event_id)
    poly.GetPointData().AddArray(arr_hit_index)
    poly.GetPointData().AddArray(arr_hit_size)

    stats = {
        "hit_count": len(records),
        "step_sizes_mm": sorted(step_sizes),
    }
    return poly, stats


def _box_corners(dx: float, dy: float, dz: float) -> List[Tuple[float, float, float]]:
    return [
        (-dx, -dy, -dz),
        (dx, -dy, -dz),
        (dx, dy, -dz),
        (-dx, dy, -dz),
        (-dx, -dy, dz),
        (dx, -dy, dz),
        (dx, dy, dz),
        (-dx, dy, dz),
    ]


BOX_FACE_TRIANGLES = (
    (0, 1, 2), (0, 2, 3),
    (4, 6, 5), (4, 7, 6),
    (0, 5, 1), (0, 4, 5),
    (1, 6, 2), (1, 5, 6),
    (2, 7, 3), (2, 6, 7),
    (3, 4, 0), (3, 7, 4),
)


def build_geometry_polydata(ROOT, gdml_path: Path, include_world: bool) -> Tuple[vtk.vtkPolyData, Dict]:
    geom = ROOT.TGeoManager.Import(str(gdml_path))
    if not geom:
        raise RuntimeError(f"Failed to import GDML: {gdml_path}")

    geo = ROOT.gGeoManager
    top = geo.GetTopNode()
    if not top:
        raise RuntimeError("No top node in GDML geometry")

    points = vtk.vtkPoints()
    polys = vtk.vtkCellArray()

    arr_depth = vtk.vtkIntArray()
    arr_depth.SetName("depth")

    arr_uid = vtk.vtkIntArray()
    arr_uid.SetName("volume_uid")

    arr_node = vtk.vtkStringArray()
    arr_node.SetName("node_name")

    arr_volume = vtk.vtkStringArray()
    arr_volume.SetName("volume_name")

    arr_source = vtk.vtkStringArray()
    arr_source.SetName("source")

    local = array("d", [0.0, 0.0, 0.0])
    master = array("d", [0.0, 0.0, 0.0])

    included_nodes = 0
    skipped_nodes = 0

    def add_box_from_node(node, mat, depth: int) -> None:
        nonlocal included_nodes, skipped_nodes
        shape = node.GetVolume().GetShape()
        if not shape:
            skipped_nodes += 1
            return

        dx = float(shape.GetDX())
        dy = float(shape.GetDY())
        dz = float(shape.GetDZ())
        if dx <= 0 or dy <= 0 or dz <= 0:
            skipped_nodes += 1
            return

        corners_local = _box_corners(dx, dy, dz)
        corner_ids: List[int] = []
        for cx, cy, cz in corners_local:
            local[0], local[1], local[2] = cx, cy, cz
            mat.LocalToMaster(local, master)
            pid = points.InsertNextPoint(master[0], master[1], master[2])
            corner_ids.append(pid)

        vol = node.GetVolume()
        uid = int(vol.GetNumber()) if vol else -1
        node_name = str(node.GetName())
        vol_name = str(vol.GetName()) if vol else ""

        for a, b, c in BOX_FACE_TRIANGLES:
            tri = vtk.vtkTriangle()
            tri.GetPointIds().SetId(0, corner_ids[a])
            tri.GetPointIds().SetId(1, corner_ids[b])
            tri.GetPointIds().SetId(2, corner_ids[c])
            polys.InsertNextCell(tri)

            arr_depth.InsertNextValue(depth)
            arr_uid.InsertNextValue(uid)
            arr_node.InsertNextValue(node_name)
            arr_volume.InsertNextValue(vol_name)
            arr_source.InsertNextValue("gdml_bbox")

        included_nodes += 1

    def walk(node, mat, depth: int) -> None:
        if include_world or depth > 0:
            add_box_from_node(node, mat, depth)

        daughters = node.GetNodes()
        if not daughters:
            return

        for i in range(daughters.GetEntriesFast()):
            d = daughters.At(i)
            child_mat = ROOT.TGeoHMatrix(mat)
            child_mat.Multiply(d.GetMatrix())
            walk(d, child_mat, depth + 1)

    top_mat = ROOT.TGeoHMatrix()
    top_mat.Multiply(top.GetMatrix())
    walk(top, top_mat, 0)

    poly = vtk.vtkPolyData()
    poly.SetPoints(points)
    poly.SetPolys(polys)
    poly.GetCellData().AddArray(arr_depth)
    poly.GetCellData().AddArray(arr_uid)
    poly.GetCellData().AddArray(arr_node)
    poly.GetCellData().AddArray(arr_volume)
    poly.GetCellData().AddArray(arr_source)

    stats = {
        "node_count_included": included_nodes,
        "node_count_skipped": skipped_nodes,
        "triangle_count": poly.GetNumberOfCells(),
        "point_count": poly.GetNumberOfPoints(),
        "include_world": include_world,
    }
    return poly, stats


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export step-scan ROOT data to 3D VTP files (traj/hits/geometry).")
    parser.add_argument("--overlay-root", required=True, help="Input ROOT produced by compare_step_sizes.C")
    parser.add_argument("--gdml", required=True, help="Detector GDML file")
    parser.add_argument("--event", type=int, default=0, help="Event id to export")
    parser.add_argument("--out", default="results/step_size_scan/exports_3d", help="Output directory")
    parser.add_argument("--export-geometry", action=argparse.BooleanOptionalAction, default=True, help="Export geometry VTP")
    parser.add_argument("--include-world", action=argparse.BooleanOptionalAction, default=False, help="Include top world volume in geometry export")
    parser.add_argument("--tube-radius", type=float, default=2.0, help="Tube radius [mm] for trajectory display mesh")
    parser.add_argument("--hit-size", type=float, default=5.0, help="Stored hit marker size attribute")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    overlay_root = Path(args.overlay_root).resolve()
    gdml_path = Path(args.gdml).resolve()
    out_dir = Path(args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if not overlay_root.exists():
        raise FileNotFoundError(f"overlay root not found: {overlay_root}")
    if args.export_geometry and not gdml_path.exists():
        raise FileNotFoundError(f"gdml not found: {gdml_path}")

    ROOT = import_root()

    root_file = ROOT.TFile.Open(str(overlay_root), "READ")
    if not root_file or root_file.IsZombie():
        raise RuntimeError(f"Failed to open ROOT file: {overlay_root}")

    traj_tree = root_file.Get("traj")
    hit_tree = root_file.Get("step_scan")
    if not traj_tree:
        raise RuntimeError("Missing tree 'traj' in overlay root")
    if not hit_tree:
        raise RuntimeError("Missing tree 'step_scan' in overlay root")

    traj_records, traj_events, skipped_tracks = load_traj_records(traj_tree, args.event)
    hit_records, hit_events = load_hit_records(hit_tree, args.event)
    available_events = sorted(traj_events | hit_events)

    if args.event not in (traj_events | hit_events):
        sample = available_events[:20]
        raise RuntimeError(
            f"event {args.event} not found in traj/step_scan. "
            f"available events (first {len(sample)}): {sample}"
        )

    traj_poly, traj_stats = build_trajectory_polydata(traj_records, args.tube_radius)
    hit_poly, hit_stats = build_hits_polydata(hit_records, args.hit_size)

    traj_vtp = out_dir / f"traj_event{args.event}.vtp"
    hits_vtp = out_dir / f"hits_event{args.event}.vtp"
    write_polydata(traj_vtp, traj_poly)
    write_polydata(hits_vtp, hit_poly)

    geometry_vtp = None
    geometry_stats = None
    if args.export_geometry:
        geo_poly, geometry_stats = build_geometry_polydata(ROOT, gdml_path, args.include_world)
        geometry_vtp = out_dir / f"geometry_event{args.event}.vtp"
        write_polydata(geometry_vtp, geo_poly)

    manifest_path = out_dir / f"manifest_event{args.event}.json"
    manifest = {
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "inputs": {
            "overlay_root": str(overlay_root),
            "gdml": str(gdml_path),
            "event_id": int(args.event),
        },
        "options": {
            "tube_radius_mm": float(args.tube_radius),
            "hit_size": float(args.hit_size),
            "export_geometry": bool(args.export_geometry),
            "include_world": bool(args.include_world),
        },
        "outputs": {
            "traj_vtp": str(traj_vtp),
            "hits_vtp": str(hits_vtp),
            "geometry_vtp": str(geometry_vtp) if geometry_vtp else None,
        },
        "stats": {
            "available_events": available_events,
            "traj": traj_stats,
            "traj_skipped_tracks": int(skipped_tracks),
            "hits": hit_stats,
            "geometry": geometry_stats,
        },
    }
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Trajectory VTP: {traj_vtp}")
    print(f"Hits VTP: {hits_vtp}")
    if geometry_vtp:
        print(f"Geometry VTP: {geometry_vtp}")
    print(f"Manifest: {manifest_path}")
    print(
        "Summary: "
        f"tracks={traj_stats['track_count']}, "
        f"traj_points={traj_stats['point_count']}, "
        f"hits={hit_stats['hit_count']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
