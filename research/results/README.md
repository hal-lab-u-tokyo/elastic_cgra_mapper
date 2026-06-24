# Research Results

Generated experiment results are ignored by git. This directory keeps only layout markers and this guide.

The runner creates result directories from each manifest's `result_group`:

```text
research/results/<result_group>/<YYYYMMDD-HHMMSS[_tag]>/
```

Common groups:

- `modulo/search/`
- `modulo/all_mappers/`
- `placement2d/search/`
- `placement2d/all_mappers/`
- `placement2d/reproduction/traversal_yott/`
- `placement2d/reproduction/yott_cases2021/`
- `placement2d/reproduction/prisa_vpr8/`
- `preflight/`
- `benchmark_compatibility/`

Each run usually contains:

- `run_info.md`
- `run_metadata.json`
- `metrics.csv`
- `summary.json`
- `summary_by_mapper.md`
- `summary_by_set.md`
- `benchmark_report.md`
- `routing_validation.md` when route checks are applicable

Run reports can be regenerated with:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir <result_dir>
```
