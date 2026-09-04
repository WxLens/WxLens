# Drives the packaged wxlens-app for interactive retests (docs/phase1-ux-feedback-2026-08-31.md).
# Each call is stateless - the app process is found by name - so a retest is a sequence of calls:
#   ui-drive.ps1 launch [-Maximize] [-Settle 12]   start the app, wait for its window, settle
#   ui-drive.ps1 smoke [-Settle 3]                 assert startup window and zero QML errors
#   ui-drive.ps1 capture -Out name.png             DPI-aware screenshot of the whole window
#   ui-drive.ps1 click|rclick -X 100 -Y 200        click at window-relative *physical* pixels
#   ui-drive.ps1 wheel -X 100 -Y 200 -Notches -5 [-IntervalMs 40]   (negative = scroll down)
#   ui-drive.ps1 type -Text "KICX"                 type characters into the focused control
#   ui-drive.ps1 key -Vk 0x1B                      press a virtual key (0x09 Tab, 0x0D Enter)
#   ui-drive.ps1 blur                              hand focus to another window on purpose
#   ui-drive.ps1 rect | maximize | close
#
# Hard-won rules baked in here, because each produced a false bug report before it was added:
#  - SetProcessDPIAware, or captures and clicks are in logical pixels on a 125 % display.
#  - Every input first forces the app to the foreground and *verifies* it got there; a capture
#    taken while another window covers the app is a screenshot of that window.
#  - The window under the cursor is verified before and after a wheel burst. If the physical
#    mouse is nudged mid-burst the rest of the burst goes wherever the cursor went, which looks
#    exactly like an event leak; such runs are reported as "CURSOR DISPLACED" and are invalid.
#  - Read every capture before trusting a pixel metric; live radar data and label placement can
#    change a map that nobody touched.
param(
    [Parameter(Position = 0)][string]$Action = "rect",
    [int]$X = 0, [int]$Y = 0, [int]$Notches = 0,
    [string]$Text = "", [int]$Vk = 0, [string]$Out = "", [int]$IntervalMs = 40,
    [switch]$Maximize,
    [int]$Settle = 0,
    [string]$ExePath = (Join-Path $PSScriptRoot "..\..\build-release-vs2026\Release\bin\wxlens-app.exe"),
    [string]$OutDir = (Join-Path $PSScriptRoot "captures"),
    [string]$LogPath = (Join-Path $env:APPDATA "WxLens\WxLens\logs\wxlens.log")
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class W {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT p);
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT { public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)] public struct IU { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public IU u; }
    public static void Mouse(uint flags, int data) {
        var i = new INPUT(); i.type = 0; i.u.mi.dwFlags = flags; i.u.mi.mouseData = unchecked((uint)data);
        SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Key(ushort vk, bool up) {
        var i = new INPUT(); i.type = 1; i.u.ki.wVk = vk; i.u.ki.dwFlags = up ? 2u : 0u;
        SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Char(char c) {
        var i = new INPUT(); i.type = 1; i.u.ki.wScan = c; i.u.ki.dwFlags = 4u; // KEYEVENTF_UNICODE
        SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT)));
        i.u.ki.dwFlags = 6u; SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@
[W]::SetProcessDPIAware() | Out-Null

