# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import dtmap
import fdt
from fdt import DtError

REPO = Path(__file__).resolve().parents[3]


def dtc():
    if shutil.which("dtc") is None:
        if os.environ.get("CI"):
            pytest.fail("needs dtc")
        pytest.skip("needs dtc")
    return "dtc"


CLASSES = {
    "clk": {
        "class": "clk",
        "type": "struct clk_ctrl",
        "include": "clk.h",
        "provides": {"clock": "CLK(&{sym}, {0})"},
    },
    "intc": {
        "class": "intc",
        "type": "struct intc",
        "include": "intc.h",
        "provides": {"irq": "IRQ(&{sym}, {0})"},
    },
    "pinctrl": {"class": "pinctrl", "type": "struct pin_state", "include": "pinctrl.h"},
    "bus": {
        "class": "bus",
        "type": "struct bus",
        "include": "bus.h",
        "provides": {"bus": "&{sym}"},
    },
    "sensor": {"class": "sensor", "type": "struct sensor", "include": "sensor.h"},
    "gpio": {
        "class": "gpio",
        "type": "struct port",
        "include": "gpio.h",
        "provides": {"gpio": "PIN(&{sym}, {0}, {1})"},
    },
}

SPECS = {
    "acme,ctrl": {
        "compatible": "acme,ctrl",
        "config": "CTRL",
        "class": ["clk", "intc"],
        "symbol": {"clk": "acme_clk", "intc": "acme_intc"},
    },
    "acme,pinctrl": {
        "compatible": "acme,pinctrl",
        "config": "PINCTRL",
        "class": "pinctrl",
        "pinctrl": {
            "define": "PIN_STATE",
            "pin": "{pin}_{function}",
            "flags": {"input-enable": "INPUT"},
            "values": {"drive-strength": "DRIVE_{value}"},
        },
    },
    "acme,bus": {
        "compatible": "acme,bus",
        "config": "BUS",
        "class": "bus",
        "define": "BUS_DT_DEFINE",
        "args": [
            {"name": True},
            {"reg": 0},
            {"irq": 0},
            {"clock": 0},
            {"prop": "clock-frequency", "default": 100},
            {"pinctrl": "default", "default": "nullptr"},
        ],
    },
    "acme,sensor": {
        "compatible": "acme,sensor",
        "config": "SENSOR",
        "class": "sensor",
        "define": "SENSOR_DT_DEFINE",
        "args": [
            {"parent": "bus"},
            {"reg": 0},
            {
                "strings": "mount-matrix",
                "default": [1, 0],
                "wrap": "MAP",
                "field": "axes",
            },
        ],
    },
}

DTS = """
/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;
    ctrl: ctrl@1000 {
        compatible = "acme,ctrl";
        reg = <0x1000 0x100>;
        #clock-cells = <1>;
        #interrupt-cells = <1>;
        interrupt-controller;
    };
    pinctrl: pinctrl@1100 {
        compatible = "acme,pinctrl";
        reg = <0x1100 0x100>;
        bus_default: state {
            scl {
                pins = "PA1";
                function = "scl";
                input-enable;
                drive-strength = <8>;
            };
        };
        unused_default: unused {
            scl {
                pins = "PA2";
                function = "scl";
            };
        };
    };
    bus: bus@2000 {
        compatible = "acme,bus";
        reg = <0x2000 0x100>;
        interrupt-parent = <&ctrl>;
        interrupts = <7>;
        clocks = <&ctrl 3>;
        pinctrl-0 = <&bus_default>;
        pinctrl-names = "default";
        clock-frequency = <400>;
        %(bus)s
        #address-cells = <1>;
        #size-cells = <0>;
        sensor: sensor@30 {
            compatible = "acme,sensor";
            reg = <0x30>;
            mount-matrix = "-1", "0";
            %(sensor)s
        };
    };
};
"""

ALL = ("CTRL", "PINCTRL", "BUS", "SENSOR")


def spec(name, data=None, include_dir=None):
    return dtmap.Spec(
        name, data if data is not None else SPECS[name], CLASSES, include_dir
    )


def generate(tmp_path, config=ALL, bus="", sensor="", specs=None, dts=DTS):
    path = tmp_path / "t.dts"
    path.write_text(dts % {"bus": bus, "sensor": sensor})
    dtb = tmp_path / "t.dtb"
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {name: spec(name) for name in SPECS}
    loaded.update(specs or {})
    gen = dtmap.Generator(fdt.load(dtb), loaded, set(config))
    gen.run()
    return gen


def body(gen):
    return "\n".join(line for out in gen.outputs.values() for line in out.body)


def instances(gen, name):
    return gen.instances()[f"devicetree/instances/{name}.inc"]


def test_instances_go_to_their_driver(tmp_path):
    gen = generate(tmp_path)
    bus = instances(gen, "acme_bus")
    sensor = instances(gen, "acme_sensor")
    assert (
        'BUS_DT_DEFINE(dt_bus, "bus", 0x2000, IRQ(&acme_intc, 7), CLK(&acme_clk, 3), 400, '
        "&dt_bus_default);" in bus
    )
    assert "SENSOR" not in bus
    assert "SENSOR_DT_DEFINE(dt_sensor, &dt_bus, 0x30, .axes = MAP(-1, 0));" in sensor


def test_instance_files_only_include_class_headers(tmp_path):
    gen = generate(tmp_path)
    assert set(re.findall(r"#include <(.+)>", instances(gen, "acme_bus"))) == {
        "clk.h",
        "intc.h",
        "devicetree/devices.h",
    }
    assert set(re.findall(r"#include <(.+)>", instances(gen, "acme_sensor"))) == {
        "bus.h",
        "devicetree/devices.h",
    }


def test_specs_hold_no_policy():
    for key in ("include", "type", "irqs", "export"):
        with pytest.raises(DtError, match="unknown"):
            spec("spec", {"compatible": "a,b", "config": "X", key: "x"})


def test_pin_states_go_to_the_pin_controller(tmp_path):
    gen = generate(tmp_path)
    pinctrl = instances(gen, "acme_pinctrl")
    assert "PIN_STATE(dt_bus_default, PA1_SCL | INPUT | DRIVE_8);" in pinctrl
    assert "PIN_STATE" not in instances(gen, "acme_bus")
    assert "extern const struct pin_state dt_bus_default;" in gen.header()


def test_unused_pin_states_are_left_out(tmp_path):
    gen = generate(tmp_path)
    assert "PA2" not in body(gen)
    assert "unused_default" not in gen.header()
    gen = generate(tmp_path, bus='status = "disabled";')
    assert "PIN_STATE" not in body(gen)


def test_header_only_needs_class_types(tmp_path):
    header = generate(tmp_path).header()
    assert "extern const struct bus dt_bus;" in header
    assert "extern const struct sensor dt_sensor;" in header
    assert "extern const struct clk_ctrl acme_clk;" in header
    assert "extern const struct intc acme_intc;" in header
    assert "#include <bus.h>" in header


def test_no_tree_gives_empty_files():
    gen = dtmap.Generator(None, {name: spec(name) for name in SPECS}, set(ALL))
    gen.run()
    assert not body(gen)
    assert sorted(gen.instances()) == [
        "devicetree/instances/acme_bus.inc",
        "devicetree/instances/acme_pinctrl.inc",
        "devicetree/instances/acme_sensor.inc",
    ]
    assert "extern" not in gen.header()


def test_unlabelled_node_has_no_public_symbol(tmp_path):
    gen = generate(tmp_path, dts=DTS.replace("sensor: sensor@30", "sensor@30"))
    assert "dt_sensor" not in gen.header()
    assert "SENSOR_DT_DEFINE(dt_node_bus_2000_sensor_30," in body(gen)


def test_unlabelled_provider_fails(tmp_path):
    with pytest.raises(DtError, match="needs a label"):
        generate(tmp_path, dts=DTS.replace("bus: bus@2000", "bus@2000"))


def test_kconfig_off_drops_the_device(tmp_path):
    gen = generate(tmp_path, config=("CTRL", "PINCTRL", "BUS"))
    assert "SENSOR" not in body(gen)
    assert "dt_sensor" not in gen.header()
    assert "/bus@2000/sensor@30" in gen.unbound


def test_status_disabled_drops_the_device(tmp_path):
    gen = generate(tmp_path, sensor='status = "disabled";')
    assert "SENSOR" not in body(gen)
    assert not gen.unbound


def test_disabled_bus_drops_its_children(tmp_path):
    gen = generate(tmp_path, bus='status = "disabled";')
    assert "DEFINE" not in body(gen)


def test_missing_provider_driver_fails(tmp_path):
    with pytest.raises(DtError, match="no enabled driver"):
        generate(tmp_path, config=("PINCTRL", "BUS", "SENSOR"))


def test_provider_lacking_the_kind_fails(tmp_path):
    data = {**SPECS["acme,ctrl"], "class": ["intc"], "symbol": {"intc": "acme_intc"}}
    with pytest.raises(DtError, match="provides no 'clock'"):
        generate(tmp_path, specs={"acme,ctrl": spec("acme,ctrl", data)})


