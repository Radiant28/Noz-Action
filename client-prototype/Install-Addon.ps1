$ErrorActionPreference = 'Stop'
$clientRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $PSScriptRoot 'addon\NozdorAction'
$destination = Join-Path $clientRoot 'Interface\AddOns\NozdorAction'
$files = @('NozdorAction.toc', 'Localization.lua', 'NozdorAction.lua', 'Bindings.xml', 'Reticles.tga')
foreach ($file in $files) {
    if (!(Test-Path -LiteralPath (Join-Path $source $file) -PathType Leaf)) {
        throw "Missing addon source: $file"
    }
}
if (Test-Path -LiteralPath $destination) {
    $toc = Join-Path $destination 'NozdorAction.toc'
    if (!(Test-Path -LiteralPath $toc -PathType Leaf) -or
        !(Select-String -LiteralPath $toc -Pattern '^## Title: Nozdor Action\s*$' -Quiet)) {
        throw 'The destination is not a recognized Nozdor Action addon.'
    }
    $backup = Join-Path $PSScriptRoot ('backups\addon-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backup | Out-Null
    foreach ($file in $files) {
        $previous = Join-Path $destination $file
        if (Test-Path -LiteralPath $previous -PathType Leaf) {
            Copy-Item -LiteralPath $previous -Destination $backup
        }
    }
    Write-Output "Previous addon files backed up to $backup"
} else {
    New-Item -ItemType Directory -Path $destination | Out-Null
}
foreach ($file in $files) {
    Copy-Item -LiteralPath (Join-Path $source $file) -Destination $destination -Force
}
Write-Output 'Installed NozdorAction. Start the test client through client-prototype\bin\NozdorActionLauncher.exe.'
