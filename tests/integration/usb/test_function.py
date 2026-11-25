# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The USB device stack and controller, through the USB test function
(samples/subsys/usb/test) and the host's own USB stack."""

import contextlib
import random
import time

import pytest
from harness.helpers import usb as husb

pytestmark = [
    pytest.mark.device_types("hardware"),
    pytest.mark.requires_config("CONFIG_USB_TEST"),
]

# Lengths around the packet size of endpoint 0, up to the function's buffer.
CONTROL_LENGTHS = [0, 1, 8, 63, 64, 65, 127, 128, 129, 255, 256, 512, 1000, 1024]
BULK_LENGTHS = [0, 1, 63, 64, 65, 511, 512, 513, 4096]
STREAM_BYTES = 256 * 1024
# Far below full speed's bulk bandwidth, which a controller moving its FIFOs
# by hand still clears.
MIN_THROUGHPUT_KBPS = 200


def _payload(length, seed):
    return random.Random(seed).randbytes(length)


def _string(dev, index):
    import usb.util

    return usb.util.get_string(dev, index, 0x0409)


def test_device_descriptor(usb_function, build):
    dev = usb_function.dev
    assert dev.bcdUSB == 0x0200
    assert dev.bMaxPacketSize0 == 64
    assert dev.idVendor == build.config["CONFIG_USB_DEVICE_VID"]
    assert dev.idProduct == build.config["CONFIG_USB_DEVICE_PID"]
    assert dev.bNumConfigurations == 1
    assert (
        _string(dev, dev.iManufacturer)
        == build.config["CONFIG_USB_DEVICE_MANUFACTURER"]
    )
    assert _string(dev, dev.iProduct) == build.config["CONFIG_USB_DEVICE_PRODUCT"]


def test_configuration_descriptor(usb_function):
    import usb.util

    dev = usb_function.dev
    cfg = dev.get_active_configuration()
    assert cfg.bConfigurationValue == 1
    assert cfg.bNumInterfaces == 1
    intf = cfg[(husb.INTERFACE, 0)]
    assert intf.bInterfaceClass == husb.VENDOR_CLASS
    eps = usb_function.endpoints
    addrs = [ep.bEndpointAddress for ep in eps.values()]
    assert len(set(addrs)) == len(addrs)
    assert all(addr & 0x0F for addr in addrs)
    bulk_mps = 512 if dev.speed == usb.util.SPEED_HIGH else 64
    for name, direction in (
        ("loop_out", usb.util.ENDPOINT_OUT),
        ("loop_in", usb.util.ENDPOINT_IN),
        ("sink", usb.util.ENDPOINT_OUT),
        ("source", usb.util.ENDPOINT_IN),
    ):
        assert usb.util.endpoint_direction(eps[name].bEndpointAddress) == direction
        assert (
            usb.util.endpoint_type(eps[name].bmAttributes)
            == usb.util.ENDPOINT_TYPE_BULK
        )
        assert eps[name].wMaxPacketSize == bulk_mps
    assert (
        usb.util.endpoint_direction(eps["int"].bEndpointAddress) == usb.util.ENDPOINT_IN
    )
    assert (
        usb.util.endpoint_type(eps["int"].bmAttributes) == usb.util.ENDPOINT_TYPE_INTR
    )
    assert eps["int"].wMaxPacketSize == husb.INT_MPS

    # Whole, truncated to the host's wLength, and short of a longer one.
    total = cfg.wTotalLength
    for wlength in (9, total - 1, total, 255):
        data = dev.ctrl_transfer(0x80, 0x06, 0x0200, 0, wlength)
        assert len(data) == min(wlength, total)
        assert data[1] == 0x02


def test_standard_requests(usb_function):
    dev = usb_function.dev
    assert usb_function.configuration() == 1
    assert len(dev.ctrl_transfer(0x80, 0x00, 0, 0, 2)) == 2
    assert bytes(dev.ctrl_transfer(0x81, 0x0A, 0, husb.INTERFACE, 1)) == b"\x00"
    assert bytes(dev.ctrl_transfer(0x80, 0x06, 0x0300, 0, 255)) == b"\x04\x03\x09\x04"