def test_default_used_when_absent(tmp_path):
    data = {**SPECS["acme,bus"], "args": [{"prop": "absent", "default": 100}]}
    gen = generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)})
    assert "BUS_DT_DEFINE(dt_bus, 100);" in body(gen)


def test_required_arg_missing_fails(tmp_path):
    data = {**SPECS["acme,bus"], "args": [{"prop": "absent"}]}
    with pytest.raises(DtError, match="missing"):
        generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)})


def ids_spec(tmp_path, values="#define HW_A 40\n#define HW_B 41\n"):
    (tmp_path / "binding.h").write_text(
        "#define ACME_CLK_A 0\n#define ACME_CLK_B 3\n#define ACME_X 9\n"
    )
    (tmp_path / "values.h").write_text(values)
    data = {
        **SPECS["acme,ctrl"],
        "ids": {
            "clock": {
                "cell": 0,
                "header": "binding.h",
                "prefix": "ACME_CLK_",
                "values": {"header": "values.h", "prefix": "HW_"},
            }
        },
    }
    return spec("acme,ctrl", data, tmp_path)


def test_ids_become_the_numbers_of_the_driver(tmp_path):
    gen = generate(tmp_path, specs={"acme,ctrl": ids_spec(tmp_path)})
    assert "CLK(&acme_clk, 41)" in body(gen)


def test_unknown_id_fails(tmp_path):
    dts = DTS.replace("<&ctrl 3>", "<&ctrl 2>")
    with pytest.raises(DtError, match="no ID 2"):
        generate(tmp_path, specs={"acme,ctrl": ids_spec(tmp_path)}, dts=dts)


def test_id_without_a_number_fails(tmp_path):
    with pytest.raises(DtError, match="lacks"):
        ids_spec(tmp_path, values="#define HW_A 40\n")


@pytest.mark.parametrize(
    "data",
    [
        {"compatible": "a,b"},
        {"compatible": "a,b", "config": "X", "typo": 1},
        {"compatible": "a,b", "config": "X", "class": "nope"},
        {"compatible": "a,b", "config": "X", "define": "D"},
        {"compatible": "a,b", "config": "X", "class": ["clk", "intc"], "define": "D"},
        {"compatible": "a,b", "config": "X", "class": "clk", "symbol": {"intc": "s"}},
        {
            "compatible": "a,b",
            "config": "X",
            "class": "clk",
            "define": "D",
            "args": [{"bogus": 1}],
        },
        {
            "compatible": "a,b",
            "config": "X",
            "class": "clk",
            "define": "D",
            "args": [{"reg": 0, "prop": "p"}],
        },
    ],
)
def test_bad_specs_are_rejected(data):
    with pytest.raises(DtError):
        spec("spec", data)


GPIO_PORT = {
    "compatible": "acme,gpio",
    "config": "CTRL",
    "class": ["gpio", "intc"],
    "define": "PORT_DEFINE",
    "symbol": {"gpio": "{sym}", "intc": "{sym}_intc"},
    "args": [{"reg": 0}],
}
GPIO_NODE = (
    '    gpio3: gpio@3000 {\n        compatible = "acme,gpio";\n'
    "        reg = <0x3000 0x100>;\n        gpio-controller;\n        #gpio-cells = <2>;\n"
    "        interrupt-controller;\n        #interrupt-cells = <1>;\n    };\n"
)


def test_one_node_several_class_objects(tmp_path):
    data = {
        **SPECS["acme,bus"],
        "args": [{"gpio": "scl"}, {"gpio": "sda", "default": "NONE"}],
    }
    dts = DTS.replace("    bus: bus@2000 {", GPIO_NODE + "    bus: bus@2000 {").replace(
        "clock-frequency = <400>;", "scl-gpios = <&gpio3 25 16>;"
    )
    gen = generate(
        tmp_path,
        specs={
            "acme,gpio": spec("acme,gpio", GPIO_PORT),
            "acme,bus": spec("acme,bus", data),
        },
        dts=dts,
    )
    assert "PORT_DEFINE(dt_gpio3, 0x3000);" in body(gen)
    assert "BUS_DT_DEFINE(dt_bus, PIN(&dt_gpio3, 25, 16), NONE);" in body(gen)
    assert "extern const struct port dt_gpio3;" in gen.header()
    assert "extern const struct intc dt_gpio3_intc;" in gen.header()


@pytest.mark.parametrize(
    "symbol",
    [{"gpio": "{sym}"}, {"gpio": "{sym}", "intc": "fixed"}],
)
def test_several_class_objects_need_their_symbols(symbol):
    with pytest.raises(DtError):
        spec("spec", {**GPIO_PORT, "symbol": symbol})


def test_fixed_symbols_are_for_drivers_defining_their_devices():
    data = {"compatible": "a,b", "config": "X", "class": "clk", "symbol": "{sym}"}
    with pytest.raises(DtError, match="names what 'define' defines"):
        spec("spec", data)


def test_drivers_sharing_a_node(tmp_path):
    clk = {
        "compatible": "acme,ctrl",
        "config": "CTRL",
        "class": "clk",
        "define": "CLK_DEFINE",
        "symbol": "acme_clk",
        "args": [{"reg": 0}],
    }
    intc = {
        "compatible": "acme,ctrl",
        "config": "INTC",
        "class": "intc",
        "define": "INTC_DEFINE",
        "symbol": "acme_intc",
        "args": [{"reg": 0}],
    }
    specs = {"acme,ctrl": [spec("clk", clk), spec("intc", intc)]}
    for s in specs["acme,ctrl"]:
        s.file = f"acme_ctrl_{s.classes[0]}"
    gen = generate(tmp_path, config=(*ALL, "INTC"), specs=specs)
    assert "CLK_DEFINE(acme_clk, 0x1000);" in instances(gen, "acme_ctrl_clk")
    assert "INTC_DEFINE(acme_intc, 0x1000);" in instances(gen, "acme_ctrl_intc")
    assert "IRQ(&acme_intc, 7)" in instances(gen, "acme_bus")
    with pytest.raises(DtError, match="provides no 'irq'"):
        generate(tmp_path, specs=specs)


def test_drivers_sharing_a_node_need_their_own_classes(tmp_path):
    root = tmp_path / "src"
    (root / "drivers").mkdir(parents=True)
    (root / "drivers" / "clk.class.yaml").write_text(
        "class: clk\ntype: t\ninclude: c.h\n"
    )
    for name in ("a", "b"):
        (root / "drivers" / f"{name}.dtmap.yaml").write_text(
            "compatible: acme,x\nconfig: X\nclass: clk\n"
        )
    with pytest.raises(DtError, match="same class"):
        dtmap.load_specs(root)


PMIC_CLASSES = {
    "pmic": {
        "class": "pmic",
        "type": "struct pmic",
        "include": "pmic.h",
        "provides": {"pmic": "&{sym}"},
    },
    "regulator": {
        "class": "regulator",
        "type": "struct regulator",
        "include": "regulator.h",
    },
}
PMIC_SPECS = [
    {
        "compatible": "acme,pmic",
        "config": "PMIC",
        "class": "pmic",
        "define": "PMIC_DEFINE",
        "args": [{"parent": "bus"}, {"reg": 0}],
    },
    {
        "compatible": "acme,pmic",
        "config": "GPIO",
        "class": "gpio",
        "define": "PMIC_GPIO_DEFINE",
        "symbol": "{sym}_gpio",
        "args": [{"self": "pmic"}],
    },
    {
        "compatible": "acme,pmic",
        "config": "CHARGER",
        "class": "sensor",
        "define": "CHARGER_DEFINE",
        "symbol": "{sym}_charger",
        "args": [
            {"self": "pmic"},
            {"prop": "charge-current-microamp", "of": "monitored-battery"},
            {"prop": "acme,beta", "default": 3380},
        ],
    },
    {
        "compatible": "acme,pmic",
        "config": "REGULATOR",
        "class": "regulator",
        "define": "REG_DEFINE",
        "children": {
            "node": "regulators",
            "names": {"BUCK1": "RAIL_BUCK1", "LDO1": "RAIL_LDO1"},
        },
        "args": [
            {"string": "regulator-name"},
            {"parent": "pmic"},
            {"id": True},
            {"prop": "regulator-max-microvolt", "default": 0},
            {"flag": "regulator-always-on"},
        ],
    },
]
PMIC_DTS = DTS.replace(
    '            mount-matrix = "-1", "0";',
    '            mount-matrix = "-1", "0";\n        };\n'
    '        pmic: pmic@6b {\n            compatible = "acme,pmic";\n            reg = <0x6b>;\n'
    "            gpio-controller;\n            #gpio-cells = <2>;\n"
    "            monitored-battery = <&battery>;\n"
    "            regulators {\n"
    '                buck1: BUCK1 {\n                    regulator-name = "buck1";\n'
    "                    regulator-max-microvolt = <1800000>;\n                    regulator-always-on;\n                };\n"
    '                ldo1: LDO1 {\n                    regulator-name = "ldo1";\n                };\n'
    "            };",
).replace(
    "    ctrl: ctrl@1000 {",
    '    battery: battery {\n        compatible = "simple-battery";\n'
    "        charge-current-microamp = <190000>;\n    };\n    ctrl: ctrl@1000 {",
)


