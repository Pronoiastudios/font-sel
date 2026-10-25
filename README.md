# Font-sel

In my opinion there are not a lot of great font preview tools out there, so I made one mainly just for myself

---

GTK4 now. The old `gtk_widget_override_font` call is gone; the font is applied
with Pango attributes on the label, which is not deprecated and which a
stylesheet can't override — so the font you pick is the font you see.

## What it does

* Shows the selected font at several sizes at once: the size you pick in the
  dialog, then 1.5x, 2x and 3x. Rungs above 120pt are dropped, so a large
  choice doesn't turn into a wall of text.
* The first block is the full character set (upper/lower case, digits,
  punctuation, ligature-prone pairs, quotes, currency, arrows, emoji).
* Type in the entry box to preview your own text at the chosen size instead.
  Clear it to get the full character set back.
* Remembers the last font and custom text between runs.
* Single instance: launching it again raises the window that's already open.

---

# Building

## Dependencies

`gtk4` (4.12 or newer) and a C++17 compiler.

On Arch:

```sh
sudo pacman -S --needed gtk4
```

## Build

```sh
sh ./build
```

That produces `./font-sel`. Then run `./font-sel`.

Don't set the font size too big or you'll probably regret it lol

# Configuration

State lives in `~/.config/font-sel/config`, a plain key file written when you
change the font or close the window:

```ini
[state]
font=Aleo Bold 32
text=
```

Delete it to go back to the default (`Sans 14`).

## Styling

The really only thing you can do is style this. A good way to do that is to use
`~/.config/gtk-4.0/gtk.css`

* selectors:
    * `#Font-Sel-Window` The window
    * `.font-sample` Every block of preview text
    * `#font-label` The preview text too, kept so older stylesheets keep working
    * use `GTK_DEBUG=interactive font-sel` to find more.

Family, weight, style and size come from the app, so CSS can't change the font
itself — colour, background, padding and spacing are all yours. The app's own
stylesheet loads at application priority, below your `gtk.css`, so anything you
set wins.
