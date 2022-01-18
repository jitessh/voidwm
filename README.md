<div align="center">
    <h1>voidwm</h1>
    <b>sanely patched <a href="https://dwm.suckless.org/">dwm</a></b>
    <p></p>
</div>

![voidwm-preview](https://github.com/pixxel8/aperture/raw/master/voidwm/voidwm-01.png/)
![voidwm-preview](https://github.com/pixxel8/aperture/raw/master/voidwm/voidwm-02.png/)
![voidwm-preview](https://github.com/pixxel8/aperture/raw/master/voidwm/voidwm-03.png/)


# Getting started
## Installation
- Clone the repository and run `make`:
```bash
git clone https://github.com/pixxel8/voidwm
cd voidwm
sudo make clean install
```

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
- Use layout, nmaster, mfact, gaps (can be toggled with `PERTAG_VANITYGAPS`), & bar on per tag basis (see [pertag](https://dwm.suckless.org/patches/pertag/)).
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
- Seamless [pywal](https://github.com/dylanaraps/pywal/) support: change colors on the fly without restarting or exiting dwm with `Mod+q` ([modified xrdb](https://github.com/pixxel8/voidwm/commit/10ba0c06ddd648bee6b9968b65d4addfd57d7ecf/)).
- Colorful tags ([modified rainbowtags](https://github.com/pixxel8/voidwm/commit/b17c5629a73c63fd4f8a3f88f8b7a8b0bbddcb97/)).
- To see list of all patches, run:
```bash
git log --oneline | grep "patch++" | cut -c17-
```


# Status bar
- `voidwm` uses [my fork](https://github.com/pixxel8/dwmblocks/) of [dwmblocks](https://github.com/torrinfail/dwmblocks/).
- Colorful status bar (status2d); toggleable with `enable_status2d` variable in [status2d settings](https://github.com/pixxel8/dwmblocks/blob/master/scripts/sb-status2d/).
- Seamless [pywal](https://github.com/dylanaraps/pywal/) support: match colors of status bar with Xresources variables; toggleable with `enable_Xresources` variable in [status2d settings](https://github.com/pixxel8/dwmblocks/blob/master/scripts/sb-status2d/).
- NOTE: You need to restart `dwmblocks` after updating Xresources variables.


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
