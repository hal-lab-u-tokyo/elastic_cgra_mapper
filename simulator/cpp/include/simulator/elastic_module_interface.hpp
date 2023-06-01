#pragma once

namespace simulator {
class IElasticModule {
 public:
  virtual void UpdateFromDownstream() = 0;
  virtual void UpdateFromUpstream() = 0;

 private:
  virtual void Execution() = 0;
};
}  // namespace simulator