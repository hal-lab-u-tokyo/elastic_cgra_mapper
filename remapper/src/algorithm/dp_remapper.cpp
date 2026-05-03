#include <functional>
#include <remapper/algorithm/dp_remapper.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/mapping_transform_op.hpp>
#include <remapper/remapper.hpp>
#include <remapper/transform.hpp>

struct DPItemId {
  int item_x;
  int item_y;
  int item_z;

  static int container_x;
  static int container_y;
  static int container_z;

  DPItemId(int _x, int _y, int _z) : item_x(_x), item_y(_y), item_z(_z){};
  DPItemId(Eigen::Vector3d vec) {
    item_x = vec.x();
    item_y = vec.y();
    item_z = vec.z();
  }

  bool operator==(const DPItemId& other) const noexcept {
    return (item_x == other.item_x) && (item_y == other.item_y) &&
           (item_z == other.item_z);
  }

  static void SetContainerSize(int x, int y, int z) {
    container_x = x;
    container_y = y;
    container_z = z;
  };
};

int DPItemId::container_x = 0;
int DPItemId::container_y = 0;
int DPItemId::container_z = 0;

struct DPItemIdHash {
  std::size_t operator()(const DPItemId& dp_item_id) const noexcept {
    return std::hash<std::size_t>{}(dp_item_id.item_x) ^
           (std::hash<std::size_t>{}(dp_item_id.item_y) << 1) ^
           (std::hash<std::size_t>{}(dp_item_id.item_z) << 2);
  }
};

struct IdAndPlacement {
  IdAndPlacement(int _id, int _x, int _y, int _rotation_type)
      : id(_id), x(_x), y(_y), rotation_type(_rotation_type){};

  int id;
  int x;
  int y;
  int rotation_type;
};

class RectangleKnapsack {
 public:
  RectangleKnapsack(const Eigen::Vector3d& container_size,
                    const std::vector<Eigen::Vector3d>& item_size_vec,
                    const std::vector<int> item_score_vec,
                    int rotation_type_num)
      : container_size_(container_size),
        item_size_vec_(item_size_vec),
        item_score_vec_(item_score_vec),
        rotation_type_num_(rotation_type_num),
        container_size_id_(container_size) {
    if (item_size_vec_.size() != item_score_vec_.size()) {
      std::cerr << "item_size_vec and item_score_vec size mismatch"
                << std::endl;
      abort();
    }
    DPItemId::SetContainerSize(container_size_.x(), container_size_.y(),
                               container_size_.z());
    // init dp_score_ and dp_id_to_placement_
    for (int x = 0; x <= container_size_.x(); x++) {
      for (int y = 0; y <= container_size_.y(); y++) {
        for (int z = 0; z <= container_size_.z(); z++) {
          DPItemId dp_item_id(x, y, z);
          dp_score_[dp_item_id] = 0;
          dp_id_to_placement_[dp_item_id] = {};
        }
      }
    }
  };

