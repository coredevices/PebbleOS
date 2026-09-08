# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Weather, as the phone app syncs it: one v4 record per location in the
weather blob DB and the ordered location list in the watch app prefs."""

import math
import random
import struct
import time
import uuid

from pbl.feeds import BlobDb, Feed

PREFS_KEY = b"weatherApp"

DB_VERSION = 4
DB_MINOR_VERSION = 5
DAYS = 7
HOURS = 24

LOCATION_NAMESPACE = uuid.UUID("3c2f5c1e-6b7a-4d0e-9a7e-1b2c3d4e5f60")

# WeatherType, as the firmware numbers it.
(
    PARTLY_CLOUDY,
    CLOUDY,
    LIGHT_SNOW,
    LIGHT_RAIN,
    HEAVY_RAIN,
    HEAVY_SNOW,
    GENERIC,
    SUN,
    SLEET,
) = range(9)

PHRASES = {
    PARTLY_CLOUDY: "Partly Cloudy",
    CLOUDY: "Cloudy",
    LIGHT_SNOW: "Light Snow",
    LIGHT_RAIN: "Light Rain",
    HEAVY_RAIN: "Heavy Rain",
    HEAVY_SNOW: "Heavy Snow",
    GENERIC: "Mixed",
    SUN: "Sunny",
    SLEET: "Sleet",
}

WMO_CODES = {
    PARTLY_CLOUDY: 2,
    CLOUDY: 3,
    LIGHT_SNOW: 71,
    LIGHT_RAIN: 61,
    HEAVY_RAIN: 65,
    HEAVY_SNOW: 75,
    GENERIC: 45,
    SUN: 0,
    SLEET: 68,
}

# How much of the clear-sky UV gets through each kind of sky.
SKY_UV = {SUN: 1.0, PARTLY_CLOUDY: 0.7, CLOUDY: 0.4, GENERIC: 0.3}

# Each city is a weather regime: metric temperatures and wind, the sky types
# it cycles through, and how wet, windy and hazy it tends to be.
CITIES = {
    "Barcelona": {
        "lat": 41.39,
        "lon": 2.17,
        "utc": 120,
        "temp": 26,
        "swing": 8,
        "types": [SUN, SUN, PARTLY_CLOUDY],
        "precip": 5,
        "wind": 12,
        "humidity": 55,
        "uv": 8,
    },
    "New York": {
        "lat": 40.71,
        "lon": -74.01,
        "utc": -240,
        "temp": 18,
        "swing": 7,
        "types": [LIGHT_RAIN, HEAVY_RAIN, CLOUDY],
        "precip": 80,
        "wind": 28,
        "humidity": 90,
        "uv": 2,
    },
    "Tokyo": {
        "lat": 35.68,
        "lon": 139.69,
        "utc": 540,
        "temp": 22,
        "swing": 6,
        "types": [CLOUDY, PARTLY_CLOUDY, LIGHT_RAIN],
        "precip": 40,
        "wind": 18,
        "humidity": 75,
        "uv": 4,
    },
    "Reykjavik": {
        "lat": 64.15,
        "lon": -21.94,
        "utc": 0,
        "temp": -3,
        "swing": 4,
        "types": [HEAVY_SNOW, LIGHT_SNOW, CLOUDY],
        "precip": 70,
        "wind": 45,
        "humidity": 85,
        "uv": 1,
    },
    "Sydney": {
        "lat": -33.87,
        "lon": 151.21,
        "utc": 600,
        "temp": 15,
        "swing": 6,
        "types": [PARTLY_CLOUDY, SUN, LIGHT_RAIN],
        "precip": 25,
        "wind": 22,
        "humidity": 60,
        "uv": 5,
    },
}

# WeatherDBEntry up to (excluding) the trailing strings, little-endian, packed.
FIXED = struct.Struct(
    "<BhBhhBhhIBBhhhHHhhB"  # v3 prefix + v4.0 scalars
    + "hhB" * DAYS  # daily[]
    + "B"
    + "B" * HOURS
    + "b" * HOURS  # today's hourly series
    + "h"
    + "BBB" * DAYS  # v4.1: utc offset, daily_metrics[]
    + "BBHH"
    + "h" * DAYS  # v4.2: warning readings, daily_feels_like[]
    + "h"
    + "h" * DAYS  # v4.3: wind direction
    + "B" * HOURS  # v4.4: hourly UV
    + "B"
    + "B" * HOURS
    + "b" * HOURS  # v4.5: tomorrow's hourly series
)
assert FIXED.size == 250


def _day(rng, city, offset):
    """One day's forecast: dominant sky, hourly sky and temperature, UV."""
    dominant = rng.choice(city["types"])
    base = city["temp"] + rng.uniform(-3, 3) + offset * rng.uniform(-1, 1)
    types, temps, uvs = [], [], []
    sky = dominant
    for hour in range(HOURS):
        if rng.random() < 0.15:
            sky = rng.choice(city["types"])
        types.append(sky)
        temps.append(
            round(
                base - city["swing"] / 2 * math.cos(2 * math.pi * (hour - 16) / HOURS)
            )
        )
        clear_sky = max(0.0, math.sin(math.pi * (hour - 6) / 12))
        uvs.append(city["uv"] * clear_sky * SKY_UV.get(sky, 0.2))
    return {
        "type": dominant,
        "types": types,
        "temps": temps,
        "uvs": uvs,
        "precip": min(100, max(0, city["precip"] + rng.randint(-20, 20))),
        "wind": max(0, city["wind"] + rng.randint(-8, 8)),
        "wind_dir": rng.randint(0, 359),
    }