def pmic_generate(
    tmp_path, config=(*ALL, "PMIC", "GPIO", "CHARGER", "REGULATOR"), dts=PMIC_DTS
):
    classes = {**CLASSES, **PMIC_CLASSES}
    group = [dtmap.Spec(f"p{i}", s, classes) for i, s in enumerate(PMIC_SPECS)]
    for s in group:
        s.file = f"acme_pmic_{s.classes[0]}"
    return generate(tmp_path, config=config, specs={"acme,pmic": group}, dts=dts)


def test_functions_of_a_multi_function_device(tmp_path):
    gen = pmic_generate(tmp_path)
    assert "PMIC_DEFINE(dt_pmic, &dt_bus, 0x6b);" in instances(gen, "acme_pmic_pmic")
    assert "PMIC_GPIO_DEFINE(dt_pmic_gpio, &dt_pmic);" in instances(
        gen, "acme_pmic_gpio"
    )
    assert "CHARGER_DEFINE(dt_pmic_charger, &dt_pmic, 190000, 3380);" in instances(
        gen, "acme_pmic_sensor"
    )
    header = gen.header()
    assert "extern const struct port dt_pmic_gpio;" in header
    assert "extern const struct pmic dt_pmic;" in header


def test_children_matched_by_name(tmp_path):
    regs = instances(pmic_generate(tmp_path), "acme_pmic_regulator")
    assert 'REG_DEFINE(dt_buck1, "buck1", &dt_pmic, RAIL_BUCK1, 1800000, true);' in regs
    assert 'REG_DEFINE(dt_ldo1, "ldo1", &dt_pmic, RAIL_LDO1, 0, false);' in regs


def test_unknown_child_fails(tmp_path):
    with pytest.raises(DtError, match="not one of"):
        pmic_generate(tmp_path, dts=PMIC_DTS.replace("ldo1: LDO1", "ldo9: LDO9"))


def test_ranges_translate_addresses(tmp_path):
    dts = DTS.replace(
        "    bus: bus@2000 {",
        "    soc {\n        #address-cells = <1>;\n        #size-cells = <1>;\n"
        "        ranges = <0x0 0x40000000 0x10000>;\n    bus: bus@2000 {",
    ).replace(
        "            %(sensor)s\n        };\n    };\n};",
        "            %(sensor)s\n        };\n    };\n    };\n};",
    )
    gen = generate(tmp_path, dts=dts)
    assert 'BUS_DT_DEFINE(dt_bus, "bus", 0x40002000,' in body(gen)
    assert "SENSOR_DT_DEFINE(dt_sensor, &dt_bus, 0x30," in body(gen)


def test_address_outside_ranges_fails(tmp_path):
    dts = DTS.replace(
        "    bus: bus@2000 {",
        "    soc {\n        #address-cells = <1>;\n        #size-cells = <1>;\n"
        "        ranges = <0x0 0x40000000 0x1000>;\n    bus: bus@2000 {",
    ).replace(
        "            %(sensor)s\n        };\n    };\n};",
        "            %(sensor)s\n        };\n    };\n    };\n};",
    )
    with pytest.raises(DtError, match="outside the ranges"):
        generate(tmp_path, dts=dts)


def test_interrupts_extended_and_names(tmp_path):
    dts = DTS.replace(
        "        interrupt-parent = <&ctrl>;\n        interrupts = <7>;\n        clocks = <&ctrl 3>;",
        '        interrupts-extended = <&ctrl 5>, <&ctrl 7>;\n        interrupt-names = "wake", "event";\n'
        '        clocks = <&ctrl 2>, <&ctrl 3>;\n        clock-names = "bus", "core";',
    )
    data = {**SPECS["acme,bus"], "args": [{"irq": "event"}, {"clock": "core"}]}
    gen = generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)}, dts=dts)
    assert "BUS_DT_DEFINE(dt_bus, IRQ(&acme_intc, 7), CLK(&acme_clk, 3));" in body(gen)
    data = {**SPECS["acme,bus"], "args": [{"clock": "nope"}]}
    with pytest.raises(DtError, match="no 'nope' in 'clock-names'"):
        generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)}, dts=dts)


@pytest.mark.parametrize("spi_status, fails", [("disabled", False), ("okay", True)])
def test_one_function_per_shared_block(tmp_path, spi_status, fails):
    dts = tmp_path / "shared.dts"
    dts.write_text(
        "/dts-v1/;\n/ {\n    #address-cells = <1>;\n    #size-cells = <1>;\n"
        '    i2c0: i2c@40003000 { reg = <0x40003000 0x1000>; status = "okay"; };\n'
        f'    spi0: spi@40003000 {{ reg = <0x40003000 0x1000>; status = "{spi_status}"; }};\n}};\n'
    )
    result = subprocess.run(
        [dtc(), *dtmap.DTC_FLAGS, "-I", "dts", "-O", "dtb", "-o", os.devnull, str(dts)],
        capture_output=True,
        text=True,
        check=False,
    )
    assert ("unique_unit_address_if_enabled" in result.stderr) == fails


def test_kconfig_symbols(tmp_path):
    gen = generate(tmp_path, sensor='status = "disabled";')
    text = dtmap.kconfig_text(gen.tree, ["acme,bus", "acme,sensor", "acme,absent"])
    assert "config DT_HAS_ACME_BUS_ENABLED\n\tdef_bool y" in text
    assert "config DT_HAS_ACME_SENSOR_ENABLED\n\tdef_bool n" in text
    assert "config DT_HAS_ACME_ABSENT_ENABLED\n\tdef_bool n" in text
    assert "def_bool n" in dtmap.kconfig_text(None, ["acme,bus"])


PARTITIONS_DTS = """/dts-v1/;
/ {
    flash {
        partitions {
            compatible = "fixed-partitions";
            #address-cells = <1>;
            #size-cells = <1>;

            partition@0 {
                label = "safe-firmware";
                reg = <0x0 0x80000>;
            };

            partition@100000 {
                label = "firmware-slot-1";
                reg = <0x100000 0x100000>;
            };

            mfg-info@ff000 {
                reg = <0xff000 0x180>;
            };
        };
    };
};
"""


