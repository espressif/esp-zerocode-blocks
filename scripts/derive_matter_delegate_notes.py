#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

"""Derive each Matter device_type block's wiring need from authoritative upstream.

A device_type block that is a stub (``matter_wiring: endpoint_only``) carries a
``matter_wiring_note`` saying what it still needs — a cluster delegate, a client
binding, or plain attribute wiring. That fact is owned by upstream, not by us, so
it must not be hand-maintained: it drifts the moment the spec or esp-matter moves.
This script rederives it, and with ``--write`` rewrites the note in place.

Two sources of truth, both pinned to a ref you pass in:

1. connectedhomeip device-type XMLs — each device type's clusters, their side
   (server/client) and conformance (mandatory/optional). Fetched from
   raw.githubusercontent at ``--chip-ref`` (default ``v1.6-branch``) /
   ``--dm-version`` (default ``1.6``), or read from a local ``--chip-dir``.

2. The clusters esp-matter exposes a delegate for — its ``*DelegateInitCB`` table.
   Pass ``--esp-matter-dir`` (an esp-matter checkout) to derive it live, the same
   way the XMLs track ``--chip-ref``. Without it the embedded snapshot
   (DELEGATE_CLUSTERS_RAW, taken from DELEGATE_CLUSTERS_SOURCE) is used and the
   report says so. When you move esp-matter releases, pass the checkout rather
   than trusting the snapshot.

Every run prints where both inputs came from, so a stale input is visible rather
than silent.

Classification per device type, in order:
  - a server cluster that is delegate-backed  -> needs a DELEGATE (mandatory ones
    are required, optional ones are opt-in);
  - else a mandatory non-global CLIENT cluster -> a client device, needs a BINDING;
  - else a mandatory non-global server cluster -> plain ATTRIBUTE wiring;
  - else only global clusters                  -> a COMPOSITE/aggregator endpoint.

Usage:
    python3 scripts/derive_matter_delegate_notes.py [--write] [--only-stubs]
            [--chip-ref v1.6-branch] [--dm-version 1.6] [--chip-dir PATH]
            [--esp-matter-dir PATH]
"""
from __future__ import annotations

import argparse
import re
import sys
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEVICE_TYPES = ROOT / "code_blocks" / "device_types"

# Embedded snapshot of esp-matter's delegate set, used only when --esp-matter-dir
# is not given. Record where it came from so a report can say so.
DELEGATE_CLUSTERS_SOURCE = "esp-matter release/v1.6 @ 31b76ad14"
DELEGATE_CALLBACKS_FILE = "components/esp_matter/data_model/esp_matter_delegate_callbacks.cpp"
DELEGATE_CLUSTERS_RAW = """
AccountLogin Actions ActivatedCarbonFilterMonitoring ApplicationBasic
ApplicationLauncher AudioOutput BooleanStateConfiguration CameraAvStreamManagement
Channel Chime ClosureControl ClosureDimension CommissionerControl CommodityPrice
CommodityTariff ContentAppObserver ContentControl DeviceEnergyManagement
DeviceEnergyManagementMode DiagnosticLogs DishwasherAlarm DishwasherMode DishWasherMode
DoorLock ElectricalEnergyMeasurement ElectricalGridConditions ElectricalPowerMeasurement
EnergyEvse EnergyEvseMode EnergyPreference FanControl HepaFilterMonitoring KeypadInput
LaundryDryerControls LaundryWasherControls LaundryWasherMode LowPower MediaInput
MediaPlayback Messages MeterIdentification MicrowaveOvenControl MicrowaveOvenMode
ModeSelect OperationalState OtaSoftwareUpdateProvider OvenCavityOperationalState OvenMode
PowerTopology PushAvStreamTransport RefrigeratorAndTCCMode
RefrigeratorAndTemperatureControlledCabinetMode RvcCleanMode RvcOperationalState
RvcRunMode ServiceArea SmokeCoAlarm TargetNavigator TemperatureControl Thermostat
ThreadBorderRouterManagement TimeSynchronization TlsCertificateManagement
TlsClientManagement ValveConfigurationAndControl WakeOnLan WaterHeaterManagement
WaterHeaterMode WebRTCTransportProvider WindowCovering
"""


def norm(name: str) -> str:
    return re.sub(r"[^a-z0-9]", "", name.lower())


def load_delegate_clusters(esp_matter_dir: str | None) -> tuple[set[str], str]:
    """-> (normalised cluster names, human-readable provenance).

    Derives from the esp-matter checkout's *DelegateInitCB table when a dir is
    given; otherwise falls back to the embedded snapshot.
    """
    if esp_matter_dir:
        src = Path(esp_matter_dir) / DELEGATE_CALLBACKS_FILE
        if not src.exists():
            sys.exit(f"--esp-matter-dir: {src} not found")
        names = set(re.findall(r"^void ([A-Za-z]+)DelegateInitCB", src.read_text(), re.M))
        return {norm(n) for n in names}, f"derived from {src}"
    return ({norm(c) for c in DELEGATE_CLUSTERS_RAW.split()},
            f"embedded snapshot ({DELEGATE_CLUSTERS_SOURCE}) — pass --esp-matter-dir "
            f"to derive from a checkout")