  void ExecKnapsack(int target_score) {
    for (int item_id = 0; item_id < item_size_vec_.size(); item_id++) {
      const int score = item_score_vec_[item_id];
      for (int rotation_type = 0; rotation_type < rotation_type_num_;
           rotation_type++) {
        if (!is_available_item_placement_(item_id, rotation_type)) {
          continue;
        }
        const Eigen::Vector3d rotated_item_size =
            get_rotated_item_size_(item_id, rotation_type);

        const Eigen::Vector3d max_shift_size =
            container_size_ - rotated_item_size;

        for (int x_shift = 0; x_shift < max_shift_size.x() + 1; x_shift++) {
          for (int y_shift = 0; y_shift < max_shift_size.y() + 1; y_shift++) {
            for (int z_shift = 0; z_shift < max_shift_size.z() + 1; z_shift++) {
              const Eigen::Vector3d shift_size =
                  Eigen::Vector3d(x_shift, y_shift, z_shift);
              const Eigen::Vector3d dp_size = rotated_item_size + shift_size;
              const DPItemId dp_size_id(dp_size);

              // 6 pattern test
              std::vector<std::vector<Eigen::Vector3d>>
                  dp_splited_rectangle_size =
                      GetDPSplitedRectangleSize(dp_size, rotated_item_size);

              for (int pattern = 0; pattern < 6; pattern++) {
                int new_dp_value = score;
                for (int rectangle_id = 0; rectangle_id < 3; rectangle_id++) {
                  Eigen::Vector3d tmp_shift_size =
                      GetRectangleShiftSize(rectangle_id, rotated_item_size);
                  Eigen::Vector3d rectangle_size =
                      dp_splited_rectangle_size[pattern][rectangle_id];
                  if (!is_available_transform_(tmp_shift_size,
                                               rectangle_size)) {
                    continue;
                  }
                  new_dp_value += dp_score_[DPItemId(rectangle_size)];
                }

                if (new_dp_value <= dp_score_[dp_size_id]) {
                  continue;
                }

                // update dp
                dp_score_[dp_size_id] = new_dp_value;
                dp_id_to_placement_[dp_size_id].clear();
                for (int rectangle_id = 0; rectangle_id < 3; rectangle_id++) {
                  Eigen::Vector3d tmp_shift_size =
                      GetRectangleShiftSize(rectangle_id, rotated_item_size);
                  Eigen::Vector3d rectangle_size =
                      dp_splited_rectangle_size[pattern][rectangle_id];
                  if (!is_available_transform_(tmp_shift_size,
                                               rectangle_size)) {
                    continue;
                  }
                  DPItemId rectangle_item_id(
                      dp_splited_rectangle_size[pattern][rectangle_id]);
                  for (const IdAndPlacement& result :
                       dp_id_to_placement_[rectangle_item_id]) {
                    IdAndPlacement new_result = result;
                    Eigen::Vector3d tmp_shift_size =
                        GetRectangleShiftSize(rectangle_id, rotated_item_size);

                    new_result = get_shifted_placement_(
                        result, tmp_shift_size.x(), tmp_shift_size.y(),
                        rectangle_size);

                    dp_id_to_placement_[dp_size_id].push_back(new_result);
                  }
                }
                IdAndPlacement new_placement(item_id, 0, 0, rotation_type);
                dp_id_to_placement_[dp_size_id].push_back(new_placement);
              }
            }
          }
        }
      }

      if (dp_score_[container_size_id_] >= target_score) {
        return;
      }
    };
  }

  void SetIsAvailableItemPlacement(
      const std::function<bool(int, int)>& is_available_item_placement) {
    is_available_item_placement_ = is_available_item_placement;
  }

  void SetIsAvailableTransform(
      const std::function<bool(Eigen::Vector3d, Eigen::Vector3d)>&
          is_available_transform) {
    is_available_transform_ = is_available_transform;
  }

  void SetGetRotationItemSize(
      const std::function<Eigen::Vector3d(int, int)>& get_rotated_item_size) {
    get_rotated_item_size_ = get_rotated_item_size;
  }

  void SetGetShiftedPlacement(
      const std::function<IdAndPlacement(
          IdAndPlacement, int, int, Eigen::Vector3d)>& get_shifted_placement) {
    get_shifted_placement_ = get_shifted_placement;
  }

  std::vector<IdAndPlacement> GetResult() const {
    if (dp_score_.find(container_size_id_) == dp_score_.end()) {
      return {};
    }
    return dp_id_to_placement_.at(container_size_id_);
  }

