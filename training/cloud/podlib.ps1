# Reusable helpers for VoLum A2 cloud training on RunPod.
# Dot-source:  . .\training\cloud\podlib.ps1
$ErrorActionPreference = "Stop"
$Key = "$env:USERPROFILE\.ssh\volum_a2_pc"

if (-not $env:RUNPOD_API_KEY) {
  $env:RUNPOD_API_KEY = ((Get-Content "$PSScriptRoot\..\..\.env" | Where-Object { $_ -match '^RUNPOD_API_KEY=' }) -replace '^RUNPOD_API_KEY=','').Trim()
}
$RpHeaders = @{ "Authorization" = "Bearer $($env:RUNPOD_API_KEY)"; "Content-Type" = "application/json" }

function Rp-Get($path) {
  Invoke-RestMethod -Uri "https://rest.runpod.io/v1/$path" -Method Get -Headers $RpHeaders -TimeoutSec 40
}
function Rp-Post($path, $obj) {
  $body = $obj | ConvertTo-Json -Depth 8 -Compress
  Invoke-RestMethod -Uri "https://rest.runpod.io/v1/$path" -Method Post -Headers $RpHeaders -Body $body -TimeoutSec 90
}
function Rp-Delete($path) {
  Invoke-RestMethod -Uri "https://rest.runpod.io/v1/$path" -Method Delete -Headers $RpHeaders -TimeoutSec 40
}
function Rp-Gql($query) {
  $body = @{ query = $query } | ConvertTo-Json -Compress
  Invoke-RestMethod -Uri 'https://api.runpod.io/graphql' -Method Post -Headers $RpHeaders -Body $body -TimeoutSec 40
}

function Convert-ToLf($path) {
  $c = [IO.File]::ReadAllText($path)
  $c = $c -replace "`r`n", "`n"
  [IO.File]::WriteAllText($path, $c)
}
function Pod-Scp($ip, $port, $local, $remote) {
  scp -i $Key -P $port -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL $local "root@${ip}:$remote"
}
function Pod-ScpFrom($ip, $port, $remote, $local) {
  scp -i $Key -P $port -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL "root@${ip}:$remote" $local
}
function Pod-Run($ip, $port, $cmd) {
  ssh -i $Key -p $port -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL -o ConnectTimeout=20 "root@$ip" $cmd
}
function Pod-Script($ip, $port, $localScript, $remotePath) {
  Convert-ToLf $localScript
  Pod-Scp $ip $port $localScript $remotePath
  Pod-Run $ip $port "bash $remotePath"
}
Write-Host "podlib loaded (key chars: $($env:RUNPOD_API_KEY.Length))"
