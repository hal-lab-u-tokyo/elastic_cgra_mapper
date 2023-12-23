from io_lib import *
from util import *
import time
import subprocess

class MappingInput:
  def __init__(self, dfg_file_path, output_dir_path, cgra, timeout_s, parallel_num, overwrite):
    self.dfg_file_path = dfg_file_path
    self.output_dir_path = output_dir_path
    self.cgra = cgra
    self.timeout_s = timeout_s
    self.parallel_num = parallel_num
    self.overwrite = overwrite

class CreateDatabaseInput:
  def __init__(self, dfg_file_path, output_dir_path, cgra, db_timeout_s, overwrite):
    self.dfg_file_path = dfg_file_path
    self.cgra = cgra
    self.output_dir_path = output_dir_path
    self.db_timeout_s = db_timeout_s
    self.overwrite = overwrite

class RemapperInput:
  def __init__(self, mapping_dir_path, cgra, output_dir, remapper_mode):
    self.mapping_dir_path = mapping_dir_path
    self.cgra = cgra
    self.output_dir = output_dir
    self.remapper_mode = remapper_mode

def mapping_exec(input):
  cgra_dir_path = os.path.join(os.getcwd(), "/tmp_cgra/mapping/")

  lock.acquire()
  try:
    check_dir_availability(cgra_dir_path)
    cgra_file_path = cgra_dir_path + str(int(time.time())) + ".json"
    time.sleep(1)
  finally:
    lock.release()

  input.cgra.dump_to_json(cgra_file_path)

  lock.acquire()
  try:
    experiment_log_file = open(experiment_log_file_path, "a")
    experiment_log_file.write("--- mapping exec ---\n")
    experiment_log_file.write("dfg_file: " + input.dfg_file_path + "\n")
    experiment_log_file.write("output_dir_path: " + input.output_dir_path + "\n")
    experiment_log_file.write("cgra: " + str(input.cgra.get_cgra_dict()) + "\n")
    experiment_log_file.write("timeout_s: " + str(input.timeout_s) + "\n")
    experiment_log_file.write("parallel_num: " + str(input.parallel_num) + "\n")
    experiment_log_file.write("overwrite: " + str(input.overwrite) + "\n")
    experiment_log_file.close()
  finally:
    lock.release()

  subprocess.run(["/home/ubuntu/elastic_cgra_mapper/build/mapping", input.dfg_file_path, cgra_file_path, input.output_dir_path, str(input.timeout_s), str(input.parallel_num)])  

  os.remove(cgra_file_path)

def create_database_exec(input):
  cgra_dir_path = os.path.join(os.getcwd(), "/tmp_cgra/mapping/")

  lock.acquire()
  try:
    check_dir_availability(cgra_dir_path)
    cgra_file_path = cgra_dir_path + str(int(time.time())) + ".json"
    time.sleep(1)
  finally:
    lock.release()

  input.cgra.dump_to_json(cgra_file_path)

  lock.acquire()
  try:
    experiment_log_file = open(experiment_log_file_path, "a")
    experiment_log_file.write("--- database exec ---\n")
    experiment_log_file.write("dfg_file: " + input.dfg_file_path + "\n")
    experiment_log_file.write("output_dir_path: " + input.output_dir_path + "\n")
    experiment_log_file.write("cgra: " + str(input.cgra.get_cgra_dict()) + "\n")
    experiment_log_file.write("db_timeout_s: " + str(input.db_timeout_s) + "\n")
    experiment_log_file.write("overwrite: " + str(input.overwrite) + "\n")
    experiment_log_file.close()
  finally:
    lock.release()

  subprocess.run(["/home/ubuntu/elastic_cgra_mapper/build/create_database", input.dfg_file_path, cgra_file_path, input.output_dir_path, str(input.db_timeout_s)])  

  os.remove(cgra_file_path)

def remapper_exec(input):
  cgra_dir_path = os.path.join(os.getcwd(), "/tmp_cgra/mapping/")

  lock.acquire()
  try:
    check_dir_availability(cgra_dir_path)
    cgra_file_path = cgra_dir_path + str(int(time.time())) + ".json"
    time.sleep(1)
  finally:
    lock.release()

  input.cgra.dump_to_json(cgra_file_path)

  lock.acquire()
  try:
    experiment_log_file = open(experiment_log_file_path, "a")
    experiment_log_file.write("--- remapper exec ---\n")
    experiment_log_file.write("mapping_dir: " + input.mapping_dir_path + "\n")
    experiment_log_file.write("output_dir: " + input.output_dir_path + "\n")
    experiment_log_file.write("cgra: " + str(input.cgra.get_cgra_dict()) + "\n")
    experiment_log_file.write("mode: " + input.mode.to_string() + "\n")
    experiment_log_file.close()
  finally:
    lock.release()

  subprocess.run(["/home/ubuntu/elastic_cgra_mapper/build/remapper", input.dfg_file_path, cgra_file_path, input.output_dir_path, str(int(input.remapper_mode))])  

  os.remove(cgra_file_path)

def init(l, _experiment_log_file_path):
  global lock
  global experiment_log_file_path
  lock = l
  experiment_log_file_path = _experiment_log_file_path