def compile_tree(tmp_path, text):
    path, dtb = tmp_path / "p.dts", tmp_path / "p.dtb"
    path.write_text(text)
    subprocess.run(
        [dtc(), "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)], check=True
    )
    return fdt.load(dtb)


def test_partitions_become_the_part_table(tmp_path):
    text = dtmap.partitions_header(compile_tree(tmp_path, PARTITIONS_DTS))
    assert "#define PART_SAFE_FIRMWARE_OFFSET 0x0000000" in text
    assert "#define PART_SAFE_FIRMWARE_SIZE   PBL_KIB(512)" in text
    assert "#define PART_FIRMWARE_SLOT_1_SIZE   PBL_MIB(1)" in text
    assert "#define PART_MFG_INFO_SIZE   0x180" in text
    assert (
        "PART_FIRMWARE_SLOT_1        PBL_FLASH_PARTITION(PART_FIRMWARE_SLOT_1)" in text
    )


def test_no_partitions_give_an_empty_table(tmp_path):
    assert "PART_" not in dtmap.partitions_header(None)
    disabled = PARTITIONS_DTS.replace(
        "    flash {", '    flash {\n        status = "disabled";'
    )
    assert "PART_" not in dtmap.partitions_header(compile_tree(tmp_path, disabled))


def test_partition_defined_twice_fails(tmp_path):
    text = PARTITIONS_DTS.replace('"firmware-slot-1"', '"safe-firmware"')
    with pytest.raises(DtError, match="defined twice"):
        dtmap.partitions_header(compile_tree(tmp_path, text))


LIST_DTS = """/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;

    ctrl: ctrl@1000 {
        compatible = "acme,ctrl";
        reg = <0x1000 0x100>;
    };

    mem: mem@2000 {
        compatible = "acme,mem";
        reg = <0x2000 0x100>, <0x80000000 0x2000000>;
        reg-names = "regs", "window";
        #address-cells = <1>;
        #size-cells = <0>;

        flash: flash@0 {
            compatible = "acme,flash";
            reg = <0>;
            spi-max-frequency = <8000000>;

            otp {
                compatible = "user-otp";

                nvmem-layout {
                    compatible = "fixed-layout";
                    #address-cells = <1>;
                    #size-cells = <1>;

                    serial-number@20 {
                        reg = <0x20 0x20>;
                    };

                    soc-config@800 {
                        reg = <0x800 0x100>;
                    };
                };
            };
        };
    };
};
"""

LIST_SPECS = {
    "acme,mem": {
        "compatible": "acme,mem",
        "config": "MEM",
        "class": "bus",
        "define": "MEM_DEFINE",
        "args": [
            {"reg": "regs"},
            {"reg": "window"},
            {"size": "window"},
            {"prop": "spi-max-frequency", "child": 0},
        ],
    },
    "acme,flash": {
        "compatible": "acme,flash",
        "config": "FLASH",
        "class": "sensor",
        "define": "FLASH_DEFINE",
        "args": [
            {"parent": "bus"},
            {
                "list": ["user-otp", "fixed-layout"],
                "item": 'CELL("{name}", {0:#x}, {1})',
                "default": [],
            },
        ],
    },
}


def list_generate(tmp_path, text=LIST_DTS):
    path, dtb = tmp_path / "l.dts", tmp_path / "l.dtb"
    path.write_text(text)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {name: spec(name) for name in ("acme,ctrl",)}
    loaded.update({name: spec(name, data) for name, data in LIST_SPECS.items()})
    gen = dtmap.Generator(fdt.load(dtb), loaded, {"CTRL", "MEM", "FLASH"})
    gen.run()
    return gen


def test_reg_sizes_and_child_properties(tmp_path):
    gen = list_generate(tmp_path)
    assert "MEM_DEFINE(dt_mem, 0x2000, 0x80000000, 0x2000000, 8000000);" in body(gen)


def test_children_become_a_list(tmp_path):
    gen = list_generate(tmp_path)
    assert (
        'FLASH_DEFINE(dt_flash, &dt_mem, CELL("serial-number", 0x20, 32), '
        'CELL("soc-config", 0x800, 256));'
    ) in body(gen)
    assert "otp" not in gen.header()


def test_list_items_read_from_each_child(tmp_path):
    text = LIST_DTS.replace(
        "    ctrl: ctrl@1000 {",
        """    keys: keys {
        compatible = "acme,keys";

        key-a {
            label = "A";
            linux,code = <30>;
            debounce-interval = <40>;
            pair = <3 7>;
        };

        key-b {
            label = "B";
            linux,code = <48>;
        };

        key-c {
            label = "C";
            linux,code = <46>;
            status = "disabled";
        };
    };

    ctrl: ctrl@1000 {""",
    )
    path, dtb = tmp_path / "k.dts", tmp_path / "k.dtb"
    path.write_text(text)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    keys = {
        "compatible": "acme,keys",
        "config": "KEYS",
        "class": "sensor",
        "define": "KEYS_DEFINE",
        "args": [
            {"name": True},
            {
                "list": [],
                "item": {
                    "define": "KEY",
                    "args": [
                        {"string": "label"},
                        {"prop": "linux,code"},
                        {"prop": "debounce-interval", "default": 5},
                        {"prop": "pair", "index": 1, "default": 0},
                    ],
                },
            },
        ],
    }
    gen = dtmap.Generator(
        fdt.load(dtb), {"acme,keys": spec("acme,keys", keys)}, {"KEYS"}
    )
    gen.run()
    assert (
        'KEYS_DEFINE(dt_keys, "keys", KEY("A", 30, 40, 7), KEY("B", 48, 5, 0));'
        in body(gen)
    )


def test_list_item_arguments_are_checked():
    data = {
        "compatible": "acme,keys",
        "config": "KEYS",
        "class": "sensor",
        "define": "KEYS_DEFINE",
        "args": [{"list": [], "item": {"define": "KEY", "args": [{"bogus": 1}]}}],
    }
    with pytest.raises(DtError, match="argument"):
        spec("acme,keys", data)


def test_empty_list_ends_the_arguments(tmp_path):
    text = LIST_DTS.replace('compatible = "user-otp";', 'compatible = "factory-otp";')
    gen = list_generate(tmp_path, text)
    assert "FLASH_DEFINE(dt_flash, &dt_mem);" in body(gen)


def test_absent_optional_argument_ends_the_arguments(tmp_path):
    specs = {
        "acme,mem": spec(
            "acme,mem",
            {
                **LIST_SPECS["acme,mem"],
                "args": [{"reg": "regs"}, {"dma": "rx", "optional": True}],
            },
        )
    }
    path, dtb = tmp_path / "o.dts", tmp_path / "o.dtb"
    path.write_text(LIST_DTS)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {"acme,ctrl": spec("acme,ctrl"), **specs}
    gen = dtmap.Generator(fdt.load(dtb), loaded, {"CTRL", "MEM"})
    gen.run()
    assert "MEM_DEFINE(dt_mem, 0x2000);" in body(gen)


def test_optional_arguments_come_last():
    data = {
        **LIST_SPECS["acme,mem"],
        "args": [{"dma": "rx", "optional": True}, {"reg": 0}],
    }
    with pytest.raises(DtError, match="must come last"):
        spec("acme,mem", data)


GRAPH_DTS = """/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;

    gpio: gpio@1000 {
        compatible = "acme,port";
        reg = <0x1000 0x100>;
        gpio-controller;
        #gpio-cells = <2>;
        gpio-ranges = <&pinmux 0 0 32>;
    };

    pinmux: pinmux@2000 {
        compatible = "acme,pinmux";
        reg = <0x2000 0x100>;
    };

    timer@3000 {
        compatible = "acme,timer";
        reg = <0x3000 0x100>;

        tpwm: pwm {
            compatible = "acme,timer-pwm";
            #pwm-cells = <3>;
        };
    };

    vdd: regulator-vdd {
        compatible = "regulator-fixed";
        regulator-name = "vdd";
        gpios = <&gpio 4 0>;
        enable-active-high;
    };

    lcdc: lcdc@4000 {
        compatible = "acme,lcdc";
        reg = <0x4000 0x100>;

        port {
            lcdc_out: endpoint {
                remote-endpoint = <&panel_in>;
            };
        };
    };

    panel: panel {
        compatible = "acme,panel";
        vdd-supply = <&vdd>;
        pwms = <&tpwm 0 16666667 1>;
        rotation = <180>;

        port {
            panel_in: endpoint {
                remote-endpoint = <&lcdc_out>;
            };
        };
    };
};
"""

GRAPH_CLASSES = {
    **CLASSES,
    "pwm": {
        "class": "pwm",
        "type": "struct pwm",
        "include": "pwm.h",
        "provides": {"pwm": "PWM(&{sym}, {0}, {1}, {2})"},
    },
    "regulator": {
        "class": "regulator",
        "type": "struct reg",
        "include": "reg.h",
        "provides": {"regulator": "&{sym}"},
    },
    "panel": {
        "class": "panel",
        "type": "struct panel",
        "include": "panel.h",
        "provides": {"panel": "&{sym}"},
    },
    "display": {"class": "display", "type": "struct display", "include": "display.h"},
    "port": {
        "class": "port",
        "type": "struct port",
        "include": "port.h",
        "provides": {"gpio": "PIN(&{sym}, {0}, {1})", "gpio-controller": "&{sym}"},
    },
}

GRAPH_SPECS = {
    "acme,port": {
        "compatible": "acme,port",
        "config": "PORT",
        "class": "port",
        "define": "PORT_DEFINE",
    },
    "acme,pinmux": {
        "compatible": "acme,pinmux",
        "config": "PINMUX",
        "class": "pinctrl",
        "define": "PINMUX_DEFINE",
        "args": [{"referrer": "gpio-ranges", "as": "gpio-controller"}],
    },
    "acme,timer-pwm": {
        "compatible": "acme,timer-pwm",
        "config": "TPWM",
        "class": "pwm",
        "define": "TPWM_DEFINE",
        "args": [{"reg": 0, "up": True}],
    },
    "regulator-fixed": {
        "compatible": "regulator-fixed",
        "config": "FIXED",
        "class": "regulator",
        "define": "FIXED_DEFINE",
        "args": [{"gpio": ""}, {"flag": "enable-active-high"}],
    },
    "acme,panel": {
        "compatible": "acme,panel",
        "config": "PANEL",
        "class": "panel",
        "define": "PANEL_DEFINE",
        "provides": {"width": "200", "height": "228"},
        "args": [{"supply": "vdd"}, {"pwm": 0}],
    },
    "acme,lcdc": {
        "compatible": "acme,lcdc",
        "config": "LCDC",
        "class": "display",
        "define": "LCDC_DEFINE",
        "args": [
            {"reg": 0},
            {"self": "panel", "remote": 0},
            {"self": "width", "remote": 0},
            {"prop": "rotation", "remote": 0, "default": 0},
        ],
    },
}


def graph_generate(tmp_path, text=GRAPH_DTS):
    path, dtb = tmp_path / "g.dts", tmp_path / "g.dtb"
    path.write_text(text)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {
        name: dtmap.Spec(name, data, GRAPH_CLASSES)
        for name, data in GRAPH_SPECS.items()
    }
    config = {spec["config"] for spec in GRAPH_SPECS.values()}
    gen = dtmap.Generator(fdt.load(dtb), loaded, config)
    gen.run()
    return gen


def test_graph_supplies_and_pwms(tmp_path):
    out = body(graph_generate(tmp_path))
    assert "LCDC_DEFINE(dt_lcdc, 0x4000, &dt_panel, 200, 180);" in out
    assert "PANEL_DEFINE(dt_panel, &dt_vdd, PWM(&dt_tpwm, 0, 16666667, 1));" in out
    assert "FIXED_DEFINE(dt_vdd, PIN(&dt_gpio, 4, 0), true);" in out


def test_function_of_a_block_and_referrer(tmp_path):
    out = body(graph_generate(tmp_path))
    assert "TPWM_DEFINE(dt_tpwm, 0x3000);" in out
    assert "PINMUX_DEFINE(dt_pinmux, &dt_gpio);" in out


def test_disabled_remote_fails(tmp_path):
    text = GRAPH_DTS.replace(
        'compatible = "acme,panel";',
        'compatible = "acme,panel";\n        status = "disabled";',
    )
    with pytest.raises(DtError, match="is disabled"):
        graph_generate(tmp_path, text)


def test_flags_and_all_gpios(tmp_path):
    specs = dict(GRAPH_SPECS)
    specs["acme,lcdc"] = {
        "compatible": "acme,lcdc",
        "config": "LCDC",
        "class": "display",
        "define": "LCDC_DEFINE",
        "args": [
            {
                "flags": {
                    "spi-cs-high": "CS_HIGH",
                    "spi-cpha": "CPHA",
                    "spi-lsb-first": "LSB",
                }
            },
            {"gpio": "cs", "all": True, "optional": True},
        ],
    }
    text = GRAPH_DTS.replace(
        'compatible = "acme,lcdc";',
        'compatible = "acme,lcdc";\n        spi-cs-high;\n        spi-lsb-first;\n'
        "        cs-gpios = <&gpio 1 0>, <&gpio 2 1>;",
    )
    path, dtb = tmp_path / "f.dts", tmp_path / "f.dtb"
    path.write_text(text)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {
        name: dtmap.Spec(name, data, GRAPH_CLASSES) for name, data in specs.items()
    }
    gen = dtmap.Generator(fdt.load(dtb), loaded, {s["config"] for s in specs.values()})
    gen.run()
    assert (
        "LCDC_DEFINE(dt_lcdc, CS_HIGH | LSB, PIN(&dt_gpio, 1, 0), PIN(&dt_gpio, 2, 1));"
        in body(gen)
    )


def test_spec_provides_of_a_driver_defined_device(tmp_path):
    specs = dict(GRAPH_SPECS)
    specs["acme,osc"] = {
        "compatible": "acme,osc",
        "config": "OSC",
        "class": "clk",
        "symbol": "acme_osc",
        "header": "acme/osc.h",
        "provides": {"clock": "CLK(&{sym}, OSC_ID)"},
    }
    specs["acme,timer-pwm"] = {
        **GRAPH_SPECS["acme,timer-pwm"],
        "args": [{"reg": 0, "up": True}, {"clock": 0, "up": True}],
    }
    text = GRAPH_DTS.replace(
        "    timer@3000 {",
        '    clock-osc {\n        compatible = "acme,osc";\n        #clock-cells = <0>;\n    };\n\n'
        "    timer@3000 {",
    ).replace(
        'compatible = "acme,timer";',
        'compatible = "acme,timer";\n        clocks = <&{osc}>;',
    )
    text = text.replace("clock-osc {", "osc: clock-osc {").replace("&{osc}", "&osc")
    path, dtb = tmp_path / "p.dts", tmp_path / "p.dtb"
    path.write_text(text)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {
        name: dtmap.Spec(name, data, GRAPH_CLASSES) for name, data in specs.items()
    }
    gen = dtmap.Generator(fdt.load(dtb), loaded, {s["config"] for s in specs.values()})
    gen.run()
    assert "TPWM_DEFINE(dt_tpwm, 0x3000, CLK(&acme_osc, OSC_ID));" in body(gen)
    assert "acme/osc.h" in instances(gen, "acme_timer_pwm")


CHILD_DTS = """/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;

    leds@1000 {
        compatible = "acme,leds";
        reg = <0x1000 0x100>;

        status_led: multi-led {
        };
    };

    backlight {
        compatible = "acme,backlight";
        leds = <&status_led>;
    };
};
"""


def test_children_are_providers(tmp_path):
    path, dtb = tmp_path / "c.dts", tmp_path / "c.dtb"
    path.write_text(CHILD_DTS)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    specs = {
        "acme,leds": spec(
            "acme,leds",
            {
                "compatible": "acme,leds",
                "config": "LEDS",
                "class": "bus",
                "define": "LED_DEFINE",
                "children": {"names": {"multi-led": 0}},
                "args": [{"reg": 0, "up": True}],
            },
        ),
        "acme,backlight": spec(
            "acme,backlight",
            {
                "compatible": "acme,backlight",
                "config": "BACKLIGHT",
                "class": "sensor",
                "define": "BACKLIGHT_DEFINE",
                "args": [{"self": "bus", "of": "leds"}],
            },
        ),
    }
    gen = dtmap.Generator(fdt.load(dtb), specs, {"LEDS", "BACKLIGHT"})
    gen.run()
    assert "LED_DEFINE(dt_status_led, 0x1000);" in body(gen)
    assert "BACKLIGHT_DEFINE(dt_node_backlight, &dt_status_led);" in body(gen)


GPIO_SPEC = {
    "compatible": "acme,gpio",
    "config": "GPIO",
    "class": "gpio",
    "define": "GPIO_DEFINE",
    "args": [],
}
GPIO_DTS = DTS.replace(
    "    ctrl: ctrl@1000 {",
    '    gpio: gpio@900 {\n        compatible = "acme,gpio";\n        reg = <0x900 0x100>;\n'
    "        #gpio-cells = <2>;\n    };\n    ctrl: ctrl@1000 {",
)


def sensor_generate(tmp_path, args, sensor="", dts=GPIO_DTS, extra=None, config=ALL):
    specs = {
        "acme,gpio": spec("acme,gpio", GPIO_SPEC),
        "acme,sensor": spec("acme,sensor", {**SPECS["acme,sensor"], "args": args}),
        **(extra or {}),
    }
    return generate(
        tmp_path, config=(*config, "GPIO"), sensor=sensor, specs=specs, dts=dts
    )


@pytest.mark.parametrize(
    "dts",
    [
        DTS.replace("bus: bus@2000", "bus: abus: bus@2000"),
        DTS + "abus: &bus { };\n",
    ],
    ids=["inline", "reference"],
)
def test_labels_of_a_node_are_aliases_of_the_first_sorted(tmp_path, dts):
    gen = generate(tmp_path, dts=dts)
    assert 'BUS_DT_DEFINE(dt_abus, "abus",' in body(gen)
    assert "SENSOR_DT_DEFINE(dt_sensor, &dt_abus," in body(gen)
    header = gen.header()
    assert "extern const struct bus dt_abus;" in header
    assert "#define dt_bus dt_abus" in header
    assert "extern const struct bus dt_bus;" not in header


def test_labels_of_a_pin_state_are_aliases(tmp_path):
    dts = DTS.replace("bus_default: state", "bus_default: bus_active: state")
    gen = generate(tmp_path, dts=dts)
    assert "PIN_STATE(dt_bus_active, " in body(gen)
    assert "#define dt_bus_default dt_bus_active" in gen.header()


def test_class_symbols_of_every_label(tmp_path):
    data = {
        **SPECS["acme,bus"],
        "class": ["bus", "sensor"],
        "symbol": {"bus": "{sym}", "sensor": "{sym}_sensor"},
        "args": [],
    }
    dts = DTS.replace("bus: bus@2000", "bus: abus: bus@2000")
    gen = generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)}, dts=dts)
    assert "#define dt_bus dt_abus\n" in gen.header()
    assert "#define dt_bus_sensor dt_abus_sensor\n" in gen.header()


