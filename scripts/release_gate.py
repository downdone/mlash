"""Fail closed: candidate artifacts alone never qualify a release."""
import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path
from qualification_summary import sha256, summarize

REQUIRED = ("automated", "native_ui", "package", "win10_clean", "win11_clean", "system_proxy", "autostart_login",
            "explorer_restart", "sleep_wake", "stability72h", "performance", "comparison", "licenses")


def xml_pass(path):
    root = ET.parse(path).getroot()
    incidents = root.findall(".//Incident")
    return len(incidents) >= 3 and all(i.get("type") == "pass" for i in incidents)


def evaluate(evidence_path, app_path, core_path):
    path = Path(evidence_path)
    evidence = json.loads(path.read_text(encoding="utf-8-sig"))
    app_hash, core_hash = sha256(app_path), sha256(core_path)
    blocks = []
    if evidence.get("schema") != 1 or evidence.get("executable_sha256") != app_hash or evidence.get("core_sha256") != core_hash:
        return {"release_qualified": False, "blockers": ["证据未绑定当前 EXE/内核"], "checks": {}}
    results = {}
    for key in REQUIRED:
        check = evidence.get("checks", {}).get(key, {})
        if check.get("status") != "pass":
            blocks.append(f"{key}: {check.get('status', 'not_run')}"); results[key] = False; continue
        try:
            files = check.get("artifacts", [])
            if not files:
                raise ValueError("缺少证据文件")
            artifacts = []
            for file in files:
                artifact = Path(file["path"])
                if not artifact.is_absolute():
                    artifact = path.parent / artifact
                if sha256(artifact) != file["sha256"]:
                    raise ValueError("证据文件已变化")
                artifacts.append(artifact)
            if key in ("automated", "native_ui"):
                xmls = [a for a in artifacts if a.suffix == ".xml"]
                if not xmls or not all(xml_pass(a) for a in xmls):
                    raise ValueError("测试 XML 包含失败/跳过或缺少实际案例")
            elif key == "stability72h":
                summary = summarize(artifacts[0])
                if not summary["stability_72h_evidence"] or summary["executable_sha256"] != app_hash or summary["core_sha256"] != core_hash:
                    raise ValueError("不足真实连续 72h，或二进制不一致/采样中断/持续增长")
            elif key == "performance":
                summaries = [summarize(a) for a in artifacts]
                if {s["scope"] for s in summaries} != {"GUI idle", "lightweight tray idle"}:
                    raise ValueError("须分别测量 GUI 与托盘")
                for s in summaries:
                    if not s["ten_minute_idle_evidence"] or s["executable_sha256"] != app_hash or s["core_sha256"] != core_hash:
                        raise ValueError("须为同版完整十分钟实际测量")
                    if s["client"]["private_mib_p95"] > 55 or (s["scope"] == "lightweight tray idle" and s["client"]["cpu_average_percent"] > .5) or s["core_ready_ms"] > 5000:
                        raise ValueError("超过当前内存/CPU/内核就绪预算")
            elif key == "package":
                data = json.loads(artifacts[0].read_text(encoding="utf-8-sig"))
                if data.get("executable_sha256") != app_hash or data.get("core_sha256") != core_hash or not data.get("files"):
                    raise ValueError("包清单未绑定当前二进制")
                for file in data["files"]:
                    relative = file["path"].replace("\\", "/")
                    if Path(relative).is_absolute() or ".." in Path(relative).parts or ":" in relative:
                        raise ValueError("包清单含不安全路径")
                    if sha256(artifacts[0].parent/relative) != file["sha256"]:
                        raise ValueError("包中文件不匹配清单")
                if len(artifacts)<2 or not json.loads(artifacts[1].read_text(encoding="utf-8-sig")).get("tls_available"):
                    raise ValueError("缺少当前包 TLS 实际运行证据")
                if evidence.get("runtime_fingerprint") != data.get("runtime_fingerprint"):
                    raise ValueError("运行依赖未绑定当前包")
            else:
                data = json.loads(artifacts[0].read_text(encoding="utf-8-sig"))
                if data.get("executable_sha256") != app_hash or data.get("core_sha256") != core_hash or data.get("result") != "pass":
                    raise ValueError("验收结果未绑定当前包")
                if key.startswith("win") or key == "system_proxy":
                    if not data.get("controlled_vm"):
                        raise ValueError("要求受控 VM 实测")
                if key in ("win10_clean", "win11_clean"):
                    if not data.get("clean_machine") or data.get("windows_major") != (10 if key == "win10_clean" else 11):
                        raise ValueError("系统版本或干净部署证据不符")
                if key == "licenses" and (not data.get("owner_confirmed") or not data.get("mlash_license") or not data.get("qt_distribution_basis") or not data.get("corresponding_sources")):
                    raise ValueError("源码/Qt 分发依据/维护者确认不完整")
                if key == "comparison" and (not data.get("same_core_config_os") or not data.get("cold_start_measured") or data.get("tray_interaction_p95_ms", 1e9) > 100 or data.get("cold_tray_ready_p95_ms", 1e9) > 1500):
                    raise ValueError("同条件对照、实际冷启动/托盘交互证据不完整或超预算")
            results[key] = True
        except (OSError, ValueError, KeyError, TypeError, ET.ParseError) as error:
            blocks.append(f"{key}: {error}"); results[key] = False
    return {"release_qualified": not blocks, "executable_sha256": app_hash, "core_sha256": core_hash, "blockers": blocks, "checks": results}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--core", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        result = evaluate(args.evidence, args.app, args.core)
    except (OSError, ValueError) as error:
        result = {"release_qualified": False, "blockers": [str(error)]}
    output = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.write_text(output+"\n", encoding="utf-8")
    print(output)
    raise SystemExit(0 if result["release_qualified"] else 2)
