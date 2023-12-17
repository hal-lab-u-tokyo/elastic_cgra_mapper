#pragma once
#include <entity/mapping.hpp>
#include <fstream>
#include <remapper/algorithm_entity.hpp>

namespace remapper {
remapper::RemappingResult DPElasticRemapping(
    const std::vector<remapper::MappingMatrix>& mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file);
}