@pytest.mark.parametrize(
    "arg, value",
    [
        ({"gpio": "reset"}, "PIN(&dt_gpio, 9, 0)"),
        ({"clock": 0}, "CLK(&acme_clk, 5)"),
        ({"irq": 0}, "IRQ(&acme_intc, 4)"),
    ],
)
def test_references_are_read_from_the_redirected_node(tmp_path, arg, value):
    sensor = (
        "reset-gpios = <&gpio 1 0>; clocks = <&ctrl 2>;\n"
        "sub { reset-gpios = <&gpio 9 0>; clocks = <&ctrl 5>;\n"
        "interrupts-extended = <&ctrl 4>; };"
    )
    gen = sensor_generate(tmp_path, [{**arg, "child": "sub"}], sensor)
    assert f"SENSOR_DT_DEFINE(dt_sensor, {value});" in body(gen)


def test_references_of_a_redirected_node_alone(tmp_path):
    sensor = "sub { reset-gpios = <&gpio 9 0>; };"
    gen = sensor_generate(tmp_path, [{"gpio": "reset", "child": "sub"}], sensor)
    assert "SENSOR_DT_DEFINE(dt_sensor, PIN(&dt_gpio, 9, 0));" in body(gen)


OPTIONAL_ARGS = [
    {"reg": 0},
    {"gpio": "reset", "optional": True},
    {"gpio": "enable", "optional": True},
]


