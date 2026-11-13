#!/usr/bin/env python3
"""Release label for the inhibit fork.

The beta/rc number comes from release-state.txt. The marker comes from
WSJT_INHIBIT_VERSION_SUFFIX in CMake/Modules/inhibit-identity.cmake.
Both the program version and the GitHub release tag use the result.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
IDENTITY_CMAKE = ROOT / "CMake" / "Modules" / "inhibit-identity.cmake"
SUFFIX_RE = re.compile(
    r'^set\s*\(\s*WSJT_INHIBIT_VERSION_SUFFIX\s+"(-[A-Za-z0-9._+-]+)"\s*\)\s*$'
)
LABEL_RE = re.compile(
    r"^(?P<numeric>\d+\.\d+\.\d+)(?:-(?P<kind>devel|beta(?P<beta>[1-9]\d*)|rc(?P<rc>[1-9]\d*)))?(?P<suffix>-[A-Za-z0-9._+-]+)$"
)


def _release_policy():
    script = ROOT / ".github" / "scripts" / "release-policy.py"
    spec = importlib.util.spec_from_file_location("release_policy", script)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load {script}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_suffix(cmake_text: str | None = None, root: Path | None = None) -> str:
    if cmake_text is None:
        text = ((root or ROOT) / "CMake" / "Modules" / "inhibit-identity.cmake").read_text(encoding="utf-8")
    else:
        text = cmake_text
    found = [
        match.group(1)
        for line in text.splitlines()
        if (match := SUFFIX_RE.match(line.strip()))
    ]
    if len(found) != 1:
        raise ValueError(
            "CMake/Modules/inhibit-identity.cmake must set "
            "WSJT_INHIBIT_VERSION_SUFFIX exactly once"
        )
    return found[0]


def upstream_version(state: dict[str, str]) -> str:
    version = state["version"]
    channel = state["channel"]
    number = state["prerelease"]
    if channel == "BETA":
        return f"{version}-beta{number}"
    if channel == "RC":
        return f"{version}-rc{number}"
    if channel == "DEVEL":
        return f"{version}-devel"
    if channel == "GA":
        return version
    raise ValueError(f"Unsupported release channel: {channel}")


def identity_from_state(state: dict[str, str], suffix: str | None = None) -> dict[str, str]:
    marker = read_suffix() if suffix is None else suffix
    base = upstream_version(state)
    label = f"{base}{marker}"
    if not LABEL_RE.fullmatch(label):
        raise ValueError(f"Inhibit release label is not safe to publish: {label}")
    return {
        "label": label,
        "tag": f"v{label}",
        "upstream_version": base,
        "upstream_tag": f"v{base}",
        "suffix": marker,
        "numeric": state["version"],
        "channel": state["channel"],
        "prerelease": state["prerelease"],
    }


def load_identity(root: Path = ROOT) -> dict[str, str]:
    state = _release_policy().read_state(root)
    return identity_from_state(state, read_suffix(root=root))


def version_key(label: str, suffix: str) -> tuple[int, ...]:
    if not label.endswith(suffix):
        raise ValueError(f"Not an inhibit label: {label}")
    base = label[: -len(suffix)]
    match = re.fullmatch(
        r"(?P<numeric>\d+\.\d+\.\d+)(?:-(?P<kind>devel|beta(?P<beta>[1-9]\d*)|rc(?P<rc>[1-9]\d*)))?",
        base,
    )
    if match is None:
        raise ValueError(f"Cannot order inhibit label: {label}")
    # devel < beta < rc < ga, matching release-policy.CHANNEL_RANK plus devel first.
    kind = match.group("kind")
    if kind == "devel":
        rank, number = -1, 0
    elif kind and kind.startswith("beta"):
        rank, number = 0, int(match.group("beta"))
    elif kind and kind.startswith("rc"):
        rank, number = 1, int(match.group("rc"))
    else:
        rank, number = 2, 0
    return tuple(int(part) for part in match.group("numeric").split(".")) + (rank, number)


def plan_release(label: str, existing_tags: list[str], suffix: str | None = None) -> dict[str, object]:
    marker = read_suffix() if suffix is None else suffix
    tag = label if label.startswith("v") else f"v{label}"
    plain = tag[1:]
    current_key = version_key(plain, marker)
    demote: list[str] = []
    highest = current_key
    for existing in existing_tags:
        if not existing.startswith("v"):
            continue
        candidate = existing[1:]
        if not candidate.endswith(marker):
            continue
        try:
            key = version_key(candidate, marker)
        except ValueError:
            continue
        if existing == tag:
            continue
        if key > highest:
            highest = key
        if key < current_key:
            demote.append(existing)
    make_latest = current_key == highest
    return {
        "tag": tag,
        "make_latest": make_latest,
        "prerelease": not make_latest,
        "demote": sorted(demote),
    }


def release_notes(info: dict[str, str], sha: str) -> str:
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Commit SHA must be 40 lowercase hex characters")
    label = info["label"]
    upstream = info["upstream_tag"]
    return f"""# WSJT-X {label}

