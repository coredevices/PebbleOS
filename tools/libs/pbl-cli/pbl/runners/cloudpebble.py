# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The CloudPebble proxy, which relays the Developer Connection of a phone
signed in to the same account as `pebble login`."""

import datetime
import json
import os
import platform

import requests
import websocket
from libpebble2.communication.transports.websocket import WebsocketTransport
from libpebble2.communication.transports.websocket.protocol import (
    WebSocketProxyConnectionStatusUpdate,
)
from libpebble2.exceptions import ConnectionError as PebbleConnectionError

PROXY_URL = "wss://cloudpebble-proxy.repebble.com/tool-v2"
PROXY_AUTHENTICATION = 0x09
PROXY_AUTHENTICATION_V2 = 0x19
PROXY_CONNECTION_STATUS = 0x08
PROXY_CONNECTED = 0xFF
PHONE_TIMEOUT = 120
TOKEN_REFRESH_URL = "https://securetoken.googleapis.com/v1/token?key={}"
TOKEN_REFRESH_MARGIN = datetime.timedelta(minutes=2)


class NotLoggedIn(Exception):
    pass


def _credentials_path():
    """Where `pebble login` keeps the account's credentials."""
    if platform.system() == "Darwin":
        sdk_dir = os.path.expanduser("~/Library/Application Support/Pebble SDK")
    elif os.path.exists(os.path.expanduser("~/.pebble-sdk")):
        sdk_dir = os.path.expanduser("~/.pebble-sdk")
    else:
        data_home = os.environ.get("XDG_DATA_HOME") or os.path.expanduser(
            "~/.local/share"
        )
        sdk_dir = os.path.join(data_home, "pebble-sdk")
    return os.path.join(sdk_dir, "oauth_firebase", "firebase_oauth_storage.json")


def token():
    """The account's ID token, refreshed (and saved back) when it expires."""
    path = _credentials_path()
    try:
        with open(path) as f:
            creds = json.load(f)
    except (OSError, ValueError):
        raise NotLoggedIn(path) from None
    if not creds.get("id_token"):
        raise NotLoggedIn(path)

    expires_at = datetime.datetime.fromisoformat(creds.get("expires_at", "1970-01-01"))
    if expires_at.tzinfo is None:
        expires_at = expires_at.replace(tzinfo=datetime.UTC)
    now = datetime.datetime.now(datetime.UTC)
    if now < expires_at - TOKEN_REFRESH_MARGIN:
        return creds["id_token"]

    try:
        r = requests.post(
            TOKEN_REFRESH_URL.format(creds["firebase_api_key"]),
            data={
                "grant_type": "refresh_token",
                "refresh_token": creds["refresh_token"],
            },
            timeout=20,
        )
        r.raise_for_status()
    except (KeyError, requests.RequestException):
        raise NotLoggedIn(path) from None
    refreshed = r.json()
    creds["id_token"] = refreshed["id_token"]
    creds["refresh_token"] = refreshed.get("refresh_token", creds["refresh_token"])
    expires_in = int(refreshed.get("expires_in", 3600))
    creds["expires_at"] = (now + datetime.timedelta(seconds=expires_in)).isoformat()
    with open(path, "w") as f:
        json.dump(creds, f)
    return creds["id_token"]


class ProxyTransport(WebsocketTransport):
    """libpebble2 transport through the proxy: authenticates, then waits for
    the phone to join, and disconnects when the phone leaves."""

    def __init__(self, id_token):
        super().__init__(PROXY_URL)
        self.id_token = id_token.encode()
        self.phone_connected = False

    def connect(self):
        super().connect()
        try:
            self.ws.send_binary(
                bytes([PROXY_AUTHENTICATION_V2])
                + len(self.id_token).to_bytes(2, "big")
                + self.id_token
            )
            if self._recv() != bytes([PROXY_AUTHENTICATION, 0]):
                raise PebbleConnectionError("the proxy rejected the account")
            self.ws.settimeout(PHONE_TIMEOUT)
            while self._recv() != bytes([PROXY_CONNECTION_STATUS, PROXY_CONNECTED]):
                pass
            self.ws.settimeout(None)
        except websocket.WebSocketTimeoutException:
            self.ws.close()
            raise PebbleConnectionError(
                f"the phone did not join in {PHONE_TIMEOUT}s: is the Developer "
                "Connection on in the Pebble app, signed in to the same account?"
            ) from None
        except (websocket.WebSocketException, OSError) as e:
            self.ws.close()
            raise PebbleConnectionError(str(e)) from e
        self.phone_connected = True

    def _recv(self):
        _opcode, message = self.ws.recv_data()
        return bytes(message)

    @property
    def connected(self):
        return super().connected and self.phone_connected

    def read_packet(self):
        target, packet = super().read_packet()
        if (
            isinstance(packet, WebSocketProxyConnectionStatusUpdate)
            and packet.status
            == WebSocketProxyConnectionStatusUpdate.StatusCode.Disconnected
        ):
            self.phone_connected = False
            self.ws.close()
        return target, packet
