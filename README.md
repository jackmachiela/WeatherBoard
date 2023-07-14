# WeatherBoard
Modern update to antique barometer/thermometer

I found this lovely old weather pane, with semi-working barometer, thermometer, and hygrometer in it, while thrift-shopping.


![](https://github.com/jackmachiela/WeatherBoard/blob/main/Graphics/Old.and.new.JPG?raw=true)

Cost me nz$8, if I remember correctly. It's a bit run down, and in all honesty I don't really have a use for the hygrometer or the barometer. They're just random numbers to me, and I'm pretty sure I get far more useful information from the internet.

So, I set about creating a new device, but with the old retro look, something that seems to be a common thread in a few of my projects. Specifically, I want to keep the old domed glass covers, and the brass surrounds, but lose the traditional "barometer" scale, update the thermometer to Celcius, and replace the hygrometer from the pretty useless "moisture" reading to a more practical "forecast precipitation" readout.

I looked at a few different weather servers, and ended up settling for open-meteo.com because it's free and doesn't require an API key, but mainly because it covers my actual semi-rural area, something a lot of other servers don't.


I chose to use three servo motors to replace the old dials, which gave me an unexpected advantage; they're quite noisy, and since the code only checks the weather every 30 mins or so, if there's a major change in the "right now, this hour" forecast, I'm alerted to it by the quick "zzzztk-zzztk-zzzztk" noise.

So display 1 shows Temperature in degrees Celcius; display 2 (slightly larger) shows the firecast weather icon, ranging from "Clear/Cloudy/Overcast/Light Rain/Heavy Rain/Thunderstorms", and display 3 shows forecasted precipitation in millimeters.

There's a small button at the bottom right hand side of the board - when first switched on, the display shows the forecast in the next hour. One click of the button shows the forecast for the whole of today; any subsequent clicks show tomorrow through to 6 days from now. The top of the display now has hidden LEDs showing the forecast days.

Components I used:

1x WeMos Mini D1 Pro
3x SG90 9gm Servo Motors
1x 5v SMD5050 RGB Smart Addressable LED strip with 8 LEDs
1x 2 pins 6x6x5mm Switch Tactile Push Button
1x Double Side Prototype PCB diy Universal Printed Circuit Board 4x6cm
2x 8-pin female sockets (for plugging WeMos board into circuit board)
3x 3-pin male pins (for servos)
1x 3-pin screw terminal (for LED strip)
2x 2-pin screw terminal (for power and for button)

The circuit description is as follows:


![](https://github.com/jackmachiela/WeatherBoard/blob/main/Graphics/WeatherBoard%20Circuit%2C%20rev%203.jpg?raw=true)

It's rev 3 - let's not talk about rev 1 and 2. Let's just say I'm getting better at soldering with every mistake I make.


I'm using the EZTime library simply because it's super simple to do complex stuff with, and it's art of my starter skeleton code. Same with the Wi-Fi Manager - and as a bonus, it means I don't have to worry about any secret API keys accidentally left in my github repo.0

I'm not quite finished with this project yet - it all works but it still looks fugly as hell. The centre display has been fairly well calibrated but the thermometer and the rain-meter need fine-tuning. Eventually I'll rip off the paper copy, and will make a better looking display for the whole thing, and will put the glass domes on properly.

I'll update photos here when I get around to that.
