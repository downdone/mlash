param([Parameter(Mandatory)][string]$Path)
$ErrorActionPreference = 'Stop'
$executablePath = (Resolve-Path -LiteralPath $Path).Path
if ([IO.Path]::GetExtension($executablePath) -ne '.exe' -or !(Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw 'An existing executable is required for the shell notification'
}
if (!('MlashPackaging.Shell' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace MlashPackaging {
    public static class Shell {
        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        public static extern void SHChangeNotify(uint eventId, uint flags, string item1, IntPtr item2);
    }
}
'@
}
# UPDATEITEM + PATHW + FLUSHNOWAIT: refresh only this replaced file, asynchronously.
[MlashPackaging.Shell]::SHChangeNotify(0x2000, 0x3005, $executablePath, [IntPtr]::Zero)
