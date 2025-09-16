param(
  [string]$ExePath = 'C:\Users\User\terminal_c\build\Debug\TradingTerminal.exe',
  [int]$Tail = 200,
  [switch]$NoChild,
  [switch]$NoWebView,
  [switch]$IgnoreClose
)

$ErrorActionPreference = 'SilentlyContinue'
$LogPath = Join-Path $PSScriptRoot '..\terminal.log'
$marker = "---- RUN START $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff') ----"
Add-Content -Path $LogPath -Value $marker

if ($IgnoreClose) { $env:CANDLE_IGNORE_CLOSE = '1' } else { Remove-Item Env:CANDLE_IGNORE_CLOSE -ErrorAction SilentlyContinue }
if ($NoChild) { $env:CANDLE_WEBVIEW_NO_CHILD = '1' } else { Remove-Item Env:CANDLE_WEBVIEW_NO_CHILD -ErrorAction SilentlyContinue }
if ($NoWebView) { $env:CANDLE_DISABLE_WEBVIEW = '1' } else { Remove-Item Env:CANDLE_DISABLE_WEBVIEW -ErrorAction SilentlyContinue }

Write-Host "Starting: $ExePath"
$p = Start-Process -FilePath $ExePath -WorkingDirectory (Split-Path $ExePath) -PassThru
try {
  Wait-Process -InputObject $p -Timeout 30 | Out-Null
} catch {}

try { $exit = $p.ExitCode } catch { $exit = '<unknown>' }
try { $pid = $p.Id } catch { $pid = '<unknown>' }
Write-Host "Process exited. ExitCode=$exit PID=$pid"
Write-Host "---- Log tail ($Tail) ----"
Get-Content -Tail $Tail $LogPath
