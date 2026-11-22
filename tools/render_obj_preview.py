#!/usr/bin/env python3
"""Renderizador Blender minimo para revisar a silhueta de uma malha OBJ."""

import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector


def olhar(obj, alvo):
    obj.rotation_euler = (Vector(alvo) - obj.location).to_track_quat("-Z", "Y").to_euler()


args = sys.argv[sys.argv.index("--") + 1 :]
entrada, saida = Path(args[0]), Path(args[1])
angulo = math.radians(float(args[2]) if len(args) > 2 else 20.0)

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.wm.obj_import(filepath=str(entrada), forward_axis="NEGATIVE_Z", up_axis="Y")
humano = bpy.context.selected_objects[0]
for poligono in humano.data.polygons:
    poligono.use_smooth = True

mat = bpy.data.materials.new("argila")
mat.diffuse_color = (0.52, 0.54, 0.57, 1.0)
mat.roughness = 0.72
humano.data.materials.append(mat)

alvo = Vector((0, 0, 0.92))
dist = 3.75
cam_data = bpy.data.cameras.new("camera")
camera = bpy.data.objects.new("camera", cam_data)
bpy.context.collection.objects.link(camera)
camera.location = (math.sin(angulo) * dist, -math.cos(angulo) * dist, 0.98)
camera.data.lens = 62
olhar(camera, alvo)
bpy.context.scene.camera = camera

for nome, energia, tamanho, pos in (
    ("principal", 650, 4.0, (-2.0, -3.0, 3.4)),
    ("recorte", 450, 3.0, (2.7, 1.5, 2.3)),
    ("preenchimento", 250, 4.5, (2.6, -2.0, 1.0)),
):
    dados = bpy.data.lights.new(nome, "AREA")
    dados.energy, dados.shape, dados.size = energia, "DISK", tamanho
    luz = bpy.data.objects.new(nome, dados)
    luz.location = pos
    bpy.context.collection.objects.link(luz)
    olhar(luz, alvo)

mundo = bpy.context.scene.world
mundo.color = (0.025, 0.025, 0.035)
cena = bpy.context.scene
cena.render.engine = "BLENDER_EEVEE_NEXT"
cena.render.resolution_x, cena.render.resolution_y = 900, 900
cena.render.resolution_percentage = 100
cena.render.image_settings.file_format = "PNG"
cena.render.filepath = str(saida)
cena.view_settings.look = "AgX - Medium High Contrast"
bpy.ops.render.render(write_still=True)
