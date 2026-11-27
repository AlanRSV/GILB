<#
gilb_keys.ps1 -- a PC keyboard for GILB programs, over the player's USB serial port.

    powershell -ExecutionPolicy Bypass -File tools\gilb_keys.ps1

With a program running that opened the port (API version 4, serial_open; Doom does), the player is a
COM port on the PC (USB VID 071B, PID 3209). This opens a window with a keyboard on it: while the
window has the focus, each key press and release goes to the player as one byte, the Windows key code
(below 128), bit 7 set for a release; 0x80 lets go of every key, and that and the keys held go again
4 times a second, so a byte lost while the program stalled does not leave a key held
(docs/PROGRAMS.md). It finds the port by itself and reconnects when the program starts again.

The window shows the keys held (green), the keys Doom uses (outlined, with what they do), whether the
player is connected, and what Doom is being told to do. Close the window to stop.

    -Demo                 for filming: a bigger window, titles saying what it shows, and the last key
                          presses in a strip that fades out
    -Snapshot file.png    draws the window into a picture and exits (to check its look)
#>
param([string]$Snapshot, [switch]$Demo)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# The window, drawn in C# (compiled here: nothing to install). ASCII only: Windows PowerShell reads
# this file as ANSI, so other characters are written as \u escapes.
Add-Type -ReferencedAssemblies System.Windows.Forms, System.Drawing -ErrorAction Stop -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Windows.Forms;

// The form: every key reaches KeyDown (arrows, Tab, Enter), and Alt or F10 do not open the window
// menu (it took the next key).
public class GilbForm : Form
{
    protected override bool IsInputKey(Keys keyData) { return true; }
    protected override bool ProcessDialogKey(Keys keyData) { return false; }
    protected override void WndProc(ref Message m)
    {
        if (m.Msg == 0x0112 && (m.WParam.ToInt64() & 0xFFF0) == 0xF100)   // WM_SYSCOMMAND SC_KEYMENU
            return;
        base.WndProc(ref m);
    }
}

public class GilbKeyboard : Control
{
    class Key
    {
        public float X, Y, W; public string Label; public int Vk;
        public Key(float x, float y, float w, string label, int vk) { X = x; Y = y; W = w; Label = label; Vk = vk; }
    }

    readonly List<Key> keys = new List<Key>();
    readonly HashSet<int> held = new HashSet<int>();
    readonly Dictionary<int, string[]> doom = new Dictionary<int, string[]>();
    readonly int[] order = { 0x57, 0x26, 0x53, 0x28, 0x25, 0x27, 0x41, 0x44, 0x10, 0x11, 0x0D, 0x20, 0x51, 0x45, 0x09, 0x1B };
    bool connected; string portName = ""; bool active = true; int sent;

    // -Demo (for filming): its own titles, and the last key presses, fading out over FadeMs.
    bool demo;
    readonly List<string> recentName = new List<string>();
    readonly List<DateTime> recentAt = new List<DateTime>();
    Timer fade;
    const double FadeMs = 4000;

    static readonly Color Bg = Color.FromArgb(13, 14, 24);
    static readonly Color Face = Color.FromArgb(27, 30, 46);
    static readonly Color FaceDoom = Color.FromArgb(25, 36, 56);
    static readonly Color Edge = Color.FromArgb(48, 54, 78);
    static readonly Color Ink = Color.FromArgb(214, 220, 236);
    static readonly Color Dim = Color.FromArgb(118, 126, 152);
    static readonly Color Cyan = Color.FromArgb(60, 214, 255);
    static readonly Color Pink = Color.FromArgb(255, 92, 214);
    static readonly Color Green = Color.FromArgb(52, 224, 128);
    static readonly Color Amber = Color.FromArgb(255, 184, 64);

    const float UnitsW = 18.5f, UnitsH = 6.5f;

