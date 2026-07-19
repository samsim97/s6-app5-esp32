<#
Starts the full GeoForce demo stack (everything except the ESP32 itself):
Mosquitto (if available), Relay, Archive, Control — each in its own window,
plus the frontend in your default browser.

Usage:  .\start-all.ps1
#>

$RepoRoot = $PSScriptRoot

function Start-NodeApp {
    param(
        [string]$Name,
        [string]$Path
    )
    Write-Host "Starting $Name..."
    Start-Process powershell -ArgumentList @(
        "-NoExit", "-Command",
        "cd '$Path'; if (-not (Test-Path node_modules)) { npm install }; `$Host.UI.RawUI.WindowTitle = '$Name'; npm start"
    )
}

# --- Mosquitto ---------------------------------------------------------
$mosquittoService = Get-Service -Name mosquitto -ErrorAction SilentlyContinue
if ($mosquittoService) {
    if ($mosquittoService.Status -ne 'Running') {
        Write-Host "Starting Mosquitto service..."
        Start-Service -Name mosquitto
    } else {
        Write-Host "Mosquitto service already running."
    }
} elseif (Get-Command mosquitto -ErrorAction SilentlyContinue) {
    Write-Host "Starting Mosquitto in its own window..."
    Start-Process powershell -ArgumentList @(
        "-NoExit", "-Command",
        "`$Host.UI.RawUI.WindowTitle = 'Mosquitto'; mosquitto -v"
    )
} else {
    Write-Warning "Mosquitto not found (no service, not on PATH). Install/start it manually on port 1883 before testing, or the chain from Relay to Archive won't work."
}

# --- Node apps -----------------------------------------------------------
Start-NodeApp -Name "Relay"   -Path (Join-Path $RepoRoot "relay")
Start-NodeApp -Name "Archive" -Path (Join-Path $RepoRoot "archive")
Start-NodeApp -Name "Control" -Path (Join-Path $RepoRoot "control")

# --- Frontend --------------------------------------------------------------
Start-Process (Join-Path $RepoRoot "frontend\index.html")

Write-Host ""
Write-Host "All set. Relay :3001, Archive :3003, Control :3002."
Write-Host "Flash and power the ESP32 separately (see README.md / ARCHITECTURE.md)."
