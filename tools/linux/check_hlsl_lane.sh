#!/bin/bash
# Run the vkd3d-shader smoke over lane HLSL's res/shaders/*.hlsl, without
# merging that branch (lane LINUX deliverable 5, PLAN.md P5 step 3: "fetch that
# branch read-only").
#
# It extracts the .hlsl files from origin/render/lane-hlsl with git show, so
# nothing is checked out and nothing is written into the worktree's tree, then
# runs build-linux/hlsl_smoke on them.
#
#   tools/linux/check_hlsl_lane.sh [branch]
set -euo pipefail

cd "$(dirname "$0")/../.."

branch="${1:-origin/render/lane-hlsl}"
out="build-linux/hlsl_lane"
smoke="build-linux/hlsl_smoke"

[ -x "$smoke" ] || { echo "$smoke is missing; run ./build_linux.sh first"; exit 1; }

git fetch -q origin "${branch#origin/}" 2>/dev/null || true
git rev-parse --verify -q "$branch" >/dev/null || { echo "no $branch"; exit 77; }

# The extracted shaders are build output, so they live under build-linux/ (which
# .gitignore already covers) and are rewritten on every run.
mkdir -p "$out"
for f in $(git ls-tree -r --name-only "$branch" -- res/shaders | grep '\.hlsl$'); do
	git show "$branch:$f" > "$out/$(basename "$f")"
done

count=$(ls "$out"/*.hlsl 2>/dev/null | wc -l)
if [ "$count" -eq 0 ]; then echo "$branch has no res/shaders/*.hlsl yet (skipping)"; exit 77; fi
echo "$branch: $count .hlsl file(s)"
exec "$smoke" "$out"

