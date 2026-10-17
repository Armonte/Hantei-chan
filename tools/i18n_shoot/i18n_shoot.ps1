# Window-only UI driver for the Japanese click-through screenshots (docs/formats/i18n_ja_review.md).
# usage: i18n_shoot.ps1 -ProcId N -Out <dir> -Actions "c 100 20; w 400; s menu_file; k {ESC}; ..."
#   c x y | rc x y | dc x y | m x y   (mouse, pixel coords of the saved screenshot: click, right click, double click, move)
#   w ms | s name [keep] (park the mouse at 250,950 unless 'keep'; save <Out>\i18n_ja_<name>.png, window rect only, app forced TOPMOST so nothing else is captured)
#   hc x y (hover then click: the first click after a layout change is otherwise swallowed)
#   sp name [keep] (like s but copies the screen rect so separate-window popups/combo lists are included; refuses if a foreign topmost window overlaps)
#   hdc x y (hover, then a double click at the same spot)
#   top (make this process's popup/viewport windows topmost too so they can be clicked)
#   h x y (robust hover: ImGui needs several mouse-move frames before a submenu opens)
#   k <SendKeys text> | wheel n | drag x1 y1 x2 y2
param([int]$ProcId,[string]$Out,[string]$Actions)
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System; using System.Runtime.InteropServices;
public class DV { public delegate bool CB(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool EnumWindows(CB cb, IntPtr l);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint fl, UIntPtr e);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f,int dx,int dy,uint d,UIntPtr e);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
 [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int sz);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 public static string Foreign(uint pid, RECT a){ string res=""; EnumWindows((h,l)=>{ uint p; GetWindowThreadProcessId(h,out p); if(p==pid||!IsWindowVisible(h)) return true; if((GetWindowLong(h,-20)&8)==0) return true; RECT r; GetWindowRect(h,out r); if(r.L<a.R&&r.R>a.L&&r.T<a.B&&r.B>a.T&&(r.R-r.L)>2&&(r.B-r.T)>2) res+=h.ToString()+" "; return true;},IntPtr.Zero); return res; }
 [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, System.Text.StringBuilder s, int n);
 public static System.Collections.Generic.List<IntPtr> Others(uint pid, IntPtr main){ var l=new System.Collections.Generic.List<IntPtr>(); EnumWindows((h,x)=>{ uint p; GetWindowThreadProcessId(h,out p); if(p==pid&&h!=main&&IsWindowVisible(h)){ var sb=new System.Text.StringBuilder(64); GetClassName(h,sb,64); if(sb.ToString().StartsWith("ImGui")) l.Add(h); } return true;},IntPtr.Zero); return l; }
 public static IntPtr Best(uint pid){ IntPtr best=IntPtr.Zero; long area=0; EnumWindows((h,l)=>{ uint p; GetWindowThreadProcessId(h,out p); if(p==pid&&IsWindowVisible(h)){ RECT r; GetWindowRect(h,out r); long a=(long)(r.R-r.L)*(r.B-r.T); if(a>area){area=a;best=h;} } return true;},IntPtr.Zero); return best; } }
'@
[DV]::SetProcessDPIAware() | Out-Null
$h=[DV]::Best([uint32]$ProcId); if($h -eq [IntPtr]::Zero){ 'no window'; exit }
[DV]::SetWindowPos($h,[IntPtr]::new(-1),0,0,0,0,0x13) | Out-Null
function FgPid { $pp=[uint32]0; [DV]::GetWindowThreadProcessId([DV]::GetForegroundWindow(),[ref]$pp) | Out-Null; $pp }
function Fg { if((FgPid) -ne [uint32]$ProcId){ $fgw=[DV]::GetForegroundWindow(); $pp=[uint32]0; $ft=[DV]::GetWindowThreadProcessId($fgw,[ref]$pp); $me=[DV]::GetCurrentThreadId()
    [DV]::AttachThreadInput($me,$ft,$true) | Out-Null; [DV]::keybd_event(0x12,0,0,[UIntPtr]::Zero); [DV]::keybd_event(0x12,0,2,[UIntPtr]::Zero)
    [DV]::BringWindowToTop($h) | Out-Null; [DV]::SetForegroundWindow($h) | Out-Null; [DV]::AttachThreadInput($me,$ft,$false) | Out-Null; Start-Sleep -Milliseconds 250 } }