Windows and Linux builds of the TX Inhibit fork. The program reports this version in the window title, Help → About, and `wsjtx_app_version -v`.

GitHub lists this as the **Latest** release when it is the newest inhibit build. GitHub only gives that badge to a release that is not marked pre-release, and the older 3.0.2 packages on this repo are pre-releases. This build is still a beta of WSJT-X {info['upstream_version']} plus TX Inhibit, not a general-availability release. Older inhibit builds are kept and marked pre-release so they do not stay current.

## Upstream

Based on official WSJT-X [{upstream}](https://github.com/WSJTX/wsjtx/releases/tag/{upstream}).

## Inhibit delta

WSJT-X {info['upstream_version']} already implements type 17 / type 18 TX Inhibit. This build adds the sole-writer pin path: the UDP dispatch thread is the only Unix writer of RTS/DTR, and type 17 carries `t_rx_ns` and `t_pin_ns`.

The UDP Heartbeat **Id** stays `WSJT-X` (KEY agents match that). The Heartbeat **version** field and the PSK Reporter software string include `{label}`.

## Build

Commit: `{sha}`

## Install

| File | Where |
|---|---|
| `wsjtx-{label}-win64.exe` | Windows 64-bit installer |
| `wsjtx-{label}-linux-x86_64.deb` | Ubuntu 24.04 amd64. `sudo apt install ./wsjtx-{label}-linux-x86_64.deb` then run `wsjtx`. No extra environment variables. |

Independent GPL-3 build. Not an official WSJT-X / ARRL release.
"""


def _parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--github-output", action="store_true",
                        help="Write label, tag, upstream_tag, and upstream_version for GITHUB_OUTPUT")
    parser.add_argument("--notes", action="store_true", help="Write release notes to stdout")
    parser.add_argument("--sha", default="", help="Full commit SHA for --notes")
    parser.add_argument("--plan-release", action="store_true",
                        help="Read existing release tags as JSON on stdin and write a publish plan")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)
    info = load_identity(args.root)
    if args.notes:
        sys.stdout.write(release_notes(info, args.sha))
        return 0
    if args.plan_release:
        existing = json.load(sys.stdin)
        if not isinstance(existing, list) or not all(isinstance(item, str) for item in existing):
            raise ValueError("Expected a JSON list of tag names")
        json.dump(plan_release(info["label"], existing, info["suffix"]), sys.stdout)
        sys.stdout.write("\n")
        return 0
    if args.github_output:
        for key in ("label", "tag", "upstream_tag", "upstream_version"):
            print(f"{key}={info[key]}")
        return 0
    json.dump(info, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"inhibit release identity error: {error}", file=sys.stderr)
        raise SystemExit(1)
