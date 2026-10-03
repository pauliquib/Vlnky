# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""Golden-file parser tests against representative RCO waves."""

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from xmb_wave.fcurve import parse_fcurves
from xmb_wave.gmo_parser import parse_gmo
from xmb_wave.prf_reader import load_rco

COLLECTION = Path(
    os.environ.get(
        "VLNKY_WAVE_COLLECTION",
        Path(__file__).resolve().parents[3] / "176 XMB WAVES for 5.00" / "176 XMB WAVES",
    )
)
GOLDEN = ("Alice", "aquadark", "Blank", "1up", "Blue Wire")


@unittest.skipUnless(COLLECTION.is_dir(), "wave collection not present")
class GoldenRcoTests(unittest.TestCase):
    def test_golden_waves_parse(self) -> None:
        for name in GOLDEN:
            rco = COLLECTION / name / "system_plugin_bg.rco"
            with self.subTest(wave=name):
                self.assertTrue(rco.is_file(), f"missing {rco}")
                prf = load_rco(rco)
                self.assertEqual(prf.version, 0x71)
                self.assertGreater(prf.gmo_offset, 0)
                self.assertGreater(len(prf.texture), 1000)
                mesh = parse_gmo(prf.data, prf.gmo_offset)
                self.assertGreater(len(mesh.positions), 100)
                anim = parse_fcurves(prf.fcurve_blobs)
                self.assertGreaterEqual(len(anim.tracks), 1)

    def test_shared_gmo_hash_large_rco(self) -> None:
        hashes = set()
        for name in ("Alice", "Blue Wire", "1up"):
            rco = COLLECTION / name / "system_plugin_bg.rco"
            if not rco.is_file():
                continue
            prf = load_rco(rco)
            if rco.stat().st_size > 100_000:
                hashes.add(prf.gmo_hash)
        self.assertEqual(len(hashes), 1, "large RCO files should share GMO mesh hash")


if __name__ == "__main__":
    unittest.main()
