#pragma once

#include <entity/architecture.hpp>
#include <entity/operation.hpp>
#include <map>
#include <queue>
#include <simulator/elastic_wire.hpp>

namespace simulator {
class IElasticModule {
 public:
  virtual void UpdateFromDownstream() = 0;
  virtual void UpdateFromUpstream() = 0;

 private:
  virtual void Execution() = 0;
};

template <typename T>
class ElasticMultiPlexer : public IElasticModule {
 public:
  ElasticMultiPlexer(){};
  ElasticMultiPlexer(ElasticWire<T> output_wire, int config_size)
      : output_wire_(output_wire), config_size_(config_size) {
    config_vec_.reserve(config_size);
    config_id_ = 0;
  };

  void UpdateFromDownstream() {
    bool updated_stop = output_wire_.Stop();
    entity::PEPositionId tmp_input_id = config_vec_[config_id_];
    input_wire_[tmp_input_id].UpdateFromDownstream(updated_stop);
    Execution();
    return;
  };

  void UpdateFromUpstream() {
    entity::PEPositionId tmp_input_id = config_vec_[config_id_];
    bool updated_valid = input_wire_[tmp_input_id].IsValid();
    int updated_value = input_wire_[tmp_input_id].GetValue();

    output_wire_.UpdateFromUpstream(updated_value, updated_valid);
    Execution();
    return;
  };

  void SetInputWire(entity::PEPositionId position_id, ElasticWire<T> wire) {
    input_wire_.emplace(position_id, wire);
    return;
  };

  int GetOutputWire() { return output_wire_.GetValue(); };

  void SetConfig(int config_id, entity::PEPositionId input_position_id) {
    config_vec_[config_id] = input_position_id;
    return;
  };

 private:
  void Execution() {
    entity::PEPositionId tmp_input_id = config_vec_[config_id_];
    if (input_wire_[tmp_input_id].IsReady() && output_wire_.IsReady()) {
      config_id_++;
      config_id_ %= config_size_;
    }
  };
  ElasticWire<T> output_wire_;
  std::unordered_map<entity::PEPositionId, ElasticWire<T>,
                     entity::HashPEPositionId>
      input_wire_;
  std::vector<entity::PEPositionId> config_vec_;
  int config_id_;
  int config_size_;
};

template <typename T>
class ElasticJoin : public IElasticModule {
 public:
  ElasticJoin(){};
  void UpdateFromDownstream() {
    bool updated_stop = false;
    for (auto wire : output_wire_) {
      updated_stop |= wire.Stop();
    }

    for (auto wire : input_wire_) {
      wire.UpdateFromDownstream(updated_stop);
    }

    Execution();
    return;
  };

  void UpdateFromUpstream() {
    bool is_input_ready = true;
    for (auto wire : input_wire_) {
      is_input_ready &= wire.IsReady();
    }

    if (is_input_ready) {
      for (int i = 0; i < input_wire_.size(); i++) {
        output_wire_[i].UpdateFromUpstream(input_wire_[i].GetValue(), 1);
      }
    }

    Execution();
    return;
  };

  void SetWire(ElasticWire<T> input_wire, ElasticWire<T> output_wire) {
    input_wire_.push_back(input_wire);
    output_wire_.push_back(output_wire);

    return;
  };

 private:
  void Execution() { return; };
  std::vector<ElasticWire<T>> output_wire_;
  std::vector<ElasticWire<T>> input_wire_;
};

template <typename T>
class ElasticFork : public IElasticModule {
 public:
  ElasticFork(){};
  ElasticFork(int config_size) : output_wire_({}), config_size_(config_size) {
    config_vec_.reserve(config_size);
    config_id_ = 0;
    return;
  };

  void UpdateFromDownstream() {
    bool updated_stop = true;
    std::vector<entity::PEPositionId> tmp_output_position_id_vec =
        config_vec_[config_id_];
    for (auto output_position_id : tmp_output_position_id_vec) {
      updated_stop &= output_wire_[output_position_id].Stop();
    }

    input_wire_.UpdateFromDownstream(updated_stop);
    Execution();

    return;
  };

  void UpdateFromUpstream() {
    std::vector<entity::PEPositionId> tmp_output_position_id_vec =
        config_vec_[config_id_];
    for (auto position_id : tmp_output_position_id_vec) {
      output_wire_[position_id].UpdateFromUpstream(input_wire_.GetValue(),
                                                   input_wire_.IsValid());
    }
    Execution();

    return;
  };

