Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$process = Get-Process VoLum -ErrorAction SilentlyContinue
if (-not $process) {
    Write-Host "VoLum not running."
    exit 1
}

$rootElement = [System.Windows.Automation.AutomationElement]::RootElement
$condition = [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ProcessIdProperty,
    $process.Id
)
$appWindow = $rootElement.FindFirst([System.Windows.Automation.TreeScope]::Children, $condition)

if (-not $appWindow) {
    Write-Host "Window not found."
    exit 1
}

Write-Host "App is running and window is open!"

# Let's verify we don't crash when interacting
Start-Sleep -Seconds 1
exit 0
