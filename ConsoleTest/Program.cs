using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ConsoleTest
{
    class Program
    {
        static KeyboardEvents.Debug _debug;

        static async Task Main(string[] args)
        {
            Console.WriteLine("Hello World!");

            LlkPress_Callback_Register(OnKeyPress);

            while(true) {
                await Task.Delay(1000);
            }
        }

        private static bool OnKeyPress(KeyboardEventArgs args)
        {
            Console.WriteLine($"Pressed key: {args.vkCode}; Scan code: {args.scanCode}; DevicePtr: {args.source.Handle}; DeviceName: {args.source.DeviceName}");

            if (args.vkCode == 65 && args.source.DeviceName.Equals(@"\\?\HID#VID_04F2&PID_1516&MI_00#8&153530d8&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}"))
            {
                Console.WriteLine("EQUALS");
                return false;
            }

            return true;
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate bool LlkPress_Callback_Register_Delegate(KeyboardEventArgs args);

        [DllImport("LowLevelKeyboard.dll")]
        static extern void LlkPress_Callback_Register(LlkPress_Callback_Register_Delegate cb);

        [DllImport("LowLevelKeyboard.dll")]
        static extern bool LlkPress_GetKeyboardStruct(IntPtr deviceHandle, ref KeyboardStruct kbS);

        [StructLayout(LayoutKind.Sequential)]
        struct KeyboardStruct
        {
            public IntPtr Handle;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1024)]
            public string DeviceName;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct KeyboardEventArgs
        {
            public UInt32 vkCode;
            public UInt32 scanCode;
            public KeyboardStruct source;
        }

    }
}
