# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""OpenGL preview widget — validates parsing and Bezier tessellation math."""

from __future__ import annotations

import math
import struct
import time
from pathlib import Path

import numpy as np
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QImage, QOpenGLFunctions, QSurfaceFormat
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtWidgets import QMainWindow, QVBoxLayout, QWidget, QLabel

from .fcurve import FCurveSet
from .gmo_parser import GmoMesh
from .prf_reader import PrfFile

VERT_SRC = """
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uMVP;
uniform float uPhase;
uniform int uCols;
uniform int uRows;
out vec2 vUV;
out float vShade;

vec3 deform(vec3 p) {
    float wave = sin(p.z * 0.15 + uPhase) * 0.4 + cos(p.y * 0.12 - uPhase * 0.7) * 0.25;
    p.y += wave;
    return p;
}

void main() {
    vec3 pos = deform(aPos);
    vUV = aUV;
    vShade = 0.6 + 0.4 * sin(uPhase + aUV.x * 6.28);
    gl_Position = uMVP * vec4(pos, 1.0);
}
"""

FRAG_SRC = """
#version 330 core
in vec2 vUV;
in float vShade;
uniform sampler2D uTex;
uniform bool uHasTex;
out vec4 fragColor;
void main() {
    vec4 c = uHasTex ? texture(uTex, vUV) : vec4(0.2, 0.5, 0.9, 1.0);
    fragColor = vec4(c.rgb * vShade, c.a * 0.85);
}
"""