 private:
  std::vector<std::vector<Eigen::Vector3d>> GetDPSplitedRectangleSize(
      const Eigen::Vector3d& container_size, const Eigen::Vector3d& item_size) {
    std::vector<std::vector<Eigen::Vector3d>> dp_splited_rectangle_size;
    const auto shift_size = container_size - item_size;

    std::vector<Eigen::Vector3d> pattern1;
    pattern1.emplace_back(shift_size.x(), container_size.y(), item_size.z());
    pattern1.emplace_back(item_size.x(), shift_size.y(), item_size.z());
    pattern1.emplace_back(container_size.x(), container_size.y(),
                          shift_size.z());
    dp_splited_rectangle_size.emplace_back(pattern1);

    std::vector<Eigen::Vector3d> pattern2;
    pattern2.emplace_back(shift_size.x(), item_size.y(), item_size.z());
    pattern2.emplace_back(container_size.x(), shift_size.y(), item_size.z());
    pattern2.emplace_back(container_size.x(), container_size.y(),
                          shift_size.z());
    dp_splited_rectangle_size.emplace_back(pattern2);

    std::vector<Eigen::Vector3d> pattern3;
    pattern3.emplace_back(shift_size.x(), item_size.y(), item_size.z());
    pattern3.emplace_back(container_size.x(), shift_size.y(),
                          container_size.z());
    pattern3.emplace_back(container_size.x(), item_size.y(), shift_size.z());
    dp_splited_rectangle_size.emplace_back(pattern3);

    std::vector<Eigen::Vector3d> pattern4;
    pattern4.emplace_back(shift_size.x(), container_size.y(), item_size.z());
    pattern4.emplace_back(item_size.x(), shift_size.y(), item_size.z());
    pattern4.emplace_back(container_size.x(), container_size.y(),
                          shift_size.z());
    dp_splited_rectangle_size.emplace_back(pattern4);

    std::vector<Eigen::Vector3d> pattern5;
    pattern5.emplace_back(shift_size.x(), item_size.y(), item_size.z());
    pattern5.emplace_back(container_size.x(), shift_size.y(), item_size.z());
    pattern5.emplace_back(container_size.x(), item_size.y(), shift_size.z());
    dp_splited_rectangle_size.emplace_back(pattern5);

    std::vector<Eigen::Vector3d> pattern6;
    pattern6.emplace_back(shift_size.x(), container_size.y(), item_size.z());
    pattern6.emplace_back(item_size.x(), shift_size.y(), item_size.z());
    pattern6.emplace_back(container_size.x(), container_size.y(),
                          shift_size.z());
    dp_splited_rectangle_size.emplace_back(pattern6);

    return dp_splited_rectangle_size;
  }

  Eigen::Vector3d GetRectangleShiftSize(int rectangle_id,
                                        Eigen::Vector3d item_size) {
    switch (rectangle_id) {
      case 0:

        return Eigen::Vector3d(item_size.x(), 0, 0);
        break;
      case 1:
        return Eigen::Vector3d(0, item_size.y(), 0);
        break;
      case 2:
        return Eigen::Vector3d(0, 0, item_size.z());
        break;
      default:
        std::cerr << "undefined rectangle id" << std::endl;
        abort();
    }
  }

  std::function<bool(int, int)> is_available_item_placement_;
  std::function<bool(Eigen::Vector3d, Eigen::Vector3d)> is_available_transform_;
  std::function<Eigen::Vector3d(int, int)> get_rotated_item_size_;
  std::function<IdAndPlacement(IdAndPlacement, int, int, Eigen::Vector3d)>
      get_shifted_placement_;

  Eigen::Vector3d container_size_;
  DPItemId container_size_id_;
  std::vector<Eigen::Vector3d> item_size_vec_;
  std::vector<int> item_score_vec_;
  std::unordered_map<DPItemId, int, DPItemIdHash> dp_score_;
  std::unordered_map<DPItemId, std::vector<IdAndPlacement>, DPItemIdHash>
      dp_id_to_placement_;
  int rotation_type_num_ = -1;
};

class DPRemappingHelper {
 public:
  DPRemappingHelper(
      const std::vector<remapper::MappingMatrix>& mapping_matrix_vec,
      const remapper::CGRAMatrix& cgra_matrix)
      : mapping_matrix_vec_(mapping_matrix_vec), cgra_matrix_(cgra_matrix) {}
  void SortMappingMatrixByOpRate() {
    auto compare = [&](const remapper::MappingMatrix& left,
                       const remapper::MappingMatrix& right) {
      return left.op_rate > right.op_rate;
    };
    std::sort(mapping_matrix_vec_.begin(), mapping_matrix_vec_.end(), compare);

    mapping_id_to_index_.clear();
    for (int i = 0; i < mapping_matrix_vec_.size(); i++) {
      mapping_id_to_index_[mapping_matrix_vec_[i].id] = i;
      mapping_index_to_id_[i] = mapping_matrix_vec_[i].id;
    }
  }

