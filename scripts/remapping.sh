# !/bin/bash

mapping_dir_path=/home/ubuntu/elastic_cgra_mapper/output/convolution_no_loop/
mrrg_file_path=/home/ubuntu/elastic_cgra_mapper/data/elastic_cgra.json
output_dir_path=/home/ubuntu/elastic_cgra_mapper/
mode=0 # 0: FullSearch, 1: GreedySearch, 2: DP, 3: DP+FullSearch
timeout_s=3600
db_num=1

/home/ubuntu/elastic_cgra_mapper/build/remapping \
    $mapping_dir_path \
    $mrrg_file_path \
    $output_dir_path \
    $mode \
    $timeout_s \
    $db_num
