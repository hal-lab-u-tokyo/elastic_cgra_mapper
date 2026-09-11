# Mapper

C++ mapping algorithms and their common interface.

## Execution

```text
mapper JSON
  -> mapper::CreateMapper(Algorithm.type)
  -> mapper::IMapper::Configure()
  -> mapper::IMapper::Execution()
  -> mapper::MappingResult
```

`mapper/src/mapper_registry.cpp` lists every accepted `Algorithm.type`,
including compatibility aliases.

## Layout

```text
mapper/include/mapper/
  mapper.hpp                 interface, options, and result
  mapper_factory.hpp         creation and registration API
  placement2d/               2D placement frontends
  modulo/                    modulo mapping frontends

mapper/src/
  mapper.cpp                 shared option application
  mapper_factory.cpp         runtime factory
  mapper_registry.cpp        built-in mapper names
  placement2d/               2D placement adapters and engines
  modulo/                    modulo placement and routing
  common/search/             shared placement/routing search
```

- [2D placement](src/placement2d/README.md)
- [Modulo mapping](src/modulo/README.md)
- [Shared search engine](src/common/search/README.md)

## Boundaries

- A frontend selects an engine kind and forwards configuration.
- An algorithm file contains traversal, candidate selection, or optimization.
- `support/` contains behavior shared by multiple algorithms.
- 2D placement returns PE assignments without routes.
- Modulo mapping assigns contexts and constructs or validates routes.

## Adding A Mapper

1. Add its public class under `mapper/include/mapper/<problem>/`.
2. Put the algorithm under the matching `mapper/src/<problem>/` tree.
3. Add its stable JSON name to `mapper/src/mapper_registry.cpp`.
4. Add a preset under `research/configs/mapper/`.
5. Run one focused experiment before adding it to broad comparisons.

CMake discovers new `.cpp` files under `mapper/src/` after reconfiguration.
