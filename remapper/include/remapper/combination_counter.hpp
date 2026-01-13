#pragma once
#include <cassert>
#include <unordered_map>
#include <vector>

namespace remapper {
class CombinationCounter {
 public:
  CombinationCounter(const std::vector<int>& max_num_vector,
                     const std::vector<int>& type_vector);
  CombinationCounter(int max_num, int size);
  void Initialize();
  bool Next();
  bool Next(int id, int num);
  std::vector<int> GetCombination() const { return tmp_combination_; };

 private:
  std::size_t size_;
  std::unordered_map<int, std::vector<int>> type_to_index_map;
  std::vector<int> max_num_vector_;
  std::vector<int> type_vector_;
  std::vector<int> tmp_combination_;
};
}  // namespace remapper
