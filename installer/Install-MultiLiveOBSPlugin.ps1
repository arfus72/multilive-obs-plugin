$ErrorActionPreference = "Stop"

$pluginName = "multilive-signature-guard"
$pluginVersion = "1.2.0"
$displayName = "Multi Live OBS Plugin"
$panelBaseUrl = "https://multi-live-pro.com/multilive"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$packageRoot = Join-Path $scriptRoot "payload"
$reportPath = Join-Path $scriptRoot "install-report.txt"

function Write-Step($message) {
    Write-Host "[Multi Live] $message"
}

function Require-Admin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Lance l'installateur en administrateur."
    }
}

function Find-ObsRoot {
    $candidates = @(
        "$env:ProgramFiles\obs-studio",
        "${env:ProgramFiles(x86)}\obs-studio"
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate "bin\64bit\obs64.exe")) {
            return $candidate
        }
    }

    throw "OBS Studio introuvable. Installe OBS Studio 64-bit puis relance cet installateur."
}

function Find-DefenderScanner {
    $candidates = @(
        "$env:ProgramFiles\Windows Defender\MpCmdRun.exe",
        "$env:ProgramData\Microsoft\Windows Defender\Platform\*\MpCmdRun.exe"
    )
    foreach ($candidate in $candidates) {
        $match = Get-ChildItem -Path $candidate -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
        if ($match) { return $match.FullName }
    }
    return $null
}

function Stop-ObsIfRunning {
    $obs = Get-Process obs64 -ErrorAction SilentlyContinue
    if (-not $obs) { return }

    Write-Step "OBS est ouvert. Fermeture demandee..."
    $obs | ForEach-Object { $_.CloseMainWindow() | Out-Null }
    Start-Sleep -Seconds 3
    $obs = Get-Process obs64 -ErrorAction SilentlyContinue
    if ($obs) {
        throw "Ferme OBS Studio puis relance l'installateur."
    }
}

function Copy-Plugin($obsRoot) {
    $sourceDll = Join-Path $packageRoot "obs-plugins\64bit\$pluginName.dll"
    $sourceData = Join-Path $packageRoot "data\obs-plugins\$pluginName"
    $targetDllDir = Join-Path $obsRoot "obs-plugins\64bit"
    $targetDataDir = Join-Path $obsRoot "data\obs-plugins\$pluginName"
    $targetDll = Join-Path $targetDllDir "$pluginName.dll"

    if (-not (Test-Path -LiteralPath $sourceDll)) {
        throw "Fichier plugin manquant: $sourceDll"
    }

    New-Item -ItemType Directory -Force -Path $targetDllDir, $targetDataDir | Out-Null
    Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
    if (Test-Path -LiteralPath $sourceData) {
        Copy-Item -LiteralPath (Join-Path $sourceData "locale") -Destination $targetDataDir -Recurse -Force
    }

    return $targetDll
}

function Get-ObsUserIniPath {
    if (-not $env:APPDATA) { return $null }
    $path = Join-Path $env:APPDATA "obs-studio\user.ini"
    if (Test-Path -LiteralPath $path) { return $path }
    return $null
}