def _to_f(celsius):
    return round(celsius * 9 / 5 + 32)


def _to_mph(kmh):
    return round(kmh / 1.609)


def simulate(name, rng, now, current, fahrenheit):
    """The blob DB value for one city as the phone would build it now."""
    city = CITIES[name]
    days = [_day(rng, city, offset) for offset in range(DAYS)]
    today, tomorrow = days[0], days[1]
    local_hour = int(((now + city["utc"] * 60) % 86400) // 3600)

    temp = _to_f if fahrenheit else (lambda c: c)
    wind = _to_mph if fahrenheit else (lambda k: k)

    sky = today["types"][local_hour]
    wet = sky in (LIGHT_RAIN, HEAVY_RAIN, LIGHT_SNOW, HEAVY_SNOW, SLEET)
    heavy = sky in (HEAVY_RAIN, HEAVY_SNOW)
    current_temp = today["temps"][local_hour]
    feels = current_temp - (today["wind"] // 10 if current_temp < 10 else 0)

    fields = [
        DB_VERSION,
        temp(current_temp),
        sky,
        temp(max(today["temps"])),
        temp(min(today["temps"])),
        tomorrow["type"],
        temp(max(tomorrow["temps"])),
        temp(min(tomorrow["temps"])),
        int(now),
        current,
        DB_MINOR_VERSION,
        temp(feels),
        round(max(today["uvs"]) * 10),
        today["precip"],
        wind(today["wind"]),
        today["wind_dir"],
        round(city["lat"] * 100),
        round(city["lon"] * 100),
        DAYS,
    ]
    for day in days:
        fields += [temp(max(day["temps"])), temp(min(day["temps"])), day["type"]]
    fields += [HOURS, *today["types"], *(temp(t) for t in today["temps"])]
    fields.append(city["utc"])
    for day in days:
        fields += [day["precip"], wind(day["wind"]), round(max(day["uvs"]) * 10)]
    fields += [
        WMO_CODES[sky],
        min(100, city["humidity"] + (10 if wet else 0)),
        2000 if heavy else 8000 if wet else 20000,
        (12 if heavy else 3) if wet else 0,
    ]
    fields += [temp(max(day["temps"]) - 1) for day in days]
    fields.append(today["wind_dir"])
    fields += [day["wind_dir"] for day in days]
    fields += [round(uv * 10) for uv in today["uvs"]]
    fields += [HOURS, *tomorrow["types"], *(temp(t) for t in tomorrow["temps"])]

    strings = b"".join(
        struct.pack("<H", len(s)) + s for s in (name.encode(), PHRASES[sky].encode())
    )
    return FIXED.pack(*fields) + struct.pack("<H", len(strings)) + strings


def location_key(name):
    return uuid.uuid5(LOCATION_NAMESPACE, name)


class Weather(Feed):
    name = "weather"
    help = "A forecast for a few cities"
    description = (
        "Write a forecast for each CITY, the first one being the current "
        "location. Cities: " + ", ".join(CITIES) + "."
    )

    def add_arguments(self, parser):
        parser.add_argument(
            "cities",
            metavar="CITY",
            nargs="*",
            help="Cities to push, in order (default: all of them)",
        )
        parser.add_argument(
            "--fahrenheit",
            action="store_true",
            help="Send temperatures in Fahrenheit and wind in mph",
        )
        parser.add_argument(
            "--seed", type=int, help="Seed for the forecast generator (default: random)"
        )
        parser.add_argument(
            "--clear", action="store_true", help="Remove every location instead"
        )

    def run(self, args, watch, inf):
        for name in args.cities:
            if name not in CITIES:
                raise ValueError(
                    f"unknown city {name!r}; pick from {', '.join(CITIES)}"
                )
        cities = [] if args.clear else (args.cities or list(CITIES))
        seed = args.seed if args.seed is not None else random.randrange(1 << 32)
        now = time.time()

        if args.clear:
            watch.blobdb_clear(BlobDb.WEATHER)
        for index, name in enumerate(cities):
            record = simulate(
                name, random.Random(f"{seed}:{name}"), now, index == 0, args.fahrenheit
            )
            watch.blobdb_insert(BlobDb.WEATHER, location_key(name), record)
        prefs = bytes([len(cities)]) + b"".join(location_key(n).bytes for n in cities)
        watch.blobdb_insert(BlobDb.WATCH_APP_PREFS, PREFS_KEY, prefs)

        if cities:
            inf(f"pushed {', '.join(cities)} (seed {seed})")
        else:
            inf("cleared every weather location")
