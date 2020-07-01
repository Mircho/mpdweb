# mpdweb
Web interface for MPD for Linux and OpenWRT

This project started as I was experimenting running mpd on openwrt routers with pulseaudio audio routing. For the UI I had to resort to either a cli linux client or a native mobile (at that moment deprecated) client.
I was contemplating building a luci extension so the ui is embedded into the regular web ui for maintaning OpenWRT, but decided against it foor two reasons:
- the web ui was very complicated and basically intermingled lua and html
- there will be problem with responsibilities of logging in the admin of the router to manage the music being played
