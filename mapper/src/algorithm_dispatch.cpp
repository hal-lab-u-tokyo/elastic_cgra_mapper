#include <stdexcept>

#include "modulo/prisa/routed/engine.hpp"
#include "modulo/sa/routed/engine.hpp"
#include "modulo/yoto/routed/engine.hpp"
#include "modulo/yott/routed/engine.hpp"
#include "placement2d/prisa/array/engine.hpp"
#include "placement2d/prisa/routed/engine.hpp"
#include "placement2d/sa/array/engine.hpp"
#include "placement2d/sa/routed/engine.hpp"
#include "placement2d/yoto/array/engine.hpp"
#include "placement2d/yoto/routed/engine.hpp"
#include "placement2d/yott/array/engine.hpp"
#include "placement2d/yott/routed/engine.hpp"
#include "placement2d/yott_core/array/engine.hpp"
#include "placement2d/yott_core_repair/array/engine.hpp"

namespace mapper::detail {

MappingResult RunPlacement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  switch (kind) {
    case Placement2DArrayKind::kSA:
      return placement2d_sa_array::Placement2DSAEngine(dfg, mrrg, kind, options)
          .Run();
    case Placement2DArrayKind::kPRISA:
      return placement2d_prisa_array::Placement2DPRISAEngine(dfg, mrrg, kind,
                                                             options)
          .Run();
    case Placement2DArrayKind::kPRISANoSIS:
      return placement2d_prisa_array::Placement2DPRISAEngine(dfg, mrrg, kind,
                                                             options)
          .Run();
    case Placement2DArrayKind::kCostAwarePRISA:
      return placement2d_prisa_array::Placement2DPRISAEngine(dfg, mrrg, kind,
                                                             options)
          .Run();
    case Placement2DArrayKind::kPaperGuidedYOTO:
      return placement2d_yoto_array::Placement2DYOTOEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    case Placement2DArrayKind::kCPUMappingYOTO:
      return placement2d_yoto_array::Placement2DYOTOEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    case Placement2DArrayKind::kPaperGuidedYOTT:
      return placement2d_yott_array::Placement2DYOTTEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    case Placement2DArrayKind::kCPUMappingYOTT:
      return placement2d_yott_array::Placement2DYOTTEngine(dfg, mrrg, kind,
                                                           options)
          .Run();
    case Placement2DArrayKind::kCPUMappingYOTTCore:
      return placement2d_yott_core_array::Placement2DYOTTCoreEngine(
                 dfg, mrrg, kind, options)
          .Run();
    case Placement2DArrayKind::kCPUMappingYOTTCoreRepair:
      return placement2d_yott_core_repair_array::
          Placement2DYOTTCoreRepairEngine(dfg, mrrg, kind, options)
              .Run();
  }
  throw std::invalid_argument("unknown placement array kind");
}

MappingResult RunPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  switch (search_kind) {
    case PlacementSearchKind::kYOTO:
      return modulo_yoto_routed::ModuloYOTOEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kYOTT:
      return modulo_yott_routed::ModuloYOTTEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kModuloPhysicalYOTO:
      return modulo_yoto_routed::ModuloYOTOEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kModuloPhysicalYOTT:
      return modulo_yott_routed::ModuloYOTTEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kModuloPhysicalPRISA:
      return modulo_prisa_routed::ModuloPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
      return modulo_prisa_routed::ModuloPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kYOTOWithFallback:
      return modulo_yoto_routed::ModuloYOTOEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kYOTTWithFallback:
      return modulo_yott_routed::ModuloYOTTEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kSA:
      return modulo_sa_routed::ModuloSAEngine(*dfg_ptr, *mrrg_ptr, search_kind,
                                              effective_timeout, log_file_path,
                                              options)
          .Run();
    case PlacementSearchKind::kPlacement2DYOTO:
      return placement2d_yoto_routed::Placement2DYOTOEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DYOTT:
      return placement2d_yott_routed::Placement2DYOTTEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DSA:
      return placement2d_sa_routed::Placement2DSAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DPRISA:
      return placement2d_prisa_routed::Placement2DPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
      return placement2d_prisa_routed::Placement2DPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      return placement2d_prisa_routed::Placement2DPRISAEngine(
                 *dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                 log_file_path, options)
          .Run();
  }
  throw std::invalid_argument("unknown placement search kind");
}

}  // namespace mapper::detail
