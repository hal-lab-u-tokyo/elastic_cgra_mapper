# Research Results

Generated experiment results are ignored by git.

The runner writes each run beneath the manifest's `result_group`:

```text
research/results/<result_group>/<timestamp>[_tag]/
```

Each run contains normalized metrics, benchmark reports, validation reports,
the expanded manifest, and environment metadata. See
[`../README.md`](../README.md#results) for the file list.

Run reports can be regenerated with:

```bash
python3 research/scripts/generate_reports.py \
  --result-dir <result_dir>
```
