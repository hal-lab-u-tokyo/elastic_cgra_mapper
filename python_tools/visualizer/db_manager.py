import sys
import os
sys.path.append(os.pardir)
from io_lib import *

class DatabaseManager:
  def __init__(self, db_info_list):
    self.db_log_path_to_db_info = {}
    self.mapping_json_path_to_db_log_path = {}

    for db_info in db_info_list:
      self.db_log_path_to_db_info[db_info.log_file_path] = db_info
      for mapping_log_path in db_info.mapping_log_file_list:
        success, mapping_info = mapping_log_reader(mapping_log_path)
        if not success:
          print("ERROR: mapping log reader failed: " + mapping_log_path)
          print("ERROR: db log: " + db_info.log_file_path)
          continue
        self.mapping_json_path_to_db_log_path[mapping_info.mapping_file_path] = db_info.log_file_path

  def get_database_info(self, remapper_info):
    db_log_file_path_list = []
    for maping_json in remapper_info.mapping_json_list:
      if maping_json not in self.mapping_json_path_to_db_log_path.keys():
        return None
      db_log_file_path_list.append(self.mapping_json_path_to_db_log_path[maping_json])

    init_db_log_file_path = db_log_file_path_list[0]
    for db_log_file_path in db_log_file_path_list:
      if init_db_log_file_path != db_log_file_path:
        print("ERROR: database log file path is different: " + remapper_info.log_file_path)

    if init_db_log_file_path not in self.db_log_path_to_db_info.keys():
      return None

    return self.db_log_path_to_db_info[init_db_log_file_path]

