using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows.Forms;
class Manager {
 [STAThread] static int Main(string[] args) {
  try {
   string self=Assembly.GetExecutingAssembly().Location;
   string root=Path.GetDirectoryName(self);
   string mode=Path.GetFileNameWithoutExtension(self).EndsWith("Uninstall",StringComparison.OrdinalIgnoreCase)?" -Uninstall":"";
   if(args.Length==1 && (args[0]=="-Uninstall" || args[0]=="-Rollback" || args[0]=="-Diagnose"))mode=" "+args[0];
   else if(args.Length!=0)throw new Exception("不支持的管理参数");
   string script=Path.Combine(root,"Install-BWTL.ps1");
   if(!File.Exists(script))throw new Exception("安装管理文件缺失，请重新运行安装包修复。");
   var p=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell\\v1.0\\powershell.exe"));
   p.Arguments="-NoProfile -ExecutionPolicy Bypass -File \""+script+"\" -InstallRoot \""+root+"\""+mode;
   p.UseShellExecute=false;p.CreateNoWindow=true;p.WindowStyle=ProcessWindowStyle.Hidden;
   Process.Start(p).Dispose();
   // Exit before deletion: the uninstaller can remove this launcher itself.
   return 0;
  }catch(Exception e){MessageBox.Show(e.Message,"霸威天龙管理");return 1;}
 }
}
