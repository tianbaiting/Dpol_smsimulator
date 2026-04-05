#!/usr/bin/env python3

import argparse
import math
from pathlib import Path

import vtk


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render a deterministic PNG snapshot from a VRML/WRL scene."
    )
    parser.add_argument("--input-wrl", type=Path, required=True, help="Input WRL file")
    parser.add_argument("--output-png", type=Path, required=True, help="Output PNG file")
    parser.add_argument("--width", type=int, default=1800, help="Image width in pixels")
    parser.add_argument("--height", type=int, default=1000, help="Image height in pixels")
    parser.add_argument(
        "--view-direction",
        type=float,
        nargs=3,
        default=(-1.30, 0.85, 0.72),
        metavar=("DX", "DY", "DZ"),
        help="Camera direction toward the focal point",
    )
    parser.add_argument(
        "--view-up",
        type=float,
        nargs=3,
        default=(0.0, 0.0, 1.0),
        metavar=("UX", "UY", "UZ"),
        help="Camera up vector",
    )
    parser.add_argument("--distance-scale", type=float, default=2.35, help="Camera distance in scene diagonals")
    parser.add_argument("--zoom", type=float, default=1.18, help="Camera zoom factor")
    parser.add_argument("--azimuth", type=float, default=0.0, help="Additional azimuth rotation in degrees")
    parser.add_argument("--elevation", type=float, default=0.0, help="Additional elevation rotation in degrees")
    parser.add_argument(
        "--focus-point",
        type=float,
        nargs=3,
        default=None,
        metavar=("FX", "FY", "FZ"),
        help="Explicit focal point in world coordinates",
    )
    parser.add_argument(
        "--camera-position",
        type=float,
        nargs=3,
        default=None,
        metavar=("PX", "PY", "PZ"),
        help="Explicit camera position in world coordinates",
    )
    parser.add_argument(
        "--camera-preset",
        type=str,
        default="xoz_from_plus_y",
        choices=("xoz_from_plus_y", "ips_event_side", "generic"),
        help="Named camera preset",
    )
    return parser.parse_args()


def normalize(vector: tuple[float, float, float]) -> tuple[float, float, float]:
    norm = math.sqrt(sum(component * component for component in vector))
    if norm <= 0.0:
        raise ValueError("camera vector norm must be positive")
    return tuple(component / norm for component in vector)


def build_renderer(input_wrl: Path, width: int, height: int) -> tuple[vtk.vtkRenderWindow, vtk.vtkRenderer]:
    render_window = vtk.vtkRenderWindow()
    render_window.OffScreenRenderingOn()
    render_window.SetSize(width, height)
    render_window.SetMultiSamples(0)

    importer = vtk.vtkVRMLImporter()
    importer.SetFileName(str(input_wrl))
    importer.SetRenderWindow(render_window)
    importer.Update()

    renderers = render_window.GetRenderers()
    renderer = renderers.GetFirstRenderer()
    if renderer is None:
        raise RuntimeError(f"no renderer found after loading {input_wrl}")
    renderer.SetBackground(1.0, 1.0, 1.0)
    return render_window, renderer


