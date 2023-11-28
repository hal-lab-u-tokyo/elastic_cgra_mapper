import subprocess
import os
import datetime

if __name__ == "__main__":
  files = os.listdir("../benchmark/kernel")

  for file in files:
    benchmark_name = file.split(".")[0]
    extension = file.split(".")[1]

    if extension != "dot": 
      continue

    dt = datetime.datetime.now()
    ymd = str(dt.year) + str(dt.month) + str(dt.day)

    print(file)
    dot_file = "../benchmark/kernel/" + file
    mapping_output_dir = "../output/mapping/" + benchmark_name + "/" + ymd + "/"
    log_output_dir = "../output/log/" + benchmark_name + "/" + ymd + "/"
    print(subprocess.run(["../build/bulk_mapping", dot_file, mapping_output_dir, log_output_dir]))

