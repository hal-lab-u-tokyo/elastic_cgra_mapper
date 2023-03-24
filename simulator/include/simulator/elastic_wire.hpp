#pragma once

#include <functional>

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

template <typename T>
class ElasticWire {
 public:
  ElasticWire() {
    value_ = std::make_shared<T>();
    self_protocol_signal_ = std::make_shared<SELFProtocolSignal>();
  }
  void UpdateFromUpstream(T value, bool valid) {
    int prev_value = *value_;
    bool prev_valid = self_protocol_signal_->valid;

    *value_ = value;
    self_protocol_signal_->valid = valid;

    if (prev_value == value && prev_valid == valid) return;
    if (prev_valid == true || valid == true) {
      for (auto func : on_updated_from_upstream_vec_) {
        (*func)();
      }
    }
    return;
  };
  void UpdateFromDownstream(bool stop) {
    bool prev_stop = self_protocol_signal_->stop;
    self_protocol_signal_->stop = stop;
    if (prev_stop == stop) return;
    for (auto func : on_updated_from_downstream_vec_) {
      (*func)();
    }
    return;
  };
  T GetValue() { return *(value_); };
  bool IsValid() { return self_protocol_signal_->valid; }
  bool Stop() { return self_protocol_signal_->stop; }
  bool IsReady() { return self_protocol_signal_->IsReady(); }
  void SetOnUpdateFromDownstreamFunc(
      std::shared_ptr<std::function<void(void)>> on_update_value_ptr) {
    on_updated_from_downstream_vec_.push_back(on_update_value_ptr);
    return;
  }
  void SetOnUpdateFromUpstreamFunc(
      std::shared_ptr<std::function<void(void)>> on_update_value_ptr) {
    on_updated_from_upstream_vec_.push_back(on_update_value_ptr);
  }

 private:
  std::shared_ptr<T> value_;
  std::shared_ptr<SELFProtocolSignal> self_protocol_signal_;
  std::vector<std::shared_ptr<std::function<void(void)>>>
      on_updated_from_downstream_vec_;
  std::vector<std::shared_ptr<std::function<void(void)>>>
      on_updated_from_upstream_vec_;
};
}  // namespace simulator