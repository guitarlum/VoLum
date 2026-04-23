Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing

$process = Get-Process VoLum -ErrorAction SilentlyContinue
if (-not $process) { exit 1 }

$rootElement = [System.Windows.Automation.AutomationElement]::RootElement
$condition = [System.Windows.Automation.PropertyCondition]::new(
    [System.Windows.Automation.AutomationElement]::ProcessIdProperty,
    $process.Id
)
$appWindow = $rootElement.FindFirst([System.Windows.Automation.TreeScope]::Children, $condition)

if (-not $appWindow) { exit 1 }

# Take screenshot of the window
$rect = $appWindow.Current.BoundingRectangle
$bmp = [System.Drawing.Bitmap]::new([int]$rect.Width, [int]$rect.Height)
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
$gfx.CopyFromScreen([int]$rect.Left, [int]$rect.Top, 0, 0, $bmp.Size)

$path = "c:\dev\VoLum\tests\ui_test_shot.png"
if (-not (Test-Path "c:\dev\VoLum\tests")) { New-Item -ItemType Directory -Path "c:\dev\VoLum\tests" }
$bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)

$gfx.Dispose()
$bmp.Dispose()

Write-Host "Screenshot saved to $path"
exit 0
