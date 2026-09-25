# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A phone, as far as the watch can tell: Bluetooth LE through a Bumble
controller (e.g. an nRF52840 dongle running Zephyr's hci_uart).

:class:`BleLink` connects to the watch, pairs or re-encrypts with a bond it
keeps, and opens a reversed PPoGATT session carrying the Pebble Protocol.
Bumble runs its own asyncio loop in a thread; the rest of the harness is
threaded, so the public methods block.
"""

import asyncio
import logging
import os
import threading

from harness.ble.ppogatt import PPoGATT
from harness.errors import HarnessError, WatchTimeout

logger = logging.getLogger(__name__)

# The UART of Zephyr's hci_uart sample; the USB CDC port ignores it.
HCI_UART_BAUDRATE = 1000000
#: Watch to connect to: the first bonded, or advertising the Pebble pairing
#: service (reconnection advertising leaves it out).
AUTO = "auto"
# The host's identity: a static random address, so bonds survive restarts.
HOST_ADDRESS = "F0:BB:1E:00:00:01"
HOST_NAME = "pbl-itest"

PEBBLE_UUID_BASE = "-328E-0FBB-C642-1AA6699BDADA"
PAIRING_SERVICE = "FED9"
CONNECTIVITY_CHARACTERISTIC = "00000001" + PEBBLE_UUID_BASE
PPOG_SERVICE = "40000000" + PEBBLE_UUID_BASE
PPOG_NOTIFY_CHARACTERISTIC = "40000001" + PEBBLE_UUID_BASE
PPOG_WRITE_CHARACTERISTIC = "40000003" + PEBBLE_UUID_BASE
PPOG_FORWARD_SERVICE = "10000000" + PEBBLE_UUID_BASE
PPOG_FORWARD_DATA_CHARACTERISTIC = "10000001" + PEBBLE_UUID_BASE
PPOG_FORWARD_META_CHARACTERISTIC = "10000002" + PEBBLE_UUID_BASE
# PPoGATT versions 0 to 1, the system app, a session inferred from it.
PPOG_FORWARD_META = bytes([0, 1]) + bytes(16) + bytes([0])

#: PPoGATT service hosted by the watch, the phone starting the session.
REVERSED = "reversed"
#: PPoGATT service hosted by the phone, the watch starting the session.
FORWARD = "forward"

REQUESTED_MTU = 339
TICK_S = 0.5
SCAN_TIMEOUT_S = 15.0
CONNECT_TIMEOUT_S = 15.0
CONNECT_ATTEMPTS = 3
# The link's parameters (ms), kept for the whole connection.
CONNECTION_INTERVAL_MS = 15
SUPERVISION_TIMEOUT_MS = 6000
ESTABLISH_S = 0.3
PAIRING_TIMEOUT_S = 40.0
SESSION_TIMEOUT_S = 15.0


def controller_transport(controller):
    """The Bumble transport of a controller: an H4 controller's serial port
    (e.g. an nRF52840 dongle running Zephyr's hci_uart), or any Bumble
    transport spec."""
    if controller.startswith("/dev/") or controller.upper().startswith("COM"):
        return f"serial:{controller},{HCI_UART_BAUDRATE}"
    return controller


class Connectivity:
    """The watch's Connectivity Status characteristic."""

    def __init__(self, value):
        flags = value[0] if value else 0
        self.connected = bool(flags & 0x01)
        self.paired = bool(flags & 0x02)
        self.encrypted = bool(flags & 0x04)
        self.has_bonded_gateway = bool(flags & 0x08)
        self.pairing_error = value[3] if len(value) > 3 else 0

    def __repr__(self):
        return (
            f"Connectivity(connected={self.connected}, paired={self.paired}, "
            f"encrypted={self.encrypted}, gateway={self.has_bonded_gateway}, "
            f"error={self.pairing_error:#x})"
        )


class BleLink:
    """``watch`` is the watch's address, its advertised name (e.g.
    ``Pebble 24F0``), or ``auto`` for the first watch bonded or advertising.
    ``confirm_pairing()``, when given, is called when the watch asks its user
    to confirm a pairing, and should confirm it (press Up); without it the
    confirmation has to be done by hand. ``address`` and ``name`` are the
    host's identity: another one is another phone to the watch. ``ppogatt``
    is :data:`REVERSED` or :data:`FORWARD`."""

    def __init__(
        self,
        watch,
        controller,
        keystore=None,
        confirm_pairing=None,
        address=HOST_ADDRESS,
        name=HOST_NAME,
        ppogatt=REVERSED,
    ):
        if ppogatt not in (REVERSED, FORWARD):
            raise HarnessError(f"no PPoGATT mode {ppogatt!r}")
        self.watch = watch
        self.ppogatt = ppogatt
        self.address = address
        self.name = name
        self.controller = controller_transport(controller)
        self.keystore = keystore or os.path.join(
            os.path.expanduser("~"), ".cache", "pbl-itest", "ble-keys.json"
        )
        self.confirm_pairing = confirm_pairing
        self.on_data = None
        self.on_disconnect = None
        self.connectivity = None
        self._loop = None
        self._thread = None
        self._transport = None
        self._device = None
        self._connection = None
        self._peer = None
        self._write_characteristic = None
        self._data_characteristic = None
        self._ppog = None
        self._session_open = None
        self._ticker = None

    # --- event loop ---------------------------------------------------------

    def _run(self, coro, timeout):
        future = asyncio.run_coroutine_threadsafe(coro, self._loop)
        try:
            return future.result(timeout)
        except TimeoutError:
            future.cancel()
            raise WatchTimeout(f"BLE operation timed out after {timeout}s") from None

    def _start_loop(self):
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(target=self._loop.run_forever, daemon=True)
        self._thread.start()

    # --- lifecycle ----------------------------------------------------------

    def open(self, timeout=60.0):
        """Connect, pair or encrypt, and open the PPoGATT session."""
        if self._loop is None:
            self._start_loop()
        self._run(self._open(), timeout)

    def close(self):
        if self._loop is None:
            return
        try:
            self._run(self._close(), 10)
        finally:
            self._loop.call_soon_threadsafe(self._loop.stop)
            self._thread.join(timeout=5)
            self._loop = None

    def forget(self):
        """Drop the bond the host keeps for the watch."""
        if os.path.exists(self.keystore):
            os.unlink(self.keystore)

    async def _power_on(self):
        from bumble.device import Device, DeviceConfiguration
        from bumble.hci import Address
        from bumble.keys import JsonKeyStore
        from bumble.pairing import PairingConfig, PairingDelegate
        from bumble.transport import open_transport

        self._transport = await open_transport(self.controller)
        config = DeviceConfiguration(name=self.name, address=Address(self.address))
        device = Device.from_config_with_hci(
            config, self._transport.source, self._transport.sink
        )
        os.makedirs(os.path.dirname(self.keystore), exist_ok=True)
        device.keystore = JsonKeyStore(namespace=self.address, filename=self.keystore)

        link = self

        class Delegate(PairingDelegate):
            def __init__(self):
                super().__init__(
                    io_capability=PairingDelegate.IoCapability.DISPLAY_OUTPUT_AND_YES_NO_INPUT
                )

            async def compare_numbers(self, number, digits):
                logger.info("pairing: confirm %0*d on the watch", digits, number)
                if link.confirm_pairing is not None:
                    # Let the watch show its prompt before confirming it.
                    await asyncio.sleep(1.0)
                    await asyncio.get_running_loop().run_in_executor(
                        None, link.confirm_pairing
                    )
                return True

            async def confirm(self, auto=False):
                return True

        device.pairing_config_factory = lambda connection: PairingConfig(
            sc=True, mitm=True, bonding=True, delegate=Delegate()
        )
        self._keep_connection_parameters(device)
        if self.ppogatt == FORWARD:
            device.add_service(self._forward_service())
        await device.power_on()
        self._device = device

    @staticmethod
    def _keep_connection_parameters(device):
        """Decline the watch's connection parameter updates, keeping the
        link at what it was opened with: updates stall the watch's sending
        for seconds, and some fail on their instant (0x28), dropping the
        link. What a connected watch draws is not what these tests measure."""
        from bumble import hci, l2cap, utils

        host = device.host
        manager = device.l2cap_channel_manager

        def on_ll_request(event):
            utils.AsyncRunner.spawn(
                host.send_sync_command(
                    hci.HCI_LE_Remote_Connection_Parameter_Request_Negative_Reply_Command(
                        connection_handle=event.connection_handle,
                        reason=hci.HCI_UNACCEPTABLE_CONNECTION_PARAMETERS_ERROR,
                    )
                )
            )

        def on_l2cap_request(connection, cid, request):
            manager.send_control_frame(
                connection,
                cid,
                l2cap.L2CAP_Connection_Parameter_Update_Response(
                    identifier=request.identifier,
                    result=l2cap.L2CAP_CONNECTION_PARAMETERS_REJECTED_RESULT,
                ),
            )

        host.on_hci_le_remote_connection_parameter_request_event = on_ll_request
        manager.on_l2cap_connection_parameter_update_request = on_l2cap_request

    def _matches(self, adv, bonded):
        from bumble.core import UUID, AdvertisingData

        if self.watch == AUTO:
            if str(adv.address) in bonded:
                return True
            uuids = (
                adv.data.get(
                    AdvertisingData.COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS
                )
                or []
            )
            return UUID(PAIRING_SERVICE) in uuids
        if ":" in self.watch:
            return str(adv.address).split("/")[0].upper() == self.watch.upper()
        name = adv.data.get(AdvertisingData.COMPLETE_LOCAL_NAME) or adv.data.get(
            AdvertisingData.SHORTENED_LOCAL_NAME
        )
        return name == self.watch

    async def _resolve_address(self):
        """The advertised address (and its type) of the watch."""
        found = asyncio.get_running_loop().create_future()
        bonded = {name for name, _ in await self._device.keystore.get_all()}

        def on_advertisement(adv):
            if not found.done() and self._matches(adv, bonded):
                found.set_result(adv.address)

        self._device.on("advertisement", on_advertisement)
        await self._device.start_scanning(active=True)
        try:
            return await asyncio.wait_for(found, SCAN_TIMEOUT_S)
        except TimeoutError:
            raise WatchTimeout(f"no watch advertising as {self.watch!r}") from None
        finally:
            self._device.remove_listener("advertisement", on_advertisement)
            await self._device.stop_scanning()

    def _forward_service(self):
        """The phone's PPoGATT service, which the watch finds once the link is
        encrypted."""
        from bumble.gatt import Characteristic, CharacteristicValue, Service

        def on_write(connection, value):
            if self._ppog is not None:
                self._ppog.receive(bytes(value))

        self._data_characteristic = Characteristic(
            PPOG_FORWARD_DATA_CHARACTERISTIC,
            Characteristic.NOTIFY | Characteristic.WRITE_WITHOUT_RESPONSE,
            Characteristic.WRITEABLE,
            CharacteristicValue(write=on_write),
        )
        meta = Characteristic(
            PPOG_FORWARD_META_CHARACTERISTIC,
            Characteristic.READ,
            Characteristic.READABLE,
            PPOG_FORWARD_META,
        )
        return Service(PPOG_FORWARD_SERVICE, [self._data_characteristic, meta])

    async def _open(self):
        from bumble.device import Peer

        if self._device is None:
            await self._power_on()
        address = await self._resolve_address()
        self._connection = await self._connect(address)
        self._connection.on("disconnection", self._on_disconnection)
        self._peer = Peer(self._connection)
        mtu = await self._peer.request_mtu(REQUESTED_MTU)

        await self._peer.discover_services()
        self.connectivity = await self._read_connectivity()
        logger.info("BLE: %s", self.connectivity)

        # Forward, the watch starts the session as soon as the link is
        # encrypted.
        self._new_session(mtu)
        await self._secure(address)
        if not self._connection.is_encrypted:
            raise HarnessError("the link to the watch is not encrypted")

        if self.ppogatt == REVERSED:
            await self._open_reversed_ppogatt()
        try:
            await asyncio.wait_for(self._session_open, SESSION_TIMEOUT_S)
        except TimeoutError:
            raise WatchTimeout("the watch did not open the PPoGATT session") from None
        logger.info("BLE: %s PPoGATT session open (MTU %d)", self.ppogatt, mtu)

    async def _connect(self, address):
        from bumble.core import ConnectionError as BumbleConnectionError
        from bumble.core import TimeoutError as BumbleTimeoutError
        from bumble.device import ConnectionParametersPreferences
        from bumble.hci import Phy

        for attempt in range(1, CONNECT_ATTEMPTS + 1):
            logger.info("BLE: connecting to %s (attempt %d)", address, attempt)
            try:
                connection = await self._device.connect(
                    address,
                    connection_parameters_preferences={
                        Phy.LE_1M: ConnectionParametersPreferences(
                            connection_interval_min=CONNECTION_INTERVAL_MS,
                            connection_interval_max=CONNECTION_INTERVAL_MS,
                            max_latency=0,
                            supervision_timeout=SUPERVISION_TIMEOUT_MS,
                        )
                    },
                    timeout=CONNECT_TIMEOUT_S,
                )
            except (BumbleConnectionError, BumbleTimeoutError) as e:
                if attempt == CONNECT_ATTEMPTS:
                    raise HarnessError(f"cannot connect to {address}: {e}") from e
                continue
            # A link that fails to establish drops right away (0x3e).
            await asyncio.sleep(ESTABLISH_S)
            if connection.handle in self._device.connections:
                return connection
        raise HarnessError(f"no stable connection to {address}")

    async def _secure(self, address):
        """Encrypt with the kept bond, or pair when there is none or the
        watch no longer has its side (e.g. its filesystem was erased)."""
        from bumble.core import BaseBumbleError
        from bumble.device import Peer

        keystore = self._device.keystore
        if await keystore.get(str(address)) is not None:
            try:
                await asyncio.wait_for(self._connection.encrypt(), PAIRING_TIMEOUT_S)
                return
            except (BaseBumbleError, TimeoutError) as e:
                logger.info(
                    "BLE: the watch refused the kept bond (%s); pairing again", e
                )
                await keystore.delete(str(address))
            if self._connection is None:
                self._connection = await self._connect(address)
                self._connection.on("disconnection", self._on_disconnection)
                self._peer = Peer(self._connection)
                await self._peer.discover_services()
        await asyncio.wait_for(self._connection.pair(), PAIRING_TIMEOUT_S)

    async def _read_connectivity(self):
        from bumble.core import UUID

        services = self._peer.get_services_by_uuid(UUID(PAIRING_SERVICE))
        if not services:
            return None
        await services[0].discover_characteristics()
        characteristics = services[0].get_characteristics_by_uuid(
            UUID(CONNECTIVITY_CHARACTERISTIC)
        )
        if not characteristics:
            return None
        return Connectivity(bytes(await characteristics[0].read_value()))

    def _new_session(self, mtu):
        self._session_open = asyncio.get_running_loop().create_future()
        self._ppog = PPoGATT(
            write=self._write,
            on_data=self._deliver,
            on_open=lambda: (
                self._session_open.done() or self._session_open.set_result(True)
            ),
        )
        self._ppog.mtu = mtu
        self._ticker = asyncio.ensure_future(self._tick())

    async def _open_reversed_ppogatt(self):
        from bumble.core import UUID

        services = self._peer.get_services_by_uuid(UUID(PPOG_SERVICE))
        if not services:
            raise HarnessError("the watch has no reversed PPoGATT service")
        service = services[0]
        await service.discover_characteristics()
        notify = service.get_characteristics_by_uuid(UUID(PPOG_NOTIFY_CHARACTERISTIC))[
            0
        ]
        self._write_characteristic = service.get_characteristics_by_uuid(
            UUID(PPOG_WRITE_CHARACTERISTIC)
        )[0]
        await notify.subscribe(lambda value: self._ppog.receive(bytes(value)))
        self._ppog.reset()

    async def _tick(self):
        while True:
            await asyncio.sleep(TICK_S)
            if self._ppog is not None:
                self._ppog.tick()

    def _write(self, packet):
        logger.debug(
            "tx type=%d sn=%d len=%d", packet[0] & 7, packet[0] >> 3, len(packet) - 1
        )
        if self.ppogatt == FORWARD:
            asyncio.ensure_future(
                self._device.notify_subscriber(
                    self._connection, self._data_characteristic, packet
                )
            )
            return
        asyncio.ensure_future(
            self._peer.write_value(
                self._write_characteristic, packet, with_response=False
            )
        )

    def _deliver(self, data):
        if self.on_data is not None:
            self.on_data(data)

    def _on_disconnection(self, reason):
        logger.info("BLE: disconnected (reason %#x)", reason)
        self._connection = None
        if self._ticker is not None:
            self._ticker.cancel()
            self._ticker = None
        if self.on_disconnect is not None:
            self.on_disconnect()

    async def _close(self):
        if self._ticker is not None:
            self._ticker.cancel()
            self._ticker = None
        if self._connection is not None:
            try:
                await self._connection.disconnect()
            except Exception:
                logger.debug("BLE: disconnect failed", exc_info=True)
        if self._transport is not None:
            await self._transport.close()
            self._transport = None
        self._device = None

    # --- data ---------------------------------------------------------------

    def send(self, data):
        """Queue Pebble Protocol bytes for the watch."""
        self._loop.call_soon_threadsafe(self._ppog.send, bytes(data))

    @property
    def is_connected(self):
        return (
            self._connection is not None
            and self._ppog is not None
            and self._ppog.is_open
        )