function Get-App {
    $p = Get-Process -Name "wxlens-app" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { throw "wxlens-app is not running" }
    return $p
}
function Get-Rect($h) { $r = New-Object W+RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null; return $r }
function Format-Rect($r) { "rect=$($r.Left),$($r.Top) $($r.Right - $r.Left)x$($r.Bottom - $r.Top)" }
function Read-NewLog($path, $offset) {
    if (-not (Test-Path -LiteralPath $path)) { return "" }
    $stream = [System.IO.File]::Open($path, [System.IO.FileMode]::Open,
                                    [System.IO.FileAccess]::Read,
                                    [System.IO.FileShare]::ReadWrite)
    try {
        $start = [Math]::Min([int64]$offset, $stream.Length)
        $stream.Position = $start
        $reader = [System.IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true, 1024, $true)
        try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally { $stream.Dispose() }
}
function Focus-App($h) {
    for ($i = 0; $i -lt 5; $i++) {
        if ([W]::GetForegroundWindow() -eq $h) { Start-Sleep -Milliseconds 150; return }
        # An Alt tap releases Windows' foreground lock so a background process may activate a window.
        [W]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [W]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
        [W]::SetForegroundWindow($h) | Out-Null
        Start-Sleep -Milliseconds 250
    }
    if ([W]::GetForegroundWindow() -ne $h) { throw "wxlens-app is not the foreground window" }
}
function Move-To($h, $x, $y) {
    $r = Get-Rect $h
    [W]::SetCursorPos($r.Left + $x, $r.Top + $y) | Out-Null; Start-Sleep -Milliseconds 150
    $p = New-Object W+POINT; [W]::GetCursorPos([ref]$p) | Out-Null
    $under = [W]::GetAncestor([W]::WindowFromPoint($p), 2)   # GA_ROOT
    if ($under -ne $h) { throw "cursor at $($p.X),$($p.Y) is not over wxlens-app" }
}

switch ($Action) {
    "smoke" {
        if (-not (Test-Path -LiteralPath $ExePath)) { throw "Executable not found: $ExePath" }
        if (Get-Process -Name "wxlens-app" -ErrorAction SilentlyContinue) {
            throw "Close the running wxlens-app before the smoke test"
        }
        $logLength = if (Test-Path -LiteralPath $LogPath) { (Get-Item -LiteralPath $LogPath).Length } else { 0 }
        $proc = Start-Process -FilePath $ExePath -PassThru
        try {
            $deadline = (Get-Date).AddSeconds(45)
            while ((Get-Date) -lt $deadline) {
                $proc.Refresh()
                if ($proc.HasExited) { throw "wxlens-app exited during startup with code $($proc.ExitCode)" }
                if ($proc.MainWindowHandle -ne 0) { break }
                Start-Sleep -Milliseconds 300
            }
            if ($proc.MainWindowHandle -eq 0) { throw "No main window appeared within 45 seconds" }
            Start-Sleep -Seconds ($(if ($Settle) { $Settle } else { 3 }))
            $proc.Refresh()
            if ($proc.HasExited) { throw "wxlens-app exited after showing its window with code $($proc.ExitCode)" }

            $newLog = Read-NewLog $LogPath $logLength
            $qmlErrors = @($newLog -split "`r?`n" | Where-Object {
                $_ -match "QML warning:|Failed to load QML application engine"
            })
            if ($qmlErrors.Count -gt 0) { throw "QML startup errors:`n$($qmlErrors -join "`n")" }
            Write-Output ("SMOKE PASS PID=$($proc.Id) " + (Format-Rect (Get-Rect $proc.MainWindowHandle)) + " QML-errors=0")
        } finally {
            if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
        }
    }
    "launch" {
        $proc = Start-Process -FilePath $ExePath -PassThru
        $deadline = (Get-Date).AddSeconds(45)
        while ((Get-Date) -lt $deadline) { $proc.Refresh(); if ($proc.MainWindowHandle -ne 0) { break }; Start-Sleep -Milliseconds 300 }
        if ($proc.MainWindowHandle -eq 0) { throw "No main window appeared" }
        Focus-App $proc.MainWindowHandle
        if ($Maximize) { [W]::ShowWindow($proc.MainWindowHandle, 3) | Out-Null }
        Start-Sleep -Seconds ($(if ($Settle) { $Settle } else { 10 }))
        Write-Output ("PID=$($proc.Id) " + (Format-Rect (Get-Rect $proc.MainWindowHandle)))
    }
    "rect" { $p = Get-App; Write-Output (Format-Rect (Get-Rect $p.MainWindowHandle)) }
    "maximize" {
        $p = Get-App; Focus-App $p.MainWindowHandle; [W]::ShowWindow($p.MainWindowHandle, 3) | Out-Null
        Start-Sleep -Seconds 3; Write-Output (Format-Rect (Get-Rect $p.MainWindowHandle))
    }
    "capture" {
        $p = Get-App; $h = $p.MainWindowHandle
        Focus-App $h
        if ($Settle) { Start-Sleep -Seconds $Settle }
        $r = Get-Rect $h
        $w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
        $bmp = New-Object System.Drawing.Bitmap($w, $ht)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size); $g.Dispose()
        $path = Join-Path $OutDir $(if ($Out) { $Out } else { "capture.png" })
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
        Write-Output "saved $path (${w}x${ht})"
    }
    "click" {
        $p = Get-App; Focus-App $p.MainWindowHandle
        Move-To $p.MainWindowHandle $X $Y
        [W]::Mouse(0x0002, 0); Start-Sleep -Milliseconds 60; [W]::Mouse(0x0004, 0)
        Start-Sleep -Milliseconds ($(if ($Settle) { $Settle * 1000 } else { 500 }))
        Write-Output "clicked $X,$Y"
    }
    "rclick" {
        $p = Get-App; Focus-App $p.MainWindowHandle
        Move-To $p.MainWindowHandle $X $Y
        [W]::Mouse(0x0008, 0); Start-Sleep -Milliseconds 60; [W]::Mouse(0x0010, 0)
        Start-Sleep -Milliseconds 500
        Write-Output "right-clicked $X,$Y"
    }
    "wheel" {
        $p = Get-App; Focus-App $p.MainWindowHandle
        Move-To $p.MainWindowHandle $X $Y
        $count = [Math]::Abs($Notches)
        $delta = if ($Notches -lt 0) { -120 } else { 120 }
        $r = Get-Rect $p.MainWindowHandle
        $moved = 0
        for ($i = 0; $i -lt $count; $i++) {
            $c = New-Object W+POINT; [W]::GetCursorPos([ref]$c) | Out-Null
            if ($c.X -ne ($r.Left + $X) -or $c.Y -ne ($r.Top + $Y)) { $moved++; [W]::SetCursorPos($r.Left + $X, $r.Top + $Y) | Out-Null; Start-Sleep -Milliseconds 30 }
            [W]::Mouse(0x0800, $delta); Start-Sleep -Milliseconds $IntervalMs
        }
        Start-Sleep -Milliseconds 600
        $c = New-Object W+POINT; [W]::GetCursorPos([ref]$c) | Out-Null
        $stable = if ($moved -eq 0 -and $c.X -eq ($r.Left + $X) -and $c.Y -eq ($r.Top + $Y)) { "cursor stable" } else { "CURSOR DISPLACED $moved time(s); now at $($c.X - $r.Left),$($c.Y - $r.Top) - run is invalid" }
        Write-Output "wheel $Notches at $X,$Y - $stable"
    }
    "type" {
        $p = Get-App; Focus-App $p.MainWindowHandle
        foreach ($c in $Text.ToCharArray()) { [W]::Char($c); Start-Sleep -Milliseconds 40 }
        Start-Sleep -Milliseconds 400
        Write-Output "typed '$Text'"
    }
    "key" {
        $p = Get-App; Focus-App $p.MainWindowHandle
        [W]::Key([uint16]$Vk, $false); Start-Sleep -Milliseconds 50; [W]::Key([uint16]$Vk, $true)
        Start-Sleep -Milliseconds 400
        Write-Output "key $Vk"
    }
    "blur" {
        $other = Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $_.ProcessName -ne "wxlens-app" -and $_.MainWindowTitle -ne "" } | Select-Object -First 1
        [W]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [W]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
        [W]::SetForegroundWindow($other.MainWindowHandle) | Out-Null
        Start-Sleep -Milliseconds ($(if ($Settle) { $Settle * 1000 } else { 1500 }))
        $p = Get-App
        Write-Output ("blurred to '{0}'; app foreground = {1}" -f $other.MainWindowTitle, ([W]::GetForegroundWindow() -eq $p.MainWindowHandle))
    }
    "close" { $p = Get-App; Stop-Process -Id $p.Id -Force; Write-Output "closed" }
    default { throw "unknown action $Action" }
}
