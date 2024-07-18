import parse

class RemapperExecLog:
    def __init__(self):
        self.mapping_num = 0
        self.shift_num = 0
        self.rotation = [0, 0, 0, 0, 0, 0, 0, 0]

def remapping_exec_log_reader(exec_log_file):
    remapper_exec_log: RemapperExecLog = RemapperExecLog()

    with open(exec_log_file) as f:
        offset = -1
        is_shift = False
        for line in f:
            if line.startswith("----- mapping -----"):
                remapper_exec_log.mapping_num = remapper_exec_log.mapping_num + 1
                offset = 0
            if offset == 4:
                parsed = parse.parse("row shift: {:d}\n", line)
                if parsed != None:
                    if parsed[0] != 0:
                        is_shift = True
            if offset == 5:
                parsed = parse.parse("column shift: {:d}\n", line)
                if parsed != None:
                    if parsed[0] != 0:
                        is_shift = True
                    if is_shift:
                        remapper_exec_log.shift_num = remapper_exec_log.shift_num + 1
            if offset == 6:
                parsed = parse.parse("rotation: {:d}\n", line)
                if parsed != None:
                    remapper_exec_log.rotation[parsed[0]] = remapper_exec_log.rotation[parsed[0]] + 1



            offset = offset + 1

            if remapper_exec_log.mapping_num > 1000:
                break

    return remapper_exec_log
