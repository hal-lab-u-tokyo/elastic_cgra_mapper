# 2D Placement

| manifest | use |
| --- | --- |
| `compare.json` | placement heuristics and VPR BB |
| `with_ilp.json` | small cases with the placement ILP baseline |
| `literature/yott_2021.json` | the 23 YOTT 2021 case-study DFGs |

The literature manifest is a documented comparison condition, not a claim of
exact paper reproduction.

Trial counts belong in a mapper entry rather than a separate preset:

```json
{
  "name": "yott_100",
  "mapper_config": "research/configs/mapper/placement2d/yott.json",
  "algorithm_overrides": {"max_trials": 100}
}
```

Grid, I/O, and network values are listed in
[`../../arch_templates/README.md`](../../arch_templates/README.md).