function Get-QueryValue($text, $key) {
    $escapedKey = [Regex]::Escape($key)
    $match = [Regex]::Match($text, "(?i)$escapedKey=([^&`"'\],}]+)")
    if ($match.Success) { return $match.Groups[1].Value }
    return ""
}

function Get-StreamerSlugFromText($text) {
    $match = [Regex]::Match($text, "/multilive/([^/`"'\],}]+)")
    if ($match.Success) { return $match.Groups[1].Value }
    return "arfustv"
}

function Repair-MultiLiveDocks {
    $iniPath = Get-ObsUserIniPath
    if (-not $iniPath) {
        Write-Step "Fichier OBS user.ini introuvable, les docks seront repares au prochain demarrage OBS."
        return
    }

    $raw = Get-Content -LiteralPath $iniPath -Raw -ErrorAction SilentlyContinue
    if (-not $raw) { $raw = "" }

    $token = Get-QueryValue $raw "widgetToken"
    if (-not $token) { $token = Get-QueryValue $raw "widgettoken" }
    if (-not $token) { $token = Get-QueryValue $raw "token" }

    if (-not $token) {
        Write-Step "Aucun token dock trouve. Ouvre une fois le panel dans OBS, puis relance l'installateur si besoin."
        return
    }

    $slug = Get-StreamerSlugFromText $raw
$versionParam = [Uri]::EscapeDataString($pluginVersion)
$liveUrl = "$panelBaseUrl/$slug/live-dock/?dock=1&pluginVersion=$versionParam&widgetToken=$token"
$chatUrl = "$panelBaseUrl/$slug/chat/?dock=1&pluginVersion=$versionParam&widgetToken=$token"
    $docks = @(
        @{ title = "Multi Live Dock Live"; url = $liveUrl; uuid = "multilive_live_dock" },
        @{ title = "Multi Live Chat"; url = $chatUrl; uuid = "multilive_chat_dock" }
    ) | ConvertTo-Json -Compress
    $line = "ExtraBrowserDocks=$docks"

    if ($raw -match "(?ms)^\[BasicWindow\].*?^ExtraBrowserDocks=$([Regex]::Escape($docks))$") {
        Write-Step "Docks OBS deja a jour."
        return
    }

    $backup = "$iniPath.multilive.bak"
    Copy-Item -LiteralPath $iniPath -Destination $backup -Force

    if ($raw -match "(?m)^ExtraBrowserDocks=.*$") {
        $raw = [Regex]::Replace($raw, "(?m)^ExtraBrowserDocks=.*$", [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $line }, 1)
    } elseif ($raw -match "(?m)^\[BasicWindow\]\s*$") {
        $raw = [Regex]::Replace($raw, "(?m)^\[BasicWindow\]\s*$", "[BasicWindow]`r`n$line", 1)
    } else {
        $raw = "$raw`r`n[BasicWindow]`r`n$line`r`n"
    }

    Set-Content -LiteralPath $iniPath -Value $raw -Encoding UTF8
    Write-Step "Docks OBS repares vers multi-live-pro.com."
}

function Write-InstallReport($obsRoot, $installedDll, $scanText) {
    $files = Get-ChildItem -LiteralPath $packageRoot -Recurse -File
    $lines = @()
    $lines += "$displayName - rapport installation"
    $lines += "Version: $pluginVersion"
    $lines += "Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    $lines += "OBS: $obsRoot"
    $lines += "DLL installee: $installedDll"
    $lines += ""
    $lines += "SHA256:"
    foreach ($file in $files) {
        $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
        $relative = $file.FullName.Substring($packageRoot.Length).TrimStart("\")
        $lines += "$($hash.Hash)  $relative"
    }
    $lines += ""
    $lines += "Scan Microsoft Defender:"
    $lines += $scanText
    $lines | Set-Content -LiteralPath $reportPath -Encoding UTF8
}

Require-Admin
Write-Step "Verification du paquet..."

$obsRoot = Find-ObsRoot
Stop-ObsIfRunning

$scanner = Find-DefenderScanner
$scanText = "Microsoft Defender non trouve sur cette machine."
if ($scanner) {
    Write-Step "Scan Microsoft Defender du paquet..."
    $scanOutput = & $scanner -Scan -ScanType 3 -File $packageRoot 2>&1
    $scanText = ($scanOutput | Out-String).Trim()
}

Write-Step "Installation dans OBS Studio..."
$installedDll = Copy-Plugin $obsRoot
Repair-MultiLiveDocks
Write-InstallReport $obsRoot $installedDll $scanText

Write-Step "Installation terminee. Version $pluginVersion."
Write-Host ""
Write-Host "Redemarre OBS Studio."
Write-Host "Rapport: $reportPath"
Write-Host ""
Read-Host "Appuie sur Entree pour fermer"