def configure_camera(
    renderer: vtk.vtkRenderer,
    view_direction: tuple[float, float, float],
    view_up: tuple[float, float, float],
    distance_scale: float,
    zoom: float,
    azimuth: float,
    elevation: float,
    focus_point: tuple[float, float, float] | None,
    camera_position: tuple[float, float, float] | None,
) -> None:
    bounds = [0.0] * 6
    renderer.ComputeVisiblePropBounds(bounds)
    if not all(math.isfinite(value) for value in bounds):
        raise RuntimeError("scene bounds are invalid")

    scene_center = (
        0.5 * (bounds[0] + bounds[1]),
        0.5 * (bounds[2] + bounds[3]),
        0.5 * (bounds[4] + bounds[5]),
    )
    diagonal = math.sqrt(
        (bounds[1] - bounds[0]) ** 2 +
        (bounds[3] - bounds[2]) ** 2 +
        (bounds[5] - bounds[4]) ** 2
    )
    diagonal = max(diagonal, 1.0)
    direction = normalize(view_direction)
    up = normalize(view_up)
    focal_point = focus_point if focus_point is not None else scene_center

    camera = renderer.GetActiveCamera()
    # [EN] Keep a fixed focal point near the target region for IPS event slides; using the whole-scene bounds would bias the view toward far-away support volumes. / [CN] 对 IPS 事件图固定聚焦在靶区附近；若使用整幅场景包围盒，会被远离靶区的支撑体积拉偏视角。
    camera.SetFocalPoint(*focal_point)
    if camera_position is not None:
        camera.SetPosition(*camera_position)
    else:
        camera.SetPosition(
            focal_point[0] - direction[0] * diagonal * distance_scale,
            focal_point[1] - direction[1] * diagonal * distance_scale,
            focal_point[2] - direction[2] * diagonal * distance_scale,
        )
    camera.SetViewUp(*up)
    renderer.ResetCameraClippingRange()
    if abs(azimuth) > 0.0:
        camera.Azimuth(azimuth)
    if abs(elevation) > 0.0:
        camera.Elevation(elevation)
    if abs(zoom - 1.0) > 1.0e-9:
        camera.Zoom(zoom)
    renderer.ResetCameraClippingRange()


def preset_defaults(args: argparse.Namespace) -> None:
    if args.camera_preset == "xoz_from_plus_y":
        # [EN] Match the requested physics view: look from +y toward the target reference point so the displayed plane is x-z with z pointing upward. / [CN] 对应所要求的物理视角：从 +y 朝 target 参考点看过去，图面即 x-z 平面且 z 方向朝上。
        if args.focus_point is None:
            args.focus_point = (-12.488, 0.009, -1069.458)
        if args.camera_position is None:
            args.camera_position = (-12.488, 26000.0, -1069.458)
        if tuple(args.view_up) == (0.0, 0.0, 1.0):
            args.view_up = (0.0, 0.0, 1.0)
        if abs(args.zoom - 1.18) < 1.0e-9:
            args.zoom = 1.00
        return

    if args.camera_preset != "ips_event_side":
        return
    if args.focus_point is None:
        args.focus_point = (0.0, 0.0, 0.0)
    if args.camera_position is None:
        args.camera_position = (-3450.71, 21819.04, 2194.8)
    if tuple(args.view_direction) == (-1.30, 0.85, 0.72):
        args.view_direction = (0.16, -0.98, -0.10)
    if tuple(args.view_up) == (0.0, 0.0, 1.0):
        args.view_up = (0.30, 0.14, -0.94)
    if abs(args.distance_scale - 2.35) < 1.0e-9:
        args.distance_scale = 1.00
    if abs(args.zoom - 1.18) < 1.0e-9:
        args.zoom = 1.00


def write_png(render_window: vtk.vtkRenderWindow, output_png: Path) -> None:
    output_png.parent.mkdir(parents=True, exist_ok=True)
    render_window.Render()

    window_to_image = vtk.vtkWindowToImageFilter()
    window_to_image.SetInput(render_window)
    window_to_image.SetScale(1)
    window_to_image.ReadFrontBufferOff()
    window_to_image.Update()

    writer = vtk.vtkPNGWriter()
    writer.SetFileName(str(output_png))
    writer.SetInputConnection(window_to_image.GetOutputPort())
    writer.Write()


def main() -> int:
    args = parse_args()
    preset_defaults(args)
    render_window, renderer = build_renderer(args.input_wrl, args.width, args.height)
    configure_camera(
        renderer=renderer,
        view_direction=tuple(args.view_direction),
        view_up=tuple(args.view_up),
        distance_scale=args.distance_scale,
        zoom=args.zoom,
        azimuth=args.azimuth,
        elevation=args.elevation,
        focus_point=tuple(args.focus_point) if args.focus_point is not None else None,
        camera_position=tuple(args.camera_position) if args.camera_position is not None else None,
    )
    write_png(render_window, args.output_png)
    print(f"rendered {args.output_png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
