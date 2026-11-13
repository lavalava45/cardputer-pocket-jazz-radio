param(
    [switch]$InstallDependencies,
    [switch]$PackageOnly,
    [switch]$AllowUnverifiedPackageOnly,
    [switch]$PackageUnverifiedPManSize, # Deprecated compatibility switch: no longer a size gate.
    # Data-only update: reuse an immutable app-only BIN and byte-compare ALL
    # compiled sketch sources to that archived release. No Arduino compile.
    [ValidatePattern('^[0-9]+\.[0-9]+(?:\.[0-9]+|[a-z][0-9]*)?$')]
    [string]$ReuseVerifiedAppFromRelease = '',
    [ValidatePattern('^[0-9]+\.[0-9]+(?:\.[0-9]+|[a-z][0-9]*)?$')]
    [string]$Version = '0.1.1'
)
$ErrorActionPreference = 'Stop'
function Get-Sha256Hex([string]$Path) {
    # Windows PowerShell editions/installations do not always expose Get-FileHash.
    # Use the framework's SHA256 implementation without any external dependency.
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $stream = [IO.File]::OpenRead($Path)
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
        finally { $stream.Dispose() }
    }
    finally { $sha.Dispose() }
}
function Invoke-CapturedProcess([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory) {
    # PowerShell 5.1 Start-Process -Wait can wait forever for MSVC's resident
    # telemetry descendants. Without -Wait its Process.ExitCode is sometimes
    # null even after WaitForExit(). Own the .NET process and both redirected
    # pipes explicitly; the exit code is taken only from this direct child.
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Executable
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' ')
    $info.WorkingDirectory = $WorkingDirectory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    try {
        $process.StartInfo = $info
        if (-not $process.Start()) { throw "Failed to start $Executable" }
        # Read streams concurrently to avoid blocking on a full OS pipe.
        $outTask = $process.StandardOutput.ReadToEndAsync()
        $errTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        [pscustomobject]@{
            ExitCode = $process.ExitCode
            Output = $outTask.GetAwaiter().GetResult() + [Environment]::NewLine +
                $errTask.GetAwaiter().GetResult()
        }
    }
    finally { $process.Dispose() }
}
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cfg = Join-Path $root 'arduino-cli.yaml'
$sketch = Join-Path $root 'PocketJazzRadio'
$out = Join-Path $root 'dist\build'
$releaseRoot = Join-Path $root 'dist\releases'
$release = Join-Path $releaseRoot $Version
$lib = Join-Path $root 'arduino-sketchbook\libraries'
$appOnly = Join-Path $out 'PocketJazzRadio.ino.bin'
$fileName = "PocketJazzRadio-v$Version-app-only.bin"
$zipName = "PocketJazzRadio-v$Version-Launcher3-SD.zip"
$fqbn = 'm5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,CDCOnBoot=cdc,USBMode=hwcdc,PSRAM=disabled'
# Verify an immutable SDK source ZIP; never compile a mutable sibling SDK
# directory and then package an unrelated or missing cross-project header.
$sdkPinnedVersion = '0.8e-host-only'
$sdkPinnedZipSha = '867bc067d73900cfcde1f41e8a0d97420bd1782bdc774a076bb822afbe7c6020'
$sdkPinnedZip = Join-Path $root ('..\..\audio-router-sdk\dist\releases\' + $sdkPinnedVersion + '\audio-router-sdk-' + $sdkPinnedVersion + '-source.zip')
$sdkPinnedHeaderNames = @('AudioRouter.h', 'CardputerPcmMirror.h')
$sdkPinnedHeaderHashes = @{}
$sdkPinnedLicenseHash = '1f05a3681ad4de9d18e0970d07dfeb804d25141825341cf4ef9f983d7737c95e'
$sdkPinnedInclude = Join-Path $out ('pinned-sdk-' + $sdkPinnedVersion + '\include')

# Plain PackageOnly is unverified. The independent verified-reuse path requires
# an existing immutable released BIN, a matching release manifest and exact
# source-byte equality with the previously compiled sketch.
if ($ReuseVerifiedAppFromRelease -and -not $PackageOnly) {
    throw '-ReuseVerifiedAppFromRelease is for -PackageOnly data-only releases.'
}
if ($ReuseVerifiedAppFromRelease -and $AllowUnverifiedPackageOnly) {
    throw 'Verified reuse and unverified PackageOnly are mutually exclusive.'
}
if ($PackageOnly -and -not $AllowUnverifiedPackageOnly -and -not $ReuseVerifiedAppFromRelease) {
    throw 'PackageOnly lacks binary/source provenance; add -AllowUnverifiedPackageOnly OR -ReuseVerifiedAppFromRelease.'
}
if ($AllowUnverifiedPackageOnly -and -not $PackageOnly) {
    throw '-AllowUnverifiedPackageOnly is accepted only with -PackageOnly.'
}
$verifiedReuse = [bool]$ReuseVerifiedAppFromRelease
$reusedBinSha = ''
$provenanceStatus = if ($verifiedReuse) {
    'VERIFIED REUSE: app-only BIN and every compiled sketch input match immutable v' + $ReuseVerifiedAppFromRelease + '; only SD/data/docs changed. Arduino compile NOT RE-RUN.'
} elseif ($PackageOnly) {
    'UNVERIFIED BUILD SOURCE: package-only reused a preexisting app BIN; do not assume the archived source produced it.'
} else {
    'Verified unchanged local sketch inputs across Arduino compilation; library properties and selected dependency files recorded.'
}

# The source-visible firmware version comes from the same -Version as the
# immutable release name, NEVER from a separately maintained handwritten
# string. In PackageOnly/verified data-only reuse, preserve the existing
# firmware version because those modes do not compile a new app.
if (-not $PackageOnly) {
    if (Test-Path -LiteralPath $release) {
        throw "Release v$Version already exists and is immutable: $release."
    }
    $buildVersionPath = Join-Path $sketch 'BuildVersion.h'
    $expectedVersionHeader = "// Generated/checked by tools/build.ps1 before each full firmware compile.`n" +
        "// Data-only releases retain their exact previous app firmware version.`n" +
        "#pragma once`n" +
        ('#define POCKETJAZZ_FIRMWARE_VERSION "v' + $Version + '"') + "`n"
    if (-not (Test-Path -LiteralPath $buildVersionPath -PathType Leaf) -or
        [IO.File]::ReadAllText($buildVersionPath) -ne $expectedVersionHeader) {
        [IO.File]::WriteAllText($buildVersionPath, $expectedVersionHeader,
                                [Text.UTF8Encoding]::new($false))
    }
}

# Establish source provenance *before* starting the potentially long compile.
# A concurrent editor changing a sketch source during this build must not yield
# a release whose saved sources differ from the compiled firmware.
$compileInputs = @()
$initialInputHashes = @{}
$arduinoReport = 'Arduino ESP32-S3: NOT COMPILED (PackageOnly; unverified provenance).'
if ($verifiedReuse) {
    if ($ReuseVerifiedAppFromRelease -eq $Version) {
        throw 'Verified reuse must point to a DIFFERENT already published source version.'
    }
    $previous = Join-Path $releaseRoot $ReuseVerifiedAppFromRelease
    $previousName = 'PocketJazzRadio-v' + $ReuseVerifiedAppFromRelease + '-app-only.bin'
    $previousBin = Join-Path $previous ('firmware\' + $previousName)
    $previousManifest = Join-Path $previous 'SHA256-MANIFEST.txt'
    $previousSource = Join-Path $previous 'source'
    $previousInputReport = Join-Path $previous 'BUILD-INPUTS.txt'
    $previousArduinoReport = Join-Path $previous 'ARDUINO-COMPILE-REPORT.txt'
    if (-not (Test-Path -LiteralPath $previousBin -PathType Leaf) -or
        -not (Test-Path -LiteralPath $previousManifest -PathType Leaf) -or
        -not (Test-Path -LiteralPath $previousInputReport -PathType Leaf) -or
        -not (Test-Path -LiteralPath $previousArduinoReport -PathType Leaf) -or
        -not (Test-Path -LiteralPath $appOnly -PathType Leaf)) {
        throw 'Verified reuse requires the complete immutable previous release and existing dist/build BIN.'
    }
    $previousManifestText = [IO.File]::ReadAllText($previousManifest)
    $expectedLine = 'firmware/' + $previousName + ' SHA256: '
    $recorded = @($previousManifestText -split '\r?\n' | Where-Object { $_.StartsWith($expectedLine) })
    if ($recorded.Count -ne 1 -or $recorded[0].Substring($expectedLine.Length) -notmatch '^[0-9a-f]{64}$') {
        throw 'Previous release manifest does not contain one valid app SHA256 entry.'
    }
    $reusedBinSha = $recorded[0].Substring($expectedLine.Length)
    if ((Get-Sha256Hex $previousBin) -ne $reusedBinSha -or
        (Get-Sha256Hex $appOnly) -ne $reusedBinSha -or
        -not ([IO.File]::ReadAllText($previousInputReport).Contains('Verified unchanged local sketch inputs')) -or
        -not ([IO.File]::ReadAllText($previousArduinoReport).Contains('App-only Cardputer ADV: PASS'))) {
        throw 'Previous app BIN differs from declared SHA or lacks verified Arduino source-bound build.'
    }
    $compiledSketchFiles = @(Get-ChildItem -LiteralPath $sketch -File |
        Where-Object { $_.Extension -in '.cpp', '.h', '.ino' } | Sort-Object Name)
    if ($compiledSketchFiles.Count -lt 3) { throw 'Missing compiled sketch files.' }
    $previousSketchFiles = @(Get-ChildItem -LiteralPath (Join-Path $previousSource 'PocketJazzRadio') -File |
        Where-Object { $_.Extension -in '.cpp', '.h', '.ino' })
    if ($previousSketchFiles.Count -ne $compiledSketchFiles.Count) {
        throw 'Compiled sketch file inventory differs from immutable prior release.'
    }
    $compileInputs = @($compiledSketchFiles | Select-Object -ExpandProperty FullName) + @($cfg)
    foreach ($inputPath in $compileInputs) {
        $relative = $inputPath.Substring($root.Length + 1)
        $archived = Join-Path $previousSource $relative
        if (-not (Test-Path -LiteralPath $archived -PathType Leaf) -or
            (Get-Sha256Hex $inputPath) -ne (Get-Sha256Hex $archived)) {
            throw "Compiled input changed since release v$ReuseVerifiedAppFromRelease : $relative"
        }
        $initialInputHashes[$inputPath] = Get-Sha256Hex $inputPath
    }
    $arduinoReport = 'Arduino ESP32-S3: NOT RECOMPILED for data-only update.' +
        [Environment]::NewLine + "Verified exact app SHA256 $reusedBinSha from immutable v$ReuseVerifiedAppFromRelease." +
        [Environment]::NewLine + 'All sketch .cpp/.h/.ino + arduino-cli.yaml match the original validated source snapshot.' +
        [Environment]::NewLine + "Original Arduino compile evidence: dist/releases/$ReuseVerifiedAppFromRelease/ARDUINO-COMPILE-REPORT.txt"
}
if (-not $PackageOnly) {
    $compileInputs = @(Get-ChildItem -LiteralPath $sketch -File |
        Where-Object { $_.Extension -in '.cpp', '.h', '.ino' } |
        Sort-Object FullName | Select-Object -ExpandProperty FullName)
    $compileInputs += $cfg
    # Opt-in host-only transports are compile-checked separately before a
    # release, and their complete source dependency set is protected too.
    foreach ($probeInput in @(
        'tests/ha_mqtt_compile/ha_mqtt_compile.ino',
        'PocketJazzRadio/HomeAssistantBridge.h',
        'PocketJazzRadio/HaMqttTransport.h',
        'PocketJazzRadio/HaMqttTransport.cpp',
        'tests/espnow_hmac_compile/espnow_hmac_compile.ino',
        'tests/pcm_mirror_app_compile/pcm_mirror_app_compile.ino',
        'experiments/StationExchangeHmac.h',
        'experiments/StationExchangeSecure.h',
        'experiments/StationExchangePacket.h'
    )) {
        $absolute = Join-Path $root $probeInput
        if (Test-Path -LiteralPath $absolute -PathType Leaf) { $compileInputs += $absolute }
    }
    # The app now carries small, locally patched ESP8266Audio stream/MP3
    # sources. Protect their exact bytes *before* Arduino compilation, not
    # merely while copying the dependency snapshot afterwards: otherwise
    # a concurrent edit could make the archived decoder differ from the BIN.
    foreach ($vendorInput in @(
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceBuffer.cpp',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceBuffer.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioSourcePullPolicy.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioShortageEpisode.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceHTTPStream.cpp',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceHTTPStream.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.cpp',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioFileSourceICYStream.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioGeneratorMP3.cpp',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioGeneratorMP3.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioLiveStallPolicy.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioLiveDecodeBudget.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioMp3SyncBoundary.h',
        'arduino-sketchbook/libraries/ESP8266Audio/src/AudioICYDeadline.h'
    )) {
        $absolute = Join-Path $root $vendorInput
        if (-not (Test-Path -LiteralPath $absolute -PathType Leaf)) {
            throw "Required vendored audio source missing: $vendorInput"
        }
        $compileInputs += $absolute
    }
    foreach ($inputPath in $compileInputs) {
        $initialInputHashes[$inputPath] = Get-Sha256Hex $inputPath
    }
}

# Reject an occupied release before any compilation or filesystem changes.
if (Test-Path -LiteralPath $release) {
    throw "Release v$Version already exists and is immutable: $release. Choose another -Version."
}
if (-not $PackageOnly) {
    $cli = (Resolve-Path (Join-Path $root '..\..\..\tools\arduino-cli\arduino-cli.exe')).Path
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    if (-not (Test-Path -LiteralPath $sdkPinnedZip -PathType Leaf) -or
        (Get-Sha256Hex $sdkPinnedZip) -ne $sdkPinnedZipSha) {
        throw "Immutable SDK $sdkPinnedVersion archive missing or not matching pinned SHA; refusing PCM probe."
    }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $sdkArchive = [IO.Compression.ZipFile]::OpenRead($sdkPinnedZip)
    try {
        foreach ($sdkHeader in $sdkPinnedHeaderNames) {
            $entryPath = 'source/include/cardputer_audio/' + $sdkHeader
            $entries = @($sdkArchive.Entries | Where-Object { $_.FullName -ceq $entryPath })
            if ($entries.Count -ne 1 -or $entries[0].Length -lt 100) {
                throw "Required SDK header missing or duplicated in pinned archive: $entryPath"
            }
            $target = Join-Path $sdkPinnedInclude ('cardputer_audio\' + $sdkHeader)
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            $source = $entries[0].Open()
            try {
                $destinationStream = [IO.File]::Create($target)
                try { $source.CopyTo($destinationStream) }
                finally { $destinationStream.Dispose() }
            } finally { $source.Dispose() }
            $sdkPinnedHeaderHashes[$sdkHeader] = Get-Sha256Hex $target
        }
        $sdkLicenseEntries = @($sdkArchive.Entries | Where-Object { $_.FullName -ceq 'source/LICENSE' })
        if ($sdkLicenseEntries.Count -ne 1) { throw 'Pinned SDK MIT LICENSE absent or duplicated.' }
        $sdkLicenseTarget = Join-Path $out ('pinned-sdk-' + $sdkPinnedVersion + '\LICENSE')
        $sdkLicenseStream = $sdkLicenseEntries[0].Open()
        try {
            $sdkLicenseOutput = [IO.File]::Create($sdkLicenseTarget)
            try { $sdkLicenseStream.CopyTo($sdkLicenseOutput) }
            finally { $sdkLicenseOutput.Dispose() }
        } finally { $sdkLicenseStream.Dispose() }
        if ((Get-Sha256Hex $sdkLicenseTarget) -ne $sdkPinnedLicenseHash) {
            throw 'Pinned SDK MIT LICENSE source bytes mismatch.'
        }
    } finally { $sdkArchive.Dispose() }
    if ((Get-Sha256Hex $sdkPinnedZip) -ne $sdkPinnedZipSha) {
        throw 'Immutable SDK ZIP changed while extracting pinned PCM headers.'
    }
    if ($InstallDependencies) {
        & $cli --config-file $cfg core update-index
        if ($LASTEXITCODE -ne 0) { throw 'Failed to update board index' }
        & $cli --config-file $cfg core install 'm5stack:esp32@3.3.9'
        if ($LASTEXITCODE -ne 0) { throw 'Failed to install M5Stack core' }
        & $cli --config-file $cfg lib install 'M5Cardputer@1.1.1' 'M5Unified@0.2.25' 'M5GFX@0.2.32'
        if ($LASTEXITCODE -ne 0) { throw 'Failed to install M5 libraries' }
    }
    $mainArduinoOutput = (& $cli --config-file $cfg compile --fqbn $fqbn --libraries $lib --output-dir $out $sketch 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Arduino application build failed: $mainArduinoOutput" }
    Write-Host $mainArduinoOutput
    $arduinoReport = "App-only Cardputer ADV: PASS`r`n$mainArduinoOutput"
    foreach ($probe in @(
        @{ Name = 'ESP-NOW HMAC, mbedTLS'; Folder = 'tests/espnow_hmac_compile'; Required = 'experiments/StationExchangeHmac.h'; Flags = '' },
        @{ Name = 'HA MQTT, opt-in TLS'; Folder = 'tests/ha_mqtt_compile'; Required = 'PocketJazzRadio/HaMqttTransport.h'; Flags = '-DPOCKETJAZZ_ENABLE_HA_MQTT_TRANSPORT' },
        @{ Name = 'Non-owning PCM mirror with actual legacy speaker, SDK v0.8e'; Folder = 'tests/pcm_mirror_app_compile'; Required = 'tests/pcm_mirror_app_compile/pcm_mirror_app_compile.ino'; Flags = ('-I' + $sdkPinnedInclude) }
    )) {
        $probeFolder = Join-Path $root $probe.Folder
        $required = Join-Path $root $probe.Required
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { continue }
        if (-not (Test-Path -LiteralPath $probeFolder -PathType Container)) {
            throw "Required standalone Arduino probe absent: $($probe.Folder)"
        }
        $probeOut = Join-Path $out ('probe-' + ($probe.Folder.Split('/')[-1]))
        if ($probe.Flags) {
            $probeOutput = (& $cli --config-file $cfg compile --fqbn $fqbn --libraries $lib --build-property ('compiler.cpp.extra_flags=' + $probe.Flags) --output-dir $probeOut $probeFolder 2>&1 | Out-String)
        } else {
            $probeOutput = (& $cli --config-file $cfg compile --fqbn $fqbn --libraries $lib --output-dir $probeOut $probeFolder 2>&1 | Out-String)
        }
        if ($LASTEXITCODE -ne 0) { throw "$($probe.Name) compile-only gate failed: $probeOutput" }
        $arduinoReport += "`r`n$($probe.Name): COMPILE-ONLY PASS`r`n$probeOutput"
    }
    foreach ($inputPath in $compileInputs) {
        if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf) -or
            (Get-Sha256Hex $inputPath) -ne $initialInputHashes[$inputPath]) {
            throw "Sketch input changed while compiling, refusing mismatched source snapshot: $inputPath"
        }
    }
    foreach ($sdkHeader in $sdkPinnedHeaderNames) {
        $actual = Join-Path $sdkPinnedInclude ('cardputer_audio\' + $sdkHeader)
        if ((Get-Sha256Hex $actual) -ne $sdkPinnedHeaderHashes[$sdkHeader]) {
            throw "Pinned SDK header changed during Arduino compilation: $sdkHeader"
        }
    }
    if ((Get-Sha256Hex (Join-Path $out ('pinned-sdk-' + $sdkPinnedVersion + '\LICENSE'))) -ne
        $sdkPinnedLicenseHash) { throw 'Pinned SDK MIT LICENSE changed during compilation.' }
    if ((Get-Sha256Hex $sdkPinnedZip) -ne $sdkPinnedZipSha) {
        throw 'Pinned SDK source archive changed during compilation.'
    }
}
if (-not (Test-Path -LiteralPath $appOnly -PathType Leaf)) { throw 'Missing app-only binary; build output is incomplete.' }
$bytes = [IO.File]::ReadAllBytes($appOnly)
if ($bytes.Length -lt 512 -or $bytes[0] -ne 0xE9) { throw 'Invalid ESP application header (expected 0xE9).' }
$hardMaxAppBytes = 0x400000
if ($bytes.Length -gt $hardMaxAppBytes) {
    throw "Invalid/oversized app image: $($bytes.Length) exceeds hard sanity cap $hardMaxAppBytes bytes."
}
# The former 0x150000 conservative threshold was a project packaging policy,
# not a measured hardware limit. Arduino compile checks its selected board
# layout; this packager only enforces a broad malformed-image sanity cap.
# The deprecated flag is accepted to keep older automation invocations valid.
$installClearance = 'APP-ONLY IMAGE: use M5Launcher 3; cancel overwrite/repartition prompts. Physical audio not tested.'
if ($PackageOnly -and -not $verifiedReuse) {
    $installClearance = 'NOT CLEARED FOR INSTALL: PackageOnly binary/source provenance unverified; recompile trusted source before installing.'
}

New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
$staging = Join-Path $releaseRoot ('.staging-' + [guid]::NewGuid().ToString('N'))
$published = $false
try {
    $firmware = Join-Path $staging 'firmware'
    $playlists = Join-Path $staging 'PocketJazz'
    New-Item -ItemType Directory -Force -Path $firmware, $playlists | Out-Null
    Copy-Item -LiteralPath $appOnly -Destination (Join-Path $firmware $fileName)
    $playlistFiles = @(Get-ChildItem -LiteralPath (Join-Path $root 'playlists') -File | Where-Object { $_.Extension -in '.m3u', '.pls' } | Sort-Object Name)
    if ($playlistFiles.Count -eq 0) { throw 'No M3U/PLS playlist files found; refusing incomplete SD archive.' }
    foreach ($playlist in $playlistFiles) { Copy-Item -LiteralPath $playlist.FullName -Destination $playlists }
    # Optional offline Jazz Notes assets, not executable firmware.
    $notesDir = Join-Path $root 'data'
    if (Test-Path -LiteralPath $notesDir -PathType Container) {
        $notesFiles = @(Get-ChildItem -LiteralPath $notesDir -File |
            Where-Object { $_.Name -in 'artist-notes.tsv', 'artist-notes.idx',
                'jazz-encyclopedia.tsv', 'jazz-facts.tsv' })
        foreach ($note in $notesFiles) {
            Copy-Item -LiteralPath $note.FullName -Destination $playlists
        }
        $clockExample = Join-Path $notesDir 'clock.example.txt'
        if (Test-Path -LiteralPath $clockExample -PathType Leaf) {
            Copy-Item -LiteralPath $clockExample -Destination $playlists
        }
    }

    # Preserve packaging-time source bytes independently
    # of the installable SD archive. Do not include generated build intermediates.
    $sourceDir = Join-Path $staging 'source'
    $sourcePaths = @(
        'LICENSE',
        'COPYRIGHT-NOTICES.md',
        'README.md',
        'ROADMAP.md',
        'docs/KEYS.md',
        'docs/CHANGELOG.md',
        'docs/build-release-notes.md',
        'tools/build.ps1'
    )
    $requiredLicense = Join-Path $root 'LICENSE'
    if (-not (Test-Path -LiteralPath $requiredLicense -PathType Leaf) -or
        (Get-Item -LiteralPath $requiredLicense).Length -lt 30000 -or
        ((Get-Content -LiteralPath $requiredLicense -TotalCount 3 | Out-String) -notmatch 'GNU GENERAL PUBLIC LICENSE')) {
        throw 'Required full original GPL-3 license text missing or incomplete.'
    }
    foreach ($optional in @('arduino-cli.yaml', 'docs/ROOT-CAUSE-CODEC-HEAP-20261112.md', 'docs/DIAGNOSTICS-GUIDE.md', 'docs/RELEASE-V07Z5-VERIFICATION.md', 'docs/EMULATOR-VALIDATION-20261112.md', 'docs/RELEASE-V07Z4-VERIFICATION.md', 'docs/WIFI-CREDENTIAL-SECURITY.md', 'docs/RELEASE-V07Z3-VERIFICATION.md', 'docs/LIVE-AUDIO-DIAGNOSTICS-20261112.md', 'docs/AUDIO-ACCEPTANCE-V07Z2.md', 'docs/RELEASE-V07Z1-VERIFICATION.md', 'docs/RELEASE-V07Z-VERIFICATION.md', 'docs/RELEASE-V07Y-VERIFICATION.md', 'docs/HTTPS-TRACE-V07X.md', 'docs/DAILY-RADIO-ALARM.md', 'docs/SD-OFFLINE-LIBRARY.md', 'docs/CURRENT-FEATURES-20261110.md', 'docs/HARDWARE-OBSERVATIONS-20261110.md', 'docs/SETTINGS-BACKUP-AND-WIFI-PROFILES.md', 'docs/AUDIO-FIVE-STATION-PCM-ACCEPTANCE.md', 'docs/V07C-RELEASE-GATE.md', 'docs/V07D-RELEASE-GATE.md', 'docs/AUDIO-ROUTER-PCM-INTEGRATION.md', 'docs/STATION-STREAM-VERIFICATION.md', 'docs/STATION-CATALOG-56-20261111.md', 'docs/STATION-STREAM-CHECK-20261110.json', 'docs/STATION-THROUGHPUT-20261110.json', 'docs/STATION-THROUGHPUT-SOMAFM-20261110.json', 'docs/AUDIO-SOMAFM-CLASS-20261110.md', 'docs/V07L-AUDIO-FIX-REVIEW.md', 'docs/V07M-DECODER-EDGE-CASES.md', 'docs/VELXIO-V07L-RUNTIME.md', 'docs/VELXIO-V07M-RUNTIME.md', 'docs/LIKES-QR.md', 'docs/LIKES-PC-BRIDGE.md', 'docs/PMAN-READONLY-V07L.md', 'docs/PMAN-READONLY-V07K.md', 'docs/AUDIO-FOUR-STATION-DIAGNOSTIC.md', 'docs/jazz-notes-data.md', 'docs/JAZZ-ENCYCLOPEDIA.md', 'docs/SD-TIMESHIFT-FEASIBILITY.md', 'docs/JAZZ-NOTES-SEARCH-TECHNICAL.md', 'docs/ICY-PEEK.md', 'docs/ESP-NOW-STATION.md', 'docs/CLOCK-SETUP.md', 'docs/STATION-LOGOS.md', 'docs/SD-EXPORTS.md', 'docs/ICY-METADATA.md', 'tests/HA_MQTT_TRANSPORT.md', 'tests/ESP-NOW-SECURITY-TESTS.md')) {
        if (Test-Path -LiteralPath (Join-Path $root $optional) -PathType Leaf) {
            $sourcePaths += $optional
        }
    }
    $sketchSources = @(Get-ChildItem -LiteralPath $sketch -File | Where-Object { $_.Extension -in '.cpp', '.h', '.ino' } | Sort-Object Name)
    if (@($sketchSources | Where-Object Name -eq 'PocketJazzRadioApp.cpp').Count -ne 1) { throw 'Missing primary app source' }
    $sourcePaths += @($sketchSources | ForEach-Object { 'PocketJazzRadio/' + $_.Name })
    $sourcePaths += @($playlistFiles | ForEach-Object { 'playlists/' + $_.Name })
    if (Test-Path -LiteralPath $notesDir -PathType Container) {
        $sourcePaths += @(Get-ChildItem -LiteralPath $notesDir -File |
            Sort-Object Name | ForEach-Object { 'data/' + $_.Name })
    }
    $sourcePaths += @(Get-ChildItem -LiteralPath (Join-Path $root 'tools') -File -Filter '*.py' |
        Sort-Object Name | ForEach-Object { 'tools/' + $_.Name })
    # Preserve the exact upstream audio-source files actually relied upon by
    # the HTTPS/ICY integration, and pin library version descriptors. Full
    # Arduino core and M5GFX tree are too large for the release snapshot:
    # this is NOT a hermetic/reproducible toolchain archive.
    $audioDeps = @(
        'ESP8266Audio/src/AudioFileSource.h',
        'ESP8266Audio/src/AudioFileSourceBuffer.cpp',
        'ESP8266Audio/src/AudioFileSourceBuffer.h',
        'ESP8266Audio/src/AudioFileSourceSD.cpp',
        'ESP8266Audio/src/AudioFileSourceSD.h',
        'ESP8266Audio/src/AudioSourcePullPolicy.h',
        'ESP8266Audio/src/AudioShortageEpisode.h',
        'ESP8266Audio/src/AudioFileSourceHTTPStream.cpp',
        'ESP8266Audio/src/AudioFileSourceHTTPStream.h',
        'ESP8266Audio/src/AudioFileSourceICYStream.cpp',
        'ESP8266Audio/src/AudioFileSourceICYStream.h',
        'ESP8266Audio/src/AudioGeneratorMP3.cpp',
        'ESP8266Audio/src/AudioGeneratorMP3.h',
        'ESP8266Audio/src/AudioLiveStallPolicy.h',
        'ESP8266Audio/src/AudioLiveDecodeBudget.h',
        'ESP8266Audio/src/AudioMp3SyncBoundary.h',
        'ESP8266Audio/src/AudioMp3FrameBitrate.h',
        'ESP8266Audio/src/AudioICYDeadline.h',
        'ESP8266Audio/src/AudioICYIncremental.h'
    )
    foreach ($dependency in $audioDeps) {
        $relative = 'arduino-sketchbook/libraries/' + $dependency
        if (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) {
            $sourcePaths += $relative
        } elseif (-not $PackageOnly) {
            throw "Required compilation dependency absent: $relative"
        }
    }
    foreach ($package in @('ESP8266Audio','M5Unified','M5GFX','M5Cardputer')) {
        $relative = 'arduino-sketchbook/libraries/' + $package + '/library.properties'
        if (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf) {
            $sourcePaths += $relative
        } elseif (-not $PackageOnly) {
            throw "Required Arduino library descriptor absent: $relative"
        }
    }
    # Preserve the actual notices for each redistributed upstream subset.
    foreach ($upstreamLicense in @(
        'arduino-sketchbook/libraries/ESP8266Audio/LICENSE',
        'arduino-sketchbook/libraries/M5Unified/LICENSE',
        'arduino-sketchbook/libraries/M5GFX/LICENSE'
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $upstreamLicense) -PathType Leaf)) {
            throw "Required redistributed dependency license absent: $upstreamLicense"
        }
        $sourcePaths += $upstreamLicense
    }
    $experimentsDir = Join-Path $root 'experiments'
    if (Test-Path -LiteralPath $experimentsDir -PathType Container) {
        $sourcePaths += @(Get-ChildItem -LiteralPath $experimentsDir -File -Filter '*.h' |
            Sort-Object Name | ForEach-Object { 'experiments/' + $_.Name })
    }
    $testsDirectory = Join-Path $root 'tests'
    if (-not (Test-Path -LiteralPath $testsDirectory -PathType Container)) {
        throw 'Required tests/ directory is absent; refusing to package without host tests.'
    }
    $testFiles = @(Get-ChildItem -LiteralPath $testsDirectory -File -Filter 'test_*.py' | Sort-Object Name)
    if ($testFiles.Count -eq 0) {
        throw 'No test_*.py files in tests/; refusing to package without host tests.'
    }
    $sourcePaths += @($testFiles | ForEach-Object { 'tests/' + $_.Name })
    $sourcePaths += @(Get-ChildItem -LiteralPath $testsDirectory -File -Filter '*.cpp' |
        Sort-Object Name | ForEach-Object { 'tests/' + $_.Name })
    if (Test-Path -LiteralPath (Join-Path $testsDirectory 'pcm/CMakeLists.txt') -PathType Leaf) {
        $sourcePaths += 'tests/pcm/CMakeLists.txt'
    }
    # Keep the actual standalone esp-mqtt opt-in Arduino compile probe in the
    # immutable source snapshot; it is not enabled in the production app.
    if (Test-Path -LiteralPath (Join-Path $testsDirectory 'ha_mqtt_compile/ha_mqtt_compile.ino') -PathType Leaf) {
        $sourcePaths += 'tests/ha_mqtt_compile/ha_mqtt_compile.ino'
    }
    if (Test-Path -LiteralPath (Join-Path $testsDirectory 'espnow_hmac_compile/espnow_hmac_compile.ino') -PathType Leaf) {
        $sourcePaths += 'tests/espnow_hmac_compile/espnow_hmac_compile.ino'
    }
    # Compile-only SDK bridge evidence is independently validated against the
    # separately immutable SDK release; it is not linked into the main app.
    if (Test-Path -LiteralPath (Join-Path $testsDirectory 'pcm_mirror_app_compile/pcm_mirror_app_compile.ino') -PathType Leaf) {
        $sourcePaths += 'tests/pcm_mirror_app_compile/pcm_mirror_app_compile.ino'
    }
    $sourceHashes = @('# SHA256 of archived files (relative to source/)')
    foreach ($relative in $sourcePaths) {
        $nativeRelative = $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $original = Join-Path $root $nativeRelative
        if (-not (Test-Path -LiteralPath $original -PathType Leaf)) {
            throw "Required release source missing: $relative"
        }
        if ($initialInputHashes.ContainsKey($original) -and
            (Get-Sha256Hex $original) -ne $initialInputHashes[$original]) {
            throw "Compiled sketch source changed before the source snapshot: $relative"
        }
        $saved = Join-Path $sourceDir $nativeRelative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saved) | Out-Null
        Copy-Item -LiteralPath $original -Destination $saved
        $sourceHash = Get-Sha256Hex $saved
        $sourceHashes += "$sourceHash  $relative"
    }
    if (-not $PackageOnly) {
        # Preserve the exact *two* SDK header files compiled by the isolated
        # speaker/mirror probe. No dependency on mutable SDK current sources.
        foreach ($sdkHeader in $sdkPinnedHeaderNames) {
            $relative = 'vendor/audio-router-sdk-' + $sdkPinnedVersion + '/include/cardputer_audio/' + $sdkHeader
            $from = Join-Path $sdkPinnedInclude ('cardputer_audio\' + $sdkHeader)
            $saved = Join-Path $sourceDir ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar))
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $saved) | Out-Null
            Copy-Item -LiteralPath $from -Destination $saved
            $hash = Get-Sha256Hex $saved
            if ($hash -ne $sdkPinnedHeaderHashes[$sdkHeader]) {
                throw "Pinned SDK source snapshot failed SHA comparison: $sdkHeader"
            }
            $sourceHashes += "$hash  $relative"
        }
        $sdkLicenseRelative = 'vendor/audio-router-sdk-' + $sdkPinnedVersion + '/LICENSE'
        $sdkLicenseSnapshot = Join-Path $sourceDir ($sdkLicenseRelative.Replace('/', [IO.Path]::DirectorySeparatorChar))
        Copy-Item -LiteralPath (Join-Path $out ('pinned-sdk-' + $sdkPinnedVersion + '\LICENSE')) -Destination $sdkLicenseSnapshot
        if ((Get-Sha256Hex $sdkLicenseSnapshot) -ne $sdkPinnedLicenseHash) {
            throw 'SDK MIT license snapshot mismatch.'
        }
        $sourceHashes += "$sdkPinnedLicenseHash  $sdkLicenseRelative"
    }
    [IO.File]::WriteAllLines((Join-Path $sourceDir 'SHA256-SOURCES.txt'), $sourceHashes, [Text.UTF8Encoding]::new($false))
    $buildInputs = @(
        'Release: ' + $Version,
        'FQBN: ' + $fqbn,
        'Board core: m5stack:esp32 3.3.9 (compile-toolchain selection; not bundled)',
        'Provenance: ' + $provenanceStatus,
        'Sketch source SHA256 checking: ' + $(if ($verifiedReuse) { 'all sketch inputs and CLI config match immutable prior release; before and after packaging' } elseif ($PackageOnly) { 'NOT PERFORMED (PackageOnly)' } else { 'before compile, after compile, before snapshot' }),
        'Arduino libraries: library.properties pinned in source/ where present',
        'Bundled dependency subset: ESP8266Audio HTTP, ICY, Buffer, MP3 source and headers',
        'Full toolchain and all transitive libraries: NOT ARCHIVED',
        'Physical Cardputer tested: NO'
    )
    if ($PackageOnly) {
        $buildInputs += 'Pinned SDK v0.8e PCM mirror compile: NOT RE-RUN (data-only or PackageOnly)'
        if ($verifiedReuse) {
            $buildInputs += "Reused immutable firmware version: $ReuseVerifiedAppFromRelease"
            $buildInputs += "$reusedBinSha  immutable-app-only-BIN"
        }
    } else {
        $buildInputs += 'Pinned SDK source archive: ' + $sdkPinnedVersion
        $buildInputs += $sdkPinnedZipSha + '  upstream/audio-router-sdk-' + $sdkPinnedVersion + '-source.zip (external immutable archive)'
        $buildInputs += $sdkPinnedLicenseHash + '  vendor/audio-router-sdk-' + $sdkPinnedVersion + '/LICENSE'
        foreach ($sdkHeader in $sdkPinnedHeaderNames) {
            $buildInputs += $sdkPinnedHeaderHashes[$sdkHeader] + '  vendor/audio-router-sdk-' + $sdkPinnedVersion + '/include/cardputer_audio/' + $sdkHeader
        }
    }
    foreach ($inputPath in $compileInputs) {
        $relative = $inputPath.Substring($root.Length + 1)
        $buildInputs += $initialInputHashes[$inputPath] + '  ' + $relative
    }
    [IO.File]::WriteAllLines((Join-Path $staging 'BUILD-INPUTS.txt'), $buildInputs, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $staging 'ARDUINO-COMPILE-REPORT.txt'),
                          $arduinoReport + [Environment]::NewLine + 'No device was flashed.',
                          [Text.UTF8Encoding]::new($false))

    # Host tests must pass before a versioned package is published.
    $python = (Get-Command python -ErrorAction Stop).Source
    # unittest summary goes to stderr, so preserve both streams.
    $runner = Invoke-CapturedProcess $python @('-m','unittest','discover','-s','tests','-p','test_*.py','-v') $root
    $testOutput = $runner.Output
    $testReport = 'Host-side unittest: exit ' + $runner.ExitCode + [Environment]::NewLine + $testOutput
    $counts = [regex]::Matches($testOutput, '(?m)^Ran ([0-9]+) tests? in ')
    if ($runner.ExitCode -ne 0 -or $counts.Count -ne 1 -or [int]$counts[0].Groups[1].Value -lt 1) {
        throw "Host-side tests failed or no tests were executed; refusing to publish v$Version. Exit code: $($runner.ExitCode)."
    }
    $testReport += [Environment]::NewLine + 'Physical Cardputer/HTTPS/audio: NOT TESTED by this build.' + [Environment]::NewLine
    [IO.File]::WriteAllText((Join-Path $staging 'TEST-REPORT.txt'), $testReport, [Text.UTF8Encoding]::new($false))

    # Mandatory C++ host tests: missing CMakeLists or zero executed tests
    # are a packaging failure, not a silent report of NOT AVAILABLE.
    if (-not (Test-Path -LiteralPath (Join-Path $testsDirectory 'pcm/CMakeLists.txt') -PathType Leaf) -or
        @((Get-ChildItem -LiteralPath $testsDirectory -File -Filter '*.cpp')).Count -lt 1) {
        throw 'Required C++ host test suite is absent; refusing to publish.'
    }
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
    $ctest = (Get-Command ctest -ErrorAction Stop).Source
    $hostBuild = Join-Path $staging '.host-ctest-build'
    $configure = (& $cmake -S (Join-Path $testsDirectory 'pcm') -B $hostBuild 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Host C++ CMake configuration failed: $configure" }
    $buildLog = (& $cmake --build $hostBuild --config Release 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "Host C++ build failed: $buildLog" }
    $ctestRunner = Invoke-CapturedProcess $ctest @('--test-dir', $hostBuild, '-C', 'Release', '--output-on-failure') $root
    $ctestLog = $ctestRunner.Output
    $passed = [regex]::Match($ctestLog, '100% tests passed out of ([1-9][0-9]*)')
    if ($ctestRunner.ExitCode -ne 0 -or -not $passed.Success) {
        throw "Host C++ tests failed or no test was executed (exit $($ctestRunner.ExitCode)): $ctestLog"
    }
    $hostReport = $configure + [Environment]::NewLine + $buildLog +
        [Environment]::NewLine + $ctestLog +
        [Environment]::NewLine + 'Physical device: NOT TESTED.' + [Environment]::NewLine
    Remove-Item -LiteralPath $hostBuild -Recurse -Force
    [IO.File]::WriteAllText((Join-Path $staging 'HOST-CTEST-REPORT.txt'), $hostReport, [Text.UTF8Encoding]::new($false))

    # Tests run from the working tree, so verify that *all* archived source
    # files remained unchanged between snapshot and successful tests.
    foreach ($relative in $sourcePaths) {
        $nativeRelative = $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $original = Join-Path $root $nativeRelative
        $saved = Join-Path $sourceDir $nativeRelative
        if ((Get-Sha256Hex $original) -ne (Get-Sha256Hex $saved)) {
            throw "A source file changed during host testing; refusing release: $relative"
        }
    }
    if (-not $PackageOnly) {
        $sdkLicenseRelative = 'vendor/audio-router-sdk-' + $sdkPinnedVersion + '/LICENSE'
        if ((Get-Sha256Hex (Join-Path $sourceDir ($sdkLicenseRelative.Replace('/', [IO.Path]::DirectorySeparatorChar)))) -ne
            $sdkPinnedLicenseHash) { throw 'Pinned SDK MIT license changed during packaging.' }
        foreach ($sdkHeader in $sdkPinnedHeaderNames) {
            $relative = 'vendor/audio-router-sdk-' + $sdkPinnedVersion + '/include/cardputer_audio/' + $sdkHeader
            if ((Get-Sha256Hex (Join-Path $sourceDir ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar)))) -ne
                $sdkPinnedHeaderHashes[$sdkHeader]) {
                throw "Pinned SDK header changed during packaging: $sdkHeader"
            }
        }
        if ((Get-Sha256Hex $sdkPinnedZip) -ne $sdkPinnedZipSha) {
            throw 'Pinned SDK immutable source archive changed before release.'
        }
    }
    $installText = @"
