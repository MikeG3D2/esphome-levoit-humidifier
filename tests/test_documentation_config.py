import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEVICE_BUILDER_CONFIG = ROOT / "levoit-vesync-classic-300s-humidifier.yaml"
README = ROOT / "README.md"


def test_device_builder_config_is_the_single_copy_paste_source() -> None:
    sample = DEVICE_BUILDER_CONFIG.read_text()
    readme = README.read_text()

    assert "github://MikeG3D2/esphome-levoit-humidifier@main" in sample, (
        "the Device Builder sample must fetch the published component"
    )
    assert "type: local" not in sample
    assert "path: components" not in sample
    assert "variant: esp32" in sample
    assert "board:" not in sample

    # These advanced diagnostics use defaults or remain optional; keep the main
    # copy-paste configuration focused on end-user controls and sensors.
    assert "status_response_timeout:" not in sample
    assert "communication_problem:" not in sample

    assert "## ESPHome Device Builder configuration" in readme
    assert "## External component source" not in readme
    assert "\n## Configuration\n" not in readme


def test_user_config_exposes_captured_water_and_mist_states() -> None:
    sample = DEVICE_BUILDER_CONFIG.read_text()
    component_schema = (ROOT / "components/levoit_classic_300s/__init__.py").read_text()
    component_cpp = (
        ROOT / "components/levoit_classic_300s/levoit_classic_300s.cpp"
    ).read_text()

    assert "manual_mist_level:" in sample
    assert "no_water:" in sample
    assert 'CONF_MANUAL_MIST_LEVEL = "manual_mist_level"' in component_schema
    assert 'CONF_NO_WATER = "no_water"' in component_schema
    assert 'options=["Auto", "Manual", "Sleep"]' in component_schema
    assert 'options=["Auto", "Manual", "Unknown"]' not in component_schema
    assert 'publish_state("Unknown")' not in component_cpp


def test_user_config_exposes_statically_recovered_controls() -> None:
    sample = DEVICE_BUILDER_CONFIG.read_text()
    component_schema = (ROOT / "components/levoit_classic_300s/__init__.py").read_text()
    component_cpp = (
        ROOT / "components/levoit_classic_300s/levoit_classic_300s.cpp"
    ).read_text()

    assert "display:" in sample
    assert "auto_stop:" in sample
    assert 'CONF_DISPLAY = "display"' in component_schema
    assert 'CONF_AUTO_STOP = "auto_stop"' in component_schema
    assert "set_sleep_mode(0)" in component_cpp
    assert "set_target_humidity(target)" in component_cpp


def test_sensitive_stock_dump_is_ignored() -> None:
    ignore_rules = (ROOT / ".gitignore").read_text().splitlines()
    assert "*.bin" in ignore_rules


def test_protocol_artifacts_are_organized_and_current() -> None:
    notes = ROOT / "docs/protocol/levoit-classic-300s-uart.md"
    mapping_path = ROOT / "docs/protocol/levoit-classic-300s-uart-mapping.json"
    scaffold = ROOT / "tools/levoit_uart_parser.py"

    assert notes.is_file()
    assert mapping_path.is_file()
    assert scaffold.is_file()
    assert not (ROOT / "levoit_humidifier_uart_protocol_notes.md").exists()
    assert not (ROOT / "levoit_humidifier_uart_mapping.json").exists()
    assert not (ROOT / "levoit_uart_parser.py").exists()

    mapping = json.loads(mapping_path.read_text())
    assert mapping["status_report"]["fields"]["D6"]["meaning"] == (
        "no_water_magnetic_float_open"
    )
    assert mapping["warnings"]["no_water"] == {
        "field": "D6",
        "active_value": 1,
        "confidence": "confirmed",
    }
    assert mapping["commands"]["A260_manual_mist"]["observed_levels"] == list(
        range(1, 10)
    )
    display_toggle = mapping["observations"]["display_toggle"]
    assert display_toggle["result"] == "D13_0x02_is_sleep_mode"
    assert display_toggle["confidence"] == "confirmed"
    assert mapping["status_report"]["fields"]["D13"]["values"]["2"] == ("sleep")
