#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

from lib import normalize_run


def main() -> None:
    parser = argparse.ArgumentParser(description="Normalize one build/mapping run directory into one metrics row.")
    parser.add_argument("--run-dir", required=True, type=Path)
    parser.add_argument("--benchmark", required=True)
    parser.add_argument("--mapper", required=True)
    parser.add_argument("--arch-name", required=True)
    parser.add_argument("--mii", type=int)
    parser.add_argument("--start-ii", type=int)
    parser.add_argument("--achieved-ii", type=int)
    parser.add_argument("--dfg", type=Path)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    row = normalize_run(
        args.run_dir,
        benchmark=args.benchmark,
        mapper=args.mapper,
        arch_name=args.arch_name,
        mii=args.mii,
        achieved_ii=args.achieved_ii,
        start_ii=args.start_ii,
        dfg_path=args.dfg,
    )
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(row, indent=2) + "\n")
    else:
        print(json.dumps(row, indent=2))


if __name__ == "__main__":
    main()
