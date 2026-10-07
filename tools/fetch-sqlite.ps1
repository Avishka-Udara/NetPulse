$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$url = 'https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip'
$zip = Join-Path $root 'vendor/sqlite.zip'
Invoke-WebRequest -Uri $url -OutFile $zip
if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne '1E71DDF93849C6A6ECF58B827C0692073D2DD7EE40196158068F7B29F422E87D') {
    throw 'SQLite archive checksum mismatch.'
}
Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $root 'vendor') -Force
Copy-Item -LiteralPath (Join-Path $root 'vendor/sqlite-amalgamation-3530400/sqlite3.c') -Destination (Join-Path $root 'vendor/sqlite3.c')
Copy-Item -LiteralPath (Join-Path $root 'vendor/sqlite-amalgamation-3530400/sqlite3.h') -Destination (Join-Path $root 'vendor/sqlite3.h')
Write-Output 'SQLite 3.53.4 source installed.'
