$ErrorActionPreference = "Stop"

$link = "https://github.com/ShadowUR0/Installer/releases/download/latest/VencordArabicInstallerCli.exe"
$outfile = "$env:TEMP\VencordArabicInstallerCli.exe"

Write-Output "Downloading Vencord Arabic Installer to $outfile"
Invoke-WebRequest -Uri $link -OutFile $outfile

try {
    Start-Process -Wait -NoNewWindow -FilePath $outfile
}
finally {
    Remove-Item -Force -ErrorAction SilentlyContinue $outfile
}
