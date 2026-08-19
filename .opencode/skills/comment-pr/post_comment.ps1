param(
    [Parameter(Mandatory = $true)][string]$Token,
    [Parameter(Mandatory = $true)][string]$Owner,
    [Parameter(Mandatory = $true)][string]$Repo,
    [Parameter(Mandatory = $true)][int]$Pr,
    [Parameter(Mandatory = $true)][string]$BodyFile,
    [string]$Path,
    [int]$Position,
    [string]$OutFile,
    [string]$MatchKey,
    [string]$CommentsFile
)

# post_comment.ps1 - Fixed, encoding-safe GitCode PR inline comment submitter.
# Works on both Windows PowerShell 5.1 (.NET Framework) and PowerShell 7+ (.NET Core).
# AVOIDS the PowerShell 5.1 ConvertTo-Json blowup bug and GBK mojibake by:
#   1. Escaping the body with System.Text.Json (PS 7+) or JavaScriptSerializer (PS 5.1).
#   2. Sending UTF-8 BYTE array (not a string) with Content-Type charset=UTF-8 (uppercase).
#   3. Reading body from an external UTF-8 file (no Chinese literals inside this script).
#   4. Writing the raw JSON response to OutFile (UTF-8) for reliable verification.

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BodyFile)) {
    Write-Error "Body file not found: $BodyFile"
    exit 2
}

# Read body as UTF-8 (no BOM interference).
$bodyText = [System.IO.File]::ReadAllText($BodyFile, [System.Text.Encoding]::UTF8)

# Escape body correctly for JSON (handles Chinese, newlines, quotes, code fences).
if ($PSVersionTable.PSVersion.Major -ge 6) {
    # PowerShell 7+ (.NET Core): System.Web.Extensions unavailable, use System.Text.Json.
    $escapedBody = [System.Text.Json.JsonSerializer]::Serialize($bodyText)
} else {
    # Windows PowerShell 5.1 (.NET Framework): JavaScriptSerializer (ConvertTo-Json is buggy).
    Add-Type -AssemblyName System.Web.Extensions -ErrorAction Stop
    $js = New-Object System.Web.Script.Serialization.JavaScriptSerializer
    $escapedBody = $js.Serialize($bodyText)
}

# Build JSON payload manually (never use ConvertTo-Json on the body).
if ([string]::IsNullOrWhiteSpace($Path)) {
    $json = '{"body":' + $escapedBody + '}'
} else {
    $json = '{"body":' + $escapedBody + ',"path":"' + $Path + '","position":' + $Position + ',"position_type":"text"}'
}

$uri = "https://api.gitcode.com/api/v5/repos/$Owner/$Repo/pulls/$Pr/comments"
$headers = @{ "Authorization" = "Bearer $Token" }

