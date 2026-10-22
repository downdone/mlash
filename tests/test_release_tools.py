import hashlib
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"scripts"))
from qualification_summary import sha256, summarize
from release_gate import REQUIRED, evaluate
from package_payload import load_plan, stage_payload, verify_payload
from ci_release_check import version_tag


class PortablePayloadTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.stage = Path(self.temp.name)/"portable"
        self.stage.mkdir()
        self.repo = Path(__file__).resolve().parents[1]
        self.plan_path = self.repo/"scripts/package-files.json"
        self.plan = load_plan(self.plan_path)
        # Deliberately fake only deployment bytes; copy real license/user assets.
        for name in self.plan["deployed_files"]:
            target = self.stage/name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"isolated deployed dependency")

    def tearDown(self):
        self.temp.cleanup()

    def manifest(self):
        records = [{"path": p.relative_to(self.stage).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
                   for p in self.stage.rglob("*") if p.is_file() and p.name != "package-manifest.json"]
        (self.stage/"package-manifest.json").write_text(json.dumps({"files": records}))

    def test_curated_payload_keeps_runtime_licenses_and_sanitizes_provenance(self):
        stage_payload(self.repo, self.stage, self.plan)
        self.manifest()
        self.assertGreater(verify_payload(self.stage, self.plan), 20)
        self.assertFalse((self.stage/"docs").exists())
        self.assertFalse((self.stage/"scripts").exists())
        self.assertFalse((self.stage/"dependencies.lock.json").exists())
        self.assertTrue((self.stage/"licenses/LGPL-3.0.txt").is_file())
        self.assertTrue((self.stage/"licenses/twemoji-NOTICE.md").is_file())
        for license_file in (self.repo/"licenses").glob("*.txt"):
            self.assertEqual(license_file.read_bytes(), (self.stage/"licenses"/license_file.name).read_bytes())
        provenance = json.loads((self.stage/"cores/manifest.json").read_text(encoding="utf-8"))
        self.assertNotIn("source", provenance)
        self.assertIn("source_commit", provenance)
        for name in ("README.txt", "licenses/README.md", "cores/manifest.json"):
            text = (self.stage/name).read_text(encoding="utf-8")
            self.assertNotIn("@CORE_VERSION@", text)
            self.assertNotIn("E:/Code", text)
            self.assertNotRegex(text, r"(?<![A-Za-z])[A-Za-z]:[/\\]")

    def test_unknown_deployed_dependency_and_missing_tls_are_rejected(self):
        extra = self.stage/"unexpected.dll"
        extra.write_bytes(b"unreviewed dependency")
        with self.assertRaisesRegex(ValueError, "extra"):
            stage_payload(self.repo, self.stage, self.plan)
        extra.unlink()
        (self.stage/"tls/qschannelbackend.dll").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            stage_payload(self.repo, self.stage, self.plan)

    def test_injected_user_data_tampering_and_missing_license_are_rejected(self):
        stage_payload(self.repo, self.stage, self.plan)
        self.manifest()
        extra = self.stage/"subscription.yaml"
        extra.write_bytes(b"must never ship user data")
        with self.assertRaisesRegex(ValueError, "extra"):
            verify_payload(self.stage, self.plan)
        extra.unlink()
        (self.stage/"README.txt").write_bytes(b"changed after manifest")
        with self.assertRaisesRegex(ValueError, "integrity"):
            verify_payload(self.stage, self.plan)
        self.manifest()
        (self.stage/"licenses/yaml-cpp-MIT.txt").unlink()
        with self.assertRaisesRegex(ValueError, "missing"):
            verify_payload(self.stage, self.plan)

    def test_plan_rejects_unsafe_paths_and_case_collisions(self):
        for target in ("../private", "C:/private", "licenses/../private", "\\private"):
            changed = json.loads(self.plan_path.read_text(encoding="utf-8"))
            changed["assets"][0]["target"] = target
            path = Path(self.temp.name)/"bad-plan.json"
            path.write_text(json.dumps(changed))
            with self.assertRaises(ValueError):
                load_plan(path)
        changed["assets"][0]["target"] = "mlash.EXE"
        path.write_text(json.dumps(changed))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            load_plan(path)

    def test_changed_manifest_cannot_hide_missing_payload(self):
        stage_payload(self.repo, self.stage, self.plan)
        (self.stage/"tls/qschannelbackend.dll").unlink()
        self.manifest()
        with self.assertRaisesRegex(ValueError, "missing"):
            verify_payload(self.stage, self.plan)


class ReleaseToolsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.app = self.root/"Mlash.exe"; self.app.write_bytes(b"isolated-test-binary")
        self.core = self.root/"mihomo.exe"; self.core.write_bytes(b"isolated-test-core")
        self.evidence = {"schema": 1, "executable_sha256": sha256(self.app), "core_sha256": sha256(self.core), "checks": {}}
        self.path = self.root/"evidence.json"

    def tearDown(self):
        self.temp.cleanup()

    def gate(self):
        self.path.write_text(json.dumps(self.evidence))
        return evaluate(self.path,self.app,self.core)

    def report(self,seconds=30,completed=True,gap=15):
        path=self.root/"qa.json"; stream=self.root/"qa.samples.jsonl"
        times=list(range(0,seconds+1,gap))
        if times[-1]!=seconds: times.append(seconds)
        rows=[{"elapsed_seconds":t,"client":{"pid":1,"private_bytes":30*1024**2,"working_set":40*1024**2,"cpu_seconds":t*.001,"handles":40,"threads":4},
               "core":{"pid":2,"private_bytes":70*1024**2,"working_set":80*1024**2,"cpu_seconds":t*.002,"handles":50,"threads":7}} for t in times]
        stream.write_text("".join(json.dumps(row)+"\n" for row in rows))
        report={"status":"completed" if completed else "running","failure":"","samples_file":stream.name,"samples_sha256":sha256(stream),"sample_count":len(rows),"elapsed_seconds":seconds,
                "environment":{"logical_processors":8},"scope":"lightweight tray idle","executable_sha256":sha256(self.app),"core_sha256":sha256(self.core),"fixture_sha256":"fixture",
                "gui_released":True,"core_ready_ms":200,"menu_preparation_p95_ms":1,"node_sort_p95_ms":1,"system_writes_blocked":True}
        path.write_text(json.dumps(report)); return path,stream

    def test_pinned_release_core_manifest_and_service_agree(self):
        repo=Path(__file__).resolve().parents[1]
        locked=json.loads((repo/"dependencies.lock.json").read_text(encoding="utf-8"))["mihomo-release"]
        manifest=json.loads((repo/"cores/manifest.json").read_text(encoding="utf-8-sig"))
        pin=re.search(r'TunCoreSha256 = "([a-f0-9]{64})"', (repo/"src/core/TunPolicy.h").read_text(encoding="utf-8")).group(1)
        self.assertEqual(locked["executable_sha256"],pin)
        self.assertEqual(manifest["executable_sha256"],pin)
        self.assertEqual(manifest["version"],locked["version"])
        self.assertEqual(manifest["origin"],locked["origin"])
        self.assertEqual(manifest["source_commit"],locked["source_commit"])
        self.assertIn(locked["version"],manifest["reported_version"][0])
        self.assertEqual(sha256(repo/"cores/mihomo.exe"),pin)

    def test_ci_tag_is_exact_version_not_arbitrary_input(self):
        source="project(Mlash VERSION 1.17.0 LANGUAGES C CXX)"
        self.assertEqual(version_tag(source,"v1.17.0"),"1.17.0")
        for bad in ("v1.16.0", "v1.17.0;echo injected", "../v1.17.0", "v1.17.0-rc1"):
            with self.assertRaises(ValueError): version_tag(source,bad)
        with self.assertRaises(ValueError): version_tag("project(invalid)")

    def test_missing_checks_never_qualify(self):
        result=self.gate(); self.assertFalse(result["release_qualified"]); self.assertEqual(len(result["blockers"]),len(REQUIRED))

    def test_changed_binary_invalidates_all_evidence(self):
        self.app.write_bytes(b"different-binary"); self.assertEqual(self.gate()["checks"],{})

    def test_pass_labels_without_artifacts_are_rejected(self):
        self.evidence["checks"]={key:{"status":"pass","artifacts":[]} for key in REQUIRED}; self.assertFalse(self.gate()["release_qualified"])

    def test_short_and_running_runs_do_not_become_72h(self):
        path,_=self.report(); summary=summarize(path); self.assertFalse(summary["ten_minute_idle_evidence"]); self.assertFalse(summary["stability_72h_evidence"])
        path,_=self.report(600,False); self.assertFalse(summarize(path)["ten_minute_idle_evidence"])

    def test_ten_minute_scope_and_cpu_denominator(self):
        path,_=self.report(600); summary=summarize(path); self.assertTrue(summary["ten_minute_idle_evidence"]); self.assertAlmostEqual(summary["client"]["cpu_average_percent"],.0125); self.assertFalse(summary["stability_72h_evidence"])

    def test_one_hour_observation_is_separate_from_72h(self):
        path,_=self.report(3599); self.assertFalse(summarize(path)["one_hour_observation_evidence"])
        path,_=self.report(3600); summary=summarize(path)
        self.assertTrue(summary["one_hour_observation_evidence"])
        self.assertFalse(summary["stability_72h_evidence"])
        path,_=self.report(3600,False); self.assertFalse(summarize(path)["one_hour_observation_evidence"])

    def test_sleep_gap_and_tampering_are_rejected(self):
        path,_=self.report(72*3600,gap=3600); self.assertFalse(summarize(path)["stability_72h_evidence"])
        path,stream=self.report(600); stream.write_text(stream.read_text().replace('"pid": 1','"pid": 3')); self.assertRaises(ValueError,summarize,path)

    def test_stability_requires_complete_stream(self):
        path,_=self.report(72*3600); self.assertTrue(summarize(path)["stability_72h_evidence"])
        report=json.loads(path.read_text()); report["sample_count"]+=1; path.write_text(json.dumps(report)); self.assertRaises(ValueError,summarize,path)

    def test_stream_path_traversal_is_rejected(self):
        path,_=self.report(); report=json.loads(path.read_text()); report["samples_file"]="../private.json"; path.write_text(json.dumps(report)); self.assertRaises(ValueError,summarize,path)

    def test_corrupt_xml_not_accepted(self):
        xml=self.root/"tests.xml"; xml.write_text('<TestCase><Incident type="pass"/><Incident type="fail"/><Incident type="pass"/></TestCase>')
        self.evidence["checks"]["automated"]={"status":"pass","artifacts":[{"path":str(xml),"sha256":sha256(xml)}]}; self.assertFalse(self.gate()["checks"]["automated"])


if __name__=="__main__": unittest.main()
