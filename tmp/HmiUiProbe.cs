using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Accessibility;

internal static class HmiUiProbe
{
    private const int ObjIdClient = unchecked((int)0xFFFFFFFC);
    private static readonly Guid IAccessibleGuid = new Guid("618736E0-3C3D-11CF-810C-00AA00389B71");

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("kernel32.dll")]
    private static extern uint GetCurrentThreadId();

    [DllImport("user32.dll")]
    private static extern bool AttachThreadInput(uint attachThread, uint attachToThread, bool attach);

    [DllImport("user32.dll")]
    private static extern IntPtr GetFocus();

    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr window);

    [DllImport("user32.dll")]
    private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr dpiContext);

    [DllImport("dwmapi.dll")]
    private static extern int DwmGetWindowAttribute(
        IntPtr window, int attribute, out Rect value, int valueSize);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr window, StringBuilder className, int capacity);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int capacity);

    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    private static extern bool GetClientRect(IntPtr window, out Rect rect);

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr window);

    [DllImport("user32.dll")]
    private static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    private static extern bool GetCursorPos(out Point point);

    [DllImport("user32.dll")]
    private static extern IntPtr SetFocus(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, string lParam);

    [DllImport("user32.dll", EntryPoint = "SendMessage")]
    private static extern IntPtr SendMessageInt(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern IntPtr GetParent(IntPtr window);

    [DllImport("user32.dll")]
    private static extern int GetDlgCtrlID(IntPtr window);

    [DllImport("user32.dll")]
    private static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);

    [DllImport("user32.dll")]
    private static extern uint SendInput(uint inputCount, Input[] inputs, int inputSize);

    [DllImport("user32.dll")]
    private static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extraInfo);

    [DllImport("oleacc.dll")]
    private static extern int AccessibleObjectFromWindow(
        IntPtr window,
        int objectId,
        ref Guid interfaceId,
        [In, Out, MarshalAs(UnmanagedType.Interface)] ref object accessibleObject);

    [DllImport("oleacc.dll")]
    private static extern int WindowFromAccessibleObject(
        [MarshalAs(UnmanagedType.Interface)] IAccessible accessibleObject,
        out IntPtr window);

    private delegate bool EnumWindowProc(IntPtr window, IntPtr parameter);
    private static double ClickScale = 1.0;

    [StructLayout(LayoutKind.Sequential)]
    private struct Rect
    {
        public int Left, Top, Right, Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Point
    {
        public int X, Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Input
    {
        public uint Type;
        public InputUnion Union;
    }

    [StructLayout(LayoutKind.Explicit)]
    private struct InputUnion
    {
        [FieldOffset(0)] public KeyboardInput Keyboard;
        [FieldOffset(0)] public MouseInput Mouse;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KeyboardInput
    {
        public ushort VirtualKey;
        public ushort ScanCode;
        public uint Flags;
        public uint Time;
        public UIntPtr ExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MouseInput
    {
        public int X;
        public int Y;
        public uint MouseData;
        public uint Flags;
        public uint Time;
        public UIntPtr ExtraInfo;
    }

    private sealed class Cell
    {
        public IAccessible Accessible;
        public object ChildId;
        public int Left, Top, Width, Height;
    }

    private static string Safe(Func<string> getter)
    {
        try { return getter() ?? string.Empty; }
        catch { return string.Empty; }
    }

    private static void DumpAccessible(IAccessible accessible, object childId, string path, int depth)
    {
        string name = Safe(delegate { return accessible.get_accName(childId); });
        string value = Safe(delegate { return accessible.get_accValue(childId); });
        string role = string.Empty;
        string state = string.Empty;

        try { role = Convert.ToString(accessible.get_accRole(childId)); } catch { }
        try { state = Convert.ToString(accessible.get_accState(childId)); } catch { }

        int left = 0, top = 0, width = 0, height = 0;
        try { accessible.accLocation(out left, out top, out width, out height, childId); } catch { }

        Console.WriteLine(
            "{0}\tdepth={1}\trole={2}\tstate={3}\trect={4},{5},{6},{7}\tname={8}\tvalue={9}",
            path, depth, role, state, left, top, width, height, Escape(name), Escape(value));

        if (depth >= 8 || !(childId is int) || (int)childId != 0)
            return;

        int count;
        try { count = accessible.accChildCount; }
        catch { return; }

        for (int i = 1; i <= count; ++i)
        {
            object child;
            try { child = accessible.get_accChild(i); }
            catch { child = null; }

            IAccessible childAccessible = child as IAccessible;
            if (childAccessible != null)
                DumpAccessible(childAccessible, 0, path + "/" + i, depth + 1);
            else
                DumpAccessible(accessible, i, path + "/" + i, depth + 1);
        }
    }

    private static string Escape(string text)
    {
        return text.Replace("\\", "\\\\").Replace("\r", "\\r").Replace("\n", "\\n").Replace("\t", "\\t");
    }

    private static IAccessible GetAccessible(IntPtr window)
    {
        object candidate = null;
        Guid guid = IAccessibleGuid;
        int result = AccessibleObjectFromWindow(window, ObjIdClient, ref guid, ref candidate);
        return result == 0 ? candidate as IAccessible : null;
    }

    private static IntPtr GetMainWindow(Process process)
    {
        var matches = new List<IntPtr>();
        EnumWindows(delegate(IntPtr window, IntPtr parameter)
        {
            uint processId;
            GetWindowThreadProcessId(window, out processId);
            if (processId != process.Id || !IsWindowVisible(window))
                return true;
            var title = new StringBuilder(1024);
            GetWindowText(window, title, title.Capacity);
            if (title.ToString().StartsWith("USART HMI("))
                matches.Add(window);
            return true;
        }, IntPtr.Zero);
        if (matches.Count != 1)
            throw new InvalidOperationException("Expected exactly one titled USART HMI project window.");
        return matches[0];
    }

    private static List<IntPtr> FindDataGrids(Process process)
    {
        var candidates = new List<IntPtr>();
        EnumChildWindows(GetMainWindow(process), delegate(IntPtr window, IntPtr parameter)
        {
            IAccessible accessible = GetAccessible(window);
            string name = accessible == null ? string.Empty : Safe(delegate { return accessible.get_accName(0); });
            if (IsWindowVisible(window) && name == "DataGridView")
                candidates.Add(window);
            return true;
        }, IntPtr.Zero);
        return candidates;
    }

    private static List<IntPtr> FindObjectCombos(Process process)
    {
        var candidates = new List<IntPtr>();
        EnumChildWindows(GetMainWindow(process), delegate(IntPtr window, IntPtr parameter)
        {
            if (!IsWindowVisible(window))
                return true;
            var className = new StringBuilder(256);
            GetClassName(window, className, className.Capacity);
            IAccessible accessible = GetAccessible(window);
            if (className.ToString().StartsWith("WindowsForms10.COMBOBOX") && accessible != null &&
                accessible.accChildCount == 3)
                candidates.Add(window);
            return true;
        }, IntPtr.Zero);
        return candidates;
    }

    private static Cell FindValueCell(IAccessible grid, string propertyName, string oldValue)
    {
        Cell found = null;
        for (int rowIndex = 1; rowIndex <= grid.accChildCount; ++rowIndex)
        {
            object rowObject = grid.get_accChild(rowIndex);
            IAccessible row = rowObject as IAccessible;
            if (row == null)
                continue;

            string rowValue = Safe(delegate { return row.get_accValue(0); });
            if (rowValue != propertyName + ";" + oldValue)
                continue;

            object cellObject = row.get_accChild(2);
            IAccessible cellAccessible = cellObject as IAccessible;
            IAccessible owner = cellAccessible ?? row;
            object childId = cellAccessible != null ? (object)0 : (object)2;
            int left, top, width, height;
            owner.accLocation(out left, out top, out width, out height, childId);
            if (found != null)
                throw new InvalidOperationException("The requested property/value pair is not unique.");
            found = new Cell { Accessible = owner, ChildId = childId, Left = left, Top = top, Width = width, Height = height };
        }
        return found;
    }

    private static void SelectComboIndex(IntPtr comboWindow, int index)
    {
        IntPtr parentWindow = GetParent(comboWindow);
        int controlId = GetDlgCtrlID(comboWindow);
        SendMessageInt(comboWindow, 0x014E, new IntPtr(index), IntPtr.Zero);
        SendMessageInt(
            parentWindow, 0x0111,
            new IntPtr((1 << 16) | (controlId & 0xFFFF)), comboWindow);
        SendMessageInt(
            parentWindow, 0x0111,
            new IntPtr((9 << 16) | (controlId & 0xFFFF)), comboWindow);
    }

    private static int GetComboCount(IntPtr comboWindow)
    {
        long count = SendMessageInt(comboWindow, 0x0146, IntPtr.Zero, IntPtr.Zero).ToInt64();
        if (count < 1 || count > 1000)
            throw new InvalidOperationException("ComboBox returned an invalid item count.");
        return (int)count;
    }

    private static void DumpGridRows(IAccessible grid)
    {
        for (int rowIndex = 1; rowIndex <= grid.accChildCount; ++rowIndex)
        {
            object rowObject = grid.get_accChild(rowIndex);
            IAccessible row = rowObject as IAccessible;
            if (row == null)
                continue;
            Console.WriteLine("  {0}", Escape(Safe(delegate { return row.get_accValue(0); })));
        }
    }

    private static void Click(int x, int y, bool doubleClick)
    {
        SetCursorPos(
            (int)Math.Round(x * ClickScale),
            (int)Math.Round(y * ClickScale));
        mouse_event(0x0002, 0, 0, 0, UIntPtr.Zero);
        mouse_event(0x0004, 0, 0, 0, UIntPtr.Zero);
        if (doubleClick)
        {
            Thread.Sleep(80);
            mouse_event(0x0002, 0, 0, 0, UIntPtr.Zero);
            mouse_event(0x0004, 0, 0, 0, UIntPtr.Zero);
        }
    }

    private static void SendUnicode(string text)
    {
        var inputs = new List<Input>();
        foreach (char character in text)
        {
            inputs.Add(new Input
            {
                Type = 1,
                Union = new InputUnion
                {
                    Keyboard = new KeyboardInput { ScanCode = character, Flags = 0x0004 }
                }
            });
            inputs.Add(new Input
            {
                Type = 1,
                Union = new InputUnion
                {
                    Keyboard = new KeyboardInput { ScanCode = character, Flags = 0x0004 | 0x0002 }
                }
            });
        }
        if (SendInput((uint)inputs.Count, inputs.ToArray(), Marshal.SizeOf(typeof(Input))) != inputs.Count)
            throw new InvalidOperationException("SendInput did not accept every Unicode keyboard event.");
    }

    private static void DumpChildWindows(Process process)
    {
        EnumChildWindows(GetMainWindow(process), delegate(IntPtr window, IntPtr parameter)
        {
            if (!IsWindowVisible(window))
                return true;
            var className = new StringBuilder(256);
            var text = new StringBuilder(1024);
            GetClassName(window, className, className.Capacity);
            GetWindowText(window, text, text.Capacity);
            Rect rect;
            GetWindowRect(window, out rect);
            Console.WriteLine(
                "CHILD\thwnd=0x{0:X}\tclass={1}\trect={2},{3},{4},{5}\ttext={6}",
                window.ToInt64(), Escape(className.ToString()), rect.Left, rect.Top,
                rect.Right - rect.Left, rect.Bottom - rect.Top, Escape(text.ToString()));
            return true;
        }, IntPtr.Zero);
    }

    private static List<IntPtr> FindVisibleEdits(Process process, Rect bounds, string expectedText)
    {
        var edits = new List<IntPtr>();
        EnumChildWindows(GetMainWindow(process), delegate(IntPtr window, IntPtr parameter)
        {
            if (!IsWindowVisible(window))
                return true;
            var className = new StringBuilder(256);
            var text = new StringBuilder(1024);
            GetClassName(window, className, className.Capacity);
            GetWindowText(window, text, text.Capacity);
            Rect rect;
            GetWindowRect(window, out rect);
            if (className.ToString() == "Edit" && (expectedText == null || text.ToString() == expectedText) &&
                rect.Left >= bounds.Left && rect.Top >= bounds.Top &&
                rect.Right <= bounds.Right && rect.Bottom <= bounds.Bottom)
                edits.Add(window);
            return true;
        }, IntPtr.Zero);
        return edits;
    }

    [STAThread]
    public static int Main(string[] args)
    {
        SetThreadDpiAwarenessContext(new IntPtr(-4));
        Process[] processes = Process.GetProcessesByName("USART HMI");
        if (processes.Length != 1)
        {
            Console.Error.WriteLine("Expected exactly one USART HMI process, found {0}.", processes.Length);
            return 2;
        }
        Process process = processes[0];
        uint dpi = GetDpiForWindow(GetMainWindow(process));
        Rect logicalFrame;
        Rect physicalFrame;
        GetWindowRect(GetMainWindow(process), out logicalFrame);
        int dwmResult = DwmGetWindowAttribute(
            GetMainWindow(process), 9, out physicalFrame, Marshal.SizeOf(typeof(Rect)));
        int logicalWidth = logicalFrame.Right - logicalFrame.Left;
        int physicalWidth = physicalFrame.Right - physicalFrame.Left;
        ClickScale = 1.0;

        if (args.Length == 2 && args[0] == "copy-editor")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            IntPtr editorWindow = new IntPtr(raw);
            Rect editorRect;
            if (!GetWindowRect(editorWindow, out editorRect) ||
                editorRect.Right <= editorRect.Left || editorRect.Bottom <= editorRect.Top)
                return 40;
            System.Drawing.Bitmap originalImage = null;
            string originalText = null;
            try
            {
                if (Clipboard.ContainsImage())
                    originalImage = new System.Drawing.Bitmap(Clipboard.GetImage());
                else if (Clipboard.ContainsText())
                    originalText = Clipboard.GetText();
                const string clipboardSentinel = "__HMI_UI_PROBE_CLIPBOARD_SENTINEL__";
                Clipboard.SetText(clipboardSentinel);
                uint targetProcessId;
                uint targetThread = GetWindowThreadProcessId(editorWindow, out targetProcessId);
                uint currentThread = GetCurrentThreadId();
                uint foregroundProcessId;
                uint foregroundThread = GetWindowThreadProcessId(
                    GetForegroundWindow(), out foregroundProcessId);
                if (targetProcessId != process.Id || targetThread == 0 ||
                    !AttachThreadInput(currentThread, targetThread, true))
                    return 42;
                bool foregroundAttached = foregroundThread != 0 && foregroundThread != targetThread &&
                    AttachThreadInput(currentThread, foregroundThread, true);
                try
                {
                    IntPtr mainWindow = GetMainWindow(process);
                    SetForegroundWindow(mainWindow);
                    Rect clientRect;
                    GetClientRect(editorWindow, out clientRect);
                    int clientX = (clientRect.Right - clientRect.Left) / 2;
                    int clientY = (clientRect.Bottom - clientRect.Top) / 2;
                    IntPtr clientPoint = new IntPtr((clientY << 16) | (clientX & 0xFFFF));
                    SendMessageInt(editorWindow, 0x0201, new IntPtr(1), clientPoint);
                    SendMessageInt(editorWindow, 0x0202, IntPtr.Zero, clientPoint);
                    SetFocus(editorWindow);
                    if (GetForegroundWindow() != mainWindow || GetFocus() != editorWindow)
                        return 43;
                    keybd_event(0x11, 0, 0, UIntPtr.Zero);
                    keybd_event(0x41, 0, 0, UIntPtr.Zero);
                    keybd_event(0x41, 0, 0x0002, UIntPtr.Zero);
                    keybd_event(0x11, 0, 0x0002, UIntPtr.Zero);
                    keybd_event(0x11, 0, 0, UIntPtr.Zero);
                    keybd_event(0x43, 0, 0, UIntPtr.Zero);
                    keybd_event(0x43, 0, 0x0002, UIntPtr.Zero);
                    keybd_event(0x11, 0, 0x0002, UIntPtr.Zero);
                    Thread.Sleep(300);
                }
                finally
                {
                    if (foregroundAttached)
                        AttachThreadInput(currentThread, foregroundThread, false);
                    AttachThreadInput(currentThread, targetThread, false);
                }
                string text = Clipboard.ContainsText() ? Clipboard.GetText() : string.Empty;
                if (text.Length == 0 || text == clipboardSentinel)
                    return 41;
                Console.WriteLine("EDITOR_TEXT_BEGIN");
                Console.WriteLine(text);
                Console.WriteLine("EDITOR_TEXT_END");
            }
            finally
            {
                if (originalImage != null)
                    Clipboard.SetImage(originalImage);
                else if (originalText != null)
                    Clipboard.SetText(originalText);
            }
            return 0;
        }

        if (args.Length == 5 && args[0] == "direct-menu-click")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            IntPtr toolbarWindow = new IntPtr(raw);
            int firstIndex = int.Parse(args[2]);
            int secondIndex = int.Parse(args[3]);
            IAccessible root = GetAccessible(toolbarWindow);
            IAccessible first = root == null ? null : root.get_accChild(firstIndex) as IAccessible;
            if (first == null || Safe(delegate { return first.get_accName(secondIndex); }) != args[4])
                return 37;
            Rect toolbarRect;
            GetWindowRect(toolbarWindow, out toolbarRect);
            int ml, mt, mw, mh;
            first.accLocation(out ml, out mt, out mw, out mh, 0);
            int menuX = ml + mw / 2 - toolbarRect.Left;
            int menuY = mt + mh / 2 - toolbarRect.Top;
            IntPtr menuPoint = new IntPtr((menuY << 16) | (menuX & 0xFFFF));
            SendMessageInt(toolbarWindow, 0x0201, new IntPtr(1), menuPoint);
            SendMessageInt(toolbarWindow, 0x0202, IntPtr.Zero, menuPoint);
            Thread.Sleep(250);

            int itemState = Convert.ToInt32(first.get_accState(secondIndex));
            object secondObject = first.get_accChild(secondIndex);
            IAccessible second = secondObject as IAccessible;
            if ((itemState & 0x8000) != 0 || second == null)
                return 38;
            IntPtr popupWindow;
            if (WindowFromAccessibleObject(second, out popupWindow) != 0 || popupWindow == IntPtr.Zero)
                return 39;
            Rect popupRect;
            GetWindowRect(popupWindow, out popupRect);
            int il, it, iw, ih;
            second.accLocation(out il, out it, out iw, out ih, 0);
            int itemX = il + iw / 2 - popupRect.Left;
            int itemY = it + ih / 2 - popupRect.Top;
            IntPtr itemPoint = new IntPtr((itemY << 16) | (itemX & 0xFFFF));
            SendMessageInt(popupWindow, 0x0201, new IntPtr(1), itemPoint);
            SendMessageInt(popupWindow, 0x0202, IntPtr.Zero, itemPoint);
            Thread.Sleep(800);
            Console.WriteLine(
                "Direct-clicked menu item {0}: toolbar=0x{1:X} popup=0x{2:X} client={3},{4}",
                args[4], toolbarWindow.ToInt64(), popupWindow.ToInt64(), itemX, itemY);
            return 0;
        }

        if (args.Length == 4 && args[0] == "menu-metrics")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            int firstIndex = int.Parse(args[2]);
            int secondIndex = int.Parse(args[3]);
            IAccessible root = GetAccessible(new IntPtr(raw));
            IAccessible first = root == null ? null : root.get_accChild(firstIndex) as IAccessible;
            if (first == null)
                return 36;
            int ml, mt, mw, mh, il, it, iw, ih;
            first.accLocation(out ml, out mt, out mw, out mh, 0);
            first.accLocation(out il, out it, out iw, out ih, secondIndex);
            Console.WriteLine(
                "dpi={0} scale={1:F2} menu={2},{3},{4},{5} item={6},{7},{8},{9} state=0x{10:X}",
                dpi, ClickScale, ml, mt, mw, mh, il, it, iw, ih,
                Convert.ToInt32(first.get_accState(secondIndex)));
            return 0;
        }

        if (args.Length == 3 && args[0] == "click-point")
        {
            int x = int.Parse(args[1]);
            int y = int.Parse(args[2]);
            SetForegroundWindow(GetMainWindow(process));
            Click(x, y, false);
            Thread.Sleep(300);
            Console.WriteLine("Clicked point {0},{1}.", x, y);
            return 0;
        }

        if (args.Length == 4 && args[0] == "scan-page-keys")
        {
            int x = int.Parse(args[1]);
            int y = int.Parse(args[2]);
            int count = int.Parse(args[3]);
            if (count < 1 || count > 100)
                return 23;
            SetForegroundWindow(GetMainWindow(process));
            Click(x, y, false);
            Thread.Sleep(300);
            keybd_event(0x24, 0, 0, UIntPtr.Zero);
            keybd_event(0x24, 0, 0x0002, UIntPtr.Zero);
            Thread.Sleep(300);
            for (int index = 0; index < count; ++index)
            {
                List<IntPtr> combos = FindObjectCombos(process);
                string value = combos.Count == 1
                    ? Safe(delegate { return GetAccessible(combos[0]).get_accValue(0); })
                    : string.Empty;
                Console.WriteLine("PAGE_KEY_INDEX {0} value={1}", index, value);
                keybd_event(0x28, 0, 0, UIntPtr.Zero);
                keybd_event(0x28, 0, 0x0002, UIntPtr.Zero);
                Thread.Sleep(400);
            }
            return 0;
        }

        if (args.Length == 2 && args[0] == "scan-pages")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            IntPtr pageList = new IntPtr(raw);
            Rect listRect;
            if (!GetWindowRect(pageList, out listRect) ||
                listRect.Right - listRect.Left < 150 || listRect.Right - listRect.Left > 400 ||
                listRect.Bottom - listRect.Top < 250 || listRect.Bottom - listRect.Top > 800)
                return 21;
            List<IntPtr> combos = FindObjectCombos(process);
            List<IntPtr> grids = FindDataGrids(process);
            if (combos.Count != 1 || grids.Count != 1)
                return 22;
            var seen = new HashSet<string>();
            SetForegroundWindow(GetMainWindow(process));
            for (int y = listRect.Top + 4; y < listRect.Bottom - 4; y += 8)
            {
                Click(listRect.Left + (listRect.Right - listRect.Left) / 2, y, false);
                Thread.Sleep(100);
                combos = FindObjectCombos(process);
                grids = FindDataGrids(process);
                if (combos.Count != 1 || grids.Count != 1)
                    continue;
                IAccessible combo = GetAccessible(combos[0]);
                string value = Safe(delegate { return combo.get_accValue(0); });
                IAccessible grid = GetAccessible(grids[0]);
                if (value.EndsWith("(页面)") && FindValueCell(grid, "type", "121") != null && seen.Add(value))
                    Console.WriteLine("PAGE y={0} value={1}", y, value);
            }
            return 0;
        }

        if (args.Length == 1 && args[0] == "inventory-page")
        {
            List<IntPtr> combos = FindObjectCombos(process);
            List<IntPtr> grids = FindDataGrids(process);
            if (combos.Count != 1 || grids.Count != 1)
                return 19;
            int itemCount = GetComboCount(combos[0]);
            for (int index = 0; index < itemCount; ++index)
            {
                SelectComboIndex(combos[0], index);
                Thread.Sleep(200);
                Console.WriteLine("OBJECT_INDEX {0}", index);
                DumpGridRows(GetAccessible(grids[0]));
            }
            return 0;
        }

        if (args.Length == 2 && args[0] == "select-object")
        {
            List<IntPtr> combos = FindObjectCombos(process);
            List<IntPtr> grids = FindDataGrids(process);
            if (combos.Count != 1 || grids.Count != 1)
            {
                Console.Error.WriteLine("Expected one object ComboBox and one DataGridView, found {0} and {1}.", combos.Count, grids.Count);
                return 16;
            }
            int itemCount = GetComboCount(combos[0]);
            for (int index = 0; index < itemCount; ++index)
            {
                SelectComboIndex(combos[0], index);
                Thread.Sleep(300);
                IAccessible grid = GetAccessible(grids[0]);
                if (FindValueCell(grid, "objname", args[1]) != null)
                {
                    Console.WriteLine("Selected object {0} at ComboBox index {1}.", args[1], index);
                    return 0;
                }
            }
            Console.Error.WriteLine("Object {0} was not found after scanning {1} ComboBox items.", args[1], itemCount);
            return 18;
        }

        if (args.Length == 1 && args[0] == "main-accessible")
        {
            IAccessible mainAccessible = GetAccessible(GetMainWindow(process));
            if (mainAccessible == null)
                return 12;
            DumpAccessible(mainAccessible, 0, "MAIN", 0);
            return 0;
        }

        if (args.Length == 2 && args[0] == "accessible-hwnd")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            IAccessible accessible = GetAccessible(new IntPtr(raw));
            if (accessible == null)
                return 13;
            DumpAccessible(accessible, 0, "0x" + raw.ToString("X"), 0);
            return 0;
        }

        if (args.Length == 4 && args[0] == "invoke-child")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            int childIndex = int.Parse(args[2]);
            IAccessible accessible = GetAccessible(new IntPtr(raw));
            if (accessible == null || childIndex < 1 || childIndex > accessible.accChildCount)
                return 14;
            object childObject = accessible.get_accChild(childIndex);
            IAccessible childAccessible = childObject as IAccessible;
            IAccessible owner = childAccessible ?? accessible;
            object childId = childAccessible != null ? (object)0 : (object)childIndex;
            string name = Safe(delegate { return owner.get_accName(childId); });
            if (name != args[3])
            {
                Console.Error.WriteLine("Accessible child name mismatch: expected {0}, found {1}.", args[3], name);
                return 15;
            }
            owner.accDoDefaultAction(childId);
            Thread.Sleep(1500);
            Console.WriteLine("Invoked accessible child: {0}", name);
            return 0;
        }

        if (args.Length == 5 && args[0] == "invoke-grandchild")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            int firstIndex = int.Parse(args[2]);
            int secondIndex = int.Parse(args[3]);
            IAccessible root = GetAccessible(new IntPtr(raw));
            if (root == null || firstIndex < 1 || firstIndex > root.accChildCount)
                return 24;
            IAccessible first = root.get_accChild(firstIndex) as IAccessible;
            if (first == null || secondIndex < 1 || secondIndex > first.accChildCount)
                return 25;
            object secondObject = first.get_accChild(secondIndex);
            IAccessible secondAccessible = secondObject as IAccessible;
            IAccessible owner = secondAccessible ?? first;
            object childId = secondAccessible != null ? (object)0 : (object)secondIndex;
            string name = Safe(delegate { return owner.get_accName(childId); });
            if (name != args[4])
            {
                Console.Error.WriteLine("Accessible grandchild name mismatch: expected {0}, found {1}.", args[4], name);
                return 26;
            }
            owner.accDoDefaultAction(childId);
            Thread.Sleep(800);
            Console.WriteLine("Invoked accessible grandchild: {0}", name);
            return 0;
        }

        if (args.Length == 5 && args[0] == "open-menu-click")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            int firstIndex = int.Parse(args[2]);
            int secondIndex = int.Parse(args[3]);
            IAccessible root = GetAccessible(new IntPtr(raw));
            IAccessible first = root == null ? null : root.get_accChild(firstIndex) as IAccessible;
            if (first == null || secondIndex < 1 || secondIndex > first.accChildCount)
                return 27;
            string initialName = Safe(delegate { return first.get_accName(secondIndex); });
            if (initialName != args[4])
                return 28;
            SetForegroundWindow(GetMainWindow(process));
            int menuLeft, menuTop, menuWidth, menuHeight;
            first.accLocation(out menuLeft, out menuTop, out menuWidth, out menuHeight, 0);
            if (menuWidth < 20 || menuHeight < 10)
                return 29;
            Click(menuLeft + menuWidth / 2, menuTop + menuHeight / 2, false);
            Thread.Sleep(200);
            Click(menuLeft + menuWidth / 2, menuTop + menuHeight / 2, false);
            Thread.Sleep(250);
            int left, top, width, height;
            first.accLocation(out left, out top, out width, out height, secondIndex);
            string visibleName = Safe(delegate { return first.get_accName(secondIndex); });
            int visibleState = Convert.ToInt32(first.get_accState(secondIndex));
            if (visibleName != args[4] || width < 20 || height < 10 || (visibleState & 0x8000) != 0)
            {
                Point cursor;
                GetCursorPos(out cursor);
                Console.Error.WriteLine(
                    "Menu did not open: scale={0:F2} menu={1},{2},{3},{4} requested={5},{6} cursor={7},{8} item={9},{10},{11},{12} state=0x{13:X}.",
                    ClickScale, menuLeft, menuTop, menuWidth, menuHeight,
                    (int)Math.Round((menuLeft + menuWidth / 2) * ClickScale),
                    (int)Math.Round((menuTop + menuHeight / 2) * ClickScale),
                    cursor.X, cursor.Y, left, top, width, height, visibleState);
                return 35;
            }
            Click(left + width / 2, top + height / 2, false);
            Thread.Sleep(800);
            Console.WriteLine("Clicked visible menu item {0} at {1},{2},{3},{4}", visibleName, left, top, width, height);
            return 0;
        }

        if (args.Length == 5 && args[0] == "open-menu-action")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            int firstIndex = int.Parse(args[2]);
            int secondIndex = int.Parse(args[3]);
            IAccessible root = GetAccessible(new IntPtr(raw));
            IAccessible first = root == null ? null : root.get_accChild(firstIndex) as IAccessible;
            if (first == null || secondIndex < 1 || secondIndex > first.accChildCount)
                return 30;
            if (Safe(delegate { return first.get_accName(secondIndex); }) != args[4])
                return 31;
            int state = Convert.ToInt32(first.get_accState(secondIndex));
            if ((state & 0x8000) != 0)
            {
                first.accDoDefaultAction(0);
                Thread.Sleep(250);
            }
            state = Convert.ToInt32(first.get_accState(secondIndex));
            if ((state & 0x8000) != 0)
                return 32;
            object secondObject = first.get_accChild(secondIndex);
            IAccessible secondAccessible = secondObject as IAccessible;
            if (secondAccessible != null)
                secondAccessible.accDoDefaultAction(0);
            else
                first.accDoDefaultAction(secondIndex);
            Thread.Sleep(800);
            Console.WriteLine("Invoked visible menu item: {0}", args[4]);
            return 0;
        }

        if (args.Length == 4 && args[0] == "open-menu-last")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            int firstIndex = int.Parse(args[2]);
            IAccessible root = GetAccessible(new IntPtr(raw));
            IAccessible first = root == null ? null : root.get_accChild(firstIndex) as IAccessible;
            if (first == null || first.accChildCount < 1)
                return 33;
            if (Safe(delegate { return first.get_accName(first.accChildCount); }) != args[3])
                return 34;
            SetForegroundWindow(GetMainWindow(process));
            first.accDoDefaultAction(0);
            Thread.Sleep(250);
            keybd_event(0x23, 0, 0, UIntPtr.Zero);
            keybd_event(0x23, 0, 0x0002, UIntPtr.Zero);
            keybd_event(0x0D, 0, 0, UIntPtr.Zero);
            keybd_event(0x0D, 0, 0x0002, UIntPtr.Zero);
            Thread.Sleep(800);
            Console.WriteLine("Invoked last menu item by keyboard: {0}", args[3]);
            return 0;
        }

        if (args.Length == 1 && args[0] == "save")
        {
            SetForegroundWindow(GetMainWindow(process));
            Rect mainRect;
            GetWindowRect(GetMainWindow(process), out mainRect);
            Click(mainRect.Left + Math.Min(500, (mainRect.Right - mainRect.Left) / 2), mainRect.Top + 11, false);
            Thread.Sleep(150);
            keybd_event(0x11, 0, 0, UIntPtr.Zero);
            keybd_event(0x53, 0, 0, UIntPtr.Zero);
            keybd_event(0x53, 0, 0x0002, UIntPtr.Zero);
            keybd_event(0x11, 0, 0x0002, UIntPtr.Zero);
            Thread.Sleep(1500);
            Console.WriteLine("Sent Ctrl+S to USART HMI.");
            return 0;
        }

        if (args.Length == 3 && args[0] == "set-active-edit")
        {
            List<IntPtr> grids = FindDataGrids(process);
            if (grids.Count != 1)
            {
                Console.Error.WriteLine("Expected exactly one visible DataGridView, found {0}.", grids.Count);
                return 7;
            }
            Rect gridRect;
            GetWindowRect(grids[0], out gridRect);
            IAccessible grid = GetAccessible(grids[0]);
            if (FindValueCell(grid, "txt", args[1]) == null)
            {
                Console.Error.WriteLine("The current txt property does not match the expected old value.");
                return 8;
            }
            List<IntPtr> edits = FindVisibleEdits(process, gridRect, null);
            if (edits.Count != 1)
            {
                Console.Error.WriteLine("Expected exactly one matching active Edit control, found {0}.", edits.Count);
                return 9;
            }

            SetForegroundWindow(GetMainWindow(process));
            Rect editRect;
            GetWindowRect(edits[0], out editRect);
            Click((editRect.Left + editRect.Right) / 2, (editRect.Top + editRect.Bottom) / 2, false);
            Thread.Sleep(100);
            keybd_event(0x11, 0, 0, UIntPtr.Zero);
            keybd_event(0x41, 0, 0, UIntPtr.Zero);
            keybd_event(0x41, 0, 0x0002, UIntPtr.Zero);
            keybd_event(0x11, 0, 0x0002, UIntPtr.Zero);
            SendUnicode(args[2]);
            Thread.Sleep(150);

            keybd_event(0x0D, 0, 0, UIntPtr.Zero);
            keybd_event(0x0D, 0, 0x0002, UIntPtr.Zero);
            Thread.Sleep(700);

            Cell committed = FindValueCell(grid, "txt", args[2]);
            if (committed == null)
            {
                Console.Error.WriteLine("The committed txt property could not be read back.");
                return 11;
            }
            Console.WriteLine("Committed txt value: {0}", args[2]);
            return 0;
        }

        if (args.Length == 2 && args[0] == "click-hwnd")
        {
            long raw = Convert.ToInt64(args[1].StartsWith("0x") ? args[1].Substring(2) : args[1], 16);
            IntPtr window = new IntPtr(raw);
            Rect rect;
            if (!GetWindowRect(window, out rect) || rect.Right <= rect.Left || rect.Bottom <= rect.Top)
            {
                Console.Error.WriteLine("Invalid or empty target window rectangle.");
                return 3;
            }
            SetForegroundWindow(GetMainWindow(process));
            Rect mainRect;
            GetWindowRect(GetMainWindow(process), out mainRect);
            Click(mainRect.Left + Math.Min(500, (mainRect.Right - mainRect.Left) / 2), mainRect.Top + 11, false);
            Thread.Sleep(150);
            Click((rect.Left + rect.Right) / 2, (rect.Top + rect.Bottom) / 2, false);
            Thread.Sleep(500);
            Console.WriteLine("Clicked hwnd=0x{0:X} rect={1},{2},{3},{4}", raw, rect.Left, rect.Top, rect.Right - rect.Left, rect.Bottom - rect.Top);
            return 0;
        }

        if (args.Length == 3 && args[0] == "activate-cell")
        {
            List<IntPtr> grids = FindDataGrids(process);
            if (grids.Count != 1)
            {
                Console.Error.WriteLine("Expected exactly one visible DataGridView, found {0}.", grids.Count);
                return 4;
            }
            IAccessible grid = GetAccessible(grids[0]);
            Cell cell = FindValueCell(grid, args[1], args[2]);
            if (cell == null)
            {
                Console.Error.WriteLine("Requested property/value cell was not found.");
                return 5;
            }
            SetForegroundWindow(GetMainWindow(process));
            Thread.Sleep(150);
            cell.Accessible.accSelect(3, cell.ChildId);
            Thread.Sleep(250);
            cell.Accessible.accLocation(
                out cell.Left, out cell.Top, out cell.Width, out cell.Height, cell.ChildId);
            Rect gridRect;
            GetWindowRect(grids[0], out gridRect);
            int clickX = cell.Left + cell.Width / 2;
            int clickY = cell.Top + cell.Height / 2;
            if (clickX < gridRect.Left || clickX >= gridRect.Right ||
                clickY < gridRect.Top || clickY >= gridRect.Bottom)
            {
                Console.Error.WriteLine(
                    "Selected cell is still outside the visible grid: cell={0},{1},{2},{3}; grid={4},{5},{6},{7}.",
                    cell.Left, cell.Top, cell.Width, cell.Height,
                    gridRect.Left, gridRect.Top, gridRect.Right - gridRect.Left, gridRect.Bottom - gridRect.Top);
                return 6;
            }
            Click(clickX, clickY, true);
            Thread.Sleep(500);
            Console.WriteLine("Activated {0};{1} rect={2},{3},{4},{5}", args[1], args[2], cell.Left, cell.Top, cell.Width, cell.Height);
            DumpChildWindows(process);
            return 0;
        }

        if (args.Length == 1 && args[0] == "windows")
        {
            DumpChildWindows(process);
            return 0;
        }

        List<IntPtr> candidates = FindDataGrids(process);

        foreach (IntPtr window in candidates)
        {
            IAccessible accessible = GetAccessible(window);
            if (accessible == null)
                continue;

            string name = Safe(delegate { return accessible.get_accName(0); });
            Console.WriteLine("WINDOW\thwnd=0x{0:X}\tchildren={1}\tname={2}", window.ToInt64(), accessible.accChildCount, Escape(name));
            DumpAccessible(accessible, 0, "0x" + window.ToInt64().ToString("X"), 0);
        }
        return 0;
    }
}
