param(
    [Parameter(Mandatory = $true)][string]$Token,
    [Parameter(Mandatory = $true)][string]$Owner,
    [Parameter(Mandatory = $true)][string]$Repo,
    [Parameter(Mandatory = $true)][int]$Pr,
    [Parameter(Mandatory = $true)][string]$OutFile,
    [switch]$Raw
)

# list_comments.ps1 - Dump all PR comments (paginated) to a UTF-8 file for agent-level
# duplicate analysis. The agent reads this file with the Read tool (UTF-8 safe) and
# semantically compares each new review issue against ALL existing comments (from any
# reviewer), not just its own, before deciding whether to submit.
# -Raw mode: output raw JSON array (consumable by post_comment.ps1 -CommentsFile).
# Must use Invoke-WebRequest + explicit UTF-8 decode (Invoke-RestMethod decodes as GBK
# on Windows 5.1 and corrupts Chinese).

$ErrorActionPreference = "Stop"

$uri = "https://api.gitcode.com/api/v5/repos/$Owner/$Repo/pulls/$Pr/comments"
$headers = @{ "Authorization" = "Bearer $Token" }
$page = 1

try {
    if ($Raw) {
        # Collect raw JSON from all pages and merge into a single JSON array.
        $rawParts = @()
        while ($true) {
            $resp = Invoke-WebRequest -Method Get -Uri "$($uri)?per_page=100&page=$page" -Headers $headers -UseBasicParsing
            $json = [System.Text.Encoding]::UTF8.GetString($resp.RawContentStream.ToArray())
            $list = $json | ConvertFrom-Json
            if (-not $list -or @($list).Count -eq 0) { break }
            $trimmed = $json.Trim()
            if ($trimmed.StartsWith("[")) { $trimmed = $trimmed.Substring(1) }
            if ($trimmed.EndsWith("]")) { $trimmed = $trimmed.Substring(0, $trimmed.Length - 1) }
            if ($trimmed) { $rawParts += $trimmed }
            if (@($list).Count -lt 100) { break }
            $page++
        }
        $rawJson = "[" + ($rawParts -join ",") + "]"
        [System.IO.File]::WriteAllText($OutFile, $rawJson, [System.Text.Encoding]::UTF8)
        Write-Output "OK WROTE $OutFile (raw json)"
        exit 0
    }

    $sb = New-Object System.Text.StringBuilder
    while ($true) {
        $resp = Invoke-WebRequest -Method Get -Uri "$($uri)?per_page=100&page=$page" -Headers $headers -UseBasicParsing
        $json = [System.Text.Encoding]::UTF8.GetString($resp.RawContentStream.ToArray())
        $list = $json | ConvertFrom-Json
        if (-not $list -or @($list).Count -eq 0) { break }
        foreach ($c in $list) {
            $line = ""
            try { $line = $c.diff_position.start_new_line } catch { $line = "" }
            $author = ""
            if ($c.user) { $author = $c.user.login }
            [void]$sb.AppendLine("===== type=$($c.comment_type) id=$($c.id) author=$author line=$line =====")
            [void]$sb.AppendLine([string]$c.body)
            [void]$sb.AppendLine("")
        }
        if (@($list).Count -lt 100) { break }
        $page++
    }
    [System.IO.File]::WriteAllText($OutFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Output "OK WROTE $OutFile"
    exit 0
} catch {
    Write-Output "FAIL"
    if (-not [string]::IsNullOrWhiteSpace($OutFile)) {
        [System.IO.File]::WriteAllText($OutFile, "list failed: $($_.Exception.Message)", [System.Text.Encoding]::UTF8)
    }
    exit 1
}