Fg
$r=New-Object DV+RECT
function Rect { [DV]::GetWindowRect($h,[ref]$r) | Out-Null }
function MoveTo($x,$y){ Fg; $e=New-Object DV+RECT; [DV]::DwmGetWindowAttribute($h,9,[ref]$e,16) | Out-Null; [DV]::SetCursorPos($e.L+[int]$x-4,$e.T+[int]$y-3) | Out-Null; Start-Sleep -Milliseconds 100; [DV]::SetCursorPos($e.L+[int]$x,$e.T+[int]$y) | Out-Null; Start-Sleep -Milliseconds 220 }
function Btn($d,$u,$n){ for($i=0;$i -lt $n;$i++){ [DV]::mouse_event($d,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 160; [DV]::mouse_event($u,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 90 } }
foreach($a in $Actions.Split(';')){
  $t=$a.Trim().Split(' ',2); $op=$t[0]; $arg= if($t.Count -gt 1){$t[1]}else{''}
  $n=@(); if($arg -ne ''){ $n=$arg.Split(' ') }
  switch($op){
    'c'  { MoveTo $n[0] $n[1]; Btn 2 4 1; Start-Sleep -Milliseconds 250 }
    'rc' { MoveTo $n[0] $n[1]; Btn 8 16 1; Start-Sleep -Milliseconds 250 }
    'dc' { MoveTo $n[0] $n[1]; Btn 2 4 2; Start-Sleep -Milliseconds 250 }
    'h'  { MoveTo $n[0] ([int]$n[1]+40); Start-Sleep -Milliseconds 250; MoveTo $n[0] $n[1]; Start-Sleep -Milliseconds 250; MoveTo ([int]$n[0]+8) ([int]$n[1]+1); Start-Sleep -Milliseconds 700 }
    'hc' { MoveTo $n[0] ([int]$n[1]+30); Start-Sleep -Milliseconds 200; MoveTo $n[0] $n[1]; Start-Sleep -Milliseconds 300; Btn 2 4 1; Start-Sleep -Milliseconds 350 }
    'top' { foreach($oh in [DV]::Others([uint32]$ProcId,$h)){ [DV]::SetWindowPos($oh,[IntPtr]::new(-1),0,0,0,0,0x13) | Out-Null }; Start-Sleep -Milliseconds 300 }
    'hdc' { MoveTo $n[0] ([int]$n[1]+40); Start-Sleep -Milliseconds 250; MoveTo $n[0] $n[1]; Start-Sleep -Milliseconds 500; Btn 2 4 1; Start-Sleep -Milliseconds 120; Btn 2 4 1; Start-Sleep -Milliseconds 500 }
    'm'  { MoveTo $n[0] $n[1]; Start-Sleep -Milliseconds 250 }
    'win' { [DV]::SetWindowPos($h,[IntPtr]::new(-1),[int]$n[0],[int]$n[1],[int]$n[2],[int]$n[3],0x40) | Out-Null; Start-Sleep -Milliseconds 500 }
    'w'  { Start-Sleep -Milliseconds ([int]$n[0]) }
    'k'  { [System.Windows.Forms.SendKeys]::SendWait($arg); Start-Sleep -Milliseconds 250 }
    'wheel' { [DV]::mouse_event(0x800,0,0,[BitConverter]::ToUInt32([BitConverter]::GetBytes([int]([int]$n[0]*120)),0),[UIntPtr]::Zero); Start-Sleep -Milliseconds 200 }
    'drag' { MoveTo $n[0] $n[1]; [DV]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 150; MoveTo $n[2] $n[3]; Start-Sleep -Milliseconds 150; [DV]::mouse_event(4,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 250 }
    'sp' { # popup shot: combo/popup lists are separate OS windows, so copy the screen rect - but only if no foreign TOPMOST window overlaps it
           if($n.Count -lt 2 -or $n[1] -ne 'keep'){ MoveTo 250 950 }; Start-Sleep -Milliseconds 300
           $e=New-Object DV+RECT; [DV]::DwmGetWindowAttribute($h,9,[ref]$e,16) | Out-Null
           $f=[DV]::Foreign([uint32]$ProcId,$e); if($f -ne ''){ "SKIPPED $($n[0]): foreign topmost window(s) overlap: $f"; continue }
           $w=$e.R-$e.L; $hh=$e.B-$e.T; $bmp=New-Object System.Drawing.Bitmap $w,$hh; $g=[System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($e.L,$e.T,0,0,$bmp.Size)
           $bmp.Save((Join-Path $Out ("i18n_ja_"+$n[0]+".png"))); $g.Dispose(); $bmp.Dispose(); "shot(screen) $($n[0]) ${w}x${hh}" }
    's'  { if($n.Count -lt 2 -or $n[1] -ne 'keep'){ MoveTo 250 950 }; Start-Sleep -Milliseconds 300; Rect; $e=New-Object DV+RECT; [DV]::DwmGetWindowAttribute($h,9,[ref]$e,16) | Out-Null   # visible frame only (no invisible resize border / desktop)
           $w=$e.R-$e.L; $hh=$e.B-$e.T
           # PrintWindow renders the app window itself (flag 2 = full DWM content), so a foreign topmost window can never end up in the image
           $full=New-Object System.Drawing.Bitmap ($r.R-$r.L),($r.B-$r.T); $gf=[System.Drawing.Graphics]::FromImage($full); $hdc=$gf.GetHdc(); [DV]::PrintWindow($h,$hdc,2) | Out-Null; $gf.ReleaseHdc($hdc); $gf.Dispose()
           $bmp=New-Object System.Drawing.Bitmap $w,$hh; $g=[System.Drawing.Graphics]::FromImage($bmp); $g.DrawImage($full,(New-Object System.Drawing.Rectangle 0,0,$w,$hh),($e.L-$r.L),($e.T-$r.T),$w,$hh,[System.Drawing.GraphicsUnit]::Pixel); $full.Dispose()
           # popups / combo lists / floating windows of this process are separate OS windows: print each one and composite it at its screen position
           foreach($oh in [DV]::Others([uint32]$ProcId,$h)){ $orr=New-Object DV+RECT; [DV]::GetWindowRect($oh,[ref]$orr) | Out-Null; $ow=$orr.R-$orr.L; $oh2=$orr.B-$orr.T; if($ow -lt 4 -or $oh2 -lt 4){ continue }
             $ob=New-Object System.Drawing.Bitmap $ow,$oh2; $og=[System.Drawing.Graphics]::FromImage($ob); $ohdc=$og.GetHdc(); [DV]::PrintWindow($oh,$ohdc,2) | Out-Null; $og.ReleaseHdc($ohdc); $og.Dispose()
             $g.DrawImage($ob,($orr.L-$e.L),($orr.T-$e.T)); $ob.Dispose() }
           $bmp.Save((Join-Path $Out ("i18n_ja_"+$n[0]+".png"))); $g.Dispose(); $bmp.Dispose(); "shot $($n[0]) ${w}x${hh}" }
  }
}
[DV]::SetWindowPos($h,[IntPtr]::new(-2),0,0,0,0,0x13) | Out-Null
