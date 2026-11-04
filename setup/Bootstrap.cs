using System;
using System.IO;
using System.IO.Compression;
using System.Diagnostics;
using System.Reflection;
using System.Windows.Forms;
class Setup {
 static void CleanTemporaryPayload(string root) {
  if(String.IsNullOrEmpty(root)||!Directory.Exists(root))return;
  string parent=Path.Combine(Path.GetTempPath(),"BWTL-Setup")+Path.DirectorySeparatorChar;
  if(!Path.GetFullPath(root).StartsWith(parent,StringComparison.OrdinalIgnoreCase))return;
  var pending=new System.Collections.Generic.Stack<string>();pending.Push(root);
  while(pending.Count>0){string dir=pending.Pop();if((File.GetAttributes(dir)&FileAttributes.ReparsePoint)!=0)return;
   foreach(string entry in Directory.GetFileSystemEntries(dir)){var attr=File.GetAttributes(entry);if((attr&FileAttributes.ReparsePoint)!=0)return;if((attr&FileAttributes.Directory)!=0)pending.Push(entry);}
  }
  Directory.Delete(root,true);
 }
 [STAThread] static int Main(string[] args) {
  string temporary=null;
  try {
   bool extractOnly=args.Length==2 && args[0]=="--extract-only";
   string root=extractOnly?Path.GetFullPath(args[1]):Path.Combine(Path.GetTempPath(),"BWTL-Setup",Guid.NewGuid().ToString("N"));
   if(!extractOnly)temporary=root;
   if(extractOnly && Directory.Exists(root))throw new Exception("测试提取目录已存在");Directory.CreateDirectory(root);
   using(Stream payload=Assembly.GetExecutingAssembly().GetManifestResourceStream("BWTLPayload"))
   using(var zip=new ZipArchive(payload,ZipArchiveMode.Read)) {
    foreach(var entry in zip.Entries) {
     string destination=Path.GetFullPath(Path.Combine(root,entry.FullName.Replace('/',Path.DirectorySeparatorChar)));
     if(!destination.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)) throw new Exception("安装包路径无效");
     if(entry.Name.Length==0){Directory.CreateDirectory(destination);continue;}
     Directory.CreateDirectory(Path.GetDirectoryName(destination));entry.ExtractToFile(destination);
    }
   }
   if(extractOnly)return 0; // No GUI or configuration changes in test mode.
   var p=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell\\v1.0\\powershell.exe"));
   p.Arguments="-NoProfile -ExecutionPolicy Bypass -File \""+Path.Combine(root,"setup","Install-BWTL.ps1")+"\" -PackageRoot \""+root+"\"";
   p.UseShellExecute=false;p.CreateNoWindow=true;p.WindowStyle=ProcessWindowStyle.Hidden;
   using(var process=Process.Start(p)){process.WaitForExit();return process.ExitCode;}
  } catch(Exception e) {if(args.Length>0)return 1;MessageBox.Show(e.Message,"霸威天龙安装失败");return 1;}
  finally {try{CleanTemporaryPayload(temporary);}catch{/* A locked payload is left for OS temporary-file cleanup. */}}
 }
}