def test_device_qualifier(usb_function):
    """A device running at high speed tells how it would be at full speed, its
    device and its configuration; a full-speed one may stall both."""
    import usb.core
    import usb.util

    dev = usb_function.dev
    try:
        data = bytes(dev.ctrl_transfer(0x80, 0x06, 0x0600, 0, 10))
    except usb.core.USBError as e:
        assert dev.speed != usb.util.SPEED_HIGH
        assert husb.is_stall(e), e
        with pytest.raises(usb.core.USBError) as e:
            dev.ctrl_transfer(0x80, 0x06, 0x0700, 0, 255)
        assert husb.is_stall(e.value), e.value
        return
    assert data == bytes(
        [
            10,
            0x06,
            dev.bcdUSB & 0xFF,
            dev.bcdUSB >> 8,
            dev.bDeviceClass,
            dev.bDeviceSubClass,
            dev.bDeviceProtocol,
            dev.bMaxPacketSize0,
            dev.bNumConfigurations,
            0,
        ]
    )

    cfg = bytes(dev.ctrl_transfer(0x80, 0x06, 0x0200, 0, 255))
    other = bytes(dev.ctrl_transfer(0x80, 0x06, 0x0700, 0, 255))
    assert other[1] == 0x07
    assert len(other) == len(cfg)
    assert other[2:9] == cfg[2:9]
    other_mps = 64 if dev.speed == usb.util.SPEED_HIGH else 512
    eps = []
    off = 9
    while off < len(other):
        if other[off + 1] == 0x05:
            eps.append(other[off : off + 7])
        off += other[off]
    for ep, name in zip(eps, husb.ENDPOINTS, strict=True):
        assert ep[2] == usb_function.ep(name)
        mps = int.from_bytes(ep[4:6], "little")
        assert mps == (husb.INT_MPS if name == "int" else other_mps)


@pytest.mark.parametrize(
    "request_type, req, value, length",
    [
        # Descriptors the device does not have: BOS, a string
        (0x80, 0x06, 0x0F00, 5),
        (0x80, 0x06, 0x03FE, 255),
        # A second configuration, SET_DESCRIPTOR
        (0x80, 0x06, 0x0201, 255),
        (0x00, 0x07, 0x0100, 0),
        # The function's own stall, each way and without data
        (husb.VENDOR_IN, husb.REQ_STALL, 0, 64),
        (husb.VENDOR_OUT, husb.REQ_STALL, 0, 0),
        (husb.VENDOR_OUT, husb.REQ_STALL, 0, 64),
        # More than the function takes
        (husb.VENDOR_OUT, husb.REQ_WRITE, 0, husb.CONTROL_LEN + 1),
        (husb.VENDOR_IN, husb.REQ_READ, husb.CONTROL_LEN + 1, husb.CONTROL_LEN + 1),
    ],
)
def test_control_stall(usb_function, request_type, req, value, length):
    import usb.core

    dev = usb_function.dev
    if request_type & 0x80:
        data_or_length = length
    else:
        data_or_length = bytes(length) if length else None
    index = husb.INTERFACE if request_type & 0x60 else 0
    with pytest.raises(usb.core.USBError) as e:
        dev.ctrl_transfer(request_type, req, value, index, data_or_length)
    assert husb.is_stall(e.value), e.value

    # Endpoint 0 takes the next request.
    assert usb_function.configuration() == 1
    payload = _payload(100, req)
    usb_function.control_write(payload)
    assert usb_function.control_read(100) == payload


@pytest.mark.parametrize("length", CONTROL_LENGTHS)
def test_control_write_read(usb_function, length):
    payload = _payload(length, length)
    assert usb_function.control_write(payload) == length
    assert usb_function.control_read(length) == payload


@pytest.mark.parametrize(
    "length, wlength",
    [(0, 64), (1, 64), (63, 64), (64, 65), (64, 128), (128, 255), (512, 1024)],
)
def test_control_read_short(usb_function, length, wlength):
    """Less than the host asks for: a short packet ends the data stage, a
    zero-length one when the data fills its packets."""
    payload = _payload(husb.CONTROL_LEN, wlength)
    usb_function.control_write(payload)
    assert usb_function.control_read(length, wlength) == payload[:length]
    assert usb_function.configuration() == 1


@pytest.mark.parametrize("length", BULK_LENGTHS)
def test_bulk_loopback(usb_function, length):
    payload = _payload(length, length)
    assert usb_function.loop(payload) == payload
    assert usb_function.stats()["loop_bytes"] == length


def test_bulk_loopback_random(usb_function):
    rng = random.Random(1234)
    total = 0
    for _ in range(200):
        length = rng.randint(0, 2048)
        payload = rng.randbytes(length)
        assert usb_function.loop(payload) == payload, f"{length} bytes"
        total += length
    assert usb_function.stats()["loop_bytes"] == total


def test_source(usb_function, record_property):
    mps = usb_function.bulk_mps
    chunk = 16 * 1024
    data = bytearray()
    start = time.monotonic()
    while len(data) < STREAM_BYTES:
        data += usb_function.source(chunk)
    elapsed = time.monotonic() - start
    assert len(data) % mps == 0
    assert husb.check_pattern(bytes(data), mps) == []

    kbps = len(data) / 1024 / elapsed
    record_property("source_kbps", round(kbps, 1))
    print(f"source: {len(data)} bytes in {elapsed:.3f} s, {kbps:.1f} KiB/s")
    assert kbps >= MIN_THROUGHPUT_KBPS


