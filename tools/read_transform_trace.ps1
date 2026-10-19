# Reads the records of one `transformtrace <tag>` request from an editor's async log (hanteichan.log in the
# editor's working directory). The editor emits them as debug lines "transform-trace <tag> {json}"; the
# transformtrace command enables debug logging itself. Read the log only after the editor process has exited.
param([Parameter(Mandatory=$true)][string]$Log,[Parameter(Mandatory=$true)][string]$Tag)
$ErrorActionPreference='Stop'
foreach($line in (Get-Content -LiteralPath $Log)) {
    $at=$line.IndexOf('transform-trace ')
    if($at -lt 0) { continue }
    $rest=$line.Substring($at+16)
    $json=$rest.IndexOf(' {"')
    if($json -lt 0 -or $rest.Substring(0,$json) -ne $Tag) { continue }
    $rest.Substring($json+1) | ConvertFrom-Json
}
