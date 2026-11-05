param(
    [Parameter(Mandatory=$true)][string]$Document,
    [Parameter(Mandatory=$true)][string]$Pdf
)
$ErrorActionPreference = 'Stop'
$documentPath = (Resolve-Path -LiteralPath $Document).Path
$pdfPath = [IO.Path]::GetFullPath($Pdf)
New-Item -ItemType Directory -Path (Split-Path -Parent $pdfPath) -Force | Out-Null
$word = $null
$opened = $null
$ownsApplication = $false
try {
    $word = New-Object -ComObject Word.Application
    $ownsApplication = $word.Documents.Count -eq 0
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $opened = $word.Documents.Open($documentPath, $false, $true)
    $opened.Repaginate()
    $opened.ExportAsFixedFormat($pdfPath, 17)
    Write-Output "Rendered to $pdfPath"
} finally {
    if ($null -ne $opened) { $opened.Close(0) }
    if ($null -ne $word -and $ownsApplication) { $word.Quit() }
}
