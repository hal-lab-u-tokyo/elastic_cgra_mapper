import os
import sys
sys.path.append(os.pardir)
from io_lib import *

benchmark_list = ["fixed_convolution2d", "fixed_ellpack", "fixed_fft_pro", "fixed_fir_pro", "fixed_latnrm_pro", "fixed_stencil", "fixed_susan_pro", "convolution_no_loop", "fixed_matrixmultiply_const"]

if __name__ == "__main__":
  output_dir = "/home/ubuntu/elastic_cgra_mapper/output/"
  for benchmark in benchmark_list:
    log_file_dir = os.path.join(output_dir, benchmark, "database/log")
    for file in os.listdir(log_file_dir):
      file_path = os.path.join(log_file_dir, file)
      log_info = database_log_reader(file_path, benchmark_list)

      if len(log_info.mapping_log_file_list) == 0:
        print("empty database: " + file_path)
        continue

      mapping_dir = os.path.dirname(log_info.mapping_log_file_list[0])
      for mapping_file_name in os.listdir(mapping_dir):
        mapping_file_path = os.path.join(mapping_dir, mapping_file_name)
        if mapping_file_path not in log_info.mapping_log_file_list:
          print("mapping file not found: " + file_path)
          break