# Dedup: before submitting, query existing comments to avoid duplicates on re-review.
# An issue is considered already-reported (skip) when EITHER:
#   (1) a diff_comment authored by US sits within +/-Tolerance lines of -Position, OR
#   (2) an existing comment's body contains -MatchKey (problem ID marker).
# Rationale: on a re-review of the same PR the diff is unchanged, so the same semantic
# issue always lands on the same code LINE regardless of how the AI rephrases the
# description or renumbers the issue ID. We therefore anchor on (line + author) rather
# than on text/ID, and filter by author so bot comments (e.g. atomgit-bot) on the same
# line are NOT mistaken for our own review comment. MatchKey remains as a secondary check
# (also the only one available for PR-level comments without a -Position).
if (-not [string]::IsNullOrWhiteSpace($MatchKey) -or $Position) {
    try {
        # Current user login, to identify only our own comments.
        $me = ""
        try {
            $mresp = Invoke-WebRequest -Method Get -Uri "https://api.gitcode.com/api/v5/user" -Headers $headers -UseBasicParsing
            $me = ([System.Text.Encoding]::UTF8.GetString($mresp.RawContentStream.ToArray()) | ConvertFrom-Json).login
        } catch { $me = "" }

        $tol = 2
        $dup = $false

        if (-not [string]::IsNullOrWhiteSpace($CommentsFile) -and (Test-Path $CommentsFile)) {
            # Use pre-fetched comments from file (raw JSON array) to avoid redundant API calls
            $gjson = [System.IO.File]::ReadAllText($CommentsFile, [System.Text.Encoding]::UTF8)
            $list = $gjson | ConvertFrom-Json
            if ($list) {
                foreach ($c in $list) {
                    if (-not [string]::IsNullOrWhiteSpace($MatchKey) -and $c.body -and $c.body.Contains($MatchKey)) { $dup = $true; break }
                    if ($Position -and $c.comment_type -eq 'diff_comment' -and $me -and $c.user -and $c.user.login -eq $me) {
                        $l = $null
                        try { $l = [int]$c.diff_position.start_new_line } catch { $l = $null }
                        if ($l -and [Math]::Abs($l - $Position) -le $tol) { $dup = $true; break }
                    }
                }
            }
        } else {
            # Fetch comments from API (paginated)
            $page = 1
            while (-not $dup) {
                $getUri = "$($uri)?per_page=100&page=$page"
                $gresp = Invoke-WebRequest -Method Get -Uri $getUri -Headers $headers -UseBasicParsing
                $gjson = [System.Text.Encoding]::UTF8.GetString($gresp.RawContentStream.ToArray())
                $list = $gjson | ConvertFrom-Json
                if (-not $list -or @($list).Count -eq 0) { break }
                foreach ($c in $list) {
                    # (2) MatchKey body match (any author; only our comments carry the ID marker).
                    if (-not [string]::IsNullOrWhiteSpace($MatchKey) -and $c.body -and $c.body.Contains($MatchKey)) { $dup = $true; break }
                    # (1) same author + nearby line.
                    if ($Position -and $c.comment_type -eq 'diff_comment' -and $me -and $c.user -and $c.user.login -eq $me) {
                        $l = $null
                        try { $l = [int]$c.diff_position.start_new_line } catch { $l = $null }
                        if ($l -and [Math]::Abs($l - $Position) -le $tol) { $dup = $true; break }
                    }
                }
                if ($dup -or @($list).Count -lt 100) { break }
                $page++
            }
        }
        if ($dup) {
            Write-Output "SKIP_ALREADY_EXISTS"
            if (-not [string]::IsNullOrWhiteSpace($OutFile)) {
                [System.IO.File]::WriteAllText($OutFile, "skipped: comment already exists (same line/author or same MatchKey)", [System.Text.Encoding]::UTF8)
            }
            exit 0
        }
    } catch {
        # If the query fails, do not silently proceed to submit (unsafe for dedup).
        Write-Output "FAIL_DEDUP_QUERY"
        if (-not [string]::IsNullOrWhiteSpace($OutFile)) {
            [System.IO.File]::WriteAllText($OutFile, "dedup query failed: $($_.Exception.Message)", [System.Text.Encoding]::UTF8)
        }
        exit 1
    }
}

try {
    $resp = Invoke-WebRequest -Method Post -Uri $uri `
        -ContentType "application/json; charset=UTF-8" `
        -Body ([System.Text.Encoding]::UTF8.GetBytes($json)) `
        -Headers $headers `
        -UseBasicParsing

    # Persist raw response for UTF-8 verification.
    if (-not [string]::IsNullOrWhiteSpace($OutFile)) {
        [System.IO.File]::WriteAllBytes($OutFile, $resp.RawContentStream.ToArray())
    }
    Write-Output "OK HTTP $($resp.StatusCode)"
    exit 0
} catch {
    Write-Output "FAIL"
    $r = $_.Exception.Response
    if ($r) {
        if ($r.GetType().FullName -eq 'System.Net.Http.HttpResponseMessage') {
            # PowerShell 7+ (HttpClient): read body from HttpContent.
            $msg = $r.Content.ReadAsStringAsync().GetAwaiter().GetResult()
            Write-Output "HTTP $([int]$r.StatusCode)"
        } else {
            # Windows PowerShell 5.1 (HttpWebResponse): read body from response stream.
            $sr = New-Object System.IO.StreamReader($r.GetResponseStream())
            $msg = $sr.ReadToEnd()
            Write-Output "HTTP $($r.StatusCode.value__)"
        }
        if (-not [string]::IsNullOrWhiteSpace($OutFile)) {
            [System.IO.File]::WriteAllText($OutFile, $msg, [System.Text.Encoding]::UTF8)
        }
    } else {
        Write-Output $_.Exception.Message
    }
    exit 1
}
