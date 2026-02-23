#pragma once
#include <entity/mapping.hpp>
#include <fstream>
#include <remapper/algorithm_entity.hpp>
#include <remapper/remapper.hpp>

namespace remapper {
RemappingResult FullSearchRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file, double timeout_s);
}
