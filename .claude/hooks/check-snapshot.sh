#!/bin/sh
# Verifies spec/paper/ against the SHA-256 table in docs/spec-freeze/SPEC_FREEZE.md.
# Usage: check-snapshot.sh session|stop
#   session: prints the result into Claude's context at session start.
#   stop:    blocks Claude from finishing (exit 2) while the snapshot is modified.

mode=${1:-session}
repo=${CLAUDE_PROJECT_DIR:-.}
freeze="$repo/docs/spec-freeze/SPEC_FREEZE.md"

if command -v sha256sum >/dev/null 2>&1; then
  hasher="sha256sum"
else
  hasher="shasum -a 256"
fi

# In stop mode, stdin carries stop_hook_active; avoid an endless block loop.
active=false
if [ "$mode" = "stop" ]; then
  stdin=$(cat)
  case "$stdin" in *'"stop_hook_active":true'*|*'"stop_hook_active": true'*) active=true ;; esac
fi

failures=""
count=0
# Table rows look like: | `spec/paper/main.pdf` | `<64 hex>` |
rows=$(grep -E '^\| `spec/paper/[^`]+` \| `[0-9a-f]{64}` \|' "$freeze" | sed -E 's/^\| `([^`]+)` \| `([0-9a-f]{64})` \|.*/\1 \2/')
while read -r file expected; do
  [ -z "$file" ] && continue
  count=$((count + 1))
  if [ ! -f "$repo/$file" ]; then
    failures="$failures $file(missing)"
    continue
  fi
  actual=$($hasher "$repo/$file" | awk '{print $1}')
  [ "$actual" = "$expected" ] || failures="$failures $file"
done <<EOF
$rows
EOF

if [ "$count" -eq 0 ]; then
  echo "Snapshot check could not read hashes from docs/spec-freeze/SPEC_FREEZE.md." >&2
  [ "$mode" = "stop" ] && [ "$active" = false ] && exit 2
  exit 0
fi

if [ -n "$failures" ]; then
  msg="INTEGRITY FAILURE: spec/paper snapshot differs from SPEC_FREEZE.md hashes:$failures. Restore it (git checkout -- spec/paper) and report the incident; this is release-blocking."
  if [ "$mode" = "stop" ]; then
    echo "$msg" >&2
    [ "$active" = true ] && exit 0
    exit 2
  fi
  echo "$msg"
  exit 0
fi

[ "$mode" = "session" ] && echo "spec/paper snapshot verified: $count files match SPEC_FREEZE.md."
exit 0
