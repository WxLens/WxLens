# Counts the pixel rows that differ between two screenshots inside one rectangle. Used by the
# packaged retests in docs/phase1-ux-feedback-2026-08-31.md to prove "the map did not move":
# a row counts as different if any pixel in it differs, so a moved or zoomed map lights up every
# row while an unchanged one reports 0.
#   compare-region.ps1 -First a.png -Second b.png -X0 600 -Y0 100 -X1 1600 -Y1 950
param([Parameter(Mandatory)][string]$First, [Parameter(Mandatory)][string]$Second,
      [int]$X0 = 0, [int]$Y0 = 0, [int]$X1 = [int]::MaxValue, [int]$Y1 = [int]::MaxValue)
Add-Type -AssemblyName System.Drawing
function Load-Bytes([string]$path) {
    $bmp = [System.Drawing.Bitmap]::FromFile($path)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $bmp.Width, $bmp.Height)
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($data.Stride * $bmp.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    $bmp.UnlockBits($data)
    $result = @{ Bytes = $bytes; Stride = $data.Stride; Width = $bmp.Width; Height = $bmp.Height }
    $bmp.Dispose()
    return $result
}
$a = Load-Bytes $First; $b = Load-Bytes $Second
$X1 = [Math]::Min($X1, [Math]::Min($a.Width, $b.Width)); $Y1 = [Math]::Min($Y1, [Math]::Min($a.Height, $b.Height))
$rowsDiffer = 0; $rows = 0
for ($y = $Y0; $y -lt $Y1; $y++) {
    $rows++
    $sa = [System.ArraySegment[byte]]::new($a.Bytes, $y * $a.Stride + $X0 * 4, ($X1 - $X0) * 4)
    $sb = [System.ArraySegment[byte]]::new($b.Bytes, $y * $b.Stride + $X0 * 4, ($X1 - $X0) * 4)
    if (-not [System.Linq.Enumerable]::SequenceEqual([byte[]]$sa, [byte[]]$sb)) { $rowsDiffer++ }
}
Write-Output "$rowsDiffer / $rows pixel rows differ in ($X0,$Y0)-($X1,$Y1)"