def test_argument_after_an_absent_optional_fails(tmp_path):
    with pytest.raises(DtError, match=r"/bus@2000/sensor@30: has what .*enable"):
        sensor_generate(tmp_path, OPTIONAL_ARGS, "enable-gpios = <&gpio 5 0>;")


def test_trailing_optionals_may_be_absent(tmp_path):
    gen = sensor_generate(tmp_path, OPTIONAL_ARGS, "reset-gpios = <&gpio 5 0>;")
    assert "SENSOR_DT_DEFINE(dt_sensor, 0x30, PIN(&dt_gpio, 5, 0));" in body(gen)
    assert "SENSOR_DT_DEFINE(dt_sensor, 0x30);" in body(
        sensor_generate(tmp_path, OPTIONAL_ARGS)
    )


def test_first_compatible_with_an_enabled_driver_binds(tmp_path):
    new = {**SPECS["acme,sensor"], "compatible": "acme,new", "config": "NEW"}
    new["define"] = "NEW_DT_DEFINE"
    dts = DTS.replace(
        'compatible = "acme,sensor";', 'compatible = "acme,new", "acme,sensor";'
    )
    specs = {"acme,new": spec("acme,new", new)}
    assert "SENSOR_DT_DEFINE(dt_sensor," in body(
        generate(tmp_path, specs=specs, dts=dts)
    )
    gen = generate(tmp_path, config=(*ALL, "NEW"), specs=specs, dts=dts)
    assert "NEW_DT_DEFINE(dt_sensor," in body(gen)
    assert "SENSOR_DT_DEFINE" not in body(gen)


def test_self_does_not_climb_past_a_disabled_driver(tmp_path):
    other = {
        "compatible": "acme,other",
        "config": "OTHER",
        "class": "sensor",
        "define": "OTHER_DEFINE",
        "provides": {"bus": "&{sym}"},
        "args": [],
    }
    sensor = 'link = <&other>;\n};\nother: other@40 { compatible = "acme,other"; reg = <0x40>;'
    extra = {"acme,other": spec("acme,other", other)}
    args = [{"self": "bus", "of": "link"}]
    with pytest.raises(DtError, match="other@40, which has no enabled driver"):
        sensor_generate(tmp_path, args, sensor, extra=extra)
    gen = sensor_generate(tmp_path, args, sensor, extra=extra, config=(*ALL, "OTHER"))
    assert "SENSOR_DT_DEFINE(dt_sensor, &dt_other);" in body(gen)


def test_disabled_partitions_are_left_out(tmp_path):
    text = PARTITIONS_DTS.replace(
        'label = "firmware-slot-1";', 'label = "firmware-slot-1"; status = "disabled";'
    )
    header = dtmap.partitions_header(compile_tree(tmp_path, text))
    assert "PART_SAFE_FIRMWARE_OFFSET" in header
    assert "FIRMWARE_SLOT_1" not in header


def test_overlapping_partitions_fail(tmp_path):
    text = PARTITIONS_DTS.replace("reg = <0xff000 0x180>", "reg = <0x7f000 0x2000>")
    with pytest.raises(DtError, match="mfg-info@ff000: overlaps .*partition@0"):
        dtmap.partitions_header(compile_tree(tmp_path, text))


def test_partition_without_reg_fails(tmp_path):
    text = PARTITIONS_DTS.replace("reg = <0xff000 0x180>;", "")
    with pytest.raises(DtError, match="mfg-info@ff000: no 'reg'"):
        dtmap.partitions_header(compile_tree(tmp_path, text))


@pytest.mark.parametrize(
    "old, new, message",
    [
        ("reg = <0x2000 0x100>;", "", "/bus@2000: no 'reg'"),
        ("#clock-cells = <1>;", "", "/ctrl@1000: no '#clock-cells'"),
        ('function = "scl";\n                input-enable;', "", "no 'function'"),
        ("clocks = <&ctrl 3>;", "clocks = <&ctrl>;", "/ctrl@1000 takes 1 cells"),
    ],
    ids=["reg", "cells", "function", "specifier"],
)
def test_missing_data_fails_with_the_node(tmp_path, old, new, message):
    assert old in DTS
    with pytest.raises(DtError, match=message):
        generate(tmp_path, dts=DTS.replace(old, new))


def test_dma_without_names_fails(tmp_path):
    data = {**SPECS["acme,bus"], "args": [{"dma": "rx"}]}
    specs = {"acme,bus": spec("acme,bus", data)}
    with pytest.raises(DtError, match="/bus@2000: no 'dma-names'"):
        generate(tmp_path, specs=specs, bus="dmas = <&ctrl 1>;")


def test_boolean_defaults_and_values_are_c(tmp_path):
    args = [
        {"prop": "absent", "default": False},
        {"prop": "missing", "default": True},
        {"string": "mode", "values": {"fast": True}},
    ]
    gen = sensor_generate(tmp_path, args, 'mode = "fast";')
    assert "SENSOR_DT_DEFINE(dt_sensor, false, true, true);" in body(gen)


def test_null_default_is_rejected():
    data = {**SPECS["acme,sensor"], "args": [{"prop": "x", "default": None}]}
    with pytest.raises(DtError, match="'default' has no value"):
        spec("acme,sensor", data)


def write_specs(root, specs):
    (root / "drivers").mkdir(parents=True)
    for name in ("clk", "bus"):
        (root / "drivers" / f"{name}.class.yaml").write_text(
            f"class: {name}\ntype: t\ninclude: {name}.h\n"
        )
    for name, text in specs.items():
        (root / "drivers" / f"{name}.dtmap.yaml").write_text(text)


def test_instance_files_of_specs_sharing_one_of_their_compatibles(tmp_path):
    write_specs(
        tmp_path,
        {
            "a": "compatible:\n  - acme,one\n  - acme,two\n"
            "config: A\nclass: clk\ndefine: A\n",
            "b": "compatible: acme,two\nconfig: B\nclass: bus\ndefine: B\n",
        },
    )
    assert dtmap.instance_files(dtmap.load_specs(tmp_path)) == [
        "devicetree/instances/acme_one_clk.inc",
        "devicetree/instances/acme_two_bus.inc",
    ]


def test_specs_sharing_a_compatible_need_a_class(tmp_path):
    write_specs(
        tmp_path,
        {
            "a": "compatible: acme,two\nconfig: A\nclass: clk\n",
            "b": "compatible: acme,two\nconfig: B\n",
        },
    )
    with pytest.raises(DtError, match="b.dtmap.yaml: sharing 'acme,two' needs a class"):
        dtmap.load_specs(tmp_path)


def test_specs_need_their_own_instance_file(tmp_path):
    write_specs(
        tmp_path,
        {
            "a": "compatible: acme,a-b\nconfig: A\nclass: clk\ndefine: A\n",
            "b": "compatible: acme,a_b\nconfig: B\nclass: bus\ndefine: B\n",
        },
    )
    with pytest.raises(DtError, match="same instance file"):
        dtmap.load_specs(tmp_path)


def test_depfile_paths_may_have_spaces():
    text = "board.dtb: /a\\ b/x.dts \\\n  /a\\ b/y.dtsi /c/z.h\n"
    assert dtmap.depfile_sources(text) == ["/a b/x.dts", "/a b/y.dtsi", "/c/z.h"]


def test_main_only_writes_what_changed_and_drops_stale_files(tmp_path, monkeypatch):
    write_specs(
        tmp_path, {"a": "compatible: acme,a\nconfig: A\nclass: clk\ndefine: A\n"}
    )
    (tmp_path / ".config").write_text("")
    out = tmp_path / "out"
    stale = out / "devicetree" / "instances" / "gone.inc"
    stale.parent.mkdir(parents=True)
    stale.write_text("")
    argv = ["dtmap.py", "--srcdir", str(tmp_path), "--outdir", str(out)]
    monkeypatch.setattr(sys, "argv", [*argv, "--config", str(tmp_path / ".config")])
    dtmap.main()
    assert not stale.exists()
    files = [out / "devicetree" / "devices.h", stale.with_name("acme_a.inc")]
    for path in files:
        os.utime(path, (1, 1))
    dtmap.main()
    assert [path.stat().st_mtime for path in files] == [1, 1]


def test_header_values_in_any_integer_form(tmp_path):
    header = tmp_path / "h.h"
    header.write_text(
        "#define P_A 1\n#define P_B 0x10\n#define P_C 3 /* c */\n#define P_D (4)\n"
        "#define P_E 5U\n#define P_F (0x6UL) // f\n#define P_G (1 << 2)\n"
    )
    values = {"A": 1, "B": 16, "C": 3, "D": 4, "E": 5, "F": 6}
    assert dtmap.header_defines(header, "P_") == values


def test_nodes_with_the_same_symbol_fail(tmp_path):
    dts = DTS.replace("sensor: sensor@30", "a-b@30").replace(
        "            %(sensor)s\n        };",
        '            %(sensor)s\n        };\n        a_b@30 { compatible = "acme,sensor";'
        ' reg = <0x30>; mount-matrix = "1", "0"; };',
    )
    with pytest.raises(DtError, match="a_b@30: 'dt_node_bus_2000_a_b_30' is also"):
        generate(tmp_path, dts=dts)


