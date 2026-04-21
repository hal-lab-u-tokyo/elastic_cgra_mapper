#!/bin/bash
CONFIG_FILE="/home/ubuntu/elastic_cgra_mapper/data/experiment_runner/mapping_config.json"

cd python_tools/experiment_runner

python3 mapping_runner.py $CONFIG_FILE
