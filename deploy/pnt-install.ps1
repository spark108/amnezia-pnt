param([string]$Destination=(Join-Path $env:ProgramFiles 'PntVPN'))
$ErrorActionPreference='Stop'
$identity=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Run PowerShell as Administrator.'}
$source=[IO.Path]::GetFullPath($PSScriptRoot)
$target=[IO.Path]::GetFullPath($Destination)
if(-not $source.Equals($target,[StringComparison]::OrdinalIgnoreCase) -and
    ($target.StartsWith($source.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -or $source.StartsWith($target.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase))){throw 'Source and destination must not contain each other.'}
if(-not (Test-Path -LiteralPath (Join-Path $source 'PntVPN.exe'))){throw 'Extract the complete PNT VPN release archive first.'}
$service=Get-Service 'PntVPN-service' -ErrorAction SilentlyContinue
if($service -and $service.Status -ne 'Stopped'){Stop-Service 'PntVPN-service'}
New-Item -ItemType Directory -Force $target | Out-Null
if(-not $source.Equals($target,[StringComparison]::OrdinalIgnoreCase)){
    Get-ChildItem -LiteralPath $source -Force | Copy-Item -Destination $target -Recurse -Force
}
$executable=Join-Path $target 'PntVPN-service.exe'
if(-not $service){New-Service -Name 'PntVPN-service' -DisplayName 'PNT VPN client service' -BinaryPathName ('"'+$executable+'"') -StartupType Automatic | Out-Null}
else{& sc.exe config PntVPN-service binPath= ('"'+$executable+'"') start= auto | Out-Null;if($LASTEXITCODE -ne 0){throw 'Cannot configure the VPN service.'}}
Start-Service 'PntVPN-service'
$shell=New-Object -ComObject WScript.Shell
$shortcut=$shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('CommonDesktopDirectory')) 'PNT VPN.lnk'))
$shortcut.TargetPath=Join-Path $target 'PntVPN.exe'
$shortcut.WorkingDirectory=$target
$shortcut.Save()
Write-Output 'PNT VPN installed. Open PNT VPN and import the profile issued by your administrator.'