  remapper::RemappingResult ConvertPlacementResultToRemappingResult(
      const std::vector<IdAndPlacement>& placement_result) const {
    remapper::RemappingResult remapping_result;

    for (const IdAndPlacement& placement : placement_result) {
      const remapper::MappingTransformOp transform_op(
          placement.x, placement.y,
          static_cast<remapper::RotateOp>(placement.rotation_type));
      remapping_result.result_transform_op_vec.push_back(transform_op);
      remapping_result.result_mapping_id_vec.push_back(
          mapping_matrix_vec_[mapping_index_to_id_.at(placement.id)].id);
    }
    return remapping_result;
  }

  Eigen::Vector3d GetContainerSize() const {
    return Eigen::Vector3d(cgra_matrix_.row_size, cgra_matrix_.column_size,
                           cgra_matrix_.context_size);
  }

  std::vector<Eigen::Vector3d> GetItemSizeVec() const {
    std::vector<Eigen::Vector3d> item_size_vec;
    for (const remapper::MappingMatrix& mapping_matrix : mapping_matrix_vec_) {
      item_size_vec.emplace_back(mapping_matrix.row_size,
                                 mapping_matrix.column_size,
                                 mapping_matrix.context_size);
    }
    return item_size_vec;
  }

  std::vector<int> GetItemScoreVec() const {
    std::vector<int> item_score_vec;
    for (const remapper::MappingMatrix& mapping_matrix : mapping_matrix_vec_) {
      item_score_vec.push_back(mapping_matrix.GetParallelNum());
    }
    return item_score_vec;
  }

  int GetRotationTypeNum() const {
    return remapper::kAllRotateOpVecWithoutMirror.size();
  }

  bool IsAvailableItemPlacement(int item_id, int rotation_type) const {
    const remapper::MappingMatrix& mapping_matrix =
        mapping_matrix_vec_[item_id];
    const remapper::RotateOp rotate_op =
        static_cast<remapper::RotateOp>(rotation_type);
    const remapper::MappingTransformOp transform_op(0, 0, rotate_op);
    return cgra_matrix_.IsAvailableRemapping(mapping_matrix, transform_op);
  }

  bool IsAvailableTransform(Eigen::Vector3d shift_size,
                            Eigen::Vector3d rectangle_size) const {
    if (shift_size == Eigen::Vector3d(0, 0, 0)) {
      return true;
    }

    bool is_available = true;
    for (int row_id = 0; row_id < rectangle_size.x(); row_id++) {
      for (int column_id = 0; column_id < rectangle_size.y(); column_id++) {
        const auto before_shift_pe_type =
            cgra_matrix_.GetPEType(row_id, column_id);
        const auto after_shift_pe_type = cgra_matrix_.GetPEType(
            row_id + shift_size.x(), column_id + shift_size.y());
        is_available &= (before_shift_pe_type == after_shift_pe_type);
        if (!is_available) break;
      }
      if (!is_available) break;
    }
    if (is_available) return true;

    return IsAvailableTransformWithRotation(shift_size, rectangle_size);
  }

  Eigen::Vector3d GetRotatedItemSize(int item_id, int rotation_type) const {
    const remapper::MappingMatrix& mapping_matrix =
        mapping_matrix_vec_[item_id];
    const remapper::RotateOp rotate_op =
        static_cast<remapper::RotateOp>(rotation_type);
    const remapper::MappingTransformOp transform_op(0, 0, rotate_op);
    const Eigen::MatrixXi rotated_op_num_mat =
        mapping_matrix.GetRotatedOpNumMatrix(transform_op.rotate_op);

    return {rotated_op_num_mat.rows(), rotated_op_num_mat.cols(),
            rotated_op_num_mat.maxCoeff()};
  }

  IdAndPlacement GetShiftedPlacement(IdAndPlacement placement, int x_shift,
                                     int y_shift,
                                     Eigen::Vector3d rectangle_size) const {
    bool is_available_without_rotation = IsAvailableTransformWithoutRotation(
        Eigen::Vector3d(x_shift, y_shift, 0), rectangle_size);
    bool is_available_with_rotation = IsAvailableTransformWithRotation(
        Eigen::Vector3d(x_shift, y_shift, 0), rectangle_size);

    bool need_rotation =
        !is_available_without_rotation && is_available_with_rotation;
    if (!(is_available_without_rotation || is_available_with_rotation)) {
      std::cerr << "invalid transform" << std::endl;
      abort();
    }

    IdAndPlacement new_placement = placement;
    new_placement.rotation_type =
        need_rotation ? static_cast<int>(remapper::RotateOp::TopIsBottom)
                      : static_cast<int>(remapper::RotateOp::TopIsTop);
    if (need_rotation) {
      new_placement.x = x_shift + rectangle_size.x() - 1 - placement.x;
      new_placement.y = y_shift + rectangle_size.y() - 1 - placement.y;
    } else {
      new_placement.x += x_shift;
      new_placement.y += y_shift;
    }
  }

