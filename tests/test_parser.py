import pytest

from levoit_uart_parser import (
    auto_mode_payload,
    build_frame,
    decode_status_payload,
    night_light_payload,
    parse_frame,
    power_payload,
)


def test_known_power_frame():
    assert build_frame(0x22, 0x06, power_payload(True)) == bytes.fromhex(
        "A5 22 06 05 00 8B 01 00 A0 00 01"
    )


def test_parse_rejects_bad_checksum():
    raw = bytearray(build_frame(0x22, 0x06, power_payload(True)))
    raw[5] ^= 1
    with pytest.raises(ValueError, match="checksum"):
        parse_frame(bytes(raw))


def test_confirmed_light_levels_only():
    assert night_light_payload(50).hex(" ") == "01 03 a0 00 01 32"
    with pytest.raises(ValueError, match="confirmed brightness"):
        night_light_payload(25)


def test_auto_target_deadband():
    assert auto_mode_payload(63).hex(" ") == "01 80 40 00 3f 3a 44 09 05 01"


def test_decode_captured_status():
    payload = bytes.fromhex(
        "01 85 40 00 00 00 02 01 00 00 01 00 01 23 35 18 01 08 32 00"
    )
    status = decode_status_payload(payload)
    assert status["power"] is True
    assert status["tank_lifted"] is False
    assert status["target_humidity_percent"] == 35
    assert status["current_humidity_percent"] == 53
    assert status["temperature_celsius"] == 24
    assert status["mode"] == "manual"
    assert status["manual_mist_level"] == 8
    assert status["night_light_brightness_percent"] == 50


def test_decode_preserves_unknown_mode_value():
    payload = bytearray.fromhex(
        "01 85 40 00 00 00 02 01 00 00 01 00 01 23 35 18 01 08 32 00"
    )
    payload[16] = 2
    status = decode_status_payload(bytes(payload))
    assert status["mode"] == "unknown_2"
