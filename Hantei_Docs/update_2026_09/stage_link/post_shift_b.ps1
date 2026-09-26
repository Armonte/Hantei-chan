# Post Shift+B (WM_KEYDOWN/WM_KEYUP) to ONE MBAA.exe, chosen by explicit PID (never by name: other games run next to
# it), to exercise pc-overlay's message latch for the stage hot reload. Posted messages take the same
# GetMessage/WndProc path as real keys.   usage: powershell -File post_shift_b.ps1 -ProcId <pid>
param([int]$ProcId)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class K { [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l); }
"@
$p = Get-Process -Id $ProcId -ErrorAction SilentlyContinue
if (-not $p -or $p.ProcessName -ne 'MBAA') { Write-Error "pid $ProcId is not MBAA.exe"; exit 2 }
$h = $p.MainWindowHandle
[void][K]::PostMessage($h, 0x100, [IntPtr]0x10, [IntPtr]0x002A0001)   # WM_KEYDOWN VK_SHIFT
Start-Sleep -Milliseconds 30
[void][K]::PostMessage($h, 0x100, [IntPtr]0x42, [IntPtr]0x00300001)   # WM_KEYDOWN 'B' (scan 0x30)
Start-Sleep -Milliseconds 30
[void][K]::PostMessage($h, 0x101, [IntPtr]0x42, [IntPtr]0xC0300001)   # WM_KEYUP 'B'
[void][K]::PostMessage($h, 0x101, [IntPtr]0x10, [IntPtr]0xC02A0001)   # WM_KEYUP VK_SHIFT
Write-Output "posted Shift+B to $h (pid $ProcId)"
