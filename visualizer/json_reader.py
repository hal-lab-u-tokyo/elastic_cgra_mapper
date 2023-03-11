import json
from mapping import *
from typing import Dict


def read_PE_from_dict(dict: Dict, context_size: int) -> PE:
    result: PE = PE()
    result.row_id = int(dict["row_id"])
    result.column_id = int(dict["column_id"])
    result.context_size = context_size
    result.config_list = []
    for config_dict in dict["config"]:
        config_element: CGRAConfig = CGRAConfig()
        config_element.context_id = int(config_dict["context_id"])
        config_element.operation_type = OperationType.get_from_string(
            config_dict["operation_type"])
        config_element.from_config_id = []
        for from_config_dict in config_dict["from_config_id"]:
            from_config_id: ConfigId = ConfigId()
            from_config_id.row_id = int(from_config_dict["row_id"])
            from_config_id.column_id = int(from_config_dict["column_id"])
            from_config_id.context_id = int(from_config_dict["context_id"])
            config_element.from_config_id.append(from_config_id)
        config_element.to_config_id = []
        for to_config_dict in config_dict["to_config_id"]:
            to_config_id: ConfigId = ConfigId()
            to_config_id.row_id = int(to_config_dict["row_id"])
            to_config_id.column_id = int(to_config_dict["column_id"])
            to_config_id.context_id = int(to_config_dict["context_id"])
            config_element.to_config_id.append(from_config_id)
        result.config_list.append(config_element)
    return result

def read_mapping_from_json(file_name: str) -> Mapping:
    result: Mapping = Mapping()
    with open(file_name) as f:
        df = json.load(f)
        result.column_num = int(df["column"])
        result.row_num = int(df["row"])
        result.context_size = int(df["context_size"])
        result.memory_io_type = MemoryIOType.get_from_string(
            df["memory_io_type"])
        result.CGRA_type = CGRAType.get_from_string(df["cgra_type"])
        result.network_type = NetworkType.get_from_string(df["network_type"])
        result.PE_array = []

        # PE_array initialize
        for _ in range(result.row_num):
            PE_column_list: list[PE] = []
            for _ in range(result.column_num):
                PE_column_list.append(PE())
            result.PE_array.append(PE_column_list)

        for PE_config_dict in df["PE_config"]:
            PE_ele: PE = read_PE_from_dict(PE_config_dict, result.context_size)
            result.PE_array[PE_ele.row_id][PE_ele.column_id] = PE_ele

    return result
