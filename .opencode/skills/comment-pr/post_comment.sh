#!/usr/bin/env bash
# post_comment.sh - Linux/macOS encoding-safe GitCode PR inline comment submitter.
# Uses python3 (stdlib json + urllib) to escape the body and send as UTF-8.
# Avoids bash string-escaping pitfalls and guarantees no mojibake.
# Body is read from an external UTF-8 file; response is written to OutFile.

set -u

usage() {
    echo "Usage: $0 --token <token> --owner <owner> --repo <repo> --pr <PR> --body-file <file> [--path <filepath>] [--position <line>] [--out-file <file>] [--match-key <substring>] [--comments-file <file>]"
    exit 2
}

TOKEN=""; OWNER=""; REPO=""; PR=""; BODY_FILE=""; PATH_ARG=""; POSITION=""; OUT_FILE=""; MATCH_KEY=""; COMMENTS_FILE=""

while [ $# -gt 0 ]; do
    case "$1" in
        --token) TOKEN="${2:-}"; shift 2;;
        --owner) OWNER="${2:-}"; shift 2;;
        --repo) REPO="${2:-}"; shift 2;;
        --pr) PR="${2:-}"; shift 2;;
        --body-file) BODY_FILE="${2:-}"; shift 2;;
        --path) PATH_ARG="${2:-}"; shift 2;;
        --position) POSITION="${2:-}"; shift 2;;
        --out-file) OUT_FILE="${2:-}"; shift 2;;
        --match-key) MATCH_KEY="${2:-}"; shift 2;;
        --comments-file) COMMENTS_FILE="${2:-}"; shift 2;;
        *) usage;;
    esac
done

if [ -z "$TOKEN" ] || [ -z "$OWNER" ] || [ -z "$REPO" ] || [ -z "$PR" ] || [ -z "$BODY_FILE" ]; then
    usage
fi
if [ ! -f "$BODY_FILE" ]; then
    echo "FAIL: body file not found: $BODY_FILE" >&2
    exit 2
fi

command -v python3 >/dev/null 2>&1 || { echo "FAIL: python3 not found" >&2; exit 2; }

export TOKEN OWNER REPO PR BODY_FILE PATH_ARG POSITION OUT_FILE MATCH_KEY COMMENTS_FILE

python3 - "$@" <<'PYEOF'
import json, os, sys, urllib.request, urllib.parse, urllib.error

token = os.environ["TOKEN"]
owner = os.environ["OWNER"]
repo = os.environ["REPO"]
pr = os.environ["PR"]
body_file = os.environ["BODY_FILE"]
path = os.environ["PATH_ARG"]
position = os.environ["POSITION"]
out_file = os.environ["OUT_FILE"]
match_key = os.environ["MATCH_KEY"]
comments_file = os.environ["COMMENTS_FILE"]

with open(body_file, "r", encoding="utf-8") as f:
    body = f.read()

base_url = "https://api.gitcode.com/api/v5/repos/{}/{}/pulls/{}/comments".format(
    urllib.parse.quote(owner), urllib.parse.quote(repo), urllib.parse.quote(pr))

def get_json(url):
    req = urllib.request.Request(url, method="GET")
    req.add_header("Accept", "application/json")
    req.add_header("Authorization", "Bearer {}".format(token))
    with urllib.request.urlopen(req) as resp:
        return json.loads(resp.read().decode("utf-8"))

# Dedup: before submitting, avoid duplicates on re-review.
# An issue is already-reported (skip) when EITHER:
#   (1) a diff_comment authored by US sits within +/-TOL lines of the target position, OR
#   (2) an existing comment's body contains match_key (problem ID marker).
# On a re-review of the same PR the diff is unchanged, so the same semantic issue lands on
# the same code LINE regardless of AI wording or ID renumbering -> anchor on (line + author).
# Filtering by author prevents bot comments (e.g. atomgit-bot) on the same line from being
# mistaken for our own review comment. match_key is a secondary check (and the only one
# available for PR-level comments without a position).
if match_key or position:
    try:
        me = ""
        try:
            mreq = urllib.request.Request("https://api.gitcode.com/api/v5/user", method="GET")
            mreq.add_header("Accept", "application/json")
            mreq.add_header("Authorization", "Bearer {}".format(token))
            with urllib.request.urlopen(mreq) as mresp:
                me = json.loads(mresp.read().decode("utf-8")).get("login", "")
        except Exception:
            me = ""

        tol = 2
        dup = False

        def is_dup(c):
            """Return True if comment c is a duplicate of what we are about to post."""
            existing_body = c.get("body") or ""
            if match_key and match_key in existing_body:
                return True
            if position and c.get("comment_type") == "diff_comment" and me:
                user = c.get("user") or {}
                if user.get("login") == me:
                    dp = c.get("diff_position") or {}
                    try:
                        nl = int(dp.get("start_new_line"))
                    except (TypeError, ValueError):
                        nl = None
                    if nl is not None and abs(nl - int(position)) <= tol:
                        return True
            return False

        if comments_file:
            # Use pre-fetched comments from file (raw JSON array) to avoid redundant API calls
            with open(comments_file, "r", encoding="utf-8") as f:
                comments = json.load(f)
            for c in comments:
                if is_dup(c):
                    dup = True
                    break
        else:
            # Fetch comments from API (paginated)
            page = 1
            while not dup:
                comments = get_json(base_url + "?per_page=100&page={}".format(page))
                if not comments:
                    break
                for c in comments:
                    if is_dup(c):
                        dup = True
                        break
                if dup or len(comments) < 100:
                    break
                page += 1
        if dup:
            print("SKIP_ALREADY_EXISTS")
            if out_file:
                with open(out_file, "w", encoding="utf-8") as f:
                    f.write("skipped: comment already exists (same line/author or same MatchKey)")
            sys.exit(0)
    except Exception as e:
        print("FAIL_DEDUP_QUERY")
        if out_file:
            with open(out_file, "w", encoding="utf-8") as f:
                f.write("dedup query failed: {}".format(e))
        sys.exit(1)

payload = {"body": body}
if path:
    payload["path"] = path
    payload["position"] = int(position)
    payload["position_type"] = "text"

data = json.dumps(payload, ensure_ascii=False).encode("utf-8")

req = urllib.request.Request(base_url, data=data, method="POST")
req.add_header("Content-Type", "application/json; charset=UTF-8")
req.add_header("Accept", "application/json")
req.add_header("Authorization", "Bearer {}".format(token))

try:
    with urllib.request.urlopen(req) as resp:
        raw = resp.read()
        if out_file:
            with open(out_file, "wb") as f:
                f.write(raw)
        print("OK HTTP", resp.status)
        sys.exit(0)
except urllib.error.HTTPError as e:
    raw = e.read()
    if out_file:
        with open(out_file, "wb") as f:
            f.write(raw)
    print("FAIL")
    print("HTTP", e.code)
    sys.exit(1)
except Exception as e:
    print("FAIL:", e)
    sys.exit(1)
PYEOF
exit $?
