[CmdletBinding()]
param(
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$dataRoot = Join-Path $ProjectRoot 'SmartMediVend\data'

function Read-JsonFile([string]$Name) {
    $path = Join-Path $dataRoot $Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing medical data file: $path"
    }
    return Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
}

$medicines = Read-JsonFile 'medicines.json'
$rules = Read-JsonFile 'medical_rules.json'
$review = Read-JsonFile 'pharmacist_review.json'

if ($medicines.schema_version -ne 1) { throw 'medicines schema_version must be 1' }
if ($medicines.catalog_version -ne 'catalog-2026-09-17') { throw 'Unexpected catalog version' }
if ($medicines.slots.Count -ne 16) { throw 'Catalog must contain 16 physical slots' }

$channels = @($medicines.slots | ForEach-Object { [int]$_.channel } | Sort-Object)
for ($channel = 0; $channel -lt 16; $channel++) {
    if ($channels[$channel] -ne $channel) { throw "Missing or duplicate channel $channel" }
}

if (@($medicines.slots | Where-Object initial_stock -ne 5).Count -ne 0) {
    throw 'Every physical slot must start with 5 blister packs'
}

$canonicalCount = @($medicines.slots.canonical_id | Sort-Object -Unique).Count
if ($canonicalCount -ne 13) { throw 'Catalog must contain 13 unique medicines' }

$expectedBackups = @{ 13 = 0; 14 = 3; 15 = 7 }
foreach ($entry in $expectedBackups.GetEnumerator()) {
    $slot = $medicines.slots | Where-Object channel -eq $entry.Key
    if ([int]$slot.backup_of_channel -ne $entry.Value) {
        throw "Channel $($entry.Key) has incorrect backup mapping"
    }
}

if ($rules.schema_version -ne 1 -or $rules.rules_version -ne 'rules-2026-09-17') {
    throw 'Unexpected medical rules schema/version'
}
if ($rules.medicine_rules.Count -ne 13) { throw 'Rules must cover 13 medicines' }
if ($review.schema_version -ne 1) { throw 'Review schema_version must be 1' }
if ($review.approved -ne $false) { throw 'Repository default must remain unapproved' }
if ($review.notice -ne 'PHARMACIST_REVIEW_REQUIRED') { throw 'Missing pharmacist notice' }

Write-Output 'MEDICAL_JSON_VALIDATION_OK slots=16 medicines=13 approved=false'
