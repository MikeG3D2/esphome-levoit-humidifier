"""Minimal parser/encoder scaffold for the captured Levoit UART protocol."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable


@dataclass(frozen=True)
class Frame:
    frame_type: int
    sequence: int
    payload: bytes


def checksum_for(parts: Iterable[int]) -> int:
    """Return checksum so that the complete frame sums to 0xFF modulo 256."""
    return (0xFF - (sum(parts) & 0xFF)) & 0xFF


def build_frame(frame_type: int, sequence: int, payload: bytes) -> bytes:
    if not (0 <= frame_type <= 0xFF and 0 <= sequence <= 0xFF):
        raise ValueError("frame_type and sequence must fit in one byte")
    if len(payload) > 0xFFFF:
        raise ValueError("payload too large")

    header_without_checksum = bytes(
        [0xA5, frame_type, sequence, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]
    )
    checksum = checksum_for(header_without_checksum + payload)
    return header_without_checksum + bytes([checksum]) + payload


def parse_frame(raw: bytes) -> Frame:
    if len(raw) < 6:
        raise ValueError("frame too short")
    if raw[0] != 0xA5:
        raise ValueError("bad preamble")

    payload_len = raw[3] | (raw[4] << 8)
    expected_len = 6 + payload_len
    if len(raw) != expected_len:
        raise ValueError(f"length mismatch: expected {expected_len}, got {len(raw)}")
    if (sum(raw) & 0xFF) != 0xFF:
        raise ValueError("checksum failure")

    return Frame(frame_type=raw[1], sequence=raw[2], payload=raw[6:])


def power_payload(on: bool) -> bytes:
    return bytes([0x01, 0x00, 0xA0, 0x00, int(on)])


def night_light_payload(percent: int) -> bytes:
    if percent not in (0, 50, 100):
        raise ValueError("confirmed brightness values are 0, 50, and 100")
    return bytes([0x01, 0x03, 0xA0, 0x00, 0x01, percent])


def manual_mist_payload(level: int) -> bytes:
    if not (0 <= level <= 0xFF):
        raise ValueError("level must fit in one byte")
    return bytes([0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, level])


def auto_mode_payload(target_humidity: int) -> bytes:
    if not (5 <= target_humidity <= 250):
        raise ValueError("target must allow target-5 and target+5 to fit")
    return bytes([
        0x01, 0x80, 0x40, 0x00,
        target_humidity,
        target_humidity - 5,
        target_humidity + 5,
        0x09, 0x05, 0x01,
    ])


def status_request_payload() -> bytes:
    return bytes([0x01, 0x84, 0x40, 0x00])


def decode_status_payload(payload: bytes) -> dict[str, int | str | bool | None]:
    if len(payload) != 20 or payload[:3] != bytes([0x01, 0x85, 0x40]):
        raise ValueError("not a known 20-byte status payload")

    d = payload[3:]
    return {
        "power": bool(d[4]),
        "tank_lifted": bool(d[5]),
        "target_humidity_percent": d[10],
        "current_humidity_percent": d[11],
        "temperature_celsius": d[12],
        "mode": {0: "auto", 1: "manual"}.get(d[13], f"unknown_{d[13]}"),
        "manual_mist_level": d[14],
        "night_light_brightness_percent": d[15],
        "out_of_water": None,  # Not mapped yet.
        "raw_state_bytes": d.hex(" "),
    }
