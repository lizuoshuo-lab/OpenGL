[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [string]$FfmpegPath = "ffmpeg",
    [string]$OutputPath,
    [int]$OutputIndex = 0,
    [int]$WarmupSeconds = 3,
    [int]$StartupTimeoutSeconds = 90
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $projectRoot "build"
}
$BuildDirectory = (Resolve-Path $BuildDirectory).Path

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $projectRoot "docs\assets\opengl-ibl-showcase-30s.mp4"
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
$outputParent = Split-Path -Parent $OutputPath
if (-not (Test-Path -LiteralPath $outputParent)) {
    New-Item -ItemType Directory -Path $outputParent | Out-Null
}

$exePath = Join-Path $BuildDirectory "Debug\openglStudy.exe"
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "OpenGL executable not found: $exePath"
}

if (Test-Path -LiteralPath $FfmpegPath) {
    $ffmpeg = (Resolve-Path $FfmpegPath).Path
} else {
    $ffmpeg = (Get-Command $FfmpegPath -ErrorAction Stop).Source
}
$ffprobe = Join-Path (Split-Path -Parent $ffmpeg) "ffprobe.exe"
if (-not (Test-Path -LiteralPath $ffprobe)) {
    $ffprobe = (Get-Command "ffprobe" -ErrorAction Stop).Source
}

if (Get-Process -Name "openglStudy" -ErrorAction SilentlyContinue) {
    throw "Close the running openglStudy process before recording."
}

if (-not ("OpenGLCapture.WindowProbe" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace OpenGLCapture {
    public static class WindowProbe {
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT { public int Left, Top, Right, Bottom; }

        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        public static extern IntPtr FindWindow(string className, string windowName);

        [DllImport("user32.dll")]
        public static extern int GetWindowLong(IntPtr hWnd, int index);

        [DllImport("user32.dll")]
        public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

        [DllImport("user32.dll")]
        public static extern int GetSystemMetrics(int index);
    }
}
'@
}

$scenes = @(
    [pscustomobject]@{ Name = "01_chess"; Showcase = 7; Frames = 240 },
    [pscustomobject]@{ Name = "02_flight_helmet"; Showcase = 5; Frames = 210 },
    [pscustomobject]@{ Name = "03_toy_car"; Showcase = 6; Frames = 210 },
    [pscustomobject]@{ Name = "04_material_matrix"; Showcase = 1; Frames = 240 }
)

$tempDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("OpenGL-IBL-Capture-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $tempDirectory | Out-Null
$activeProcess = $null

try {
    foreach ($scene in $scenes) {
        $activeProcess = Start-Process `
            -FilePath $exePath `
            -ArgumentList "--fullscreen", ("--showcase={0}" -f $scene.Showcase) `
            -WorkingDirectory $BuildDirectory `
            -PassThru

        $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
        $fullscreenReady = $false
        while ([DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 500
            $activeProcess.Refresh()
            if ($activeProcess.HasExited) {
                break
            }

            $window = [OpenGLCapture.WindowProbe]::FindWindow("GLFW30", "OpenGL IBL Showcase")
            if ($window -eq [IntPtr]::Zero) {
                continue
            }

            $style = [OpenGLCapture.WindowProbe]::GetWindowLong($window, -16)
            $rect = New-Object OpenGLCapture.WindowProbe+RECT
            [void][OpenGLCapture.WindowProbe]::GetWindowRect($window, [ref]$rect)
            $screenWidth = [OpenGLCapture.WindowProbe]::GetSystemMetrics(0)
            $screenHeight = [OpenGLCapture.WindowProbe]::GetSystemMetrics(1)
            $windowWidth = $rect.Right - $rect.Left
            $windowHeight = $rect.Bottom - $rect.Top
            $hasCaption = ($style -band 0x00C00000) -ne 0

            if (-not $hasCaption -and
                $windowWidth -ge $screenWidth -and
                $windowHeight -ge $screenHeight) {
                $fullscreenReady = $true
                break
            }
        }

        if (-not $fullscreenReady) {
            throw "Showcase $($scene.Showcase) did not reach borderless fullscreen."
        }

        Start-Sleep -Seconds $WarmupSeconds
        $segmentPath = Join-Path $tempDirectory ($scene.Name + ".mp4")
        $captureInput = "ddagrab=output_idx=${OutputIndex}:framerate=30:draw_mouse=0"
        & $ffmpeg `
            -y `
            -hide_banner `
            -loglevel error `
            -f lavfi `
            -i $captureInput `
            -vf "setpts=N/(30*TB)" `
            -frames:v $scene.Frames `
            -c:v h264_nvenc `
            -preset p4 `
            -tune hq `
            -rc vbr `
            -cq 19 `
            -b:v 0 `
            -g 60 `
            -movflags +faststart `
            $segmentPath

        if ($LASTEXITCODE -ne 0) {
            throw "FFmpeg failed while recording $($scene.Name)."
        }

        Stop-Process -Id $activeProcess.Id -Force
        Wait-Process -Id $activeProcess.Id -ErrorAction SilentlyContinue
        $activeProcess = $null
        Write-Host ("Recorded {0} ({1} frames)" -f $scene.Name, $scene.Frames)
    }

    $concatPath = Join-Path $tempDirectory "segments.txt"
    $concatLines = $scenes | ForEach-Object { "file '$($_.Name).mp4'" }
    [System.IO.File]::WriteAllLines($concatPath, $concatLines)

    & $ffmpeg `
        -y `
        -hide_banner `
        -loglevel error `
        -f concat `
        -safe 0 `
        -i $concatPath `
        -c copy `
        -movflags +faststart `
        $OutputPath

    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg failed while concatenating the recorded segments."
    }

    $probe = & $ffprobe `
        -v error `
        -select_streams "v:0" `
        -show_entries "stream=width,height,r_frame_rate,nb_frames,pix_fmt" `
        -show_entries "format=duration" `
        -of json `
        $OutputPath | ConvertFrom-Json

    $stream = $probe.streams[0]
    $valid = $stream.width -eq 3840 -and
        $stream.height -eq 2160 -and
        $stream.r_frame_rate -eq "30/1" -and
        $stream.nb_frames -eq "900" -and
        $stream.pix_fmt -eq "yuv420p" -and
        [math]::Abs([double]$probe.format.duration - 30.0) -lt 0.001
    if (-not $valid) {
        throw "Recorded output failed the 4K/30 FPS/900-frame validation."
    }

    Write-Host "Created $OutputPath"
    Write-Host "Validated 3840x2160, 30 FPS, 900 frames, 30.000 seconds."
} finally {
    if ($null -ne $activeProcess) {
        $activeProcess.Refresh()
        if (-not $activeProcess.HasExited) {
            Stop-Process -Id $activeProcess.Id -Force
        }
    }
    if (Test-Path -LiteralPath $tempDirectory) {
        Remove-Item -LiteralPath $tempDirectory -Recurse -Force
    }
}
