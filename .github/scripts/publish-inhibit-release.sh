#!/usr/bin/env bash
# Create or update the inhibit GitHub release for the current tree.
#
# Re-running for the same version replaces the named assets and retargets the
# tag at the commit that was built. A newer inhibit label becomes GitHub's
# Latest release. Older vX.Y.Z-*-inhibit releases are marked pre-release so
# they stay downloadable but are not the current release. Other releases,
# including the 3.0.2 rc pre-releases, are left unchanged.
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: publish-inhibit-release.sh SHA ASSET_DIR" >&2
  exit 2
fi

sha="$1"
asset_dir="$2"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$script_dir/../.." && pwd)"
repo="${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

if [ -z "${GH_TOKEN:-}${GITHUB_TOKEN:-}" ]; then
  echo "GH_TOKEN or GITHUB_TOKEN is required" >&2
  exit 2
fi
export GH_TOKEN="${GH_TOKEN:-$GITHUB_TOKEN}"

info="$(python3 "$script_dir/inhibit-release-identity.py" --root "$root")"
label="$(python3 -c 'import json,sys; print(json.load(sys.stdin)["label"])' <<<"$info")"
tag="$(python3 -c 'import json,sys; print(json.load(sys.stdin)["tag"])' <<<"$info")"
title="WSJT-X ${label}"
notes="$(mktemp)"
trap 'rm -f "$notes"' EXIT
python3 "$script_dir/inhibit-release-identity.py" --root "$root" --notes --sha "$sha" > "$notes"

windows="$(find "$asset_dir" -type f -name "wsjtx-${label}-win64.exe" -print -quit)"
linux="$(find "$asset_dir" -type f -name "wsjtx-${label}-linux-x86_64.deb" -print -quit)"
if [ -z "$windows" ] || [ -z "$linux" ]; then
  echo "Release assets for ${label} are incomplete in ${asset_dir}" >&2
  find "$asset_dir" -type f -print >&2
  exit 1
fi

existing_json="$(gh release list --repo "$repo" --limit 200 --json tagName --jq '[.[].tagName]')"
plan="$(python3 "$script_dir/inhibit-release-identity.py" --root "$root" --plan-release <<<"$existing_json")"
make_latest="$(python3 -c 'import json,sys; print("true" if json.load(sys.stdin)["make_latest"] else "false")' <<<"$plan")"
prerelease="$(python3 -c 'import json,sys; print("true" if json.load(sys.stdin)["prerelease"] else "false")' <<<"$plan")"

move_tag_if_needed() {
  local current_type current_sha
  current_type="$(gh api "repos/${repo}/git/ref/tags/${tag}" --jq '.object.type')"
  current_sha="$(gh api "repos/${repo}/git/ref/tags/${tag}" --jq '.object.sha')"
  if [ "$current_type" = "tag" ]; then
    current_sha="$(gh api "repos/${repo}/git/tags/${current_sha}" --jq '.object.sha')"
  fi
  if [ "$current_sha" = "$sha" ]; then
    return 0
  fi
  echo "Retargeting ${tag} from ${current_sha} to ${sha}"
  if [ "$current_type" = "commit" ]; then
    gh api --method PATCH "repos/${repo}/git/refs/tags/${tag}" -f sha="$sha" >/dev/null
  else
    gh api --method DELETE "repos/${repo}/git/refs/tags/${tag}" >/dev/null
    gh api --method POST "repos/${repo}/git/refs" -f ref="refs/tags/${tag}" -f sha="$sha" >/dev/null
  fi
}

if gh api "repos/${repo}/git/ref/tags/${tag}" >/dev/null 2>&1; then
  move_tag_if_needed
fi

if gh release view "$tag" --repo "$repo" >/dev/null 2>&1; then
  gh release edit "$tag" --repo "$repo" \
    --title "$title" \
    --notes-file "$notes" \
    --latest="$make_latest" \
    --prerelease="$prerelease"
else
  gh release create "$tag" --repo "$repo" \
    --target "$sha" \
    --title "$title" \
    --notes-file "$notes" \
    --latest="$make_latest" \
    --prerelease="$prerelease"
fi

gh release upload "$tag" --repo "$repo" --clobber "$windows" "$linux"

mapfile -t demote < <(python3 -c 'import json,sys; [print(tag) for tag in json.load(sys.stdin)["demote"]]' <<<"$plan")
for old_tag in "${demote[@]+"${demote[@]}"}"; do
  [ -n "$old_tag" ] || continue
  echo "Marking ${old_tag} as a pre-release so ${tag} stays current"
  gh release edit "$old_tag" --repo "$repo" --prerelease --latest=false
done

echo "Published ${tag} (latest=${make_latest}) with $(basename "$windows") and $(basename "$linux")"
