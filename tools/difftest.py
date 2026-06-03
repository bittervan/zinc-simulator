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
    r"(?P<effects>.*)$"
)


def hex64(value: int) -> str:
    return f"0x{value:016x}"


def hex32(value: int) -> str:
    return f"0x{value:08x}"


def normalize_mem_value(value: int, size: int) -> str:
    mask = (1 << (size * 8)) - 1
    return hex64(value & mask)


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

        reg_write = None
        memory_writes = []
        tokens = match.group("effects").split()
        index = 0
        while index < len(tokens):
            token = tokens[index]
            if token.startswith("x") and index + 1 < len(tokens):
                reg_write = {
                    "reg": int(token[1:]),
                    "value": hex64(int(tokens[index + 1], 16)),
                }
                index += 2
            elif token == "mem" and index + 1 < len(tokens):
                address = int(tokens[index + 1], 16)
                if index + 2 < len(tokens) and tokens[index + 2].startswith("0x"):
                    value_token = tokens[index + 2]
                    size = max(1, (len(value_token) - 2 + 1) // 2)
                    memory_writes.append(
                        {
                            "address": hex64(address),
                            "size": size,
                            "value": normalize_mem_value(int(value_token, 16), size),
                        }
                    )
                    index += 3
                else:
                    # Spike also logs memory reads as "mem <addr>"; reads are not
                    # architecture-visible commits for the current difftest.
                    index += 2
            else:
                index += 1

        commits.append(
            {
                "priv": int(match.group("priv")),
                "pc": hex64(int(match.group("pc"), 16)),
                "instruction": hex32(int(match.group("instruction"), 16)),
                "reg_write": reg_write,
                "memory_writes": memory_writes,
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


def compare_commits(case: str, spike: list[dict], zinc: list[dict]) -> None:
    count = min(len(spike), len(zinc))
    for index in range(count):
        spike_commit = comparable(spike[index])
        zinc_commit = comparable(zinc[index])
        if spike_commit != zinc_commit:
            raise AssertionError(
                f"commit mismatch in case '{case}' at index {index}\n"
                f"spike: {json.dumps(spike_commit, sort_keys=True)}\n"
                f"zinc:  {json.dumps(zinc_commit, sort_keys=True)}"
            )

    if len(spike) != len(zinc):
        raise AssertionError(f"commit count mismatch in case '{case}': spike={len(spike)} zinc={len(zinc)}")


def write_json(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")


def run_case(args: argparse.Namespace, case: str) -> int:
    elf = args.tests_dir / f"{case}.elf"
    binary = args.tests_dir / f"{case}.bin"
    out_dir = args.out_dir / case

    spike_log = run_command(
        [
            str(args.spike),
            f"--isa={args.isa}",
            f"--pc={args.pc}",
            f"--instructions={args.steps}",
            "-l",
            "--log-commits",
            str(elf),
        ]
    )
    spike_commits = parse_spike_log(spike_log)
    spike_document = {
        "source": "spike",
        "case": case,
        "isa": args.isa,
        "entry": hex64(int(args.pc, 16)),
        "commits": spike_commits,
    }

    zinc_json = run_command(
        [str(args.zinc), "--max-steps", str(args.steps), "--trace-json", str(binary)]
    )
    zinc_document = json.loads(zinc_json)
    zinc_document["case"] = case
    zinc_commits = zinc_document["commits"]

    write_json(out_dir / "spike.json", spike_document)
    write_json(out_dir / "zinc.json", zinc_document)

    compare_commits(case, spike_commits, zinc_commits)
    print(f"{case}: passed {len(spike_commits)} commits")
    return len(spike_commits)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]

    parser = argparse.ArgumentParser()
    parser.add_argument("--steps", type=int, default=16)
    parser.add_argument("--spike", type=Path, default=repo_root / "build/third_party/spike/spike")
    parser.add_argument(
        "--zinc", type=Path, default=repo_root / "build/zinc-simulator/zinc-simulator"
    )
    parser.add_argument("--tests-dir", type=Path, default=repo_root / "build/tests")
    parser.add_argument("--case", action="append", dest="cases")
    parser.add_argument("--out-dir", type=Path, default=repo_root / "build/difftest")
    parser.add_argument("--isa", default="RV64I")
    parser.add_argument("--pc", default="0x80000000")
    args = parser.parse_args()

    cases = args.cases
    if not cases:
        cases = sorted(path.stem for path in args.tests_dir.glob("*.elf"))
    if not cases:
        raise RuntimeError(f"no test ELF files found in {args.tests_dir}")

    total = 0
    for case in cases:
        total += run_case(args, case)

    print(f"difftest passed: {len(cases)} cases, {total} commits")
    print(f"wrote {args.out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
