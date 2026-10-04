#!/bin/sh
# PreToolUse guard for Edit/Write/MultiEdit/NotebookEdit.
# Enforces the AGENTS.md immutability rules that a file-writing tool could break:
#   - spec/paper/ is an immutable paper snapshot;
#   - the sibling manuscript repository is read-only from this project;
#   - evidence/raw/ is append-only (new files only; no edits or overwrites).
# Exit 2 blocks the tool call and returns stderr to Claude.

input=$(cat)

json_get() {
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "$input" | jq -r "$1 // empty"
  else
    printf '%s' "$input" | python3 -c "import json,sys
d=json.load(sys.stdin)
for k in '$2'.split('.'):
    d=d.get(k) if isinstance(d,dict) else None
print(d or '')"
  fi
}

tool=$(json_get '.tool_name' 'tool_name')
path=$(json_get '.tool_input.file_path' 'tool_input.file_path')
[ -z "$path" ] && path=$(json_get '.tool_input.notebook_path' 'tool_input.notebook_path')
[ -z "$path" ] && exit 0

repo=$(cd "${CLAUDE_PROJECT_DIR:-.}" && pwd -P)
case "$path" in
  /*) abs=$path ;;
  *) abs="$repo/$path" ;;
esac
dir=$(dirname "$abs")
if [ -d "$dir" ]; then
  abs="$(cd "$dir" && pwd -P)/$(basename "$abs")"
fi
paper_repo=$(cd "$repo/.." && pwd -P)/dsl_research_paper

case "$abs" in
  "$repo"/spec/paper/*)
    echo "BLOCKED: spec/paper/ is the immutable paper snapshot (AGENTS.md). Record clarifications in docs/spec-freeze/SPEC_AMENDMENTS.md instead." >&2
    exit 2 ;;
  "$paper_repo"/*)
    echo "BLOCKED: the manuscript repository is read-only from this project. Put paper-facing outputs in evidence/derived/paper/ and list propagations in its HANDOFF.md." >&2
    exit 2 ;;
  "$repo"/evidence/raw/*)
    if [ "$tool" != "Write" ] || [ -e "$abs" ]; then
      echo "BLOCKED: evidence/raw/ is append-only (AGENTS.md). Create a new record or dataset version; never edit or overwrite an observed record." >&2
      exit 2
    fi ;;
esac
exit 0
