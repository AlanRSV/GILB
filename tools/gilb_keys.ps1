<#
gilb_keys.ps1 -- a PC keyboard for GILB programs, over the player's USB serial port.

    powershell -ExecutionPolicy Bypass -File tools\gilb_keys.ps1

With a program running that opened the port (API version 4, serial_open; Doom does), the player is a
COM port on the PC (USB VID 071B, PID 3209). This opens a small window: while it has the focus, each
key press and release goes to the player as one byte, the Windows key code (below 128), bit 7 set for
a release (docs/PROGRAMS.md). It finds the port by itself and reconnects when the program starts again.
Close the window to stop.
#>
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

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
    } catch {
        Close-GilbPort
    }
}

function Release-All {
    foreach ($vk in @($script:held.Keys)) { Send-Byte ($vk -bor 0x80) }
    $script:held.Clear()
}

$form = New-Object System.Windows.Forms.Form
$form.Text = 'GILB keys'
$form.Size = New-Object System.Drawing.Size(420, 220)
$form.KeyPreview = $true
$form.TopMost = $true
$label = New-Object System.Windows.Forms.Label
$label.Dock = 'Fill'
$label.Font = New-Object System.Drawing.Font('Segoe UI', 10)
$label.Padding = New-Object System.Windows.Forms.Padding(10)
$form.Controls.Add($label)

$help = "Doom: W/S or arrows move, left/right turn, A/D strafe,`r`nCtrl fire, Space use, Shift run, Q/E weapon, Tab map, Esc menu, Enter select.`r`n`r`nKeep this window in front while you play."

function Show-State {
    if ($script:port) { $state = "Connected to the player ($($script:port.PortName))." }
    else { $state = 'Waiting for the player: start a program that opens the USB port (Doom does), with the cable in.' }
    $label.Text = "$state`r`n`r`n$help"
}

$form.Add_KeyDown({
    param($s, $e)
    $vk = [int]$e.KeyCode
    $e.Handled = $true
    $e.SuppressKeyPress = $true
    if ($vk -ge 128 -or $script:held.ContainsKey($vk)) { return }
    $script:held[$vk] = $true
    Send-Byte $vk
})
$form.Add_KeyUp({
    param($s, $e)
    $vk = [int]$e.KeyCode
    $e.Handled = $true
    if ($vk -ge 128 -or -not $script:held.ContainsKey($vk)) { return }
    $script:held.Remove($vk)
    Send-Byte ($vk -bor 0x80)
})
$form.Add_Deactivate({ Release-All })     # (no key stays held when the window loses the focus)

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 1000
$timer.Add_Tick({
    if ($script:port -and -not $script:port.IsOpen) { Close-GilbPort }
    if ($script:port -and -not (Find-GilbPort)) { Close-GilbPort }
    if (-not $script:port) { [void](Open-GilbPort) }
    Show-State
})
$timer.Start()

[void](Open-GilbPort)
Show-State
$form.Add_FormClosed({ Release-All; Close-GilbPort; $timer.Stop() })
[void]$form.ShowDialog()
