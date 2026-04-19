using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace DotNetLib2
{
  internal class TestClass
  {
    [UnmanagedCallersOnly]
    public static void Hello2(IntPtr message)
    {
      Console.WriteLine($"{Marshal.PtrToStringUTF8(message)}");
    }
  }
}
