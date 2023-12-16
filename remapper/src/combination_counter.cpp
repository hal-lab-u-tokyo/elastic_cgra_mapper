#include "remapper/combination_counter.hpp"

remapper::CombinationCounter::CombinationCounter(
    const std::vector<int>& max_num_vector, const std::vector<int>& type_vector)
    : max_num_vector_(max_num_vector), type_vector_(type_vector) {
  assert(max_num_vector_.size() == type_vector_.size());
  size_ = max_num_vector_.size();
  for (std::size_t i = 0; i < size_; i++) {
    if (type_to_index_map.count(type_vector_[i]) == 0) {
      type_to_index_map.emplace(type_vector_[i], std::vector<int>({}));
    }
    type_to_index_map.at(type_vector_[i]).push_back(i);
  }
  Initialize();
};

remapper::CombinationCounter::CombinationCounter(int max_num, int size)
    : size_(size) {
  max_num_vector_ = std::vector<int>(size, max_num);
  type_vector_ = std::vector<int>(size, 0);

  for (std::size_t i = 0; i < size_; i++) {
    if (type_to_index_map.count(type_vector_[i]) == 0) {
      type_to_index_map.emplace(type_vector_[i], std::vector<int>({}));
    }
    type_to_index_map.at(type_vector_[i]).push_back(i);
  }
  Initialize();
};

void remapper::CombinationCounter::Initialize() {
  tmp_combination_ = std::vector<int>(size_, 0);
}

bool remapper::CombinationCounter::Next() {
  for (std::size_t i = 0; i < size_; i++) {
    if (i == 0) {
      tmp_combination_[i]++;
    }
    if (tmp_combination_[i] > max_num_vector_[i]) {
      if (i == size_ - 1) return false;
      tmp_combination_[i] = 0;
      tmp_combination_[i + 1]++;
    }
  }

  for (auto itr = type_to_index_map.begin(); itr != type_to_index_map.end();
       itr++) {
    if (itr->second.size() == 1) continue;
    for (std::size_t i = 0; i < itr->second.size() - 1; i++) {
      int tmp = itr->second[i];
      int next = itr->second[i + 1];
      if (tmp_combination_[tmp] < tmp_combination_[next]) {
        int diff = tmp_combination_[next] - tmp_combination_[tmp];
        return Next(tmp, diff);
      }
    }
  }

  return true;
}

bool remapper::CombinationCounter::Next(int id, int num) {
  for (std::size_t i = static_cast<std::size_t>(id); i < size_; i++) {
    if (i == id) {
      tmp_combination_[i] += num;
    }
    if (tmp_combination_[i] > max_num_vector_[i]) {
      if (i == size_ - 1) return false;
      int q = tmp_combination_[i] / (max_num_vector_[i] + 1);
      int r = tmp_combination_[i] % (max_num_vector_[i] + 1);
      tmp_combination_[i] = r;
      tmp_combination_[i + 1] += q;
    }
  }

  for (auto itr = type_to_index_map.begin(); itr != type_to_index_map.end();
       itr++) {
    if (itr->second.size() == 1) continue;
    for (std::size_t i = 0; i < itr->second.size() - 1; i++) {
      int tmp = itr->second[i];
      int next = itr->second[i + 1];
      if (tmp < next) {
        return Next();
      }
    }
  }

  return true;
}