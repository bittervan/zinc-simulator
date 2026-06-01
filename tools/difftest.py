#!/usr/bin/env python3

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


COMMIT_RE = re.compile(
    r"^core\s+\d+:\s+"
    r"(?P<priv>\d+)\s+"
    r"(?P<pc>0x[0-9a-fA-F]+)\s+"
    r"\((?P<instruction>0x[0-9a-fA-F]+)\)"
    r"(?:\s+x(?P<reg>\d+)\s+(?P<value>0x[0-9a-fA-F]+))?\s*$"
)


def hex64(value: int) -> str:
    return f"0x{value:016x}"


def hex32(value: int) -> str:
    return f"0x{value:08x}"


def run_command(command: list[str]) -> str:
    result = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed with exit code {result.returncode}: {' '.join(command)}\n"
            f"{result.stdout}"
        )
    return result.stdout


def parse_spike_log(log: str) -> list[dict]:
    commits = []
    for line in log.splitlines():
        match = COMMIT_RE.match(line)
        if match is None:
            continue

        reg = match.group("reg")
        reg_write = None
        if reg is not None:
            reg_write = {
                "reg": int(reg),
                "value": hex64(int(match.group("value"), 16)),
            }

        commits.append(
            {
                "priv": int(match.group("priv")),
                "pc": hex64(int(match.group("pc"), 16)),
                "instruction": hex32(int(match.group("instruction"), 16)),
                "reg_write": reg_write,
                "memory_writes": [],
            }
        )

    return commits


def comparable(commit: dict) -> dict:
    return {
        "priv": commit["priv"],
        "pc": commit["pc"],
        "instruction": commit["instruction"],
        "reg_write": commit["reg_write"],
        "memory_writes": commit.get("memory_writes", []),
    }


def compare_commits(spike: list[dict], zinc: list[dict]) -> None:
    count = min(len(spike), len(zinc))
    for index in range(count):
        spike_commit = comparable(spike[index])
        zinc_commit = comparable(zinc[index])
        if spike_commit != zinc_commit:
            raise AssertionError(
                f"commit mismatch at index {index}\n"
                f"spike: {json.dumps(spike_commit, sort_keys=True)}\n"
                f"zinc:  {json.dumps(zinc_commit, sort_keys=True)}"
            )

    if len(spike) != len(zinc):
        raise AssertionError(f"commit count mismatch: spike={len(spike)} zinc={len(zinc)}")


def write_json(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]

    parser = argparse.ArgumentParser()
    parser.add_argument("--steps", type=int, default=16)
    parser.add_argument("--spike", type=Path, default=repo_root / "build/third_party/spike/spike")
    parser.add_argument(
        "--zinc", type=Path, default=repo_root / "build/zinc-simulator/zinc-simulator"
    )
    parser.add_argument("--elf", type=Path, default=repo_root / "build/tests/add.elf")
    parser.add_argument("--bin", type=Path, default=repo_root / "build/tests/add.bin")
    parser.add_argument("--out-dir", type=Path, default=repo_root / "build/difftest")
    parser.add_argument("--isa", default="RV64I")
    parser.add_argument("--pc", default="0x80000000")
    args = parser.parse_args()

    spike_log = run_command(
        [
            str(args.spike),
            f"--isa={args.isa}",
            f"--pc={args.pc}",
            f"--instructions={args.steps}",
            "-l",
            "--log-commits",
            str(args.elf),
        ]
    )
    spike_commits = parse_spike_log(spike_log)
    spike_document = {
        "source": "spike",
        "isa": args.isa,
        "entry": hex64(int(args.pc, 16)),
        "commits": spike_commits,
    }

    zinc_json = run_command(
        [str(args.zinc), "--max-steps", str(args.steps), "--trace-json", str(args.bin)]
    )
    zinc_document = json.loads(zinc_json)
    zinc_commits = zinc_document["commits"]

    write_json(args.out_dir / "spike.json", spike_document)
    write_json(args.out_dir / "zinc.json", zinc_document)

    compare_commits(spike_commits, zinc_commits)
    print(f"difftest passed: {len(spike_commits)} commits")
    print(f"wrote {args.out_dir / 'spike.json'}")
    print(f"wrote {args.out_dir / 'zinc.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