 private:
  std::vector<remapper::MappingMatrix> mapping_matrix_vec_;
  std::unordered_map<int, int> mapping_id_to_index_;
  std::unordered_map<int, int> mapping_index_to_id_;
  remapper::CGRAMatrix cgra_matrix_;

  bool IsAvailableTransformWithRotation(Eigen::Vector3d shift_size,
                                        Eigen::Vector3d rectangle_size) const {
    // check if shift is available with rotation
    int max_row_id = shift_size.x() + rectangle_size.x() - 1;
    int max_column_id = shift_size.y() + rectangle_size.y() - 1;
    for (int row_id = 0; row_id < rectangle_size.x(); row_id++) {
      for (int column_id = 0; column_id < rectangle_size.y(); column_id++) {
        const auto before_shift_pe_type =
            cgra_matrix_.GetPEType(row_id, column_id);
        const auto after_shift_pe_type = cgra_matrix_.GetPEType(
            max_row_id - row_id, max_column_id - column_id);
        if (before_shift_pe_type != after_shift_pe_type) {
          return false;
        }
      }
    }

    return true;
  }

  bool IsAvailableTransformWithoutRotation(
      Eigen::Vector3d shift_size, Eigen::Vector3d rectangle_size) const {
    if (shift_size == Eigen::Vector3d(0, 0, 0)) {
      return true;
    }

    for (int row_id = 0; row_id < rectangle_size.x(); row_id++) {
      for (int column_id = 0; column_id < rectangle_size.y(); column_id++) {
        const auto before_shift_pe_type =
            cgra_matrix_.GetPEType(row_id, column_id);
        const auto after_shift_pe_type = cgra_matrix_.GetPEType(
            row_id + shift_size.x(), column_id + shift_size.y());
        if (before_shift_pe_type != after_shift_pe_type) {
          return false;
        }
      }
    }
    return true;
  }
};

remapper::RemappingResult remapper::DPRemapping(
    std::vector<remapper::MappingMatrix> mapping_matrix_vec,
    const remapper::CGRAMatrix& cgra_matrix, const int target_parallel_num,
    std::ofstream& log_file) {
  const auto start_time = clock();

  DPRemappingHelper helper(mapping_matrix_vec, cgra_matrix);
  helper.SortMappingMatrixByOpRate();

  RectangleKnapsack solver(helper.GetContainerSize(), helper.GetItemSizeVec(),
                           helper.GetItemScoreVec(),
                           helper.GetRotationTypeNum());
  solver.SetIsAvailableItemPlacement([&](int item_id, int rotation_type) {
    return helper.IsAvailableItemPlacement(item_id, rotation_type);
  });
  solver.SetIsAvailableTransform(
      [&](Eigen::Vector3d shift_size, Eigen::Vector3d rectangle_size) {
        return helper.IsAvailableTransform(shift_size, rectangle_size);
      });
  solver.SetGetRotationItemSize([&](int item_id, int rotation_type) {
    return helper.GetRotatedItemSize(item_id, rotation_type);
  });
  solver.SetGetShiftedPlacement([&](IdAndPlacement placement, int x_shift,
                                    int y_shift,
                                    Eigen::Vector3d rectangle_size) {
    return helper.GetShiftedPlacement(placement, x_shift, y_shift,
                                      rectangle_size);
  });
  solver.ExecKnapsack(target_parallel_num);

  remapper::RemappingResult result =
      helper.ConvertPlacementResultToRemappingResult(solver.GetResult());
  const auto end_time = clock();
  result.remapping_time_s =
      (end_time - start_time) / static_cast<double>(CLOCKS_PER_SEC);
  return result;
}
