# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
"""fcurve animation parser — PSP loop timing from RCO embedded tracks."""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field


@dataclass
class FCurveKeyframe:
    time: float
    value: float
    in_tangent: float = 0.0
    out_tangent: float = 0.0


@dataclass
class FCurveTrack:
    name: str
    channel: int
    duration: float
    keys: list[FCurveKeyframe] = field(default_factory=list)

    def evaluate(self, t: float) -> float:
        if not self.keys:
            return 0.0
        if len(self.keys) == 1:
            return self.keys[0].value

        duration = self.duration if self.duration > 0 else 1501.0
        t = t % duration
        keys = self.keys

        for i in range(len(keys) - 1):
            k0, k1 = keys[i], keys[i + 1]
            if k0.time <= t <= k1.time:
                span = k1.time - k0.time
                if span <= 0:
                    return k1.value
                u = (t - k0.time) / span
                # Hermite-style with tangents
                u2 = u * u
                u3 = u2 * u
                h00 = 2 * u3 - 3 * u2 + 1
                h10 = u3 - 2 * u2 + u
                h01 = -2 * u3 + 3 * u2
                h11 = u3 - u2
                return (
                    h00 * k0.value
                    + h10 * span * k0.out_tangent
                    + h01 * k1.value
                    + h11 * span * k1.in_tangent
                )

        return keys[-1].value


@dataclass
class FCurveSet:
    tracks: list[FCurveTrack] = field(default_factory=list)
    loop_duration: float = 1501.0

    def evaluate_channel(self, channel: int, t: float) -> float:
        for tr in self.tracks:
            if tr.channel == channel:
                return tr.evaluate(t)
        return 0.0

    def to_anim_json(self) -> str:
        data = {
            "loop_duration": self.loop_duration,
            "tracks": [
                {
                    "name": tr.name,
                    "channel": tr.channel,
                    "duration": tr.duration,
                    "keys": [
                        {
                            "time": k.time,
                            "value": k.value,
                            "in": k.in_tangent,
                            "out": k.out_tangent,
                        }
                        for k in tr.keys
                    ],
                }
                for tr in self.tracks
            ],
        }
        return json.dumps(data, indent=2)


def _parse_track_blob(name: str, blob: bytes) -> FCurveTrack:
    if len(blob) < 12:
        return FCurveTrack(name=name, channel=0, duration=1501.0)

    num_keys = struct.unpack_from("<I", blob, 0)[0]
    channel = struct.unpack_from("<I", blob, 4)[0]
    duration = float(struct.unpack_from("<I", blob, 8)[0] or 1501)

    keys: list[FCurveKeyframe] = []
    p = 20  # skip header/padding region used by PSP exporter

    if num_keys <= 1:
        # Single-key + baked curve samples as float pairs
        while p + 8 <= len(blob):
            a, b = struct.unpack_from("<2f", blob, p)
            if abs(a) > 1e6 or abs(b) > 1e6:
                p += 4
                continue
            keys.append(FCurveKeyframe(time=float(len(keys)), value=b, out_tangent=0.0))
            p += 8
            if len(keys) >= 64:
                break
        # Normalize times to duration
        if len(keys) > 1:
            for i, k in enumerate(keys):
                k.time = (i / (len(keys) - 1)) * duration
    else:
        for i in range(min(num_keys, 64)):
            if p + 16 > len(blob):
                break
            time, val, in_t, out_t = struct.unpack_from("<4f", blob, p)
            keys.append(FCurveKeyframe(time, val, in_t, out_t))
            p += 16

    if not keys:
        keys.append(FCurveKeyframe(0.0, 0.0))

    return FCurveTrack(name=name, channel=channel, duration=duration, keys=keys)


def parse_fcurves(blobs: dict[str, bytes]) -> FCurveSet:
    tracks = [_parse_track_blob(name, blob) for name, blob in sorted(blobs.items())]
    duration = tracks[0].duration if tracks else 1501.0
    return FCurveSet(tracks=tracks, loop_duration=duration)
