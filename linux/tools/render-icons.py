#!/usr/bin/env python3
"""Render the original vector icon to the packaged freedesktop sizes.

Requires the system's Python GObject bindings, librsvg and pycairo.
"""
from pathlib import Path

import cairo
import gi

gi.require_version('Rsvg', '2.0')
from gi.repository import Rsvg

assets = Path(__file__).resolve().parents[1] / 'assets'
icon = Rsvg.Handle.new_from_file(str(assets / 'pt.notemd.NoteMD.svg'))
for size in (32, 48, 64, 128, 256, 512, 1024):
    directory = assets / 'icons' / f'{size}x{size}'
    directory.mkdir(parents=True, exist_ok=True)
    surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, size, size)
    context = cairo.Context(surface)
    bounds = Rsvg.Rectangle()
    bounds.x = bounds.y = 0
    bounds.width = bounds.height = size
    icon.render_document(context, bounds)
    surface.write_to_png(str(directory / 'pt.notemd.NoteMD.png'))
