using System;
using System.Runtime.InteropServices;
using System.Threading;

internal static unsafe class Internal
{
  [UnmanagedCallersOnly]
  public static void CppToCs(IntPtr message)
  {
    Console.WriteLine("This is called from C++!!");
    CsToCpp();
  }

  [StructLayout(LayoutKind.Sequential, Pack = 1)]
  public readonly struct InternalCall
  {
    private readonly IntPtr NamePtr;
    public readonly IntPtr CppFuncPointer;

    public string? Name => Marshal.PtrToStringAuto(NamePtr);
  }

  [UnmanagedCallersOnly]
  internal static void SetDelegate(IntPtr internalCall, int InLength)
  {
    //Marshal.PtrToStructure<InternalCall>(IntPtr.Add(m_NativeArray, InIndex * Marshal.SizeOf<T>()));
    InternalCall? call = (InternalCall?)Marshal.PtrToStructure(internalCall, typeof(InternalCall));
    CsToCpp = (delegate* unmanaged<void>)call.Value.CppFuncPointer;

    Console.WriteLine("Delegate set!");
  }

  internal static delegate* unmanaged<void> CsToCpp;
}

public class App
{
    private static byte isWaiting = 0;
    private static int s_CallCount = 0;
    private static ManualResetEvent mre = new ManualResetEvent(false);

    public static void Main(string[] args)
    {
        Console.WriteLine($"{nameof(App)} started - args = [ {string.Join(", ", args)} ]");
        isWaiting = 1;
        mre.WaitOne();
    }

    [UnmanagedCallersOnly]
    public static byte IsWaiting() => isWaiting;

    [UnmanagedCallersOnly]
    public static void Hello(IntPtr message)
    {
        Console.WriteLine($"Hello, world! from {nameof(App)} [count: {++s_CallCount}]");
        Console.WriteLine($"-- message: {Marshal.PtrToStringUTF8(message)}");
        if (s_CallCount >= 3)
        {
            Console.WriteLine("Signaling app to close");
            mre.Set();
        }
    }

  [UnmanagedCallersOnly]
  public static void Hello2(IntPtr message)
  {

  }
}
