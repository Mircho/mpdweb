# mpdweb
Web interface for MPD for Linux and OpenWRT

This project started as I was experimenting running mpd on openwrt routers with pulseaudio audio routing. For the UI, at the time, I had to resort to a cli linux client or a native mobile (at that moment deprecated) client.

So I decided to build some webUI for controlling mpd.

I was contemplating building a luci extension so the ui is embedded into the regular web ui for maintaning OpenWRT, but decided against it foor two reasons:
- the web ui was very complicated and basically was intermingled lua and html
- there will be problem with responsibilities of luci - logging in the admin of the router to manage the music being played

So then the decision was to build some local app that serves we content and act as a control proxy for passing commands to mpd.

Thus this all started
