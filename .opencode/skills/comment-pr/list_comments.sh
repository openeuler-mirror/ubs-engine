#!/usr/bin/env bash
# list_comments.sh - Linux/macOS dump of all PR comments (paginated) to a UTF-8 file,
# for agent-level duplicate analysis across all reviewers. Uses python3 stdlib.
set -u

usage() {
    echo "Usage: $0 --token <token> --owner <owner> --repo <repo> --pr <PR> --out-file <file> [--raw]"
    exit 2
}

TOKEN=""; OWNER=""; REPO=""; PR=""; OUT_FILE=""; RAW=""
while [ $# -gt 0 ]; do
    case "$1" in
        --token) TOKEN="${2:-}"; shift 2;;
        --owner) OWNER="${2:-}"; shift 2;;
        --repo) REPO="${2:-}"; shift 2;;
        --pr) PR="${2:-}"; shift 2;;
        --out-file) OUT_FILE="${2:-}"; shift 2;;
        --raw) RAW="1"; shift 1;;
        *) usage;;
    esac
done

if [ -z "$TOKEN" ] || [ -z "$OWNER" ] || [ -z "$REPO" ] || [ -z "$PR" ] || [ -z "$OUT_FILE" ]; then
    usage
fi
command -v python3 >/dev/null 2>&1 || { echo "FAIL: python3 not found" >&2; exit 2; }

export TOKEN OWNER REPO PR OUT_FILE RAW

python3 - <<'PYEOF'
import json, os, sys, urllib.request, urllib.parse, urllib.error

token = os.environ["TOKEN"]
owner = os.environ["OWNER"]
repo = os.environ["REPO"]
pr = os.environ["PR"]
out_file = os.environ["OUT_FILE"]
raw_mode = os.environ.get("RAW", "")

base = "https://api.gitcode.com/api/v5/repos/{}/{}/pulls/{}/comments".format(
    urllib.parse.quote(owner), urllib.parse.quote(repo), urllib.parse.quote(pr))

def get_json(url):
    req = urllib.request.Request(url, method="GET")
    req.add_header("Accept", "application/json")
    req.add_header("Authorization", "Bearer {}".format(token))
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode("utf-8"))

lines = []
all_comments = []
page = 1
try:
    while True:
        comments = get_json(base + "?per_page=100&page={}".format(page))
        if not comments:
            break
        if raw_mode:
            all_comments.extend(comments)
        else:
            for c in comments:
                user = c.get("user") or {}
                dp = c.get("diff_position") or {}
                nl = dp.get("start_new_line") or ""
                lines.append("===== type={} id={} author={} line={} =====".format(
                    c.get("comment_type"), c.get("id"), user.get("login"), nl))
                lines.append((c.get("body") or "").rstrip())
                lines.append("")
        if len(comments) < 100:
            break
        page += 1
    with open(out_file, "w", encoding="utf-8") as f:
        if raw_mode:
            json.dump(all_comments, f, ensure_ascii=False)
        else:
            f.write("\n".join(lines))
    print("OK WROTE", out_file)
    sys.exit(0)
except Exception as e:
    try:
        with open(out_file, "w", encoding="utf-8") as f:
            f.write("list failed: {}".format(e))
    except Exception:
        pass
    print("FAIL:", e)
    sys.exit(1)
PYEOF
exit $?
