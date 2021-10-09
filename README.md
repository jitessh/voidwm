<div align="center">
    <h1>voidwm</h1>
    <b>Jitesh's build of <a href="https://dwm.suckless.org/">dwm</a></b>
    <p></p>
</div>

![dwm-preview-03](https://github.com/voidstarsh/aperture/raw/master/dwm-03.png)
![dwm-preview-04](https://github.com/voidstarsh/aperture/raw/master/dwm-04.png)
![dwm-preview-05](https://github.com/voidstarsh/aperture/raw/master/dwm-05.png)


# Getting started
## Installation
- Clone the repository and run `make`:
```bash
git clone https://github.com/voidstarsh/voidwm
cd voidwm
sudo make clean install
```
- I've also got my build of [dwmblocks](https://github.com/voidstarsh/dwmblocks) (status bar for dwm).

## Running dwm
### Using startx
- Add the following in your `.xinitrc` file appropriately:
```bash
exec dwm
```

### Using a display manager
- Make a desktop entry for dwm in `/usr/share/xsessions` directory:
```bash
sudo cp voidwm.desktop /usr/share/xsessions
```

## Usage
See [config.h](config.h) for various key bindings, layouts supported, and more. See man page for more information.


# Features & patches
- Launch anything on startup by editing `~/.config/dwm/autostart.sh` file (see [autostart](https://dwm.suckless.org/patches/autostart/)).
- Extensive control on bar with [barmodules](https://github.com/bakkeby/patches/wiki/barmodules/) patch (with systray).
- Cycle through layouts with `Mod+(shift+)tab` (see [cyclelayouts](https://dwm.suckless.org/patches/cyclelayouts/)).
- Cycle through non-empty tags with `Alt+(shift+)tab` (see [shiftviewclients](https://github.com/bakkeby/patches/wiki/shiftviewclients/)).
- Use Xresources variables to set colors, borderpx, barheight and more (see [xresources](https://dwm.suckless.org/patches/xresources/)).
- Make clients go fullscreen or fake fullscreen with `Mod+(shift+)f` (see [fullscreen-compilation](https://github.com/bakkeby/patches/wiki/fullscreen-compilation/)).
- Use layout, nmaster, mfact, gaps, & bar on per tag basis (see [pertag](https://dwm.suckless.org/patches/pertag/)).
- Reorganize and distribute tags with `Mod+(shift+)r` (see [reorganizetags](https://dwm.suckless.org/patches/reorganizetags/)).
- Extensive mouse support
    - Change master area with `Mod+shift+Button1` (see [dragmfact](https://github.com/bakkeby/patches/wiki/dragmfact/)).
    - Change stack area with `Mod+shift+Button3` (see [dragcfact](https://github.com/bakkeby/patches/wiki/dragcfact/)).
    - Change position of clients with `Mod+Button1` (see [placemouse](https://github.com/bakkeby/patches/wiki/placemouse/)).
    - Resize clients with `Mod+Button3` (see [resizepoint](https://github.com/bakkeby/patches/wiki/resizepoint/)).
    - NOTE: All the above actions can also be done with key bindings. See [config.h](config.h) for the same.
- Toggle scratchpad terminal with `Mod+p`.
- Set/unset any existing window as scratchpad with `Mod+(shift+)n` (see [renamedscratchpads](https://github.com/bakkeby/patches/wiki/renamedscratchpads/)).
- Make windows sticky with `Mod+s` (see [sticky](https://dwm.suckless.org/patches/sticky/)).
- Restart dwm with `Mod+shift+q` (see [restartsig](https://dwm.suckless.org/patches/restartsig/)).
- Swallow windows & allow exceptions (see [swallow](https://github.com/bakkeby/patches/wiki/swallow/)).
- Seamless [pywal](https://github.com/dylanaraps/pywal/) support: change colors on the fly without restarting or exiting dwm with `Mod+q` (modified [xrdb](https://dwm.suckless.org/patches/xrdb/): [diff](https://github.com/voidstarsh/voidwm/commit/10ba0c06ddd648bee6b9968b65d4addfd57d7ecf)).
- To see list of all patches, run:
```bash
git log --oneline | grep "patch++" | cut -c17-
```


# Touchpad functionality
To get [natural scrolling with touchpad](https://askubuntu.com/questions/1122513/how-to-add-natural-inverted-mouse-scrolling-in-i3-window-manager/), edit `/usr/share/X11/xorg.conf.d/40-libinput.conf` file:
```
Section "InputClass"
        Identifier "libinput touchpad catchall"
        MatchIsTouchpad "on"
        MatchDevicePath "/dev/input/event*"
        Driver "libinput"
        Option "NaturalScrolling" "True"
EndSection
```

To [enable taping on touchpad](https://wiki.archlinux.org/title/Libinput#Via_Xorg_configuration_file/), create a file `/etc/X11/xorg.conf.d/30-touchpad.conf`:
```
Section "InputClass"
        Identifier "touchpad catchall"
        Driver "libinput"
        Option "Tapping" "on"
EndSection
```