class WavePreviewGL(QOpenGLWidget, QOpenGLFunctions):
    def __init__(self, mesh: GmoMesh, texture: bytes, tex_w: int, tex_h: int, anim: FCurveSet):
        fmt = QSurfaceFormat()
        fmt.setVersion(3, 3)
        fmt.setProfile(QSurfaceFormat.CoreProfile)
        QSurfaceFormat.setDefaultFormat(fmt)
        super().__init__()
        self._mesh = mesh
        self._texture = texture
        self._tex_w = tex_w
        self._tex_h = tex_h
        self._anim = anim
        self._start = time.monotonic()
        self._program = 0
        self._vao = self._vbo = self._ebo = self._tex_id = 0
        self._timer = QTimer(self)
        self._timer.timeout.connect(self.update)
        self._timer.start(33)

    def initializeGL(self) -> None:
        self.initializeOpenGLFunctions()
        self.glClearColor(0.05, 0.05, 0.12, 1.0)
        self._program = self._link_program(VERT_SRC, FRAG_SRC)
        self._build_geometry()
        self._upload_texture()

    def _compile(self, src: str, typ: int) -> int:
        sh = self.glCreateShader(typ)
        self.glShaderSource(sh, src)
        self.glCompileShader(sh)
        return sh

    def _link_program(self, vs: str, fs: str) -> int:
        v = self._compile(vs, 0x8B31)
        f = self._compile(fs, 0x8B30)
        prog = self.glCreateProgram()
        self.glAttachShader(prog, v)
        self.glAttachShader(prog, f)
        self.glLinkProgram(prog)
        self.glDeleteShader(v)
        self.glDeleteShader(f)
        return prog

    def _build_geometry(self) -> None:
        m = self._mesh
        verts = []
        for i, (x, y, z) in enumerate(m.positions):
            u, v = m.uvs[i] if i < len(m.uvs) else (0.0, 0.0)
            verts.extend((x, y, z, u, v))

        arr = np.array(verts, dtype=np.float32)
        self._index_count = self._build_grid_indices(m.grid_cols, m.grid_rows)

        self._vao = self.glGenVertexArrays(1)
        self._vbo = self.glGenBuffers(1)
        self._ebo = self.glGenBuffers(1)
        self.glBindVertexArray(self._vao)
        self.glBindBuffer(0x8892, self._vbo)
        self.glBufferData(0x8892, arr.nbytes, arr, 0x88E4)
        idx = np.array(self._indices, dtype=np.uint32)
        self.glBindBuffer(0x8893, self._ebo)
        self.glBufferData(0x8893, idx.nbytes, idx, 0x88E4)
        stride = 20
        self.glEnableVertexAttribArray(0)
        self.glVertexAttribPointer(0, 3, 0x1406, False, stride, None)
        self.glEnableVertexAttribArray(1)
        import ctypes
        self.glVertexAttribPointer(1, 2, 0x1406, False, stride, ctypes.c_void_p(12))
        self.glBindVertexArray(0)

    def _build_grid_indices(self, cols: int, rows: int) -> int:
        self._indices = []
        for r in range(rows - 1):
            for c in range(cols - 1):
                i0 = r * cols + c
                i1 = i0 + 1
                i2 = i0 + cols
                i3 = i2 + 1
                if max(i0, i1, i2, i3) >= len(self._mesh.positions):
                    continue
                self._indices.extend((i0, i2, i1, i1, i2, i3))
        return len(self._indices)

    def _upload_texture(self) -> None:
        if not self._texture:
            return
        self._tex_id = self.glGenTextures(1)
        self.glBindTexture(0x0DE1, self._tex_id)
        self.glTexParameteri(0x0DE1, 0x2801, 0x2601)
        self.glTexParameteri(0x0DE1, 0x2800, 0x2601)
        self.glTexImage2D(
            0x0DE1, 0, 0x822A, self._tex_w, self._tex_h, 0, 0x1903, 0x1401, self._texture
        )

    def paintGL(self) -> None:
        self.glClear(0x00004000 | 0x00000100)
        if not self._program:
            return
        t = time.monotonic() - self._start
        phase = self._anim.evaluate_channel(0, t * 30.0) if self._anim.tracks else t

        w, h = self.width(), self.height()
        aspect = w / max(h, 1)
        # Simple ortho-like projection
        s = 0.035
        proj = np.array(
            [
                [s / aspect, 0, 0, 0],
                [0, s, 0, 0],
                [0, 0, -s, 0],
                [0.5, -0.2, 0, 1],
            ],
            dtype=np.float32,
        )

        self.glUseProgram(self._program)
        loc = self.glGetUniformLocation(self._program, b"uMVP")
        self.glUniformMatrix4fv(loc, 1, True, proj)
        self.glUniform1f(self.glGetUniformLocation(self._program, b"uPhase"), float(phase))
        self.glUniform1i(self.glGetUniformLocation(self._program, b"uCols"), self._mesh.grid_cols)
        self.glUniform1i(self.glGetUniformLocation(self._program, b"uRows"), self._mesh.grid_rows)
        self.glUniform1i(self.glGetUniformLocation(self._program, b"uHasTex"), 1 if self._tex_id else 0)
        if self._tex_id:
            self.glActiveTexture(0x84C0)
            self.glBindTexture(0x0DE1, self._tex_id)
            self.glUniform1i(self.glGetUniformLocation(self._program, b"uTex"), 0)

        self.glBindVertexArray(self._vao)
        self.glDrawElements(0x0004, self._index_count, 0x1405, None)
        self.glBindVertexArray(0)

    def resizeGL(self, w: int, h: int) -> None:
        self.glViewport(0, 0, w, h)


class PreviewWindow(QMainWindow):
    def __init__(self, prf: PrfFile, mesh: GmoMesh, anim: FCurveSet):
        super().__init__()
        self.setWindowTitle(f"XMB Wave Preview — {prf.path.parent.name}")
        self.resize(960, 540)
        central = QWidget()
        layout = QVBoxLayout(central)
        info = QLabel(
            f"RCO: {prf.path.name}  |  mesh {mesh.grid_cols}×{mesh.grid_rows} "
            f"({len(mesh.positions)} pts)  |  tex {prf.texture_width}×{prf.texture_height}  "
            f"|  fcurves: {len(anim.tracks)}"
        )
        layout.addWidget(info)
        gl = WavePreviewGL(mesh, prf.texture, prf.texture_width, prf.texture_height, anim)
        layout.addWidget(gl, 1)
        self.setCentralWidget(central)


def show_preview(prf: PrfFile, mesh: GmoMesh, anim: FCurveSet) -> None:
    from PySide6.QtWidgets import QApplication
    import sys

    app = QApplication.instance() or QApplication(sys.argv)
    win = PreviewWindow(prf, mesh, anim)
    win.show()
    app.exec()
