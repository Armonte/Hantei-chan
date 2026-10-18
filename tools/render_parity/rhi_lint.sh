#!/bin/bash
# rhi_lint.sh <repo-root>: no GL call, GL enum or glad include may appear outside
# src/rhi/gl (PLAN.md 5.1, promoted to the P1 gate and the P6 definition of done).
# Phase H has no RHI yet, so today it checks the harness directory itself is GL
# free and that the corpus never mentions a gl* symbol; once P1 lands the same
# command covers src/ and turns this into the phase gate.
set -u
ROOT="${1:?usage: rhi_lint.sh <repo-root>}"
cd "$ROOT" || exit 2

hits="$(grep -rnE '\bgl[A-Z][A-Za-z0-9]*\(|\bGL_[A-Z0-9_]+|<glad/glad\.h>' tools/render_parity 2>/dev/null)"
if [ -n "$hits" ]; then
	echo "rhi_lint: GL symbols in the parity harness:" >&2
	echo "$hits" >&2
	exit 1
fi
echo "rhi_lint: ok (harness is GL free)"
