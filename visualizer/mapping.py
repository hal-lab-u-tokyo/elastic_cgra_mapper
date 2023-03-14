from enum import Enum

class MemoryIOType(Enum):
    All = 0

    def get_from_string(input: str):
        if input == "all":
            return MemoryIOType.All
        else:
            print("ERROR: MemoryIOType get_from_string invalid input")
            raise ValueError


class CGRAType(Enum):
    Elastic = 0
    Default = 1

    def get_from_string(input: str):
        if input == "elastic":
            return CGRAType.Elastic
        if input == "default":
            return CGRAType.Default
        else:
            print("ERROR: CGRAType get_from_string invalid input")
            raise ValueError


class NetworkType(Enum):
    Diagonal = 0
    Orthogonal = 1

    def get_from_string(input: str):
        if input == "diagonal":
            return NetworkType.Diagonal
        if input == "orthogonal":
            return NetworkType.Orthogonal
        else: 
            print("ERROR: NetworkType get_from_string invalid input")
            raise ValueError

class OperationType(Enum):
    Add = 0
    Sub = 1
    Mul = 2
    Div = 3
    Const = 4
    Load = 5
    Output = 6
    Nop = 7
    Route = 8

    def get_from_string(input: str):
        if input == "add":
            return OperationType.Add
        elif input == "sub":
            return OperationType.Sub
        elif input == "mul":
            return OperationType.Mul
        elif input == "div":
            return OperationType.Div
        elif input == "const":
            return OperationType.Const
        elif input == "load":
            return OperationType.Load
        elif input == "output":
            return OperationType.Output
        elif input == "nop":
            return OperationType.Nop
        elif input == "route":
            return OperationType.Route
        else:
            print("ERROR: OperationType get_from_string invalid input")
            raise ValueError

    def to_string(input):
        if input == OperationType.Add:
            return "add"
        elif input == OperationType.Sub:
            return "sub"
        elif input == OperationType.Mul:
            return "mul"
        elif input == OperationType.Div:
            return "div"
        elif input ==OperationType.Const:
            return "const"
        elif input == OperationType.Load:
            return "load"
        elif input == OperationType.Output:
            return "output"
        elif input == OperationType.Nop:
            return "nop"
        elif input == OperationType.Route:
            return "route"
        else:
            print("ERROR: OperationType to_string invalid input")
            raise ValueError



class ConfigId:
    row_id: int
    column_id: int
    context_id: int


class CGRAConfig:
    context_id: int
    operation_type: OperationType
    operation_name: str
    to_config_id: list[ConfigId]
    from_config_id: list[ConfigId]


class PE:
    row_id: int
    column_id: int
    context_size: int
    config_list: list[CGRAConfig]


class Mapping:
    column_num: int
    row_num: int
    context_size: int
    memory_io_type: MemoryIOType
    CGRA_type: CGRAType
    network_type: NetworkType

    PE_array: list[list[PE]]