Pocket Jazz Radio v$Version - Cardputer ADV / M5Launcher 3

$installClearance
$provenanceStatus
App-only BIN size: $($bytes.Length) bytes; no arbitrary Launcher size threshold is applied.
Install ONLY firmware/$fileName using M5Launcher SD installation.
Copy PocketJazz/*.m3u and *.pls to SD /PocketJazz/ if needed.
Keep a backup of existing SD /PocketJazz/*.m3u and *.pls first.
Do NOT overwrite your personal /PocketJazz/04-my-stations.m3u;
the ZIP copy is an empty example, not a replacement for your stations.
R reloads SD playlists without flashing firmware.
Copy PocketJazz/artist-notes.tsv and artist-notes.idx as a matched pair
to SD /PocketJazz/ for verified offline artist notes (if supplied).
Copy PocketJazz/jazz-encyclopedia.tsv (1400+ musicians) and
PocketJazz/jazz-facts.tsv (Jazz Spark facts + checked brief quotes) to the
same SD directory. U searches the large SD encyclopedia: type query, Enter
search, W/S move through ALL results, Enter opens, Del returns to editing.
N opens the current artist when ICY has 'Artist - Track'. Z opens Jazz Spark:
W/S or Enter next fact, T toggles automatic 10-second Spark cards every
12 minutes (NVS; default on), Z/Del back. Source URLs are deliberately NOT shown
on the small device screen (full provenance is in release/source/data/).
RADIO displays the PCM VU by default, without 64x64 station logos or their
8-KiB static RAM cache. V toggles the VU display; it never mutes/stops audio.
H opens Help at page 1. The footer no longer duplicates navigation keys.
S opens TUNING SOUND FX: 1 toggles quiet vinyl crackle; 2 toggles very quiet
white noise; Del returns. Both are OFF by default and saved separately in
NVS. FX can be skipped under memory pressure; no sound on automatic retries.
Holding S *during boot* still disables autoplay (SAFE START).
From v0.7a: E while viewing I/Diagnostics or Y/ICY history explicitly saves
bounded app-owned TSV logs under /PocketJazz/ (max 16 KiB per file). No
automatic logs, and no deletion/rotation; source/docs/SD-EXPORTS.md explains.
Newer builds: Diagnostics page 2 shows PCM-queue gap estimates (>=100 ms),
STOP drain latency and latency to first PCM for a selected stream. This is
NOT a microphone or analog DAC measurement. I then X resets these counters;
I then E exports a numeric snapshot to the existing bounded TSV.
At RADIO, B opens SETTINGS BACKUP (C remains the sole clock entry). Stop the
stream first; B E E manually exports settings.bak, B I I manually imports.
The CRC32/whitelist file excludes Wi-Fi credentials, but station URLs are
hex encoded and NOT ENCRYPTED; may contain private query tokens.
The original backup is rotated only into an unused settings.prev0..prev9;
existing personal copies, playlists, liked.txt and log files are preserved.
Two Wi-Fi profiles: W scans, Enter reconnects a saved network, Fn+Enter edits
its password. A new profile is committed only after stable IP >= 2 seconds.
See source/docs/SETTINGS-BACKUP-AND-WIFI-PROFILES.md for limitations.
RADIO Left/Right tunes the previous/next station; P changes playlist.
F opens station browsing (Up/Down select, P playlist, Enter tunes,
Del closes). Optional verified HTTPS ICY station title previews are OFF
unless /PocketJazz/peek.cfg is created manually with exact line PEEK=1;
these previews are best-effort and never change playing audio/history.
U opens offline Jazz Notes search, with W/S list and Enter open. Large SD
encyclopedia takes priority over the 36 legacy verified MusicBrainz cards.
No MQTT, ESP-NOW, USB/BLE or SD time-shift hardware integration is bundled.
The ZIP contains LICENSE (GPLv3 text) and COPYRIGHT-NOTICES.md for this
firmware and bundled upstream source references. Preserve these notices
when redistributing the firmware. Complete corresponding app sources and
per-file hashes are alongside this ZIP under release/source/.
Optional timezone: copy PocketJazz/clock.example.txt as /PocketJazz/clock.txt,
edit its TZ=POSIX-RULE line and reboot the radio. Existing clock.txt is never
replaced automatically; default with no file or NVS setting is UTC0. C opens
the single Blue Note clock, UP/DOWN adjusts the fixed UTC offset
by +/-30 minutes, 0 clears the manual override and restores the
SD POSIX timezone rule (if present), DEL returns to radio. The offset is
saved in NVS. A fixed offset does NOT auto-adjust for DST.
Do NOT flash a merged image, bootloader or partition binary.
Use only the app-only BIN through the existing M5Launcher 3.
Cancel any prompt to overwrite/repartition existing applications.
Physical device playback: NOT TESTED by this packaging script.
"@
    [IO.File]::WriteAllText((Join-Path $staging 'README-INSTALL.txt'), $installText, [Text.UTF8Encoding]::new($false))

    # Archive only SD payload. ZIP entry names MUST use '/' (ZIP standard).
    # Compress-Archive on Windows emits '\' paths, which some non-Windows
    # extractors treat as literal filename characters rather than directories.
    # Explicit forward-slash names avoid that portability/data-copy failure.
    # Keep the manifest outside to avoid a circular ZIP hash.
    $archive = Join-Path $staging $zipName
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $zipInputs = @(@{ Source = (Join-Path $firmware $fileName); Entry = ('firmware/' + $fileName) })
        # ZIP notices must come from the SAME verified source snapshot, not a
        # mutable working-tree file read again after the successful test gate.
        foreach ($notice in @('LICENSE', 'COPYRIGHT-NOTICES.md')) {
            $zipInputs += @{ Source = (Join-Path $sourceDir $notice); Entry = $notice }
        }
        # The staged PocketJazz folder also contains data TSV copies for
        # README/install use. Package playlists here and data ONLY from the
        # verified source snapshot below: one entry per name and byte provenance.
        foreach ($item in @(Get-ChildItem -LiteralPath $playlists -File |
                Where-Object { $_.Extension -in '.m3u', '.pls' } | Sort-Object Name)) {
            $zipInputs += @{ Source = $item.FullName; Entry = ('PocketJazz/' + $item.Name) }
        }
        # Data must ship in the *actual SD ZIP*, not only in source/ or users
        # reasonably report "Notes does not work" after installing firmware.
        # Read all packaged data from the already hash-verified source snapshot.
        foreach ($dataName in @('artist-notes.tsv', 'artist-notes.idx',
                                'jazz-encyclopedia.tsv', 'jazz-facts.tsv',
                                'clock.example.txt')) {
            $inputData = Join-Path $sourceDir ('data\' + $dataName)
            if (-not (Test-Path -LiteralPath $inputData -PathType Leaf)) {
                if ($PackageOnly) { continue } # Legacy minimal synthetic packaging fixtures.
                throw "Missing mandatory read-only SD data in source snapshot: $dataName"
            }
            $zipInputs += @{ Source = $inputData; Entry = ('PocketJazz/' + $dataName) }
        }
        $zipInputs += @{ Source = (Join-Path $staging 'README-INSTALL.txt'); Entry = 'README-INSTALL.txt' }
        foreach ($file in $zipInputs) {
            if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf) -or $file.Entry.Contains('\\')) {
                throw "Unsafe or missing ZIP entry: $($file.Entry)"
            }
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $zip, $file.Source, $file.Entry,
                [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    }
    finally { $zip.Dispose() }

    $binHash = Get-Sha256Hex (Join-Path $firmware $fileName)
    $zipHash = Get-Sha256Hex $archive
    $testsHash = Get-Sha256Hex (Join-Path $staging 'TEST-REPORT.txt')
    $hostTestsHash = Get-Sha256Hex (Join-Path $staging 'HOST-CTEST-REPORT.txt')
    $buildInputsHash = Get-Sha256Hex (Join-Path $staging 'BUILD-INPUTS.txt')
    $arduinoCompileHash = Get-Sha256Hex (Join-Path $staging 'ARDUINO-COMPILE-REPORT.txt')
    $manifest = @"
Version: $Version
Artifact type: ESP32 application only (M5Launcher 3 SD)
App header: 0xE9
App size bytes: $($bytes.Length)
Absolute hard sanity maximum bytes: $hardMaxAppBytes
Install clearance: $installClearance
firmware/$fileName SHA256: $binHash
$zipName SHA256: $zipHash
TEST-REPORT.txt SHA256: $testsHash
HOST-CTEST-REPORT.txt SHA256: $hostTestsHash
BUILD-INPUTS.txt SHA256: $buildInputsHash
ARDUINO-COMPILE-REPORT.txt SHA256: $arduinoCompileHash
Build provenance: $provenanceStatus
Source snapshot: source/ (SHA256-SOURCES.txt lists individual file hashes)
Physical hardware: NOT TESTED
No flashing performed.
"@
    [IO.File]::WriteAllText((Join-Path $staging 'SHA256-MANIFEST.txt'), $manifest, [Text.UTF8Encoding]::new($false))
    # Last source-provenance fence before atomic release publication: another
    # editor may have changed a file after tests/snapshot validation but while
    # composing the ZIP. Preserve fail-closed behavior without copying it anew.
    foreach ($relative in $sourcePaths) {
        $nativeRelative = $relative.Replace('/', [IO.Path]::DirectorySeparatorChar)
        if ((Get-Sha256Hex (Join-Path $root $nativeRelative)) -ne
            (Get-Sha256Hex (Join-Path $sourceDir $nativeRelative))) {
            throw "Source changed during final packaging; refusing release: $relative"
        }
    }
    if (Test-Path -LiteralPath $release) { throw "Release v$Version appeared during packaging; refusing overwrite." }
    # Directory.Move fails atomically if another worker won this release name:
    # do not merge staged artifacts into an already published release.
    [IO.Directory]::Move($staging, $release)
    $published = $true
    Write-Host "Release: $release"
    Write-Host "App-only BIN: $(Join-Path $release ('firmware\' + $fileName))"
    Write-Host "SD archive: $(Join-Path $release $zipName)"
    Write-Host "Manifest: $(Join-Path $release 'SHA256-MANIFEST.txt')"
    Write-Host "Source snapshot: $(Join-Path $release 'source')"
    Write-Host "Size: $($bytes.Length) bytes; SHA256: $binHash"
    Write-Host $installClearance
    Write-Host 'No flash/upload commands were executed.'
}
finally {
    if (-not $published -and (Test-Path -LiteralPath $staging)) { Remove-Item -LiteralPath $staging -Recurse -Force }
}
