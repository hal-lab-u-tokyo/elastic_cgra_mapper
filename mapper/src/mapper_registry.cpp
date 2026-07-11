#include <initializer_list>
#include <mapper/mapper_factory.hpp>
#include <mapper/modulo/connectivity_path_ilp_mapper.hpp>
#include <mapper/modulo/full_routing_ilp_mapper.hpp>
#include <mapper/modulo/modulo_placement_first_mapper.hpp>
#include <mapper/modulo/modulo_prisa_mapper.hpp>
#include <mapper/modulo/modulo_sa_mapper.hpp>
#include <mapper/modulo/modulo_yoto_mapper.hpp>
#include <mapper/modulo/modulo_yoto_with_fallback_mapper.hpp>
#include <mapper/modulo/modulo_yott_mapper.hpp>
#include <mapper/modulo/modulo_yott_with_fallback_mapper.hpp>
#include <mapper/placement2d/placement2d_ilp_mapper.hpp>
#include <mapper/placement2d/placement2d_prisa_mapper.hpp>
#include <mapper/placement2d/placement2d_sa_mapper.hpp>
#include <mapper/placement2d/placement2d_yoto_mapper.hpp>
#include <mapper/placement2d/placement2d_yott_core_mapper.hpp>
#include <mapper/placement2d/placement2d_yott_core_repair_mapper.hpp>
#include <mapper/placement2d/placement2d_yott_mapper.hpp>

namespace {

template <typename MapperT>
void RegisterNames(std::initializer_list<const char*> names) {
  for (const char* name : names) {
    mapper::RegisterMapperType<MapperT>(name);
  }
}

bool RegisterBuiltInMappers() {
  // 2D placement.
  RegisterNames<mapper::Placement2DILPMapper>({"Placement2DILPMapper"});
  RegisterNames<mapper::Placement2DYOTOMapper>(
      {"Placement2DYOTOMapper", "Placement2DYOTO"});
  RegisterNames<mapper::Placement2DYOTTMapper>(
      {"Placement2DYOTTMapper", "Placement2DYOTT"});
  RegisterNames<mapper::Placement2DPaperGuidedArrayYOTOMapper>(
      {"Placement2DPaperGuidedArrayYOTOMapper",
       "Placement2DPaperGuidedArrayYOTO", "Placement2DFaithfulArrayYOTOMapper",
       "Placement2DFaithfulArrayYOTO"});
  RegisterNames<mapper::Placement2DPaperGuidedArrayYOTTMapper>(
      {"Placement2DPaperGuidedArrayYOTTMapper",
       "Placement2DPaperGuidedArrayYOTT", "Placement2DFaithfulArrayYOTTMapper",
       "Placement2DFaithfulArrayYOTT"});
  RegisterNames<mapper::Placement2DCPUMappingYOTOMapper>(
      {"Placement2DCPUMappingYOTOMapper", "Placement2DCPUMappingYOTO"});
  RegisterNames<mapper::Placement2DCPUMappingYOTTMapper>(
      {"Placement2DCPUMappingYOTTMapper", "Placement2DCPUMappingYOTT"});
  RegisterNames<mapper::Placement2DCPUMappingYOTTCoreMapper>(
      {"Placement2DCPUMappingYOTTCoreMapper", "Placement2DCPUMappingYOTTCore"});
  RegisterNames<mapper::Placement2DCPUMappingYOTTCoreRepairMapper>(
      {"Placement2DCPUMappingYOTTCoreRepairMapper",
       "Placement2DCPUMappingYOTTCoreRepair",
       "Placement2DCPUMappingYOTTCoreBottleneckRepairMapper",
       "Placement2DCPUMappingYOTTCoreBottleneckRepair"});
  RegisterNames<mapper::Placement2DSAMapper>(
      {"Placement2DSAMapper", "Placement2DSA"});
  RegisterNames<mapper::Placement2DArraySAMapper>(
      {"Placement2DArraySAMapper", "Placement2DArraySA"});
  RegisterNames<mapper::Placement2DPRISAMapper>(
      {"Placement2DPRISAMapper", "Placement2DPRISA"});
  RegisterNames<mapper::Placement2DPRISANoSISMapper>(
      {"Placement2DPRISANoSISMapper", "Placement2DPRISANoSIS"});
  RegisterNames<mapper::Placement2DCostAwarePRISAMapper>(
      {"Placement2DCostAwarePRISAMapper", "Placement2DCostAwarePRISA"});
  RegisterNames<mapper::Placement2DArrayPRISAMapper>(
      {"Placement2DArrayPRISAMapper", "Placement2DArrayPRISA"});
  RegisterNames<mapper::Placement2DArrayPRISANoSISMapper>(
      {"Placement2DArrayPRISANoSISMapper", "Placement2DArrayPRISANoSIS"});
  RegisterNames<mapper::Placement2DArrayCostAwarePRISAMapper>(
      {"Placement2DArrayCostAwarePRISAMapper",
       "Placement2DArrayCostAwarePRISA"});

  // Modulo placement and routing.
  RegisterNames<mapper::ConnectivityPathILPMapper>(
      {"ConnectivityPathILPMapper", "ConnectivityBasedILPMapper"});
  RegisterNames<mapper::FullRoutingILPMapper>(
      {"FullRoutingILPMapper", "ILPMapper", "GurobiILPMapper"});
  RegisterNames<mapper::ModuloPlacementFirstMapper>(
      {"ModuloPlacementFirstMapper", "ModuloPlacementFirst",
       "PlacementFirstHeuristicMapper"});
  RegisterNames<mapper::ModuloSAMapper>(
      {"ModuloSAMapper", "ModuloSA", "SAMapper", "SA"});
  RegisterNames<mapper::ModuloYOTOMapper>(
      {"ModuloYOTOMapper", "ModuloYOTO", "YOTOMapper", "YOTO"});
  RegisterNames<mapper::ModuloPhysicalYOTOMapper>(
      {"ModuloPhysicalYOTOMapper", "ModuloPhysicalYOTO"});
  RegisterNames<mapper::ModuloYOTOWithFallbackMapper>(
      {"ModuloYOTOWithFallbackMapper", "ModuloYOTOWithFallback"});
  RegisterNames<mapper::ModuloYOTTMapper>(
      {"ModuloYOTTMapper", "ModuloYOTT", "YOTTMapper", "YOTT"});
  RegisterNames<mapper::ModuloPhysicalYOTTMapper>(
      {"ModuloPhysicalYOTTMapper", "ModuloPhysicalYOTT"});
  RegisterNames<mapper::ModuloYOTTWithFallbackMapper>(
      {"ModuloYOTTWithFallbackMapper", "ModuloYOTTWithFallback"});
  RegisterNames<mapper::ModuloPhysicalPRISAMapper>(
      {"ModuloPhysicalPRISAMapper", "ModuloPhysicalPRISA"});
  RegisterNames<mapper::ModuloPhysicalPRISAManhattanMapper>(
      {"ModuloPhysicalPRISAManhattanMapper", "ModuloPhysicalPRISAManhattan"});
  return true;
}

const bool kBuiltInMappersRegistered = RegisterBuiltInMappers();

}  // namespace