    public GilbKeyboard()
    {
        SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint |
                 ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
        SetStyle(ControlStyles.Selectable, false);
        BackColor = Bg;

        Doom(0x57, "FWD", "Forward"); Doom(0x26, "FWD", "Forward");
        Doom(0x53, "BACK", "Backward"); Doom(0x28, "BACK", "Backward");
        Doom(0x25, "TURN", "Turn left"); Doom(0x27, "TURN", "Turn right");
        Doom(0x41, "STRAFE", "Strafe left"); Doom(0x44, "STRAFE", "Strafe right");
        Doom(0x10, "RUN", "Run"); Doom(0x11, "FIRE", "Fire"); Doom(0x0D, "FIRE", "Fire");
        Doom(0x20, "USE", "Use"); Doom(0x51, "WPN -", "Previous weapon"); Doom(0x45, "WPN +", "Next weapon");
        Doom(0x09, "MAP", "Map"); Doom(0x1B, "MENU", "Menu");

        float x = 0, y = 0;
        Add(ref x, y, 1, "Esc", 27); x += 1;
        for (int i = 0; i < 12; i++) {
            Add(ref x, y, 1, "F" + (i + 1), 112 + i);
            if (i == 3 || i == 7) x += 0.5f;
        }
        x = 0; y = 1.5f;
        Add(ref x, y, 1, "`", 192);
        string digits = "1234567890";
        for (int i = 0; i < 10; i++) Add(ref x, y, 1, digits.Substring(i, 1), digits[i]);
        Add(ref x, y, 1, "-", 189); Add(ref x, y, 1, "=", 187); Add(ref x, y, 2, "Backspace", 8);
        x = 0; y = 2.5f;
        Add(ref x, y, 1.5f, "Tab", 9); Letters(ref x, y, "QWERTYUIOP");
        Add(ref x, y, 1, "[", 219); Add(ref x, y, 1, "]", 221); Add(ref x, y, 1.5f, "\\", 220);
        x = 0; y = 3.5f;
        Add(ref x, y, 1.75f, "Caps", 20); Letters(ref x, y, "ASDFGHJKL");
        Add(ref x, y, 1, ";", 186); Add(ref x, y, 1, "'", 222); Add(ref x, y, 2.25f, "Enter", 13);
        x = 0; y = 4.5f;
        Add(ref x, y, 2.25f, "Shift", 16); Letters(ref x, y, "ZXCVBNM");
        Add(ref x, y, 1, ",", 188); Add(ref x, y, 1, ".", 190); Add(ref x, y, 1, "/", 191);
        Add(ref x, y, 2.75f, "Shift", 16);
        x = 0; y = 5.5f;
        Add(ref x, y, 1.25f, "Ctrl", 17); Add(ref x, y, 1.25f, "Win", 91); Add(ref x, y, 1.25f, "Alt", 18);
        Add(ref x, y, 6.25f, "Space", 32); Add(ref x, y, 1.25f, "Alt", 18); Add(ref x, y, 1.25f, "Win", 92);
        Add(ref x, y, 1.25f, "Menu", 93); Add(ref x, y, 1.25f, "Ctrl", 17);
        float n = 15.5f;
        x = n; y = 1.5f;
        Add(ref x, y, 1, "Ins", 45); Add(ref x, y, 1, "Home", 36); Add(ref x, y, 1, "PgUp", 33);
        x = n; y = 2.5f;
        Add(ref x, y, 1, "Del", 46); Add(ref x, y, 1, "End", 35); Add(ref x, y, 1, "PgDn", 34);
        x = n + 1; y = 4.5f;
        Add(ref x, y, 1, "\u2191", 38);
        x = n; y = 5.5f;
        Add(ref x, y, 1, "\u2190", 37); Add(ref x, y, 1, "\u2193", 40); Add(ref x, y, 1, "\u2192", 39);
    }

    void Doom(int vk, string shortName, string longName) { doom[vk] = new string[] { shortName, longName }; }
    void Add(ref float x, float y, float w, string label, int vk) { keys.Add(new Key(x, y, w, label, vk)); x += w; }
    void Letters(ref float x, float y, string s) { foreach (char c in s) Add(ref x, y, 1, c.ToString(), c); }

    public void Press(int vk)
    {
        if (!held.Add(vk))
            return;
        if (demo) {
            string name = null;
            string[] act;
            foreach (Key k in keys)
                if (k.Vk == vk) { name = k.Label; break; }
            if (name == null) name = "#" + vk;
            if (doom.TryGetValue(vk, out act)) name += "  " + act[1];
            recentName.Insert(0, name);
            recentAt.Insert(0, DateTime.Now);
            if (recentName.Count > 8) { recentName.RemoveAt(8); recentAt.RemoveAt(8); }
        }
        Invalidate();
    }

