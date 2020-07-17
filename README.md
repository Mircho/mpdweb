# mpdweb
Web interface for MPD for Linux and OpenWRT

This project started as I was experimenting running mpd on openwrt routers with pulseaudio audio routing. For the UI, at the time, I had to resort to a cli linux client or a native mobile (at that moment deprecated) client.

So I decided to build some webUI for controlling mpd.

I was contemplating building a luci extension so the ui is embedded into the regular web ui for maintaning OpenWRT, but decided against it foor two reasons:
- the web ui was very complicated and basically was intermingled lua and html
- there will be problem with responsibilities of luci - logging in the admin of the router to manage the music being played

So then the decision was to build some local app that serves we content and act as a control proxy for passing commands to mpd.

Thus this all started

# Server side

The server side steps on mongoose web server <https://github.com/cesanta/mongoose>, or at least the version that was relevant back in 2014-2015. I can not express my gratitude to cesanta for making this library and then later I found Mongoose OS - a stack for running on wifi connected devices, that I also employ into my automation project. Check them!

For configuration, as the intention was to run on OpenWRT I used libuci <https://oldwiki.archive.openwrt.org/doc/techref/uci>

For json parsing and building jansson library was used <https://digip.org/jansson/>

For controlling mpd I used libmpdclient <https://github.com/MusicPlayerDaemon/libmpdclient>:

# Client side

For the client I wrote a single page app with Knockout JS <https://knockoutjs.com/> and bootstrap for the page structure
