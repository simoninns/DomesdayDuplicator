# The main window

The window is a frame around six panels. There is no fixed layout: every panel can be
moved, resized, closed, or dragged out into a window of its own, and where you leave them is
where they are next time.

## The panels

| Panel | What it is for | Where it starts |
| --- | --- | --- |
| **Capture** | What the capture is called, what it is written as, and the buttons — see [Capture control](capture-control.md) | Left, top |
| **Statistics** | What the run is doing, second by second — see [Statistics](statistics.md) | Left, below Capture |
| **Waveform** | The scope: the signal right now — see [Signal analysis](signal-analysis.md#waveform) | Right, top |
| **Spectrum** | What the signal is made of, live or over time — see [Signal analysis](signal-analysis.md#spectrum) | Right, middle |
| **Amplitude History** | Level over the last five minutes — see [Signal analysis](signal-analysis.md#amplitude-history) | Right, bottom |
| **Log** | Diagnostics | Bottom, hidden |

The Log panel starts hidden on purpose: it is a diagnostic view, and the first thing a new
window should show is the signal rather than the plumbing. **View ▸ Panels ▸ Log** reveals
it, and so does starting the application with [`--debug`](command-line.md).

## Rearranging

Drag a panel by its title bar to move it. Panels can be dropped beside one another as well
as above and below, so a side-by-side scope and spectrum is a matter of dragging one onto
the other's edge. Drag one outside the window and it becomes a window of its own — useful
on a second screen, where a spectrogram can be given the width it deserves.

**View ▸ Panels** lists all six with a tick beside each. The list is built from the panels
themselves, so closing one with its own title-bar button unticks it here, and there is no
way for the two to disagree.

The window's size, position, and the whole arrangement — including which panels are floating
and which are hidden — are saved when the application closes and restored when it opens.

## Themes

**View ▸ Theme** offers **Auto**, **Light** and **Dark**. Auto follows the desktop's own
light/dark setting and changes with it while the application is running.

Everything the application draws itself — the scope trace, the spectrogram colours, the
green on an active button, the pass or fail verdict on a test analysis — is drawn from the
window's own palette, so a theme switched in the middle of a capture recolours the displays
rather than leaving one theme's colours on the other's background.

## The status bar

The status bar says the same thing the Capture panel does, because the Capture panel can be
closed and the status bar cannot. Someone who has hidden everything but the spectrum can
still see that the device has been unplugged.

It shows, in order of precedence: the file being written during a capture, *Monitoring*
while the stream is running, how many devices are attached, or why the attached device
cannot capture.

When a capture ends it reports the file and its size there rather than in a message box. A
capture ending is the expected outcome, and a dialog to dismiss after every one would be in
the way of somebody taking both sides of a disc.

## The menus

| Menu | Entry | What it does |
| --- | --- | --- |
| **File** | Settings… | [Settings](settings.md) |
| | Exit | Closes the window, saving the layout |
| **View** | Panels | Show and hide the six panels |
| | Theme | Auto, Light, Dark |
| **Tools** | Player control | Look for a LaserDisc player — see [Player control](player-control.md) |
| | Search now | Look for the player again straight away |
| | Remote control… | The player window: transport, connection, disc codes, manual commands |
| | Examine disc… | Find out what is in the player, and report it |
| | Automatic capture… | Examine, name, capture a side, and see what was written |
| | Test data mode | Capture the gateware's test pattern instead of the RF input — see [Test mode](test-mode.md) |
| | Analyse test data… | Check a test-mode capture off the disk |
| | Firmware ▸ Update firmware… | Which build each part of the device is running, and the Update page — see [Updating your Duplicator](updating-your-domesday-duplicator.md) |
| **Help** | About | The build this binary was made from |

The player entries sit above the instrument ones, separated by a rule. They are on **Tools**
rather than in a menu or a panel of their own because almost everybody has one player, set up
once and never touched again: it is a tool rather than something that earns permanent screen
space. Whether it is connected is on the status bar, which cannot be closed.

Player *settings* — model, port, speed — are on the **Player** tab of **File ▸ Settings…**
rather than repeated here. This menu is what you do to a player; that dialog is what you set
about one.

**Remote control…** opens whether or not a player is connected, because it is the window that
says *why* one is not. The entries that need a player to do anything — **Examine disc…** and
**Automatic capture…** — are greyed out until there is one.

**Help ▸ About** carries the same version string `--version` prints, and that is deliberate
rather than duplication: the Windows build has no console attached, so on the platform with
the most installations the dialog is the only way to identify a binary. If you are reporting
a problem, that is the string to quote.

## The Log panel

Every diagnostic the application produces arrives here, timestamped, in a fixed-pitch font
so the columns line up. **Follow** keeps the newest record in view; **Clear** empties it.

By default it records informational messages and above. Starting with
[`--debug`](command-line.md) lowers that to debug level and shows the panel at startup,
which is what to do before reproducing a fault you intend to report.
