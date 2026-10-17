# Placement2DYOTTEngine

This directory owns YOTT variants only. `mapper/include/mapper/yott/engine/array/yott_array_engine.hpp` declares engine state and
methods; placement, objective, option, and routing helpers remain local.
The implementation retains the original operations, defaults, and random-number
ordering. The outer mapper dispatcher selects this engine without hosting any
algorithm logic.
