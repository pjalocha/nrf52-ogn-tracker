#!/usr/bin/env python3
"""Live and recorded OGN receiver packet timing monitor.

Examples:
  ./timing_mon.py --host receiver.local:50012
  ./timing_mon.py --host receiver.local:50012 --record capture.timing
  ./timing_mon.py --file capture.timing --address 07:87B190

Recorded files made with --record include local receive timestamps. Untimed
files can still be inspected in packet order, but cannot provide a true rate
or wall-clock packet timing plot.
"""

import argparse
from collections import deque
from dataclasses import dataclass
from datetime import datetime
import re
import select
import socket
import sys
import time

import matplotlib.pyplot as plt


COLORS = {
    "OGN": "#0072B2",  # blue
    "MDR": "#E69F00",  # orange
    "HDR": "#009E73",  # green
    "LDR": "#D55E00",  # vermillion
    "PAW": "#CC79A7",  # purple-pink
    "FLR": "#56B4E9",  # sky blue
}
TYPE_Y = {
    "MDR":  0.00,
    "OGN": -0.1,
    "FLR":  0.1,
    "LDR":  1.00,
    "PAW":  0.9,
    "HDR":  1.1,
}
TYPES = tuple(COLORS)
LABEL_RE = re.compile(r"^\s*(OGN1?|ADSL|ADS-L|MDR|MR|HDR|LDR|PAW|FLR6?)\s*:", re.I)
ADDRESS_RE = re.compile(r"\b([0-9A-Fa-f]{2}:[0-9A-Fa-f]{6})\b")
OGN_ADDRESS_RE = re.compile(r"(?:^|[^A-Za-z0-9])s?([0-9A-Fa-f]:[0-9]:[0-9A-Fa-f]{6})\b")
ISO_PREFIX_RE = re.compile(
    r"^\s*\[?(\d{4}-\d\d-\d\d[T ]\d\d:\d\d:\d\d(?:\.\d+)?)\]?\s+(.*)$"
)
EPOCH_PREFIX_RE = re.compile(r"^\s*(\d{10}(?:\.\d+)?)(?::\s*|\s+)(.*)$")
PACKET_TIME_PREFIX_RE = re.compile(r"^\s*(\d{10}(?:\.\d+)?):\s*(.*)$")


@dataclass
class Packet:
    when: float | None
    kind: str
    address: str


def packet_kind(label: str) -> str:
    label = label.upper()
    if label in ("OGN", "OGN1"):
        return "OGN"
    if label in ("ADSL", "ADS-L", "MDR", "MR"):
        return "MDR"
    if label in ("FLR", "FLR6"):
        return "FLR"
    return label


def split_timestamp(line: str) -> tuple[float | None, str]:
    match = ISO_PREFIX_RE.match(line)
    if match:
        stamp = match.group(1).replace(" ", "T")
        try:
            return datetime.fromisoformat(stamp).timestamp(), match.group(2)
        except ValueError:
            return None, line
    match = EPOCH_PREFIX_RE.match(line)
    if match:
        body = match.group(2)
        event_time = float(match.group(1))
        # Recorded lines have an outer local-arrival timestamp and commonly
        # retain the receiver's packet timestamp immediately before the label.
        packet_time = PACKET_TIME_PREFIX_RE.match(body)
        if packet_time:
            event_time = float(packet_time.group(1))
            body = packet_time.group(2)
        return event_time, body
    return None, line


def parse_packet(line: str) -> Packet | None:
    when, body = split_timestamp(line)
    # Relayed packets are not local RF receptions at this receiver.
    if re.search(r"\bR1\b", body):
        return None
    label = LABEL_RE.match(body)
    if not label:
        return None
    kind = packet_kind(label.group(1))
    address = OGN_ADDRESS_RE.search(body) or ADDRESS_RE.search(body)
    if not address:
        return None
    return Packet(
        when=when,
        kind=kind,
        address=address.group(1).upper(),
    )


