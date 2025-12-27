import argparse
import json
import logging
import os
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List

try:
    import yaml  # type: ignore
except ImportError:  # pragma: no cover - optional dependency
    yaml = None


DEFAULT_ENTRIES = 8192
DEFAULT_ENTRY_SIZE = 8256


@dataclass
class ChannelInfo:
    channel_type: str
    shm_path: str
    shm_size: int
    entries: int
    entry_size: int


@dataclass
class PortInfo:
    name: str
    channel_type: str
    shm_path: str
    in_offset: int
    out_offset: int
    entries: int
    entry_size: int


@dataclass
class Simulator:
    name: str
    exec_path: str
    args: List[str]
    ports: List[str]


def load_config(path: Path) -> Dict:
    raw = path.read_text(encoding="utf-8")
    if path.suffix == ".json":
        return json.loads(raw)
    if yaml is None:
        raise RuntimeError("PyYAML is required for YAML topology files")
    return yaml.safe_load(raw)


def resolve_path(base_dir: Path, raw_path: str) -> Path:
    p = Path(raw_path)
    if p.is_absolute():
        return p
    return (base_dir / p).resolve()


def prepare_channel(base_dir: Path, channel_cfg: Dict) -> ChannelInfo:
    channel_type = channel_cfg["type"]
    options = channel_cfg.get("options", {})
    entries = int(options.get("entries", DEFAULT_ENTRIES))
    entry_size = int(options.get("entry_size", DEFAULT_ENTRY_SIZE))

    shm_path = Path("-")
    shm_size = 0

    if channel_type == "shm_ring":
        shm_path = resolve_path(base_dir, options["shm_path"])
        shm_size = int(options.get("shm_size", entries * entry_size * 2))
    elif channel_type == "socket":
        shm_path = Path("-")
    else:
        raise ValueError(f"Unsupported channel type: {channel_type}")

    if channel_type == "shm_ring":
        shm_path.parent.mkdir(parents=True, exist_ok=True)
        if shm_path.exists():
            shm_path.unlink()
        with open(shm_path, "wb") as handle:
            handle.truncate(shm_size)

    return ChannelInfo(
        channel_type=channel_type,
        shm_path=str(shm_path),
        shm_size=shm_size,
        entries=entries,
        entry_size=entry_size,
    )


def parse_simulators(cfg: Dict) -> Dict[str, Simulator]:
    simulators = {}
    for sim in cfg.get("simulators", []):
        name = sim["name"]
        simulators[name] = Simulator(
            name=name,
            exec_path=sim["exec"],
            args=[str(arg) for arg in sim.get("args", [])],
            ports=[port["name"] for port in sim.get("ports", [])],
        )
    return simulators


def build_ports(cfg: Dict, base_dir: Path) -> Dict[str, List[PortInfo]]:
    simulators = parse_simulators(cfg)
    ports_by_sim: Dict[str, List[PortInfo]] = {name: [] for name in simulators}

    for link in cfg.get("links", []):
        a = link["a"]
        b = link["b"]
        channel_info = prepare_channel(base_dir, link["channel"])

        def split_ref(ref: str) -> List[str]:
            sim_name, port_name = ref.split(".")
            return [sim_name, port_name]

        a_sim, a_port = split_ref(a)
        b_sim, b_port = split_ref(b)
        queue_size = channel_info.entries * channel_info.entry_size

        ports_by_sim[a_sim].append(
            PortInfo(
                name=a_port,
                channel_type=channel_info.channel_type,
                shm_path=channel_info.shm_path,
                out_offset=0,
                in_offset=queue_size,
                entries=channel_info.entries,
                entry_size=channel_info.entry_size,
            )
        )
        ports_by_sim[b_sim].append(
            PortInfo(
                name=b_port,
                channel_type=channel_info.channel_type,
                shm_path=channel_info.shm_path,
                out_offset=queue_size,
                in_offset=0,
                entries=channel_info.entries,
                entry_size=channel_info.entry_size,
            )
        )

    return ports_by_sim


def write_port_file(path: Path, ports: List[PortInfo]) -> None:
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("# UBSIM manager ports v1\n")
        for port in ports:
            shm_path = port.shm_path if port.shm_path else "-"
            handle.write(
                f"{port.name} {port.channel_type} {shm_path} {port.in_offset} "
                f"{port.entries} {port.entry_size} {port.out_offset} "
                f"{port.entries} {port.entry_size}\n"
            )


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def run_simulators(cfg: Dict, ports_by_sim: Dict[str, List[PortInfo]]) -> int:
    simulators = parse_simulators(cfg)
    tmp_base = repo_root() / "tmp"
    tmp_base.mkdir(parents=True, exist_ok=True)
    run_dir = tmp_base / f"ubsim-run-{int(time.time())}-{os.getpid()}"
    run_dir.mkdir(parents=True, exist_ok=True)
    logging.info("run directory: %s", run_dir)

    procs: List[subprocess.Popen] = []
    proc_logs: List[tuple] = []

    def cleanup() -> None:
        for proc in procs:
            if proc.poll() is None:
                proc.terminate()
        for proc in procs:
            if proc.poll() is None:
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
        for stdout_handle, stderr_handle in proc_logs:
            stdout_handle.close()
            stderr_handle.close()

    def handle_signal(signum, _frame):
        logging.error("manager received signal %s, shutting down", signum)
        cleanup()
        sys.exit(1)

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    try:
        for sim in simulators.values():
            port_file = run_dir / f"{sim.name}.ports"
            write_port_file(port_file, ports_by_sim[sim.name])
            env = os.environ.copy()
            env["UBSIM_MANAGER_PORTS"] = str(port_file)
            stdout_path = run_dir / f"{sim.name}.stdout.log"
            stderr_path = run_dir / f"{sim.name}.stderr.log"
            cmd_path = run_dir / f"{sim.name}.cmd"
            cmd_line = " ".join([sim.exec_path] + sim.args)
            cmd_path.write_text(cmd_line + "\n", encoding="utf-8")
            logging.info("launching %s: %s", sim.name, cmd_line)
            stdout_handle = open(stdout_path, "w", encoding="utf-8")
            stderr_handle = open(stderr_path, "w", encoding="utf-8")
            proc = subprocess.Popen(
                [sim.exec_path] + sim.args,
                env=env,
                stdout=stdout_handle,
                stderr=stderr_handle,
            )
            procs.append(proc)
            proc_logs.append((stdout_handle, stderr_handle))

        while True:
            for proc in procs:
                ret = proc.poll()
                if ret is not None:
                    for other in procs:
                        if other is not proc and other.poll() is None:
                            other.terminate()
                    for other in procs:
                        if other is not proc and other.poll() is None:
                            try:
                                other.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                other.kill()
                    return ret
            time.sleep(0.1)
    finally:
        cleanup()


def main() -> int:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] manager: %(message)s",
    )

    parser = argparse.ArgumentParser(description="UBSIM central manager")
    parser.add_argument("topology", help="Topology configuration file (YAML/JSON)")
    args = parser.parse_args()

    config_path = Path(args.topology).resolve()
    config_dir = config_path.parent
    cfg = load_config(config_path)
    ports_by_sim = build_ports(cfg, config_dir)
    return run_simulators(cfg, ports_by_sim)


if __name__ == "__main__":
    raise SystemExit(main())
