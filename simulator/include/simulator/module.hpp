#pragma once

namespace simulator {
class IModule {
  virtual void Update() = 0;
  virtual void RegisterUpdate() = 0;
};
}  // namespace simulator