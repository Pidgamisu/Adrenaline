<#
    Pushes the packaged Shipping build to itch.io with butler.

    What it does, in order: checks the build looks like a real staged build, copies .itch.toml into
    the root of it so the itch app knows which exe to launch, then runs `butler push`.

    Run it from anywhere. The only argument you must supply is the itch.io target, which is your
    account name and the project's URL slug joined with a slash:

        .\Tools\itch\push.ps1 -Target yourname/adrenaline

    The first push sends the whole 609 MB. Every push after that sends only what changed, which is
    the whole reason for using butler instead of the browser.

    See docs/itch.md.
#>

param(
    # Your itch.io account name and the project slug, e.g. "nathan/adrenaline". The slug is the last
    # part of the project's URL, and it is fixed when you create the project.
    [Parameter(Mandatory = $true)]
    [string]$Target,

    # butler tags the upload by platform from the channel name: anything containing "win" is treated
    # as a Windows build, which is what makes the itch app offer it to Windows users.
    [string]$Channel = "windows",

    # The staged Shipping build. This is the -archivedirectory from the packaging command plus
    # \Windows.
    [string]$BuildDir = "C:\Users\natha\Desktop\AdrenalineShipping\Windows",

    # butler lives in Downloads until it is put somewhere permanent and added to PATH.
    [string]$Butler = "C:\Users\natha\Downloads\butler-windows-amd64\butler.exe",

    # Print what would happen and stop, without uploading anything.
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Butler)) {
    throw "butler not found at $Butler. Pass -Butler with the right path, or put butler.exe on PATH."
}

if (-not (Test-Path $BuildDir)) {
    throw "No build at $BuildDir. Package it first - see docs/itch.md."
}

# A staged build has the launcher stub at its root. If that is missing, something was pointed at the
# wrong folder, and pushing it would publish junk.
$exe = Join-Path $BuildDir "Getaway.exe"
if (-not (Test-Path $exe)) {
    throw "No Getaway.exe at the root of $BuildDir. That does not look like a staged build."
}

# Guard against pushing a Development build by accident: the Shipping binary has Shipping in its
# name, and a public upload should be the Shipping one.
$shippingBinary = Join-Path $BuildDir "Getaway\Binaries\Win64\Getaway-Win64-Shipping.exe"
if (-not (Test-Path $shippingBinary)) {
    Write-Warning "No Getaway-Win64-Shipping.exe found - this looks like a Development build, not Shipping."
    Write-Warning "Development builds are bigger and keep the ~ console. Continue only if that is deliberate."
}

$toml = Join-Path $PSScriptRoot ".itch.toml"
if (-not (Test-Path $toml)) {
    throw "Missing $toml - it should sit next to this script."
}
Copy-Item $toml (Join-Path $BuildDir ".itch.toml") -Force
Write-Output "Copied .itch.toml into the build root."

$sizeMb = [math]::Round(((Get-ChildItem $BuildDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB))
$fileCount = (Get-ChildItem $BuildDir -Recurse -File | Measure-Object).Count
Write-Output ("Build: {0}  ({1} files, {2} MB)" -f $BuildDir, $fileCount, $sizeMb)
Write-Output ("Target: {0}:{1}" -f $Target, $Channel)

if ($DryRun) {
    Write-Output "-DryRun given, so stopping here. Nothing was uploaded."
    return
}

Write-Output "Pushing..."
& $Butler push $BuildDir ("{0}:{1}" -f $Target, $Channel)
if ($LASTEXITCODE -ne 0) {
    throw "butler push failed with exit code $LASTEXITCODE."
}

Write-Output ""
Write-Output "Pushed. Check the build processed cleanly with:"
Write-Output ("    `"{0}`" status {1}" -f $Butler, $Target)