  void SetConfig(int config_id,
                 std::vector<entity::PEPositionId> output_position_id_vec) {
    config_vec_[config_id] = output_position_id_vec;

    return;
  };

  void SetInputWire(ElasticWire<T> wire) {
    input_wire_ = wire;

    return;
  };

  void SetOutputWire(entity::PEPositionId position_id, ElasticWire<T> wire) {
    if (output_wire_.count(position_id) == 0) {
      output_wire_.emplace(position_id, wire);
    }

    return;
  };

 private:
  void Execution() {
    std::vector<entity::PEPositionId> tmp_output_position_id_vec =
        config_vec_[config_id_];
    for (auto position_id : tmp_output_position_id_vec) {
      if (received_data_PE_set_.count(position_id) > 0) continue;
      if (output_wire_[position_id].IsReady()) {
        received_data_PE_set_.emplace(position_id);
      }
    }
    if (received_data_PE_set_.size() == tmp_output_position_id_vec.size()) {
      config_id_++;
      config_id_ %= config_size_;
    }

    return;
  };

  ElasticWire<T> input_wire_;
  std::unordered_map<entity::PEPositionId, ElasticWire<T>,
                     entity::HashPEPositionId>
      output_wire_;
  std::vector<std::vector<entity::PEPositionId>> config_vec_;
  std::set<entity::PEPositionId> received_data_PE_set_;
  int config_id_;
  int config_size_;
};

template <typename T>
class ElasticVLU : public IElasticModule {
 public:
  ElasticVLU(){};
  ElasticVLU(
      std::vector<ElasticWire<T>> input, ElasticWire<T> output, int config_size,
      std::function<int(entity::OpType, int, int)> execute_operation_func)
      : input_wire_(input),
        output_wire_(output),
        execute_operation_func_(execute_operation_func),
        config_size_(config_size) {
    config_id_ = 0;
    config_vec_.reserve(config_size);
  };

  void UpdateFromDownstream() {
    for (auto wire : input_wire_) {
      wire.UpdateFromDownstream(output_wire_.Stop());
    }
    Execution();

    return;
  };

  void UpdateFromUpstream() {
    bool updated_valid = true;
    for (auto wire : input_wire_) {
      updated_valid &= wire.IsValid();
    }

    entity::OpType tmp_op = config_vec_[config_id_];
    bool updated_value = execute_operation_func_(
        tmp_op, input_wire_[0].GetValue(), input_wire_[1].GetValue());
    output_wire_.UpdateFromUpstream(updated_value, updated_valid);

    Execution();

    return;
  };

 private:
  void Execution() {
    bool is_input_ready = true;
    for (auto wire : input_wire_) {
      is_input_ready &= wire.IsReady();
    }
    bool is_output_ready = output_wire_.IsReady();

    if (is_input_ready && is_output_ready) {
      config_id_++;
      config_id_ %= config_size_;
    }
  };
  std::vector<ElasticWire<T>> input_wire_;
  ElasticWire<T> output_wire_;
  std::function<int(entity::OpType, int, int)> execute_operation_func_;
  std::vector<entity::OpType> config_vec_;
  int config_id_;
  int config_size_;
};

template <typename T>
class ElasticBuffer : public IElasticModule {
 public:
  ElasticBuffer(){};
  ElasticBuffer(ElasticWire<T> input, ElasticWire<T> output, int buffer_size) {
    input_wire_ = input;
    output_wire_ = output;
    buffer_size_ = buffer_size;
  };

  void UpdateFromDownstream() {
    if (output_wire_.IsReady()) {
      buffer_.pop_front();
    }

    Execution();
  };

  void UpdateFromUpstream() {
    if (input_wire_.IsReady()) {
      buffer_.push_back(input_wire_.GetValue());
    }

    Execution();
  };

 private:
  void Execution() {
    bool updated_stop = buffer_.size() >= buffer_size_;
    input_wire_.UpdateFromDownstream(updated_stop);

    bool valid = buffer_.size() > 0;
    output_wire_.UpdateFromUpstream(buffer_.front(), valid);
  };
  ElasticWire<T> input_wire_, output_wire_;

  int buffer_size_;
  std::deque<T> buffer_;
};
}  // namespace simulator