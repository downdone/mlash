"""Numeric QA evidence reader. Short, interrupted, changed-binary runs never become 72h proof."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def percentile95(values):
    values = sorted(values)
    return values[max(0, math.ceil(len(values) * .95) - 1)]


def summarize(path):
    path = Path(path)
    report = json.loads(path.read_text(encoding="utf-8-sig"))
    name = report.get("samples_file", "")
    if not name or Path(name).name != name or "/" in name or "\\" in name:
        raise ValueError("invalid sample stream name")
    stream = path.parent / name
    if stream.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("sample stream too large")
    rows = [json.loads(line) for line in stream.read_text(encoding="utf-8").splitlines()]
    if len(rows) < 2 or len(rows) != report.get("sample_count"):
        raise ValueError("incomplete sample stream")
    completed = report.get("status") == "completed" and not report.get("failure")
    if completed and sha256(stream) != report.get("samples_sha256"):
        raise ValueError("sample stream checksum mismatch")
    times = [row["elapsed_seconds"] for row in rows]
    if any(not isinstance(x, (int, float)) or not math.isfinite(x) or x < 0 for x in times):
        raise ValueError("invalid time samples")
    gaps = [b - a for a, b in zip(times, times[1:])]
    if any(gap <= 0 for gap in gaps):
        raise ValueError("non-monotonic samples")
    logical = report["environment"]["logical_processors"]
    if type(logical) is not int or logical < 1:
        raise ValueError("invalid CPU denominator")
    result = {"status": report["status"], "elapsed_seconds": times[-1] - times[0], "sample_count": len(rows),
              "max_sample_gap_seconds": max(gaps), "scope": report["scope"], "executable_sha256": report["executable_sha256"],
              "core_sha256": report["core_sha256"], "fixture_sha256": report["fixture_sha256"],
              "gui_released": report["gui_released"], "core_ready_ms": report["core_ready_ms"],
              "menu_preparation_p95_ms": report["menu_preparation_p95_ms"], "node_sort_p95_ms": report["node_sort_p95_ms"]}
    for process in ("client", "core"):
        metrics = [row[process] for row in rows]
        for metric in metrics:
            if any(not isinstance(metric.get(key), (int, float)) or not math.isfinite(metric[key]) or metric[key] < 0
                   for key in ("private_bytes", "working_set", "cpu_seconds", "handles", "threads", "pid")):
                raise ValueError("invalid process metrics")
        if len({metric["pid"] for metric in metrics}) != 1:
            raise ValueError("process restarted")
        cpu = [metric["cpu_seconds"] for metric in metrics]
        if any(b < a for a, b in zip(cpu, cpu[1:])):
            raise ValueError("CPU time moved backwards")
        duration = times[-1] - times[0]
        private = [metric["private_bytes"] / 1024**2 for metric in metrics]
        mean_t, mean_m = sum(times)/len(times), sum(private)/len(private)
        denominator = sum((t-mean_t)**2 for t in times)
        slope = sum((t-mean_t)*(m-mean_m) for t, m in zip(times, private))/denominator*3600 if denominator else 0
        quarter = max(1, len(private)//4)
        growth = sum(private[-quarter:])/quarter - sum(private[:quarter])/quarter
        result[process] = {"private_mib_p95": percentile95(private),
                           "working_set_mib_p95": percentile95([metric["working_set"]/1024**2 for metric in metrics]),
                           "cpu_average_percent": (cpu[-1]-cpu[0])/duration/logical*100,
                           "handles_max": max(metric["handles"] for metric in metrics), "threads_max": max(metric["threads"] for metric in metrics),
                           "private_trend_mib_per_hour": slope, "last_vs_first_quarter_mib": growth}
    stable_window = (max(gaps) <= 45 and times[0] <= 1 and abs(times[-1]-report["elapsed_seconds"]) <= 5)
    result["ten_minute_idle_evidence"] = bool(completed and stable_window and duration >= 600)
    # An observed hour is not a 72-hour qualification or a memory-budget pass.
    result["one_hour_observation_evidence"] = bool(completed and stable_window and duration >= 3600 and report["system_writes_blocked"])
    result["stability_72h_evidence"] = bool(completed and stable_window and duration >= 72*3600 and report["system_writes_blocked"]
        and result["client"]["private_trend_mib_per_hour"] <= 1 and result["client"]["last_vs_first_quarter_mib"] <= 8
        and result["core"]["private_trend_mib_per_hour"] <= 1 and result["core"]["last_vs_first_quarter_mib"] <= 8)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = summarize(args.report)
    output = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(output+"\n", encoding="utf-8")
    print(output)
