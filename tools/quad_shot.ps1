# Quad-shot: renders the vehicle from four telling angles and stitches them into
# one PNG, so a single Read gives the whole picture (front symmetry, rear, side
# profile, 3/4 overview). The ImGui panel on the right is cropped out.
#
# The caller sets the mood via env vars (JOZZ_M6_DIAG / JOZZ_M6_WHEEL /
# JOZZ_M6_DUMPER / JOZZ_M6_ARMTINT / JOZZ_M6_HERTZ ...); this script only drives
# JOZZ_M6_CAM per view. Example:
#   $env:JOZZ_M6_DIAG="1"; $env:JOZZ_M6_ARMTINT="1"; $env:JOZZ_M6_WHEEL="0"
#   .\tools\quad_shot.ps1 -Out C:\tmp\quad.png
param(
	[string]$SampleName = "Suspension Rig",
	[Parameter(Mandatory = $true)][string]$Out,
	[int]$Frames = 150,
	[string]$Exe = ".\build\bin\Debug\samples.exe"
)
$ErrorActionPreference = "Stop"

# label ; "yaw,pitch,radius,targetX,targetY,targetZ"  (forward +X, up +Y, left -Z)
$views = @(
	@{ label = "FRONT (symmetry)"; cam = "180,5,4.1,1.28,0.45,0" },
	@{ label = "REAR";             cam = "0,5,4.1,-1.28,0.45,0" },
	@{ label = "LEFT SIDE";        cam = "90,4,6.6,0,0.45,0" },
	@{ label = "3/4 OVERVIEW";     cam = "-130,18,6.6,0,0.35,0" }
)

$tmp = Join-Path $env:TEMP ("quadshot_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmp | Out-Null
$shots = @()
foreach ($v in $views) {
	$p = Join-Path $tmp (($v.label -replace '[^A-Za-z0-9]', '_') + ".png")
	$env:JOZZ_M6_CAM = $v.cam
	& $Exe --sample-name $SampleName --frames $Frames --screenshot $p | Out-Null
	if (-not (Test-Path $p)) { throw "capture failed for view '$($v.label)' (cam $($v.cam))" }
	$shots += @{ label = $v.label; path = $p }
}

Add-Type -AssemblyName System.Drawing
$cropW = 1560   # drop the ImGui panel (~360 px on the right of a 1920 frame)
$cellW = 780
$cellH = 540
$canvas = New-Object System.Drawing.Bitmap (($cellW * 2), ($cellH * 2))
$g = [System.Drawing.Graphics]::FromImage($canvas)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.Color]::Black)
$font = New-Object System.Drawing.Font ("Consolas", 15, [System.Drawing.FontStyle]::Bold)
$pos = @(@(0, 0), @($cellW, 0), @(0, $cellH), @($cellW, $cellH))
for ($i = 0; $i -lt 4; $i++) {
	$img = [System.Drawing.Image]::FromFile($shots[$i].path)
	$sw = [Math]::Min($cropW, $img.Width)
	$src = New-Object System.Drawing.Rectangle (0, 0, $sw, $img.Height)
	$dst = New-Object System.Drawing.Rectangle ($pos[$i][0], $pos[$i][1], $cellW, $cellH)
	$g.DrawImage($img, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
	# label with a dark plate behind for legibility
	$g.FillRectangle([System.Drawing.Brushes]::Black, $pos[$i][0], $pos[$i][1], 250, 26)
	$g.DrawString($shots[$i].label, $font, [System.Drawing.Brushes]::Yellow, ($pos[$i][0] + 6), ($pos[$i][1] + 3))
	$img.Dispose()
}
$g.Dispose()
$canvas.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose()
Remove-Item -Recurse -Force $tmp
Write-Host "quad -> $Out"
