from enum import Enum
from typing import List

class MemoryIOType(Enum):
    All = 0
    BothEnds = 1
    OneEnd = 2

    def get_from_string(input: str):
        if input == "all":
            return MemoryIOType.All
        if input == "both_ends":
            return MemoryIOType.BothEnds
        if input == "one_end":
            return MemoryIOType.OneEnd
        else:
            print("ERROR: MemoryIOType get_from_string invalid input")
            raise ValueError
    
    def to_string(input: str):
        if input == MemoryIOType.All:
            return "all"
        if input == MemoryIOType.BothEnds:
            return "both_ends"
        if input == MemoryIOType.OneEnd:
            return "one_end"
        else:
            print("ERROR: MemoryIOType to_string invalid input")
            raise ValueError

    def __int__(self):
        return self.value

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
        
    def to_string(input: str):
        if input == CGRAType.Elastic:
            return "elastic"
        if input == CGRAType.Default:
            return "default"
        else:
            print("ERROR: CGRAType to_string invalid input")
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
    
    def to_string(input: str):
        if input == NetworkType.Diagonal:
            return "diagonal"
        if input == NetworkType.Orthogonal:
            return "orthogonal"
        else:
            print("ERROR: NetworkType to_string invalid input")
            raise ValueError

class OperationType(Enum):
    Add = 0
    Sub = 1
    Mul = 2
    Div = 3
    Const = 4
    Load = 5
    Output = 6
    Store = 7
    Nop = 8
    Route = 9
    Or = 10
    Shift = 11

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
        elif input == "store":
            return OperationType.Store
        elif input == "nop":
            return OperationType.Nop
        elif input == "route":
            return OperationType.Route
        elif input == "or":
            return OperationType.Or
        elif input == "shift":
            return OperationType.Shift
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
        elif input == OperationType.Store:
            return "store"
        elif input == OperationType.Nop:
            return "nop"
        elif input == OperationType.Route:
            return "route"
        elif input == OperationType.Or:
            return "or"
        elif input == OperationType.Shift:
            return "shift"
        else:
            print("ERROR: OperationType to_string invalid input")
            raise ValueError

    def is_memory_access_op(self) -> bool:
        if self == OperationType.Load or self == OperationType.Store or self == OperationType.Output:
            return True
        else:
            return False



class ConfigId:
    row_id: int
    column_id: int
    context_id: int


class CGRAConfig:
    context_id: int
    operation_type: OperationType
    operation_name: str
    to_config_id: List[ConfigId]
    from_config_id: List[ConfigId]


class PE:
    row_id: int
    column_id: int
    context_size: int
    config_list: List[CGRAConfig]


class Mapping:
    column_num: int
    row_num: int
    context_size: int
    memory_io_type: MemoryIOType
    CGRA_type: CGRAType
    network_type: NetworkType

    PE_array: List[List[PE]]
