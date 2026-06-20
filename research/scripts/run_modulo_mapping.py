#!/usr/bin/env python3

"""Compatibility wrapper for the renamed mapper-case runner."""

from run_mapper_case import (
    EXTERNAL_RUNNERS,
    is_external_runner,
    main,
    mapper_runner,
    prepare_cli_mapper_config,
    resolve_mii,
    run_mapper_case,
    run_one,
)


if __name__ == "__main__":
    main()
