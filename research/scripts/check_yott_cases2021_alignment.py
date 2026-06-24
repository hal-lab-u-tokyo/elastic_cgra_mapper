#!/usr/bin/env python3

import argparse
import ast
import csv
from pathlib import Path


NAME_ALIASES = {
    "Cplx8": "cplx8",
    "FilterRGB": "filter_rgb",
    "Fir16": "fir16",
}


def canonical_name(name: str) -> str:
    stem = Path(name).stem
    return NAME_ALIASES.get(stem, stem).lower()


def read_paper_rows(path: Path) -> dict:
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        rows = {}
        for row in reader:
            benchmark = row.get("benchmark", "")
            if not benchmark or benchmark == "avg":
                continue
            rows[canonical_name(benchmark)] = row
        return rows


def read_normalization_rows(path: Path) -> dict:
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        rows = {}
        for row in reader:
            key = canonical_name(row["source"])
            op_counts = {}
            if row.get("op_counts"):
                op_counts = ast.literal_eval(row["op_counts"])
            row["normalized_loads"] = str(op_counts.get("load", 0))
            row["normalized_outputs"] = str(op_counts.get("output", 0))
            rows[key] = row
        return rows


def as_int(row: dict, key: str) -> int:
    value = row.get(key, "")
    return int(value) if value != "" else -1


def build_alignment(paper_rows: dict, normalized_rows: dict) -> list:
    rows = []
    for key in sorted(paper_rows, key=lambda k: as_int(paper_rows[k], "index")):
        paper = paper_rows[key]
        normalized = normalized_rows.get(key, {})
        status = normalized.get("status", "missing")
        row = {
            "benchmark": key,
            "status": status,
            "paper_nodes": paper["nodes"],
            "normalized_nodes": normalized.get("nodes", ""),
            "nodes_match": str(paper["nodes"] == normalized.get("nodes", "")).lower(),
            "paper_edges": paper["edges"],
            "normalized_edges": normalized.get("edges", ""),
            "edges_match": str(paper["edges"] == normalized.get("edges", "")).lower(),
            "paper_inputs": paper["inputs"],
            "normalized_loads": normalized.get("normalized_loads", ""),
            "input_count_match": str(paper["inputs"] == normalized.get("normalized_loads", "")).lower(),
            "paper_outputs": paper["outputs"],
            "normalized_outputs": normalized.get("normalized_outputs", ""),
            "output_count_match": str(paper["outputs"] == normalized.get("normalized_outputs", "")).lower(),
        }
        rows.append(row)
    return rows


def write_csv(path: Path, rows: list) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys()) if rows else []
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_markdown(path: Path, rows: list) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    ready = sum(1 for row in rows if row["status"] == "ready")
    node_matches = sum(1 for row in rows if row["nodes_match"] == "true")
    edge_matches = sum(1 for row in rows if row["edges_match"] == "true")
    input_matches = sum(1 for row in rows if row["input_count_match"] == "true")
    output_matches = sum(1 for row in rows if row["output_count_match"] == "true")

    lines = [
        "# YOTT Cases 2021 Alignment",
        "",
        f"Ready DOTs: {ready}/{len(rows)}",
        f"Node counts matching Table 3: {node_matches}/{len(rows)}",
        f"Edge counts matching Table 3: {edge_matches}/{len(rows)}",
        f"Input counts matching mapper `load` nodes: {input_matches}/{len(rows)}",
        f"Output counts matching mapper `output` nodes: {output_matches}/{len(rows)}",
        "",
        "The node and edge columns compare the normalized mapper DOTs against YOTT Table 3. The input/output columns compare Table 3 IN/OUT against the mapper's normalized `load`/`output` opcode counts; they are reported separately because the public DOT files do not encode paper-level I/O ports in a uniform way.",
        "",
        "| benchmark | ready | nodes | edges | paper IN | mapper load | paper OUT | mapper output |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in rows:
        nodes = f"{row['normalized_nodes']}/{row['paper_nodes']}"
        edges = f"{row['normalized_edges']}/{row['paper_edges']}"
        lines.append(
            f"| `{row['benchmark']}` | {row['status']} | {nodes} | {edges} | "
            f"{row['paper_inputs']} | {row['normalized_loads']} | "
            f"{row['paper_outputs']} | {row['normalized_outputs']} |"
        )
    path.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare normalized YOTT Cases 2021 DOTs with YOTT Table 3.")
    parser.add_argument("--paper-table3", type=Path, default=Path("research/data/papers/yott_table3_benchmarks.csv"))
    parser.add_argument("--normalization-csv", type=Path, default=Path("benchmark/literature/yott_cases2021_normalized/normalization.csv"))
    parser.add_argument("--csv-out", type=Path, default=Path("benchmark/literature/yott_cases2021_normalized/table3_alignment.csv"))
    parser.add_argument("--report-out", type=Path, default=Path("benchmark/literature/yott_cases2021_normalized/table3_alignment.md"))
    args = parser.parse_args()

    paper_rows = read_paper_rows(args.paper_table3)
    normalized_rows = read_normalization_rows(args.normalization_csv)
    rows = build_alignment(paper_rows, normalized_rows)
    write_csv(args.csv_out, rows)
    write_markdown(args.report_out, rows)

    if any(row["status"] != "ready" for row in rows):
        raise SystemExit(1)
    if any(row["nodes_match"] != "true" or row["edges_match"] != "true" for row in rows):
        raise SystemExit(2)


if __name__ == "__main__":
    main()