def test_sink(usb_function, record_property):
    mps = usb_function.bulk_mps
    data = husb.pattern(STREAM_BYTES // mps, mps)
    chunk = 16 * 1024
    start = time.monotonic()
    for off in range(0, len(data), chunk):
        assert usb_function.sink(data[off : off + chunk]) == chunk
    elapsed = time.monotonic() - start

    stats = usb_function.stats()
    assert stats["sink_bytes"] == len(data)
    assert stats["sink_errors"] == 0

    kbps = len(data) / 1024 / elapsed
    record_property("sink_kbps", round(kbps, 1))
    print(f"sink: {len(data)} bytes in {elapsed:.3f} s, {kbps:.1f} KiB/s")
    assert kbps >= MIN_THROUGHPUT_KBPS


def test_interrupt(usb_function):
    counts = []
    start = time.monotonic()
    for _ in range(50):
        packet = usb_function.interrupt()
        assert len(packet) == husb.INT_MPS
        counts.append(int.from_bytes(packet[:4], "little"))
    elapsed = time.monotonic() - start
    assert counts == list(range(counts[0], counts[0] + len(counts)))
    # One packet a frame at most, polled every frame.
    assert elapsed >= 0.040
    assert elapsed < 2.0


@pytest.mark.parametrize("name", ["loop_out", "loop_in"])
def test_halt_loopback(usb_function, name):
    import usb.core

    ep = usb_function.ep(name)
    mps = usb_function.mps(name)
    payload = _payload(100, ep)
    assert usb_function.loop(payload) == payload

    usb_function.set_halt(ep)
    assert usb_function.halted(ep)
    with pytest.raises(usb.core.USBError) as e:
        if ep & 0x80:
            usb_function.dev.read(ep, mps, 1000)
        else:
            usb_function.dev.write(ep, payload, 1000)
    assert husb.is_stall(e.value), e.value

    # The host resets its data toggle as the device does.
    usb_function.dev.clear_halt(ep)
    assert not usb_function.halted(ep)
    for length in (100, mps, 1):
        payload = _payload(length, length + ep)
        assert usb_function.loop(payload) == payload


def test_halt_source_sink(usb_function):
    import usb.core

    mps = usb_function.bulk_mps
    source = usb_function.ep("source")
    sink = usb_function.ep("sink")
    # The source has its next packet in the controller, which the stall flushes.
    before = usb_function.source(8 * mps)
    usb_function.set_halt(source)
    usb_function.set_halt(sink)
    assert usb_function.halted(source)
    assert usb_function.halted(sink)
    with pytest.raises(usb.core.USBError) as e:
        usb_function.source(mps)
    assert husb.is_stall(e.value), e.value
    with pytest.raises(usb.core.USBError) as e:
        usb_function.sink(husb.pattern(1, mps))
    assert husb.is_stall(e.value), e.value

    usb_function.dev.clear_halt(source)
    usb_function.dev.clear_halt(sink)
    assert not usb_function.halted(source)
    assert not usb_function.halted(sink)

    # Nothing lost: the stream goes on where it stopped.
    data = before + usb_function.source(16 * mps)
    assert husb.check_pattern(data, mps) == []
    usb_function.reset_stats()
    usb_function.sink(husb.pattern(16, mps))
    assert usb_function.stats()["sink_bytes"] == 16 * mps
    assert usb_function.stats()["sink_errors"] == 0


def test_set_configuration_cycle(usb_function):
    import usb.util

    dev = usb_function.dev
    for i in range(5):
        usb.util.release_interface(dev, husb.INTERFACE)
        dev.set_configuration(0)
        assert usb_function.configuration() == 0
        dev.set_configuration(1)
        usb.util.claim_interface(dev, husb.INTERFACE)
        assert usb_function.configuration() == 1
        payload = _payload(200 + i, i)
        assert usb_function.loop(payload) == payload


def test_bus_reset(usb_target, usb_function):
    import usb.core
    from harness.helpers.usb import TestFunction

    lb = usb_function
    for i in range(3):
        # The host may re-enumerate the device and drop this handle.
        with contextlib.suppress(usb.core.USBError):
            lb.dev.reset()
        lb.release()
        lb = TestFunction(usb_target.find(timeout=10.0))
        lb.configure()
        assert lb.configuration() == 1
        payload = _payload(300 + i, i)
        assert lb.loop(payload) == payload
    lb.release()