class TimingPlot:
    def __init__(self, window: int, seconds: int, address: str | None, untimed: bool):
        self.window = window
        self.seconds = seconds
        self.address = address.upper() if address else None
        self.untimed = untimed
        self.packets: deque[Packet] = deque()
        self.latest_time: float | None = None
        self.fig, (self.freq_ax, self.rate_ax) = plt.subplots(
            2, 1, figsize=(12, 7), gridspec_kw={"height_ratios": [2, 1]}, sharex=False
        )
        self.fig.subplots_adjust(hspace=0.32, bottom=0.12)
        self.fig.canvas.manager.set_window_title("OGN packet timing monitor")
        self.freq_ax.set_title("Packet timing by band and type")
        self.freq_ax.set_ylabel("Band and packet type")
        self.freq_ax.set_yticks((0, 1))
        self.freq_ax.set_yticklabels(("M-Band", "O-Band"))
        self.freq_ax.set_ylim(-0.5, 1.5)
        self.freq_ax.axhline(0.0, color="#555555", linewidth=1.0, alpha=0.65, zorder=0)
        self.freq_ax.axhline(1.0, color="#555555", linewidth=1.0, alpha=0.65, zorder=0)
        self.rate_ax.set_title(f"Packets per second, trailing {window} s")
        self.rate_ax.set_ylabel("packets/s")
        self.rate_ax.set_xlabel("Receive time")
        self.rate_ax.grid(True, alpha=0.25)
        self.status = self.fig.text(0.01, 0.01, "Waiting for packets")

    def add(self, packet: Packet) -> bool:
        if self.address and packet.address != self.address:
            return False
        if packet.when is None:
            self.untimed = True
            packet.when = float(len(self.packets))
        elif self.latest_time is None or packet.when > self.latest_time:
            self.latest_time = packet.when
        self.packets.append(packet)
        if not self.untimed:
            # Keep the visible span plus one full averaging window so rate
            # values at the left edge have their required preceding history.
            cutoff = self.latest_time - (self.seconds + self.window)
            self.packets = deque(p for p in self.packets if p.when is not None and p.when >= cutoff)
        return True

    def draw(self, title: str = "") -> None:
        self.freq_ax.clear()
        self.rate_ax.clear()
        self.freq_ax.set_title("Packet timing by band and type")
        self.freq_ax.set_ylabel("Band and packet type")
        self.freq_ax.set_yticks((0, 1))
        self.freq_ax.set_yticklabels(("M-Band", "O-Band"))
        self.freq_ax.set_ylim(-0.5, 1.5)
        self.freq_ax.axhline(0.0, color="#555555", linewidth=1.0, alpha=0.65, zorder=0)
        self.freq_ax.axhline(1.0, color="#555555", linewidth=1.0, alpha=0.65, zorder=0)
        self.rate_ax.set_title(f"Packets per second, trailing {self.window} s")
        self.rate_ax.set_ylabel("packets/s")
        self.rate_ax.set_xlabel("Receive time")
        self.rate_ax.grid(True, alpha=0.25)

        packets = sorted(self.packets, key=lambda p: p.when if p.when is not None else 0.0)
        timed = not self.untimed and all(p.when is not None for p in packets)
        if packets:
            times = [p.when for p in packets]
            start = times[0]
            rel = [t - start for t in times]
            for kind in TYPES:
                selected = [p for p in packets if p.kind == kind]
                if not selected:
                    continue
                xs = [p.when % 1.0 for p in selected] if timed else [p.when - start for p in selected]
                ys = [TYPE_Y[kind]] * len(selected)
                self.freq_ax.scatter(xs, ys, s=24, alpha=0.85, color=COLORS[kind], label=kind)
            if timed:
                self.freq_ax.set_xlim(0.0, 1.0)
                self.freq_ax.set_xticks((0.0, 0.200, 0.450, 0.825, 1.0))
                self.freq_ax.set_xlabel("Fractional part of receive timestamp (s)")
                for phase in (0.200, 0.450, 0.825):
                    self.freq_ax.axvline(phase, color="#444444", linestyle=":",
                                         linewidth=0.8, alpha=0.65, zorder=0)
            else:
                self.freq_ax.set_xlim(0.0, max(self.seconds, rel[-1]))
                self.freq_ax.set_xlabel("Packet order (untimed file)")
            self.freq_ax.legend(loc="upper left", ncol=3)

            if timed:
                visible_start = max(start, times[-1] - self.seconds)
                rate_start = max(visible_start, start + self.window)
                if rate_start <= times[-1]:
                    sample_times = sorted(set([rate_start, *[t for t in times if t >= rate_start], times[-1]]))
                    for kind in TYPES:
                        kind_times = [t for p, t in zip(packets, times) if p.kind == kind]
                        rates = [sum(1 for t in kind_times if sample - self.window < t <= sample) / self.window
                                 for sample in sample_times]
                        self.rate_ax.plot([s - start for s in sample_times], rates,
                                          color=COLORS[kind], label=kind, linewidth=1.6)
                    self.rate_ax.legend(loc="upper left", ncol=3)
                else:
                    self.rate_ax.text(0.5, 0.5,
                                      f"Collecting a full {self.window} s rate window",
                                      ha="center", va="center", transform=self.rate_ax.transAxes)
                self.rate_ax.set_xlim(max(0.0, times[-1] - start - self.seconds),
                                      max(self.seconds, times[-1] - start))
            else:
                self.rate_ax.text(0.5, 0.5,
                                  "No receive timestamps in this file.\nUse a timestamped capture for packet rates.",
                                  ha="center", va="center", transform=self.rate_ax.transAxes)
                self.rate_ax.set_xticks([])

        subtitle = f"{len(packets)} packets"
        if self.address:
            subtitle += f" for {self.address}"
        if title:
            subtitle += " | " + title
        if self.untimed:
            subtitle += " | untimed input"
        self.status.set_text(subtitle)
        self.fig.canvas.draw_idle()


