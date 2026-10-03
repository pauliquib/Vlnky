#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare plugin screenshots against PSP reference images using SSIM."""

from __future__ import annotations

import argparse
import html
import sys
from datetime import datetime
from pathlib import Path

try:
    import cv2
    import numpy as np
except ImportError:
    print("Requires: pip install opencv-python numpy", file=sys.stderr)
    sys.exit(1)


def load_rgba(path: Path) -> np.ndarray:
    img = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
    if img is None:
        raise FileNotFoundError(path)
    if img.ndim == 2:
        img = cv2.cvtColor(img, cv2.COLOR_GRAY2BGRA)
    elif img.shape[2] == 3:
        img = cv2.cvtColor(img, cv2.COLOR_BGR2BGRA)
    return img


def to_gray(img: np.ndarray) -> np.ndarray:
    if img.shape[2] == 4:
        rgb = cv2.cvtColor(img, cv2.COLOR_BGRA2BGR)
    else:
        rgb = img
    return cv2.cvtColor(rgb, cv2.COLOR_BGR2GRAY)


def resize_match(a: np.ndarray, b: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    if a.shape[:2] == b.shape[:2]:
        return a, b
    h = min(a.shape[0], b.shape[0])
    w = min(a.shape[1], b.shape[1])
    return cv2.resize(a, (w, h)), cv2.resize(b, (w, h))


def ssim_score(a: np.ndarray, b: np.ndarray) -> float:
    a, b = resize_match(a, b)
    ga, gb = to_gray(a), to_gray(b)
    ga = ga.astype(np.float64)
    gb = gb.astype(np.float64)
    c1 = (0.01 * 255) ** 2
    c2 = (0.03 * 255) ** 2
    mu_a = cv2.GaussianBlur(ga, (11, 11), 1.5)
    mu_b = cv2.GaussianBlur(gb, (11, 11), 1.5)
    mu_a_sq = mu_a * mu_a
    mu_b_sq = mu_b * mu_b
    mu_ab = mu_a * mu_b
    sigma_a_sq = cv2.GaussianBlur(ga * ga, (11, 11), 1.5) - mu_a_sq
    sigma_b_sq = cv2.GaussianBlur(gb * gb, (11, 11), 1.5) - mu_b_sq
    sigma_ab = cv2.GaussianBlur(ga * gb, (11, 11), 1.5) - mu_ab
    num = (2 * mu_ab + c1) * (2 * sigma_ab + c2)
    den = (mu_a_sq + mu_b_sq + c1) * (sigma_a_sq + sigma_b_sq + c2)
    return float(np.mean(num / (den + 1e-9)))


def diff_highlight(ref: np.ndarray, test: np.ndarray, threshold: float = 25.0) -> np.ndarray:
    ref, test = resize_match(ref, test)
    ref_rgb = ref[:, :, :3].astype(np.float32)
    test_rgb = test[:, :, :3].astype(np.float32)
    diff = np.abs(ref_rgb - test_rgb)
    mask = np.max(diff, axis=2) > threshold
    out = test_rgb.copy()
    out[mask] = out[mask] * 0.35 + np.array([0, 0, 255], dtype=np.float32) * 0.65
    if ref.shape[2] == 4:
        alpha = ref[:, :, 3:4]
        out = np.concatenate([out, alpha], axis=2)
    return out.astype(np.uint8)


def find_pairs(ref_dir: Path, test_dir: Path) -> list[tuple[str, Path, Path]]:
    refs = {p.stem.lower(): p for p in ref_dir.glob("*") if p.suffix.lower() in {".png", ".jpg", ".jpeg"}}
    tests = {p.stem.lower(): p for p in test_dir.glob("*") if p.suffix.lower() in {".png", ".jpg", ".jpeg"}}
    pairs: list[tuple[str, Path, Path]] = []
    for name, ref_path in sorted(refs.items()):
        if name in tests:
            pairs.append((name, ref_path, tests[name]))
        elif name + "_final" in tests:
            pairs.append((name, ref_path, tests[name + "_final"]))
    if not pairs and test_dir.is_file():
        for name, ref_path in sorted(refs.items()):
            pairs.append((name, ref_path, test_dir))
    return pairs


def write_report(out_dir: Path, rows: list[dict]) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    report = out_dir / "comparison_report.html"
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    parts = [
        "<!DOCTYPE html><html><head><meta charset='utf-8'>",
        "<title>XMB Wave SSIM Report</title>",
        "<style>body{font-family:sans-serif;background:#0a1020;color:#dce6ff;padding:16px}",
        "table{border-collapse:collapse;width:100%}td,th{border:1px solid #284060;padding:8px}",
        "img{max-width:320px;background:#000}.bad{color:#ff8888}.good{color:#88ffaa}</style></head><body>",
        f"<h1>Vlnky vs PSP wave comparison</h1><p>Generated {html.escape(ts)}</p>",
        "<table><tr><th>Name</th><th>SSIM</th><th>Reference</th><th>Plugin</th><th>Diff</th></tr>",
    ]
    for row in rows:
        cls = "good" if row["ssim"] >= 0.85 else "bad"
        parts.append(
            f"<tr><td>{html.escape(row['name'])}</td>"
            f"<td class='{cls}'>{row['ssim']:.4f}</td>"
            f"<td><img src='{html.escape(row['ref_rel'])}'></td>"
            f"<td><img src='{html.escape(row['test_rel'])}'></td>"
            f"<td><img src='{html.escape(row['diff_rel'])}'></td></tr>"
        )
    parts.append("</table></body></html>")
    report.write_text("".join(parts), encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_dir", type=Path, help="Directory of PSP reference screenshots")
    parser.add_argument("test_dir", type=Path, help="Plugin screenshot directory or single PNG")
    parser.add_argument("--output", type=Path, default=Path.home() / ".cache/vlnky/comparisons")
    args = parser.parse_args()

    if not args.reference_dir.is_dir():
        print(f"Reference directory not found: {args.reference_dir}", file=sys.stderr)
        return 1
    if not args.test_dir.exists():
        print(f"Test path not found: {args.test_dir}", file=sys.stderr)
        return 1

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_dir = args.output / stamp
    out_dir.mkdir(parents=True, exist_ok=True)

    pairs = find_pairs(args.reference_dir, args.test_dir)
    if not pairs:
        print("No matching image pairs found (match by filename stem).", file=sys.stderr)
        return 2

    rows: list[dict] = []
    for name, ref_path, test_path in pairs:
        ref = load_rgba(ref_path)
        test = load_rgba(test_path)
        score = ssim_score(ref, test)
        diff = diff_highlight(ref, test)

        ref_copy = out_dir / f"{name}_ref.png"
        test_copy = out_dir / f"{name}_test.png"
        diff_copy = out_dir / f"{name}_diff.png"
        cv2.imwrite(str(ref_copy), ref)
        cv2.imwrite(str(test_copy), test)
        cv2.imwrite(str(diff_copy), diff)

        rows.append({
            "name": name,
            "ssim": score,
            "ref_rel": ref_copy.name,
            "test_rel": test_copy.name,
            "diff_rel": diff_copy.name,
        })
        print(f"{name}: SSIM={score:.4f}")

    report = write_report(out_dir, rows)
    print(f"Report: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