    public void SetDemo()
    {
        demo = true;
        fade = new Timer();
        fade.Interval = 80;
        fade.Tick += delegate {
            if (recentAt.Count > 0) {
                while (recentAt.Count > 0 && (DateTime.Now - recentAt[recentAt.Count - 1]).TotalMilliseconds > FadeMs) {
                    recentAt.RemoveAt(recentAt.Count - 1);
                    recentName.RemoveAt(recentName.Count - 1);
                }
                Invalidate();
            }
        };
        fade.Start();
        Invalidate();
    }
    public void Release(int vk) { if (held.Remove(vk)) Invalidate(); }
    public void ReleaseAll() { held.Clear(); Invalidate(); }
    public void CountSent() { sent++; Invalidate(); }
    public void SetStatus(bool isConnected, string port) { connected = isConnected; portName = port ?? ""; Invalidate(); }
    public void SetActive(bool isActive) { active = isActive; Invalidate(); }

    static GraphicsPath Round(RectangleF r, float rad)
    {
        GraphicsPath p = new GraphicsPath();
        float d = Math.Min(rad * 2, Math.Min(r.Width, r.Height));
        if (d <= 0) { p.AddRectangle(r); return p; }
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    static void Centered(Graphics g, string s, Font f, Color c, RectangleF r)
    {
        using (StringFormat sf = new StringFormat())
        using (SolidBrush b = new SolidBrush(c)) {
            sf.Alignment = StringAlignment.Center;
            sf.LineAlignment = StringAlignment.Center;
            sf.Trimming = StringTrimming.None;
            sf.FormatFlags = StringFormatFlags.NoWrap;
            g.DrawString(s, f, b, r, sf);
        }
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
        g.Clear(Bg);
        float pad = 22, headH = 74, footH = 70;
        float w = ClientSize.Width, h = ClientSize.Height;

        // The header: the name, what it is for, and whether the player is there.
        using (Font title = new Font("Segoe UI Semibold", 20f, FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font small = new Font("Segoe UI", 13f, FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font pill = new Font("Segoe UI Semibold", 13f, FontStyle.Regular, GraphicsUnit.Pixel))
        using (SolidBrush tb = new SolidBrush(Ink))
        using (SolidBrush db = new SolidBrush(Dim)) {
            g.DrawString(demo ? "Doom on the SNOWSKY Echo Mini" : "GILB Keys", title, tb, pad, 18);
            g.DrawString(demo ? "GILB firmware \u00B7 Programs (early beta) \u00B7 PC keyboard over USB"
                              : "A PC keyboard for programs on the player", small, db, pad + 1, 44);

            string st = connected ? "Connected \u00B7 " + portName : "Waiting for the player";
            Color sc = connected ? Green : Amber;
            SizeF sz = g.MeasureString(st, pill);
            RectangleF pr = new RectangleF(w - pad - sz.Width - 40, 26, sz.Width + 40, 30);
            using (GraphicsPath pp = Round(pr, 15))
            using (SolidBrush pb = new SolidBrush(Color.FromArgb(30, sc)))
            using (Pen pe = new Pen(Color.FromArgb(120, sc), 1.2f)) {
                g.FillPath(pb, pp);
                g.DrawPath(pe, pp);
            }
            using (SolidBrush glow = new SolidBrush(Color.FromArgb(60, sc)))
                g.FillEllipse(glow, pr.X + 9, pr.Y + 8, 14, 14);
            using (SolidBrush dot = new SolidBrush(sc))
                g.FillEllipse(dot, pr.X + 12, pr.Y + 11, 8, 8);
            g.DrawString(st, pill, new SolidBrush(sc), pr.X + 28, pr.Y + 6);
        }

        // The keyboard.
        RectangleF area = new RectangleF(pad, headH, w - 2 * pad, h - headH - footH);
        float u = Math.Min((area.Width - 24) / UnitsW, (area.Height - 24) / UnitsH);
        if (u < 8) return;
        float ox = area.X + (area.Width - UnitsW * u) / 2, oy = area.Y + (area.Height - UnitsH * u) / 2;
        RectangleF board = new RectangleF(ox - u * 0.35f, oy - u * 0.35f, UnitsW * u + u * 0.7f, UnitsH * u + u * 0.7f);
        using (GraphicsPath bp = Round(board, u * 0.3f)) {
            using (SolidBrush bb = new SolidBrush(Color.FromArgb(18, 20, 34)))
                g.FillPath(bb, bp);
            using (LinearGradientBrush lg = new LinearGradientBrush(board, Cyan, Pink, LinearGradientMode.Horizontal)) {
                using (Pen glowPen = new Pen(Color.FromArgb(36, Pink), 7f))
                    g.DrawPath(glowPen, bp);
                using (Pen rim = new Pen(lg, 2f))
                    g.DrawPath(rim, bp);
            }
        }

        float m = u * 0.06f;
        using (Font kf = new Font("Segoe UI Semibold", Math.Max(7f, u * 0.27f), FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font af = new Font("Segoe UI", Math.Max(6f, u * 0.19f), FontStyle.Bold, GraphicsUnit.Pixel)) {
            foreach (Key k in keys) {
                RectangleF r = new RectangleF(ox + k.X * u + m, oy + k.Y * u + m, k.W * u - 2 * m, u - 2 * m);
                bool on = held.Contains(k.Vk);
                string[] act;
                bool mapped = doom.TryGetValue(k.Vk, out act);
                using (GraphicsPath kp = Round(r, u * 0.14f)) {
                    if (on) {
                        for (int i = 3; i >= 1; i--) {
                            RectangleF gr = RectangleF.Inflate(r, i * u * 0.05f, i * u * 0.05f);
                            using (GraphicsPath gp = Round(gr, u * 0.14f + i * u * 0.05f))
                            using (SolidBrush gb = new SolidBrush(Color.FromArgb(34, Green)))
                                g.FillPath(gb, gp);
                        }
                        using (SolidBrush fb = new SolidBrush(Green))
                            g.FillPath(fb, kp);
                    } else {
                        using (SolidBrush fb = new SolidBrush(mapped ? FaceDoom : Face))
                            g.FillPath(fb, kp);
                        using (Pen pe = new Pen(mapped ? Color.FromArgb(210, Cyan) : Edge, mapped ? 1.5f : 1f))
                            g.DrawPath(pe, kp);
                    }
                }
                Color lc = on ? Bg : mapped ? Ink : Dim;
                if (mapped) {
                    Centered(g, k.Label, kf, lc, new RectangleF(r.X, r.Y + r.Height * 0.04f, r.Width, r.Height * 0.58f));
                    Centered(g, act[0], af, on ? Bg : Pink, new RectangleF(r.X - 4, r.Y + r.Height * 0.56f, r.Width + 8, r.Height * 0.36f));
                } else {
                    Centered(g, k.Label, kf, lc, r);
                }
            }
        }

        if (!active) {
            using (GraphicsPath bp = Round(board, u * 0.3f))
            using (SolidBrush veil = new SolidBrush(Color.FromArgb(200, Bg)))
                g.FillPath(veil, bp);
            using (Font big = new Font("Segoe UI Semibold", 22f, FontStyle.Regular, GraphicsUnit.Pixel))
            using (Font small = new Font("Segoe UI", 13f, FontStyle.Regular, GraphicsUnit.Pixel)) {
                Centered(g, "Click here to play", big, Ink, new RectangleF(board.X, board.Y + board.Height / 2 - 30, board.Width, 32));
                Centered(g, "Keys are let go while this window is in the background", small, Dim,
                         new RectangleF(board.X, board.Y + board.Height / 2 + 6, board.Width, 22));
            }
        }

        // The footer: what Doom is told to do, and the bytes sent.
        List<string> doing = new List<string>();
        foreach (int vk in order) {
            string[] act;
            if (held.Contains(vk) && doom.TryGetValue(vk, out act) && !doing.Contains(act[1]))
                doing.Add(act[1]);
        }
        float fy = h - footH + 14;
        using (Font lab = new Font("Segoe UI", 13f, FontStyle.Regular, GraphicsUnit.Pixel))
        using (Font val = new Font("Segoe UI Semibold", 15f, FontStyle.Regular, GraphicsUnit.Pixel))
        using (SolidBrush db = new SolidBrush(Dim))
        using (SolidBrush gb = new SolidBrush(doing.Count > 0 ? Green : Dim)) {
            g.DrawString("DOOM", lab, new SolidBrush(Pink), pad, fy + 1);
            g.DrawString(doing.Count > 0 ? string.Join("  \u00B7  ", doing.ToArray()) : "Nothing held", val, gb, pad + 52, fy - 1);
            string cnt = sent + (sent == 1 ? " key change sent" : " key changes sent");
            SizeF cs = g.MeasureString(cnt, lab);
            g.DrawString(cnt, lab, db, w - pad - cs.Width, fy + 1);
            if (!demo) {
                g.DrawString("Keep this window in front while you play. On the player, hold POWER for 2 s to leave the program.",
                             lab, db, pad, fy + 26);
            } else {
                // The last presses, newest first, fading out.
                float cx = pad + 52, cy = fy + 26;
                g.DrawString("KEYS", lab, new SolidBrush(Cyan), pad, cy + 2);
                string link = "github.com/AlanRSV/GILB";
                SizeF ls = g.MeasureString(link, lab);
                g.DrawString(link, lab, db, w - pad - ls.Width, cy + 2);
                using (Font chip = new Font("Segoe UI Semibold", 12f, FontStyle.Regular, GraphicsUnit.Pixel)) {
                    for (int i = 0; i < recentName.Count; i++) {
                        double age = (DateTime.Now - recentAt[i]).TotalMilliseconds;
                        int a = (int)Math.Max(0, Math.Min(255, 255 * (1 - age / FadeMs)));
                        if (a == 0) continue;
                        SizeF cs2 = g.MeasureString(recentName[i], chip);
                        RectangleF cr = new RectangleF(cx, cy, cs2.Width + 14, 22);
                        if (cr.Right > w - pad - ls.Width - 12) break;
                        Color cc = i == 0 ? Green : Cyan;
                        using (GraphicsPath cp = Round(cr, 11))
                        using (SolidBrush cb = new SolidBrush(Color.FromArgb(a * 40 / 255, cc)))
                        using (Pen cpen = new Pen(Color.FromArgb(a * 170 / 255, cc), 1.2f)) {
                            g.FillPath(cb, cp);
                            g.DrawPath(cpen, cp);
                        }
                        using (SolidBrush tb2 = new SolidBrush(Color.FromArgb(a, Ink)))
                            g.DrawString(recentName[i], chip, tb2, cx + 7, cy + 3);
                        cx = cr.Right + 8;
                    }
                }
            }
        }
    }
}
'@

$script:port = $null
$script:held = @{}

# The player's COM name from the registry (instant; a device search takes over a second and froze
# the window), and only if that port is there now.
function Find-GilbPort {
    $present = [System.IO.Ports.SerialPort]::GetPortNames()
    $keys = Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Enum\USB\VID_071B&PID_3209' -ErrorAction SilentlyContinue
    foreach ($k in $keys) {
        $name = (Get-ItemProperty (Join-Path $k.PSPath 'Device Parameters') -ErrorAction SilentlyContinue).PortName
        if ($name -and $present -contains $name) { return $name }
    }
    return $null
}

function Close-GilbPort {
    if ($script:port) {
        try { $script:port.Close() } catch { }
        $script:port = $null
    }
}

function Open-GilbPort {
    $name = Find-GilbPort
    if (-not $name) { return $false }
    try {
        $p = New-Object System.IO.Ports.SerialPort $name, 115200
        $p.DtrEnable = $true
        $p.RtsEnable = $true
        $p.WriteTimeout = 200
        $p.Open()
        $script:port = $p
        return $true
    } catch {
        return $false
    }
}

function Send-Byte([int]$b) {
    if (-not $script:port) { return }
    try {
        $script:port.Write([byte[]]@([byte]$b), 0, 1)
        $kb.CountSent()
    } catch [System.TimeoutException] {
        # the program is busy: the next resend (Send-Held) puts the keys right
    } catch {
        Close-GilbPort
    }
}

# 0x80 (every key up), then each key held, in one write.
function Send-Held {
    if (-not $script:port) { return }
    $b = [byte[]](@(0x80) + @($script:held.Keys | Where-Object { $_ -lt 128 } | ForEach-Object { [byte]$_ }))
    try {
        $script:port.Write($b, 0, $b.Length)
    } catch [System.TimeoutException] {
    } catch {
        Close-GilbPort
    }
}

function Release-All {
    foreach ($vk in @($script:held.Keys)) {
        if ($vk -lt 128) { Send-Byte ($vk -bor 0x80) }
    }
    $script:held.Clear()
    $kb.ReleaseAll()
}

function Show-State {
    if ($script:port) { $kb.SetStatus($true, $script:port.PortName) }
    else { $kb.SetStatus($false, '') }
}

$form = New-Object GilbForm
$form.Text = 'GILB Keys'
$form.ClientSize = New-Object System.Drawing.Size(940, 470)
$form.MinimumSize = New-Object System.Drawing.Size(640, 380)
$form.StartPosition = 'CenterScreen'
$form.BackColor = [System.Drawing.Color]::FromArgb(13, 14, 24)
$form.KeyPreview = $true
$form.TopMost = $true
$kb = New-Object GilbKeyboard
$kb.Dock = 'Fill'
$form.Controls.Add($kb)
if ($Demo) {
    $kb.SetDemo()
    $form.Text = 'GILB Keys - Doom on the Echo Mini'
    $form.ClientSize = New-Object System.Drawing.Size(1280, 600)
}

if ($Snapshot) {                        # (a picture of the window, with a few keys held)
    $kb.SetStatus($true, 'COM5')
    if ($Demo) {                        # (a few presses before, for the strip)
        foreach ($vk in 0x20, 0x45, 0x27, 0x26) { $kb.Press($vk); $kb.Release($vk) }
    }
    foreach ($vk in 0x57, 0x10, 0x11, 0x25) { $kb.Press($vk) }
    $bmp = New-Object System.Drawing.Bitmap $form.ClientSize.Width, $form.ClientSize.Height
    $kb.Size = $form.ClientSize
    $kb.DrawToBitmap($bmp, (New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height))
    $bmp.Save($Snapshot, [System.Drawing.Imaging.ImageFormat]::Png)
    $kb.SetActive($false)
    $kb.DrawToBitmap($bmp, (New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height))
    $bmp.Save(($Snapshot -replace '\.png$', '-away.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    return
}

$form.Add_KeyDown({
    param($s, $e)
    $vk = [int]$e.KeyCode
    $e.Handled = $true
    $e.SuppressKeyPress = $true
    if ($script:held.ContainsKey($vk)) { return }
    $script:held[$vk] = $true
    $kb.Press($vk)
    if ($vk -lt 128) { Send-Byte $vk }
})
$form.Add_KeyUp({
    param($s, $e)
    $vk = [int]$e.KeyCode
    $e.Handled = $true
    if (-not $script:held.ContainsKey($vk)) { return }
    $script:held.Remove($vk)
    $kb.Release($vk)
    if ($vk -lt 128) { Send-Byte ($vk -bor 0x80) }
})
$form.Add_Activated({ $kb.SetActive($true) })
$form.Add_Deactivate({ Release-All; $kb.SetActive($false) })   # (no key stays held in the background)

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 1000
$timer.Add_Tick({
    if ($script:port -and -not $script:port.IsOpen) { Close-GilbPort }
    if ($script:port -and -not (Find-GilbPort)) { Close-GilbPort }
    if (-not $script:port) { [void](Open-GilbPort) }
    Show-State
})
$timer.Start()
$resend = New-Object System.Windows.Forms.Timer
$resend.Interval = 250
$resend.Add_Tick({ Send-Held })
$resend.Start()

[void](Open-GilbPort)
Show-State
$form.Add_FormClosed({ Release-All; Close-GilbPort; $timer.Stop(); $resend.Stop() })
[void]$form.ShowDialog()