def depends(gen):
    return [line for line in body(gen).splitlines() if "PBL_DEVICE_DEPENDS" in line]


def test_devices_a_node_refers_to_come_first(tmp_path):
    gen = generate(tmp_path)
    # The sensor only refers to its bus, which is its parent
    assert depends(gen) == [
        "PBL_DEVICE_DEPENDS(dt_bus, &dt_bus.dev, &acme_intc.dev, &acme_clk.dev);"
    ]
    assert instances(gen, "acme_bus").endswith(
        "&dt_bus_default);\nPBL_DEVICE_DEPENDS(dt_bus, &dt_bus.dev, &acme_intc.dev, &acme_clk.dev);\n"
    )


def test_a_needed_device_is_listed_once(tmp_path):
    args = [{"gpio": "reset"}, {"gpio": "enable"}, {"clock": 0}, {"parent": "bus"}]
    sensor = (
        "reset-gpios = <&gpio 1 0>; enable-gpios = <&gpio 2 0>; clocks = <&ctrl 1>;"
    )
    gen = sensor_generate(tmp_path, args, sensor)
    assert (
        "PBL_DEVICE_DEPENDS(dt_sensor, &dt_sensor.dev, &dt_gpio.dev, &acme_clk.dev);"
        in instances(gen, "acme_sensor")
    )
    assert set(re.findall(r"#include <(.+)>", instances(gen, "acme_sensor"))) == {
        "bus.h",
        "clk.h",
        "gpio.h",
        "devicetree/devices.h",
    }


def test_redirected_and_listed_references_are_needed(tmp_path):
    args = [{"gpio": "reset", "child": "sub"}, {"self": "bus", "of": "link"}]
    sensor = "link = <&pinctrl>; sub { reset-gpios = <&gpio 9 0>; };"
    data = {**SPECS["acme,pinctrl"], "class": "bus", "define": "CTL_DEFINE", "args": []}
    del data["pinctrl"]
    extra = {"acme,pinctrl": spec("acme,pinctrl", data)}
    dts = GPIO_DTS.replace("pinctrl-0 = <&bus_default>;", "").replace(
        'pinctrl-names = "default";', ""
    )
    gen = sensor_generate(tmp_path, args, sensor, dts=dts, extra=extra)
    assert (
        "PBL_DEVICE_DEPENDS(dt_sensor, &dt_sensor.dev, &dt_gpio.dev, &dt_pinctrl.dev);"
        in depends(gen)
    )


def test_every_class_object_of_an_instance_gets_what_it_needs(tmp_path):
    data = {
        **SPECS["acme,bus"],
        "class": ["bus", "sensor"],
        "symbol": {"bus": "{sym}", "sensor": "{sym}_sensor"},
        "args": [{"clock": 0}],
    }
    gen = generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)})
    assert depends(gen) == [
        "PBL_DEVICE_DEPENDS(dt_bus, &dt_bus.dev, &acme_clk.dev);",
        "PBL_DEVICE_DEPENDS(dt_bus_sensor, &dt_bus_sensor.dev, &acme_clk.dev);",
    ]


def test_instances_with_their_own_symbol_get_what_they_need(tmp_path):
    data = {**SPECS["acme,bus"], "symbol": "acme_bus0", "args": [{"clock": 0}]}
    gen = generate(tmp_path, specs={"acme,bus": spec("acme,bus", data)})
    assert depends(gen) == [
        "PBL_DEVICE_DEPENDS(acme_bus0, &acme_bus0.dev, &acme_clk.dev);"
    ]


def test_classes_say_where_their_device_is(tmp_path):
    classes = {
        **CLASSES,
        "bus": {**CLASSES["bus"], "device": "&{sym}"},
        "clk": {**CLASSES["clk"], "device": False},
        "gpio": {**CLASSES["gpio"], "device": "&{sym}.port.dev"},
    }
    args = [{"clock": 0}, {"gpio": "reset"}]
    specs = {
        "acme,ctrl": dtmap.Spec("ctrl", SPECS["acme,ctrl"], classes),
        "acme,gpio": dtmap.Spec("gpio", GPIO_SPEC, classes),
        "acme,bus": dtmap.Spec("bus", {**SPECS["acme,bus"], "args": args}, classes),
    }
    bus = "reset-gpios = <&gpio 1 0>;"
    gen = generate(tmp_path, config=(*ALL, "GPIO"), bus=bus, specs=specs, dts=GPIO_DTS)
    assert depends(gen) == ["PBL_DEVICE_DEPENDS(dt_bus, &dt_bus, &dt_gpio.port.dev);"]


def test_functions_and_children_of_a_device_do_not_need_it(tmp_path):
    classes = {**CLASSES, **PMIC_CLASSES}
    classes["regulator"] = {
        **PMIC_CLASSES["regulator"],
        "provides": {"regulator": "&{sym}"},
    }
    group = [dtmap.Spec(f"p{i}", s, classes) for i, s in enumerate(PMIC_SPECS)]
    for s in group:
        s.file = f"acme_pmic_{s.classes[0]}"
    args = [{"supply": "vdd"}, {"gpio": "reset"}]
    specs = {
        "acme,pmic": group,
        "acme,sensor": spec("acme,sensor", {**SPECS["acme,sensor"], "args": args}),
    }
    # The PMIC is its own supply: a device inside it is no dependency of it either
    dts = PMIC_DTS.replace(
        "monitored-battery", "vin-supply = <&ldo1>; monitored-battery"
    ).replace(
        'mount-matrix = "-1", "0";',
        'mount-matrix = "-1", "0"; vdd-supply = <&buck1>; reset-gpios = <&pmic 3 0>;',
    )
    group[0].args.append({"supply": "vin"})
    config = (*ALL, "PMIC", "GPIO", "CHARGER", "REGULATOR")
    gen = generate(tmp_path, config=config, specs=specs, dts=dts)
    assert "PMIC_DEFINE(dt_pmic, &dt_bus, 0x6b, &dt_ldo1);" in body(gen)
    assert [line for line in depends(gen) if "dt_bus," not in line] == [
        "PBL_DEVICE_DEPENDS(dt_sensor, &dt_sensor.dev, &dt_buck1.dev, &dt_pmic_gpio.dev);"
    ]


def test_absent_optional_providers_are_not_needed(tmp_path):
    args = [{"reg": 0}, {"gpio": "reset", "optional": True}]
    assert "dt_sensor" not in "".join(depends(sensor_generate(tmp_path, args)))


def test_disabled_needed_device_fails(tmp_path):
    dts = GPIO_DTS.replace(
        "#gpio-cells = <2>;", '#gpio-cells = <2>; status = "disabled";'
    )
    with pytest.raises(DtError, match="'gpio' from /gpio@900, which is disabled"):
        sensor_generate(
            tmp_path, [{"gpio": "reset"}], "reset-gpios = <&gpio 1 0>;", dts=dts
        )


def test_devices_needing_each_other_fail(tmp_path):
    gate = {
        "compatible": "acme,gate",
        "config": "GATE",
        "class": "clk",
        "define": "GATE_DEFINE",
        "args": [{"gpio": "enable"}],
    }
    specs = {
        "acme,gate": spec("acme,gate", gate),
        "acme,gpio": spec("acme,gpio", {**GPIO_SPEC, "args": [{"clock": 0}]}),
    }
    dts = GPIO_DTS.replace(
        "#gpio-cells = <2>;",
        "#gpio-cells = <2>;\n        clocks = <&gate 0>;\n    };\n"
        '    gate: gate@800 {\n        compatible = "acme,gate";\n        reg = <0x800 0x100>;\n'
        "        #clock-cells = <1>;\n        enable-gpios = <&gpio 1 0>;",
    )
    with pytest.raises(
        DtError, match=r"need each other: &dt_gpio.dev -> &dt_gate.dev -> &dt_gpio.dev"
    ):
        generate(tmp_path, config=(*ALL, "GPIO", "GATE"), specs=specs, dts=dts)


def test_specs_do_not_share_their_arguments():
    data = {**SPECS["acme,sensor"], "args": [{"reg": 0}]}
    spec("acme,sensor", data).args.append({"prop": "x"})
    assert data["args"] == [{"reg": 0}]


def test_children_of_a_disabled_container_are_left_out(tmp_path):
    dts = PMIC_DTS.replace(
        "            regulators {",
        '            regulators {\n                status = "disabled";',
    )
    gen = pmic_generate(tmp_path, dts=dts)
    assert "REG_DEFINE" not in instances(gen, "acme_pmic_regulator")


def test_label_clashing_with_a_class_object_fails(tmp_path):
    dts = DTS.replace("    bus: bus@2000 {", GPIO_NODE + "    bus: bus@2000 {")
    dts = dts.replace("sensor: sensor@30", "gpio3_intc: sensor@30")
    specs = {**{k: spec(k) for k in SPECS}, "acme,gpio": spec("acme,gpio", GPIO_PORT)}
    with pytest.raises(DtError, match="'dt_gpio3_intc' is also"):
        generate(tmp_path, specs=specs, dts=dts)