DELEGATE_CLUSTERS: set[str] = set()  # filled in main() from load_delegate_clusters

# Clusters that are structural, not a device's reason to exist — excluded when
# deciding what a device type "needs".
GLOBAL_CLUSTERS = {
    norm(c) for c in [
        "Identify", "Groups", "Scenes Management", "Scenes", "Descriptor",
        "Binding", "Fixed Label", "User Label", "Localization Configuration",
    ]
}

# Clusters esp-matter exposes a delegate for but that are normally driven by
# plain attribute writes — the delegate only serves an optional feature or
# command. Having a DelegateInitCB is not the same as requiring one; the wired
# fan block, for instance, drives FanControl with attribute::update and no
# delegate at all. esp-matter semantic knowledge, so a maintained set.
FEATURE_GATED_DELEGATES = {
    norm("Fan Control"): "the Step command",
    norm("Temperature Control"): "the TemperatureLevel feature",
    norm("Thermostat"): "presets and schedules",
}

# esp-matter endpoint name -> connectedhomeip device-type XML basename.
# Default is CamelCase of the snake_case name; only the irregular ones are listed.
CHIP_XML_OVERRIDES = {
    "dish_washer": "Dishwasher",
    "energy_evse": "EVSE",
    "mode_select": "ModeSelectDeviceType",
}


def chip_xml_name(endpoint: str) -> str:
    if endpoint in CHIP_XML_OVERRIDES:
        return CHIP_XML_OVERRIDES[endpoint]
    return "".join(w.capitalize() for w in endpoint.split("_"))


def matter_endpoint(block_yml: Path) -> str | None:
    """Read device_types.matter without a YAML dep (keeps comments/format intact)."""
    in_dt = False
    for line in block_yml.read_text().splitlines():
        if re.match(r"^device_types:", line):
            in_dt = True
            continue
        if in_dt:
            if re.match(r"^\S", line):
                break
            m = re.match(r"\s+matter:\s*(\S+)", line)
            if m:
                return m.group(1)
    return None


def fetch_xml(name: str, args) -> str | None:
    if args.chip_dir:
        p = Path(args.chip_dir) / f"{name}.xml"
        return p.read_text() if p.exists() else None
    url = (
        f"https://raw.githubusercontent.com/project-chip/connectedhomeip/"
        f"{args.chip_ref}/data_model/{args.dm_version}/device_types/{name}.xml"
    )
    try:
        return urllib.request.urlopen(url, timeout=20).read().decode()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None  # genuinely absent on this ref (or a name mismatch)
        sys.exit(f"fetch {url}: HTTP {e.code}")
    except urllib.error.URLError as e:
        sys.exit(f"fetch {url}: {e.reason} — network problem, not a missing file")


def conformance(cluster_el) -> str:
    """mandatory | optional | disallow — the cluster's first-level conform tag."""
    for child in cluster_el:
        tag = child.tag.lower()
        if tag.endswith("conform"):
            if tag.startswith("mandatory"):
                return "mandatory"
            if tag.startswith("disallow") or tag.startswith("deprecate"):
                return "disallow"
            return "optional"  # optional / otherwise / provisional
    return "optional"


def parse_device(xml: str):
    """-> (own_clusters, composed_names).

    own_clusters: (name, side, conformance) under the device type's OWN <clusters>
    (they carry a side). composed_names: device types this one composes (their
    clusters — and delegates — belong to those endpoints, not this one), read from
    nested <deviceType deviceTypeName=...> refs.
    """
    root = ET.fromstring(xml)
    own = []
    clusters_el = root.find("clusters")
    if clusters_el is not None:
        for cl in clusters_el.findall("cluster"):
            name, side = cl.get("name"), cl.get("side")
            if not name or not side or norm(name) in GLOBAL_CLUSTERS:
                continue
            own.append((name, side, conformance(cl)))
    # Composed device types appear two ways: a sibling <deviceType deviceTypeName=…>
    # (e.g. Electrical Sensor on solar/EVSE) or a <conditionRequirements><deviceType
    # name=…> (e.g. Temperature Controlled Cabinet on oven/fridge). Both are nested
    # deviceType elements other than the root.
    composed = []
    for dt in root.iter("deviceType"):
        if dt is root:
            continue
        nm = dt.get("deviceTypeName") or dt.get("name")
        if nm:
            composed.append(nm)
    return own, composed


def _composed_txt(endpoint: str, composed) -> str:
    blocks = sorted({re.sub(r"\s+", "_", c.lower()) for c in composed
                     if norm(c) != norm(endpoint)})
    if not blocks:
        return ""
    return f"; composes {', '.join(blocks)} (their delegates live on those endpoints)"


