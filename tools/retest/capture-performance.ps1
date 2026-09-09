# Capture a repeatable held-button camera gesture in an already running app.
# Start WxLens with WXLENS_FRAME_TIMINGS pointing to a new CSV, let data load,
# choose layout/overlays, then run this once per explicitly named scenario.
param(
    [Parameter(Mandatory)][string]$Scenario,
    [Parameter(Mandatory)][string]$Destination,
    [ValidateRange(5, 60)][int]$Seconds = 60,
    [switch]$Idle
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ui-drive.ps1') -Action rect | Out-Null
$appProcess = Get-App
$appHandle = $appProcess.MainWindowHandle
Focus-App $appHandle
$bounds = Get-Rect $appHandle
$originX = [int](($bounds.Left + $bounds.Right) / 2)
$originY = [int](($bounds.Top + $bounds.Bottom) / 2)
$logicalProcessors = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
$samples = [Collections.Generic.List[object]]::new()
$appProcess.Refresh()
$lastCpu = $appProcess.TotalProcessorTime.TotalSeconds
$lastSample = 0.0
$started = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$clock = [Diagnostics.Stopwatch]::StartNew()
$valid = $false
try {
    [W]::SetCursorPos($originX, $originY) | Out-Null
    $lastX = $originX
    $lastY = $originY
    if (-not $Idle) { [W]::Mouse(0x0002, 0) }
    while ($clock.Elapsed.TotalSeconds -lt $Seconds) {
        if ($appProcess.HasExited) { throw 'App exited during capture' }
        if ([W]::GetForegroundWindow() -ne $appHandle) { throw 'App lost foreground; capture invalid' }
        if (-not $Idle) {
            $cursor = New-Object W+POINT
            [W]::GetCursorPos([ref]$cursor) | Out-Null
            if ([Math]::Abs($cursor.X - $lastX) -gt 5 -or [Math]::Abs($cursor.Y - $lastY) -gt 5) {
                throw "Cursor displaced from $lastX,$lastY to $($cursor.X),$($cursor.Y); capture invalid"
            }
            # Smooth, bounded camera motion: same trajectory with overlays on/off.
            $phase = $clock.Elapsed.TotalSeconds * [Math]::PI / 2
            $lastX = $originX + [int](80 * [Math]::Sin($phase))
            $lastY = $originY + [int](40 * [Math]::Sin(2 * $phase))
            [W]::SetCursorPos($lastX, $lastY) | Out-Null
        }
        $elapsed = $clock.Elapsed.TotalSeconds
        if ($elapsed - $lastSample -ge 1) {
            $appProcess.Refresh()
            $cpu = $appProcess.TotalProcessorTime.TotalSeconds
            $samples.Add([pscustomobject]@{
                elapsed_seconds = $elapsed
                cpu_percent = 100 * ($cpu - $lastCpu) / ($elapsed - $lastSample) / $logicalProcessors
                working_set_bytes = $appProcess.WorkingSet64
                private_bytes = $appProcess.PrivateMemorySize64
            })
            $lastCpu = $cpu
            $lastSample = $elapsed
        }
        Start-Sleep -Milliseconds 16
    }
    $valid = $true
} finally {
    if (-not $Idle) { [W]::Mouse(0x0004, 0) }
    [pscustomobject]@{
        scenario = $Scenario
        valid = $valid
        gesture = $(if ($Idle) { 'idle' } else { 'press-drag-release' })
        start_utc_ms = $started
        end_utc_ms = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
        logical_processors = $logicalProcessors
        window_width = $bounds.Right - $bounds.Left
        window_height = $bounds.Bottom - $bounds.Top
        process_samples = $samples.ToArray()
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Destination
}
Write-Output "Captured $Scenario ($Seconds seconds) to $Destination"
