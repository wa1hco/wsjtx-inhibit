import importlib.util
import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SCRIPT = ROOT / ".github" / "scripts" / "inhibit-release-identity.py"
SPEC = importlib.util.spec_from_file_location("inhibit_release_identity", SCRIPT)
identity = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(identity)


class InhibitReleaseIdentityTest(unittest.TestCase):
    def test_suffix_is_the_cmake_marker(self):
        self.assertEqual(identity.read_suffix(), "-inhibit")

    def test_current_tree_follows_release_state(self):
        info = identity.load_identity(ROOT)
        state = identity._release_policy().read_state(ROOT)
        self.assertEqual(info["label"], f"{info['upstream_version']}-inhibit")
        self.assertEqual(info["tag"], f"v{info['label']}")
        self.assertEqual(info["upstream_tag"], f"v{info['upstream_version']}")
        if state["channel"] == "BETA":
            self.assertEqual(
                info["label"],
                f"{state['version']}-beta{state['prerelease']}-inhibit",
            )
        self.assertTrue(info["label"].endswith("-inhibit"))

    def test_channels(self):
        suffix = "-inhibit"
        cases = [
            ({"version": "3.3.0", "channel": "BETA", "prerelease": "6"}, "3.3.0-beta6-inhibit"),
            ({"version": "3.3.0", "channel": "BETA", "prerelease": "10"}, "3.3.0-beta10-inhibit"),
            ({"version": "3.4.0", "channel": "RC", "prerelease": "2"}, "3.4.0-rc2-inhibit"),
            ({"version": "3.4.0", "channel": "GA", "prerelease": ""}, "3.4.0-inhibit"),
            ({"version": "3.4.0", "channel": "DEVEL", "prerelease": ""}, "3.4.0-devel-inhibit"),
        ]
        for state, label in cases:
            info = identity.identity_from_state(state, suffix)
            self.assertEqual(info["label"], label)
            self.assertEqual(info["tag"], f"v{label}")

    def test_duplicate_suffix_is_rejected(self):
        text = '\n'.join([
            'set (WSJT_INHIBIT_VERSION_SUFFIX "-inhibit")',
            'set (WSJT_INHIBIT_VERSION_SUFFIX "-other")',
        ])
        with self.assertRaises(ValueError):
            identity.read_suffix(text)

    def test_order_puts_a_newer_beta_above_the_previous_one(self):
        suffix = "-inhibit"
        self.assertLess(
            identity.version_key("3.3.0-beta6-inhibit", suffix),
            identity.version_key("3.3.0-beta10-inhibit", suffix),
        )
        self.assertLess(
            identity.version_key("3.3.0-beta10-inhibit", suffix),
            identity.version_key("3.3.0-rc1-inhibit", suffix),
        )
        self.assertLess(
            identity.version_key("3.3.0-rc1-inhibit", suffix),
            identity.version_key("3.3.0-inhibit", suffix),
        )

    def test_newest_label_is_latest_and_older_inhibit_tags_are_demoted(self):
        plan = identity.plan_release(
            "3.3.0-beta7-inhibit",
            ["v3.3.0-beta6-inhibit", "build/v3.0.2-rc5", "wsjtx-inhibit-rc1"],
            "-inhibit",
        )
        self.assertTrue(plan["make_latest"])
        self.assertFalse(plan["prerelease"])
        self.assertEqual(plan["demote"], ["v3.3.0-beta6-inhibit"])

    def test_republishing_an_older_label_does_not_steal_latest(self):
        plan = identity.plan_release(
            "3.3.0-beta6-inhibit",
            ["v3.3.0-beta7-inhibit"],
            "-inhibit",
        )
        self.assertFalse(plan["make_latest"])
        self.assertTrue(plan["prerelease"])
        self.assertEqual(plan["demote"], [])

    def test_notes_name_the_upstream_tag_the_delta_and_the_commit(self):
        info = identity.identity_from_state(
            {"version": "3.3.0", "channel": "BETA", "prerelease": "6"},
            "-inhibit",
        )
        sha = "5e7751ee8b55d12e15325477756ae97dded5fb4d"
        notes = identity.release_notes(info, sha)
        self.assertIn("v3.3.0-beta6", notes)
        self.assertIn("sole-writer", notes)
        self.assertIn(sha, notes)
        self.assertIn("wsjtx-3.3.0-beta6-inhibit-win64.exe", notes)
        self.assertIn("wsjtx-3.3.0-beta6-inhibit-linux-x86_64.deb", notes)
        self.assertIn("WSJT-X", notes)

    def test_cli_github_output(self):
        completed = subprocess.run(
            ["python3", str(SCRIPT), "--github-output", "--root", str(ROOT)],
            check=True,
            capture_output=True,
            text=True,
        )
        fields = dict(line.split("=", 1) for line in completed.stdout.splitlines())
        info = identity.load_identity(ROOT)
        self.assertEqual(fields["label"], info["label"])
        self.assertEqual(fields["tag"], info["tag"])
        self.assertEqual(fields["upstream_tag"], info["upstream_tag"])

    def test_cli_plan(self):
        completed = subprocess.run(
            ["python3", str(SCRIPT), "--plan-release", "--root", str(ROOT)],
            check=True,
            capture_output=True,
            text=True,
            input=json.dumps(["build/v3.0.2-rc5"]),
        )
        plan = json.loads(completed.stdout)
        info = identity.load_identity(ROOT)
        self.assertTrue(plan["make_latest"])
        self.assertEqual(plan["tag"], info["tag"])
        self.assertEqual(plan["demote"], [])


if __name__ == "__main__":
    unittest.main()
