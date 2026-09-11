# Configs

```text
configs/
  arch_templates/   CGRA templates
  mapper/            one preset per mapper
  experiments/       benchmark, architecture, and mapper combinations
```

Start with [`experiments/placement2d/compare.json`](experiments/placement2d/compare.json)
or [`experiments/modulo/compare.json`](experiments/modulo/compare.json).

Experiment manifests may override preset values with `algorithm_overrides`.
