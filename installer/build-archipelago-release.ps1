<#
.SYNOPSIS
Builds the Nebula Archipelago installer (NebulaArchipelago-Setup.exe) from a built game.

.DESCRIPTION
Compiles the launcher and Nebula Settings (in-box C# compiler), builds the disc import tool (Cargo), zips the APWorld,
gathers everything in a stage folder and runs Inno Setup on installer\inno\NebulaArchipelago.iss. Writes the installer and
its SHA-256 (NebulaArchipelago-Setup.exe.sha256) to -OutputDirectory (default: dist\).

  -Version   e.g. 1.0.0
  -Bin       folder with NebulaNative.exe and smg_game.dll (and tables\*.tsv if you have them)

Requires Rust (stable, MSVC target), Visual Studio 2022 with the C++ tools, and Inno Setup 6
(https://jrsoftware.org/isdl.php). The GitHub workflow .github\workflows\release.yml runs this.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Bin,
    [string]$OutputDirectory = '',
    [string]$Repository = 'squidkud/NebulaArchipelago'
)
$ErrorActionPreference = 'Stop'
$installer = Split-Path -Parent $PSCommandPath
$repo = Split-Path -Parent $installer
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'dist' }
$Bin = (Resolve-Path -LiteralPath $Bin).Path
$work = Join-Path ([IO.Path]::GetTempPath()) ('nebula-archipelago-' + [Guid]::NewGuid().ToString('N'))
$stage = Join-Path $work 'stage'
New-Item -ItemType Directory -Force $work, $stage, (Join-Path $stage 'tools'), (Join-Path $stage 'apworld'), $OutputDirectory | Out-Null

function Invoke-Checked([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$File failed with exit code $LASTEXITCODE" }
}

try {
    foreach ($name in 'NebulaNative.exe', 'smg_game.dll') {
        if (-not (Test-Path -LiteralPath (Join-Path $Bin $name))) { throw "The bin folder has no $name" }
    }
    $commit = ''
    try { $commit = (& git -C $repo rev-parse HEAD).Trim() } catch { }

    # The disc import tool, with the C runtime linked in (it runs on PCs without the Visual C++ runtime).
    . (Join-Path $repo 'tools\enter_msvc_environment.ps1')
    $cargo = Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
    if (-not (Test-Path -LiteralPath $cargo)) { $cargo = 'cargo' }
    Push-Location $repo
    $ErrorActionPreference = 'Continue'   # Cargo reports progress on stderr.
    try {
        & $cargo build --release --locked -p nebula-import --bin nebula-import `
            --config "target.x86_64-pc-windows-msvc.rustflags=['-C','target-feature=+crt-static']"
        if ($LASTEXITCODE -ne 0) { throw "cargo failed with exit code $LASTEXITCODE" }
    } finally {
        $ErrorActionPreference = 'Stop'
        Pop-Location
    }
    $targetDir = if ($env:CARGO_TARGET_DIR) { $env:CARGO_TARGET_DIR } else { Join-Path $repo 'target' }
    Copy-Item -LiteralPath (Join-Path $targetDir 'release\nebula-import.exe') -Destination (Join-Path $stage 'tools\nebula-import.exe')

    # The launcher and Nebula Settings (the same program; the second always opens the settings).
    $buildInfo = Join-Path $work 'BuildInfo.cs'
    [IO.File]::WriteAllText($buildInfo, @"
namespace Nebula
{
    internal static class BuildInfo
    {
        public const string Version = "$Version";
        public const string Commit = "$commit";
        public const string Repository = "$Repository";
    }
}
"@, (New-Object Text.UTF8Encoding $false))
    $csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    $common = @(Get-ChildItem (Join-Path $installer 'src\Common\*.cs') | ForEach-Object FullName) + $buildInfo
    $references = @('/r:System.Web.Extensions.dll', '/r:System.IO.Compression.dll', '/r:System.IO.Compression.FileSystem.dll',
        '/r:System.Windows.Forms.dll', '/r:System.Drawing.dll', '/r:System.Core.dll')
    $manifest = Join-Path $installer 'app.manifest'
    $icon = Join-Path $installer 'assets\nebula.ico'
    $launcherSources = @(Get-ChildItem (Join-Path $installer 'src\Launcher\*.cs') | ForEach-Object FullName)
    $common_args = @('/nologo', '/target:winexe', '/platform:x64', '/optimize+', "/win32manifest:$manifest", "/win32icon:$icon")
    Invoke-Checked $csc ($common_args + @("/out:$(Join-Path $stage 'Nebula.exe')") + $references + $common + $launcherSources)
    Invoke-Checked $csc ($common_args + @('/define:NEBULA_SETTINGS_APP', "/out:$(Join-Path $stage 'NebulaSettings.exe')") + $references + $common + $launcherSources)

    # The game and its tables.
    Copy-Item -LiteralPath (Join-Path $Bin 'NebulaNative.exe'), (Join-Path $Bin 'smg_game.dll') -Destination $stage
    foreach ($table in 'material-names.tsv', 'rt-materials.tsv', 'rt-suns.tsv') {
        $path = Join-Path $Bin "tables\$table"
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $stage }
    }
    foreach ($doc in 'LICENSE', 'THIRD-PARTY-NOTICES.md', 'README.md') { Copy-Item -LiteralPath (Join-Path $repo $doc) -Destination $stage }
    Copy-Item -LiteralPath (Join-Path $repo 'docs\ARCHIPELAGO.md') -Destination (Join-Path $stage 'ARCHIPELAGO.md') -ErrorAction SilentlyContinue

    # The APWorld: the folder super_mario_galaxy_pc_port zipped, named .apworld.
    $apworldVersion = (Get-Content -LiteralPath (Join-Path $repo 'apworld\super_mario_galaxy_pc_port\archipelago.json') -Raw | ConvertFrom-Json).world_version
    $apZip = Join-Path $work 'super_mario_galaxy_pc_port.zip'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory((Join-Path $repo 'apworld\super_mario_galaxy_pc_port'), $apZip, [IO.Compression.CompressionLevel]::Optimal, $true)
    $apworld = Join-Path $stage 'apworld\super_mario_galaxy_pc_port.apworld'
    Move-Item -LiteralPath $apZip -Destination $apworld
    Copy-Item -LiteralPath $apworld -Destination (Join-Path $OutputDirectory 'super_mario_galaxy_pc_port.apworld') -Force
    Write-Host "APWorld $apworldVersion"

    # The installer.
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
        Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $iscc) { throw 'Inno Setup 6 is not installed (https://jrsoftware.org/isdl.php).' }
    Invoke-Checked $iscc @("/DAppVersion=$Version", "/DStageDir=$stage", "/O$OutputDirectory", (Join-Path $installer 'inno\NebulaArchipelago.iss'))
    $setup = Join-Path $OutputDirectory 'NebulaArchipelago-Setup.exe'
    $hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$setup.sha256", "$hash  NebulaArchipelago-Setup.exe`n", (New-Object Text.UTF8Encoding $false))
    Write-Host "NebulaArchipelago-Setup.exe $Version ($commit) $hash"
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
