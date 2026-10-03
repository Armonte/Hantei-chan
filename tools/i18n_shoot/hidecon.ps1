# hides (SW_HIDE) the stray console window the app's log console creates; matches by title, never touches the GUI window
param([int]$ProcId)
Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices;
public class HC { public delegate bool CB(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumWindows(CB cb, IntPtr l);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
 [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 public static string Hide(){ string r=""; EnumWindows((h,l)=>{ if(!IsWindowVisible(h)) return true; var t=new StringBuilder(256); GetWindowText(h,t,256); var c=new StringBuilder(128); GetClassName(h,c,128);
   if(t.ToString().EndsWith("gonptechan.exe")){ ShowWindow(h,0); r+=c+"|"+t+" "; } return true;},IntPtr.Zero); return r; } }
'@
$r=[HC]::Hide(); if($r){ "hid console: $r" } else { "no console window found" }
