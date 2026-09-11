#!/usr/bin/env python3

import argparse
from pathlib import Path

from lib import make_arch_for_ii, write_json


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a CGRA architecture JSON for a candidate II.")
    parser.add_argument("--arch-template", required=True, type=Path)
    parser.add_argument("--ii", required=True, type=int)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--allow-non-default", action="store_true")
    args = parser.parse_args()

    arch = make_arch_for_ii(args.arch_template, args.ii, force_default=not args.allow_non_default)
    write_json(args.out, arch)


if __name__ == "__main__":
    main()
