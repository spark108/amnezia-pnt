param(
    [Parameter(Mandatory=$true)][string]$Archive,
    [Parameter(Mandatory=$true)][string]$Destination,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedHash,
    [Parameter(Mandatory=$true)][int]$ParentProcessId
)
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
$parent=Split-Path -Parent $target
$backup=Join-Path $parent ('PntVPN-backup-'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $parent ('PntVPN-update-'+[guid]::NewGuid().ToString('N'))
$swapped=$false
$hadService=$false
$installed=$false
try {
    $principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator privileges are required.'}
    if(-not (Test-Path -LiteralPath (Join-Path $target 'PntVPN.exe'))){throw 'The installed PNT VPN directory was not found.'}
    if((Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash -ne $ExpectedHash){throw 'The update SHA-256 does not match.'}
    foreach($path in @($stage,$backup)) {
        if((Split-Path -Parent ([IO.Path]::GetFullPath($path))) -ne $parent -or $path -eq $target){throw 'Invalid staging directory.'}
    }
    Expand-Archive -LiteralPath $Archive -DestinationPath $stage
    foreach($file in @('PntVPN.exe','PntVPN-service.exe','pnt-transport.exe','install.ps1')) {
        if(-not (Test-Path -LiteralPath (Join-Path $stage $file) -PathType Leaf)){throw "Incomplete update: $file"}
    }
    $process=Get-Process -Id $ParentProcessId -ErrorAction SilentlyContinue
    if($process -and -not $process.WaitForExit(30000)){throw 'Close PNT VPN before updating.'}
    $service=Get-Service 'PntVPN-service' -ErrorAction SilentlyContinue
    $hadService=$null -ne $service
    if($service -and $service.Status -ne 'Stopped') {
        Stop-Service 'PntVPN-service'
        $service.WaitForStatus('Stopped',[TimeSpan]::FromSeconds(30))
    }
    Move-Item -LiteralPath $target -Destination $backup
    $swapped=$true
    Move-Item -LiteralPath $stage -Destination $target
    & (Join-Path $target 'install.ps1') -Destination $target
    $installed=$true
    if((Split-Path -Parent ([IO.Path]::GetFullPath($backup))) -eq $parent -and (Split-Path -Leaf $backup) -like 'PntVPN-backup-*') {
        Remove-Item -LiteralPath $backup -Recurse -Force -ErrorAction SilentlyContinue
    }
    Write-Output 'PNT VPN has been updated.'
} catch {
    $failure=$_.Exception.Message
    if($swapped -and -not $installed) {
        Stop-Service 'PntVPN-service' -ErrorAction SilentlyContinue
        if(Test-Path -LiteralPath $target) {
            if((Split-Path -Parent ([IO.Path]::GetFullPath($target))) -ne $parent){throw 'Invalid rollback path.'}
            Move-Item -LiteralPath $target -Destination $stage
        }
        Move-Item -LiteralPath $backup -Destination $target
    }
    if($hadService){Start-Service 'PntVPN-service' -ErrorAction SilentlyContinue}
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show("PNT VPN update failed. The previous installation was retained.`n$failure",'PNT VPN') | Out-Null
    exit 1
}
try {
    $shell=New-Object -ComObject Shell.Application
    $shell.ShellExecute((Join-Path $target 'PntVPN.exe'),'',$target,'open',1)
} catch {
    Write-Output 'Open PNT VPN using the desktop shortcut.'
}