def test_unlabelled_providers_are_needed(tmp_path):
    reg_class = {
        "class": "regulator",
        "type": "struct regulator",
        "include": "regulator.h",
    }
    classes = {**CLASSES, "regulator": reg_class}
    ldo = dtmap.Spec(
        "ldo",
        {
            "compatible": "acme,ldo",
            "config": "LDO",
            "class": "regulator",
            "define": "LDO_DEFINE",
            "provides": {"regulator": "LDO_ID"},
            "args": [],
        },
        classes,
    )
    args = [{"supply": "vdd"}]
    dts = GPIO_DTS.replace(
        "    ctrl: ctrl@1000 {",
        '    ldo@800 {\n        compatible = "acme,ldo";\n        reg = <0x800 0x10>;\n'
        "        phandle = <0x99>;\n    };\n    ctrl: ctrl@1000 {",
    )
    gen = sensor_generate(
        tmp_path,
        args,
        "vdd-supply = <0x99>;",
        dts=dts,
        extra={"acme,ldo": [ldo]},
        config=(*ALL, "LDO"),
    )
    assert "SENSOR_DT_DEFINE(dt_sensor, LDO_ID);" in body(gen)
    assert (
        "PBL_DEVICE_DEPENDS(dt_sensor, &dt_sensor.dev, &dt_node_ldo_800.dev);"
        in depends(gen)
    )
    assert "extern const struct regulator dt_node_ldo_800;" in gen.header()


def test_specs_of_the_application(tmp_path):
    write_specs(tmp_path / "os", {})
    app = tmp_path / "app" / "drivers"
    app.mkdir(parents=True)
    (app / "thing.dtmap.yaml").write_text(
        "compatible: acme,thing\nconfig: THING\nclass: bus\ndefine: THING_DEFINE\nargs: []\n"
    )
    assert "acme,thing" not in dtmap.load_specs(tmp_path / "os")
    assert "acme,thing" in dtmap.load_specs(tmp_path / "os", tmp_path / "app")


BOARD_TREES = sorted(
    path.relative_to(REPO).as_posix() for path in (REPO / "boards").glob("*/*.dts")
)


@pytest.mark.parametrize("dts", BOARD_TREES)
def test_board_trees_generate_with_every_driver(tmp_path, dts):
    dtc()
    if shutil.which("cc") is None:
        pytest.skip("needs cc")
    specs = dtmap.load_specs(REPO)
    dtb, _ = dtmap.compile_dts(
        REPO / dts, "cc", [REPO / "boards", REPO / "dts", REPO / "include"], tmp_path
    )
    tree = fdt.load(dtb)
    config = {s.config for group in specs.values() for s in group}
    gen = dtmap.Generator(tree, specs, config)
    gen.run()
    assert "_DEFINE(" in "".join(gen.instances().values())
    assert "extern const" in gen.header()
    dtmap.partitions_header(tree)


def test_every_board_has_its_tree():
    assert len(BOARD_TREES) >= 9


def assigned_generate(tmp_path, bus):
    data = {
        **SPECS["acme,bus"],
        "args": [{"name": True}, {"assigned-clocks": "ASSIGNED", "optional": True}],
    }
    return generate(tmp_path, bus=bus, specs={"acme,bus": spec("acme,bus", data)})


def test_assigned_clocks_become_items(tmp_path):
    gen = assigned_generate(
        tmp_path,
        "assigned-clocks = <&ctrl 3>, <&ctrl 4>, <&ctrl 5>;\n"
        "        assigned-clock-parents = <&ctrl 1>, <0>, <&ctrl 2>;\n"
        "        assigned-clock-rates = <0>, <1000>;",
    )
    assert (
        'BUS_DT_DEFINE(dt_bus, "bus", ASSIGNED(CLK(&acme_clk, 3), CLK(&acme_clk, 1), 0), '
        "ASSIGNED(CLK(&acme_clk, 4), {}, 1000), ASSIGNED(CLK(&acme_clk, 5), CLK(&acme_clk, 2), 0));"
        in body(gen)
    )


def test_no_assigned_clocks_end_the_arguments(tmp_path):
    gen = assigned_generate(tmp_path, "")
    assert 'BUS_DT_DEFINE(dt_bus, "bus");' in body(gen)


@pytest.mark.parametrize(
    "bus, message",
    [
        ("assigned-clocks = <&ctrl 3>;", "has no parent nor rate"),
        (
            "assigned-clocks = <&ctrl 3>;\n        assigned-clock-parents = <0>;",
            "has no parent nor rate",
        ),
        (
            "assigned-clocks = <&ctrl 3>;\n        assigned-clock-rates = <1>, <2>;",
            "more assigned parents or rates",
        ),
    ],
    ids=["nothing", "empty-parent", "too-many-rates"],
)
def test_assigned_clocks_need_a_parent_or_rate(tmp_path, bus, message):
    with pytest.raises(DtError, match=message):
        assigned_generate(tmp_path, bus)


def test_named_clock_a_node_may_lack(tmp_path):
    data = {
        **SPECS["acme,bus"],
        "args": [{"clock": "core"}, {"clock": "ref", "default": "NO_CLK"}],
    }
    specs = {"acme,bus": spec("acme,bus", data)}
    bus = 'clock-names = "core";'
    assert "BUS_DT_DEFINE(dt_bus, CLK(&acme_clk, 3), NO_CLK);" in body(
        generate(tmp_path, bus=bus, specs=specs)
    )
    data["args"][1] = {"clock": "ref"}
    with pytest.raises(DtError, match="no 'ref' in 'clock-names'"):
        generate(tmp_path, bus=bus, specs={"acme,bus": spec("acme,bus", data)})


def test_power_domains_and_required_opps(tmp_path):
    classes = {
        **CLASSES,
        "pd": {
            "class": "pd",
            "type": "struct pd",
            "include": "pd.h",
            "provides": {"power-domain": "PD(&{sym}, {0})"},
        },
    }
    dts = """/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;
    opp-table {
        compatible = "operating-points-v2";
        fast: opp-4 { opp-level = <4>; };
    };
    pmu: pmu@1000 {
        compatible = "acme,pmu";
        reg = <0x1000 0x100>;
        #power-domain-cells = <1>;
    };
    rcc: rcc@2000 {
        compatible = "acme,rcc";
        reg = <0x2000 0x100>;
        power-domains = <&pmu 0>;
        required-opps = <&fast>;
    };
};
"""
    specs = {
        "acme,pmu": {
            "compatible": "acme,pmu",
            "config": "PMU",
            "class": "pd",
            "define": "PMU",
        },
        "acme,rcc": {
            "compatible": "acme,rcc",
            "config": "RCC",
            "class": "pd",
            "define": "RCC",
            "args": [{"power-domain": 0}, {"prop": "opp-level", "of": "required-opps"}],
        },
    }
    path, dtb = tmp_path / "pd.dts", tmp_path / "pd.dtb"
    path.write_text(dts)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    loaded = {name: dtmap.Spec(name, data, classes) for name, data in specs.items()}
    gen = dtmap.Generator(fdt.load(dtb), loaded, {"PMU", "RCC"})
    gen.run()
    assert "RCC(dt_rcc, PD(&dt_pmu, 0), 4);" in body(gen)
    assert "PBL_DEVICE_DEPENDS(dt_rcc, &dt_rcc.dev, &dt_pmu.dev);" in body(gen)


REGS_DTS = """/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;

    soc {
        compatible = "simple-bus";
        #address-cells = <1>;
        #size-cells = <1>;
        ranges = <0x0 0x40000000 0x100000>;

        rcc: clock-controller@1000 {
            reg = <0x1000 0x400>;
        };

        nameless: bus-node {
        };
    };
};
"""


def regs(tmp_path, labels, config="SOC_ACME"):
    def error(message):
        return DtError(f"acme.regs.yaml: {message}")

    path, dtb = tmp_path / "r.dts", tmp_path / "r.dtb"
    path.write_text(REGS_DTS)
    subprocess.run(
        [dtc(), "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtb), str(path)],
        check=True,
    )
    return dtmap.regs_header(fdt.load(dtb), [("SOC_ACME", labels, error)], {config})


def test_regs_are_translated_through_ranges(tmp_path):
    text = regs(tmp_path, ["rcc"])
    assert "#define PBL_DT_REG_RCC 0x40001000UL" in text
    assert "#define PBL_DT_SIZE_RCC 0x400U" in text


def test_regs_of_disabled_code_are_left_out(tmp_path):
    assert "PBL_DT_" not in regs(tmp_path, ["rcc"], config="OTHER")
    assert "PBL_DT_" not in dtmap.regs_header(None, [], set())


def test_regs_of_unknown_label_fail(tmp_path):
    with pytest.raises(DtError, match="no node labelled 'pmu'"):
        regs(tmp_path, ["pmu"])


def test_regs_of_node_without_reg_fail(tmp_path):
    with pytest.raises(DtError, match="bus-node has no reg"):
        regs(tmp_path, ["nameless"])