def load_file(path: str, plot: TimingPlot) -> None:
    seen = 0
    timed_count = 0
    with open(path, "r", errors="replace") as source:
        for line in source:
            packet = parse_packet(line)
            if packet:
                seen += 1
                timed_count += packet.when is not None
                plot.add(packet)
    if seen and timed_count != seen:
        plot.untimed = True
    plot.draw(path)
    plt.show()


def parse_host(value: str) -> tuple[str, int]:
    if ":" not in value:
        raise argparse.ArgumentTypeError("host must be written host:port")
    host, port_text = value.rsplit(":", 1)
    try:
        port = int(port_text)
    except ValueError as error:
        raise argparse.ArgumentTypeError("port must be a number") from error
    if not host or not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("invalid host or port")
    return host, port


def run_live(host: str, port: int, plot: TimingPlot, record: str | None) -> None:
    out = open(record, "a", buffering=1) if record else None
    sock = None
    buffer = b""
    last_draw = 0.0
    plt.show(block=False)
    try:
        while plt.fignum_exists(plot.fig.number):
            if sock is None:
                try:
                    sock = socket.create_connection((host, port), timeout=1)
                    sock.setblocking(False)
                    buffer = b""
                except OSError as error:
                    plot.draw(f"connecting to {host}:{port} ({error})")
                    plot.fig.canvas.flush_events()
                    time.sleep(0.5)
                    continue
            ready, _, _ = select.select([sock], [], [], 0.05)
            changed = False
            if ready:
                try:
                    data = sock.recv(65536)
                except OSError:
                    data = b""
                if not data:
                    sock.close()
                    sock = None
                    continue
                buffer += data
                while b"\n" in buffer:
                    raw, buffer = buffer.split(b"\n", 1)
                    line = raw.decode("utf-8", "replace").rstrip("\r")
                    stamp = time.time()
                    packet = parse_packet(line)
                    if out:
                        out.write(f"{stamp:.6f} {line}\n")
                    if packet and packet.when is not None and plot.add(packet):
                        changed = True
                    elif packet and packet.when is None:
                        print("Ignoring packet without a monitor timestamp", file=sys.stderr)
            now = time.monotonic()
            if changed or now - last_draw >= 0.5:
                plot.draw(f"{host}:{port}")
                last_draw = now
            # Process GUI events without repeatedly calling show(), which can
            # raise the plot window and steal focus on some backends.
            plot.fig.canvas.flush_events()
    finally:
        if sock:
            sock.close()
        if out:
            out.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--host", type=parse_host, metavar="HOST:PORT", help="live receiver monitoring port")
    mode.add_argument("--file", help="recorded receiver output to plot")
    parser.add_argument("--address", help="show only this source address; default is all addresses")
    parser.add_argument("--window", type=int, choices=(10, 20), default=20,
                        help="running packet-rate average window (default: 20 seconds)")
    parser.add_argument("--seconds", type=int, default=60,
                        help="visible timing history in seconds (default: 60)")
    parser.add_argument("--record", help="save live input with receive timestamps for later replay")
    args = parser.parse_args()
    plot = TimingPlot(args.window, args.seconds, args.address, untimed=False)
    if args.file:
        load_file(args.file, plot)
    else:
        run_live(args.host[0], args.host[1], plot, args.record)
    return 0


if __name__ == "__main__":
    sys.exit(main())
