from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEVICE_BUILDER_CONFIG = ROOT / "levoit-vesync-classic-300s-humidifier.yaml"
README = ROOT / "README.md"


def test_device_builder_config_is_the_single_copy_paste_source() -> None:
    sample = DEVICE_BUILDER_CONFIG.read_text()
    readme = README.read_text()

    assert (
        "github://MikeG3D2/esphome-levoit-humidifier@main" in sample
    ), "the Device Builder sample must fetch the published component"
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
    assert 'options=["Auto", "Manual"]' in component_schema
    assert 'options=["Auto", "Manual", "Unknown"]' not in component_schema
    assert 'publish_state("Unknown")' not in component_cpp
