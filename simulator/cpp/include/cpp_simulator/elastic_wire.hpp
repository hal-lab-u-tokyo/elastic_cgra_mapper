#pragma once

#include <functional>
#include <cpp_simulator/elastic_module_interface.hpp>

namespace simulator {
struct SELFProtocolSignal {
  SELFProtocolSignal() : valid(1), stop(0) {}
  void UpdateValue(bool _valid, bool _stop) {
    valid = _valid;
    stop = _stop;
  }
  bool IsReady() { return valid & !stop; };
  bool valid;
  bool stop;
};

using ElasticModulePtr = std::shared_ptr<IElasticModule>;

template <typename T>
class ElasticWire {
 public:
  ElasticWire() {
    value_ = std::make_shared<T>();
    self_protocol_signal_ = std::make_shared<SELFProtocolSignal>();
    downstream_elastic_module_vec_ =
        std::make_shared<std::vector<ElasticModulePtr>>();
    upstream_elastic_module_vec_ =
        std::make_shared<std::vector<ElasticModulePtr>>();
  }
  void UpdateFromUpstream(T value, bool valid) {
    int prev_value = *value_;
    bool prev_valid = self_protocol_signal_->valid;

    *value_ = value;
    self_protocol_signal_->valid = valid;

    if (prev_value == value && prev_valid == valid) return;
    if (prev_valid == true || valid == true) {
      for (auto module : *downstream_elastic_module_vec_) {
        module->UpdateFromUpstream();
      }
    }
    return;
  };
  void UpdateFromDownstream(bool stop) {
    bool prev_stop = self_protocol_signal_->stop;
    self_protocol_signal_->stop = stop;
    if (prev_stop == stop) return;
    for (auto module : *upstream_elastic_module_vec_) {
      module->UpdateFromDownstream();
    }
    return;
  };
  T GetValue() { return *(value_); };
  bool IsValid() { return self_protocol_signal_->valid; }
  bool Stop() { return self_protocol_signal_->stop; }
  bool IsReady() { return self_protocol_signal_->IsReady(); }
  void SetUpstreamElasticModule(ElasticModulePtr upstream_elastic_module) {
    upstream_elastic_module_vec_->push_back(upstream_elastic_module);
    return;
  }
  void SetDownstreamElasticModule(ElasticModulePtr downstream_elastic_module) {
    downstream_elastic_module_vec_->push_back(downstream_elastic_module);
  }

 private:
  std::shared_ptr<T> value_;
  std::shared_ptr<SELFProtocolSignal> self_protocol_signal_;
  std::shared_ptr<std::vector<ElasticModulePtr>> upstream_elastic_module_vec_;
  std::shared_ptr<std::vector<ElasticModulePtr>> downstream_elastic_module_vec_;
};
}  // namespace simulator