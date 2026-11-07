# Phone Remote

*Boards with WiFi: the ESP32-S3 boards*

The *Phone Remote* page, the last one in [Settings](settings.md), lets people run the display from a phone's web browser, so nobody has to reach the display in the mask.  They can pick what it shows, control the [Lovense](lovense.md) devices, and type messages onto the [Terminal](terminal.md).  The phone needs no app.

1. Connect the devices on the display first (the Lovense feature's Devices page).  The phone can connect them too, but the display is quicker.
2. On the *Phone Remote* page, switch it on.  The board starts a WiFi hotspot of its own, called `RCAT-` and four letters, and shows a QR code with its name and password under it.
3. On the phone, point the camera at the QR code and tap *Join*.  (Or join the network in the phone's WiFi settings and type the password.)
4. The remote's page opens on its own after a few seconds, the way a hotel's WiFi sign-in page does.  If it doesn't, open `http://192.168.4.1` in the phone's browser.

The page has three tabs:

- **Screen**: what's on the display now, with arrows to step through every page, and every feature's pages listed to jump straight to one.
- **Toys**: a slider for each motor of each connected device, the patterns and their speed, the reactions, a list of devices to connect, and a big **STOP** at the bottom that stops every device the moment it's touched.
- **Terminal**: type a message and *Send* it (or press Enter) to show it on the Terminal.  *Switch to it* (on to start with) brings the Terminal on screen when you send.  *Add a line* puts the message under what's there instead of replacing it, dropping the oldest lines when it's full.  *Clear* empties it, and *Letter size* picks how big it's shown.  The last few messages are listed under it, to send again with a tap.

The display and the phones control the same things, so each shows what the others do.  Up to four phones can join at once, and the title on the display says how many have.  Adding `#screen`, `#toys` or `#term` to the page's address opens that tab.

The name and password are made up the first time the remote is switched on, and kept from then on.  Anyone who has the password can join, so if it's got around, switch the remote off, which drops every phone, and tap *New password*.  The new QR code lets the phones you want back in.  The hotspot is off at startup, and while it's on, the phone has no internet over WiFi (it may use mobile data for everything else).

On an iPhone, the page opens in a small sign-in window.  Leave it open, or tap *Done* and open `http://192.168.4.1` in Safari.  Android phones may show a "Sign in to network" notification while they're joined; tapping it opens the page.

The Phone Remote and the [Voice Lab](voice-lab.md) both use WiFi, so they can't run at once: switching one on stops the other.

The page is `core/app/remote_page.html`.  After changing it, run `python3 tools/gen_remote_page.py` to turn it into the C file that's built into the firmware.
