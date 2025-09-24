# ifstat2_utils.py
from __future__ import annotations
import time
from pathlib import Path

_DEF_SAFETY = 0.00  # add e.g. 0.20 to overrun a bit if you want

def _read_bytes(path: Path) -> int:
    return int(path.read_text().strip())

def _iface_paths(iface: str) -> tuple[Path, Path]:
    base = Path("/sys/class/net") / iface / "statistics"
    return base / "rx_bytes", base / "tx_bytes"

def run_ifstat_during_test(iface: str, duration: int | float,
                           unit: str = "Mbps") -> tuple[float, float]:
    """
    Measure RX/TX throughput for `iface` over `duration` seconds by reading
    kernel counters directly. Returns (rx_rate, tx_rate) in Mbps by default,
    or Gbps if unit='Gbps'.

    This blocks for ~duration seconds. Start it in parallel with your iperf thread.
    """
    rx_p, tx_p = _iface_paths(iface)
    if not (rx_p.exists() and tx_p.exists()):
        raise FileNotFoundError(f"Stats not found for interface '{iface}'")

    # T0
    t0 = time.monotonic()
    rx0 = _read_bytes(rx_p)
    tx0 = _read_bytes(tx_p)

    # Sleep the requested duration (optionally a tiny safety overrun)
    time.sleep(max(0.0, float(duration) + _DEF_SAFETY))

    # T1
    t1 = time.monotonic()
    rx1 = _read_bytes(rx_p)
    tx1 = _read_bytes(tx_p)

    # Compute deltas and rate
    dt = max(1e-6, t1 - t0)  # seconds
    drx = (rx1 - rx0) if rx1 >= rx0 else (2**64 - rx0 + rx1)  # handle wrap
    dtx = (tx1 - tx0) if tx1 >= tx0 else (2**64 - tx0 + tx1)

    # Bytes/sec -> bits/sec
    rx_bps = (drx * 8.0) / dt
    tx_bps = (dtx * 8.0) / dt

    if unit.lower() == "gbps":
        scale = 1_000_000_000.0
    else:  # Mbps default
        scale = 1_000_000.0

    return rx_bps / scale, tx_bps / scale
