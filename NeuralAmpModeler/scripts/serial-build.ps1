# Dot-sourced by run-tests-win.ps1 and run-app-win.ps1.
# Every worktree runs its own /m build; three at once ran a 16 GB laptop out of commit
# (MSB4018, C1060 out of heap space). The build waits for any other worktree's build.
# Keep the name: worktrees on older revisions of run-tests-win.ps1 lock the same mutex.
function Invoke-SerialBuild([string] $msbuild, [string[]] $msbuildArgs) {
  $mutex = [System.Threading.Mutex]::new($false, "Local\VoLum-run-tests-win-build")
  $owned = $false
  try {
    try { $owned = $mutex.WaitOne(0) } catch [System.Threading.AbandonedMutexException] { $owned = $true }
    if (-not $owned) {
      Write-Host "Waiting for another worktree's build to finish..."
      try { $owned = $mutex.WaitOne() } catch [System.Threading.AbandonedMutexException] { $owned = $true }
    }
    & $msbuild @msbuildArgs
  }
  finally {
    if ($owned) { $mutex.ReleaseMutex() }
    $mutex.Dispose()
  }
}
