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
    FAdd = 1
    Sub = 2
    Mul = 3
    FMul = 4
    Div = 5
    Const = 6
    Load = 7
    Output = 8
    Store = 9
    Nop = 10
    Route = 11
    Or = 12
    And = 13
    ShiftL = 14
    ShiftR = 15
    icmp = 16
    Loop = 17
    Select = 18
    Cmpgt = 19
    Cmpge = 20
    Cmpeq = 21
    Xor = 22
    SDiv = 23
    FDiv = 24
    FSub = 25
    TM = 26
    SPM = 27
    Cmpne = 28

    def get_from_string(input: str):
        if input == "add":
            return OperationType.Add
        elif input == "fadd":
            return OperationType.FAdd
        elif input == "sub":
            return OperationType.Sub
        elif input == "fsub":
            return OperationType.FSub
        elif input == "mul":
            return OperationType.Mul
        elif input == "fmul":
            return OperationType.FMul
        elif input == "div":
            return OperationType.Div
        elif input == "sdiv":
            return OperationType.SDiv
        elif input == "fdiv":
            return OperationType.FDiv
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
        elif input == "and":
            return OperationType.And
        elif input == "xor":
            return OperationType.Xor
        elif input == "shiftl" or input == "shl":
            return OperationType.ShiftL
        elif input == "shiftr" or input == "lshr" or input == "ashr" or input == "shift":
            # Keep compatibility with legacy "shift" op-string.
            return OperationType.ShiftR
        elif input == "icmp":
            return OperationType.icmp
        elif input == "cmpgt" or input == "icmpgt":
            return OperationType.Cmpgt
        elif input == "cmpge" or input == "icmpge":
            return OperationType.Cmpge
        elif input == "cmpeq" or input == "icmpeq":
            return OperationType.Cmpeq
        elif input == "cmpne" or input == "icmpne":
            return OperationType.Cmpne
        elif input == "loop":
            return OperationType.Loop
        elif input == "tm":
            return OperationType.TM
        elif input == "spm":
            return OperationType.SPM
        elif input == "select":
            return OperationType.Select
        else:
            print("ERROR: OperationType get_from_string invalid input")
            raise ValueError

    def to_string(input):
        if input == OperationType.Add:
            return "add"
        elif input == OperationType.FAdd:
            return "fadd"
        elif input == OperationType.Sub:
            return "sub"
        elif input == OperationType.FSub:
            return "fsub"
        elif input == OperationType.Mul:
            return "mul"
        elif input == OperationType.FMul:
            return "fmul"
        elif input == OperationType.Div:
            return "div"
        elif input == OperationType.SDiv:
            return "sdiv"
        elif input == OperationType.FDiv:
            return "fdiv"
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
        elif input == OperationType.And:
            return "and"
        elif input == OperationType.Xor:
            return "xor"
        elif input == OperationType.ShiftL:
            return "shiftl"
        elif input == OperationType.ShiftR:
            return "shiftr"
        elif input == OperationType.icmp:
            return "icmp"
        elif input == OperationType.Cmpgt:
            return "cmpgt"
        elif input == OperationType.Cmpge:
            return "cmpge"
        elif input == OperationType.Cmpeq:
            return "cmpeq"
        elif input == OperationType.Cmpne:
            return "cmpne"
        elif input == OperationType.Loop:
            return "loop"
        elif input == OperationType.TM:
            return "tm"
        elif input == OperationType.SPM:
            return "spm"
        elif input == OperationType.Select:
            return "select"
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
    dropped_to_config_id: List[ConfigId]
    dropped_from_config_id: List[ConfigId]


class PE:
    row_id: int
    column_id: int
    context_size: int
    config_list: List[CGRAConfig]


class ArchitectureNode:
    node_id: str
    row_id: int
    column_id: int
    context_id: int
    supported_operations: List[OperationType]
    memory_accessible: bool


class ArchitectureEdge:
    from_node_id: str
    to_node_id: str


class Mapping:
    column_num: int
    row_num: int
    context_size: int
    memory_io_type: MemoryIOType
    CGRA_type: CGRAType
    network_type: NetworkType
    extension: str

    PE_array: List[List[PE]]
    extra_PE_list: List[PE]
    PE_dict: dict
    architecture_node_list: List[ArchitectureNode]
    architecture_edge_list: List[ArchitectureEdge]
