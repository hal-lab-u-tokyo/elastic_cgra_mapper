#!/usr/bin/env python3

import argparse
from pathlib import Path

from lib import compute_res_mii, write_json


def main() -> None:
    parser = argparse.ArgumentParser(description="Compute the initial modulo mapping MII lower bound.")
    parser.add_argument("--dfg", required=True, type=Path)
    parser.add_argument("--arch-template", required=True, type=Path)
    parser.add_argument(
        "--missing-distance-policy",
        choices=["strict", "self_loop"],
        default="self_loop",
        help="How to handle recurrence edges without explicit distance.",
    )
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()

    result = compute_res_mii(args.dfg, args.arch_template, args.missing_distance_policy)
    if args.out:
        write_json(args.out, result)
    else:
        import json

        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
