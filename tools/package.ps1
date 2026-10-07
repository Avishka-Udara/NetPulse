param([string]$Iscc)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $projectRoot
$package = Join-Path $projectRoot 'dist/NetPulse'
New-Item -ItemType Directory -Path "$package/licenses" -Force | Out-Null
Copy-Item build/NetPulse.exe,config.ini,README.md,LICENSE,THIRD_PARTY_NOTICES.md,VALIDATION.md,ROADMAP.md -Destination $package -Force
# Preserve runtime notices with every binary distribution.
Copy-Item licenses/*.txt -Destination "$package/licenses" -Force
if (-not (Test-Path "$package/licenses/LLVM.txt") -or -not (Test-Path "$package/licenses/MinGW-runtime.txt")) {
    throw 'Runtime license notices missing from dist/NetPulse/licenses. Supply the matching compiler runtime notices before packaging.'
}
Compress-Archive -Path $package -DestinationPath dist/NetPulse-windows-x64.zip -Force
if ($Iscc) {
    & $Iscc installer/NetPulse.iss
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
}
# Allowlist source files so profiles, credentials, toolchain binaries and databases cannot enter the archive.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceZip = Join-Path $projectRoot 'dist/NetPulse-1.1.1-source.zip'
$archive = [IO.Compression.ZipArchive]::new([IO.File]::Open($sourceZip,[IO.FileMode]::Create),[IO.Compression.ZipArchiveMode]::Create)
try {
    $files = @(Get-Item LICENSE,README.md,THIRD_PARTY_NOTICES.md,CONTRIBUTING.md,ROADMAP.md,RELEASE.md,VALIDATION.md,config.ini,build.bat,.gitignore)
    $files += Get-ChildItem src,tests,installer,.github,licenses -Recurse -File | Where-Object { $_.Extension -in '.c','.h','.py','.ico','.rc','.manifest','.iss','.yml','.txt' }
    $files += Get-Item vendor/sqlite3.c,vendor/sqlite3.h,tools/fetch-sqlite.ps1,tools/build-wsl.sh,tools/package.ps1
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($projectRoot.Length + 1).Replace('\','/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,"NetPulse-1.1.1/$relative",[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
Get-ChildItem dist -File | Where-Object { $_.Name -in 'NetPulse-windows-x64.zip','NetPulse-1.1.1-source.zip','NetPulse-1.1.1-windows-x64-setup.exe' } |
    Get-FileHash -Algorithm SHA256 | ForEach-Object { "$($_.Hash.ToLower())  $([IO.Path]::GetFileName($_.Path))" } | Set-Content dist/SHA256SUMS.txt
