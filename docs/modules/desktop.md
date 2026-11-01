# desktop: clipboard, notifications, opening things

`import desktop`. Reaches the user's graphical session by running the platform's own tools.
These functions act on the person's real desktop, so use them deliberately. They need a logged-in
graphical session; over SSH or on a server they fail with a fatal error such as
`clipboard is unavailable.`

| function | returns | notes |
|---|---|---|
| `clipboard()` | str | current clipboard text (empty string if empty or not text) |
| `copy(text)` | None | replace the clipboard text |
| `notify(title, text)` | None | show a desktop notification |
| `open(target)` | None | open a file, folder or URL with its default application (`target` must not be empty or start with `-`) |

Tools used: macOS `pbpaste`, `pbcopy`, `osascript`, `open`. Linux: `xclip`, `notify-send`, `xdg-open` (install them first). Windows is not supported.

These examples touch the desktop and are not executed by the test suite:

```my-norun
import desktop

desktop.copy("copied by a script")
print(desktop.clipboard())
desktop.notify("Build finished", "all 42 tests passed")
desktop.open("https://example.com")
desktop.open("/tmp")
```

Check availability without a fatal error by testing the tool first:

```my-norun
import desktop
import proc

if proc.which("pbcopy") != None or proc.which("xclip") != None:
    desktop.copy("hello")
```