def classify(endpoint: str, own, composed) -> str:
    server = [(n, c) for (n, s, c) in own if s == "server" and c != "disallow"]
    client = [(n, c) for (n, s, c) in own if s == "client" and c != "disallow"]
    comp = _composed_txt(endpoint, composed)

    # A delegate cluster REQUIRES one only if its delegate is not feature-gated;
    # a feature-gated one is attribute-wired, with the delegate as an extra.
    required = [(n, c) for (n, c) in server
                if norm(n) in DELEGATE_CLUSTERS and norm(n) not in FEATURE_GATED_DELEGATES]
    gated = [n for (n, c) in server if norm(n) in FEATURE_GATED_DELEGATES]
    attr_only = [n for (n, c) in server
                 if c == "mandatory" and norm(n) not in DELEGATE_CLUSTERS]

    parts = []
    mand = [n for (n, c) in required if c == "mandatory"]
    opt = [n for (n, c) in required if c != "mandatory"]
    if mand:
        parts.append(f"requires the {' + '.join(mand)} delegate"
                     f"{'s' if len(mand) > 1 else ''} (mandatory)")
    if opt:
        parts.append(("optional: " if mand else "optional delegate(s): ") + ", ".join(opt))
    if gated:
        parts.append("; ".join(
            f"{n} is attribute-wired, its delegate optional (only for "
            f"{FEATURE_GATED_DELEGATES[norm(n)]})" for n in gated))
    if attr_only and (mand or opt or gated):
        parts.append(f"attribute wiring for {', '.join(attr_only)}")
    if parts:
        return "; ".join(parts) + comp

    mand_client = [n for (n, c) in client if c == "mandatory"]
    if mand_client:
        return f"client device — needs bindings to {', '.join(mand_client)}, not a delegate"

    if attr_only:
        return f"no delegate — attribute wiring for {', '.join(attr_only)}" + comp

    if comp:
        return "composite — no cluster of its own" + comp
    return "no delegate — endpoint-only, no cluster that requires one"


def write_note(block_yml: Path, note: str) -> bool:
    text = block_yml.read_text()
    quoted = '"' + note.replace('"', '\\"') + '"'
    if re.search(r"^matter_wiring_note:", text, re.M):
        new = re.sub(r"^matter_wiring_note:.*$", f"matter_wiring_note: {quoted}",
                     text, count=1, flags=re.M)
    elif re.search(r"^matter_wiring:\s*endpoint_only", text, re.M):
        new = re.sub(r"^(matter_wiring:\s*endpoint_only.*)$",
                     r"\1\n" + f"matter_wiring_note: {quoted}",
                     text, count=1, flags=re.M)
    else:
        return False  # not a stub; nothing to annotate
    if new != text:
        block_yml.write_text(new)
        return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--chip-ref", default="v1.6-branch")
    ap.add_argument("--dm-version", default="1.6")
    ap.add_argument("--chip-dir", help="local data_model/<ver>/device_types dir "
                                        "(offline; overrides --chip-ref fetch)")
    ap.add_argument("--esp-matter-dir", help="esp-matter checkout; derives the delegate "
                                              "set from its DelegateInitCB table instead "
                                              "of the embedded snapshot")
    ap.add_argument("--only-stubs", action="store_true",
                    help="only device types marked matter_wiring: endpoint_only")
    ap.add_argument("--write", action="store_true",
                    help="rewrite matter_wiring_note in each stub block.yml")
    args = ap.parse_args()

    global DELEGATE_CLUSTERS
    DELEGATE_CLUSTERS, delegate_src = load_delegate_clusters(args.esp_matter_dir)
    xml_src = (f"local dir {args.chip_dir}" if args.chip_dir
               else f"connectedhomeip {args.chip_ref} data_model/{args.dm_version}")
    print(f"device-type XMLs : {xml_src}")
    print(f"delegate set     : {len(DELEGATE_CLUSTERS)} clusters, {delegate_src}\n")

    rows, missing, wrote = [], [], 0
    for block_yml in sorted(DEVICE_TYPES.glob("*/block.yml")):
        block = block_yml.parent.name
        text = block_yml.read_text()
        is_stub = bool(re.search(r"^matter_wiring:\s*endpoint_only", text, re.M))
        if args.only_stubs and not is_stub:
            continue
        ep = matter_endpoint(block_yml)
        if not ep:
            continue
        xml = fetch_xml(chip_xml_name(ep), args)
        if xml is None:
            missing.append((block, chip_xml_name(ep)))
            continue
        own, composed = parse_device(xml)
        note = classify(ep, own, composed)
        rows.append((block, note, is_stub))
        if args.write and is_stub:
            wrote += write_note(block_yml, note)

    w = max((len(b) for b, _, _ in rows), default=0)
    for block, note, is_stub in rows:
        print(f"{'*' if is_stub else ' '} {block:<{w}}  {note}")
    if missing:
        print("\nNO XML (name mismatch or absent on ref):", file=sys.stderr)
        for block, name in missing:
            print(f"  {block} -> {name}.xml", file=sys.stderr)
    if args.write:
        print(f"\nwrote matter_wiring_note in {wrote} stub block(s)")
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
