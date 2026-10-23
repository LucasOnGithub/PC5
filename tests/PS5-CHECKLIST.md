# Testing PC5 on a PS5

What to try on a console, with what to look for on screen and in the log. A session's log is
`/data/prospero-win/logs/session-N.log` while it runs; when PC5 next opens it deletes the
logs of sessions that went well and keeps the others as `logs/issue-<when>_<game>.log`. To
keep every log while testing, set Settings › Keep all logs to On first. PC5's lines start
with `PC5 `.

## PC5

| # | Do | Expect on screen | Expect in the log |
| --- | --- | --- | --- |
| 1 | Replace the `PPSA11018` folder with the new one, start PC5 | the tile shows the tower without lettering; the library | `PC5 start version=1.1.0 …`, `PC5 controller reset light=0 triggers=0` |
| 2 | Options › About PC5 | version 1.1.0 and the free space | |
| 3 | Options › Updates | "No PC5 release is published on GitHub yet" (until there is one) | `PC5 update state=2` (2: none published) |
| 4 | Options › In games: turn Show FPS on, pick a corner, change Vibration, Light bar and Adaptive triggers | the values change and stay after restarting PC5; Trigger rumble / Trigger resistance appear only with their effect on | `PC5 settings fps=1 …`; `/data/prospero-win/pc5/settings.txt` |
| 5 | Options › Install games from USB, tick two folders with □, press Options | "Getting ready 1 of 2", then the list with covers | `PC5 folder=… queue=1/2`, `PC5 cover choices=… queue=…` |
| 6 | ✕ on one, L1 / R1, ✕ (Done) | another cover; back to the list | |
| 7 | Options (Install all) | "Game 1 of 2", "Step 1 of 3 …" with MB/s on every step, then a summary | `PC5 install id=… queue=1/2`, `PC5 queue done installed=2 of=2` |
| 8 | Install again, press ○ during the copy, choose Stop this game | that game stops, the next one installs | `PC5 install ended cancelled=1` |
| 9 | △ on a game › Change program | its programs; choosing one says "… starts … now" | `PC5 program id=… exe=…` |

## In a game

| # | Do | Expect | Expect in the log |
| --- | --- | --- | --- |
| 10 | Start GTA IV | green falling code with "Grand Theft Auto IV", then the game | `PC5 game fps=… tint=…`, `PW_WINE64 profile … dll_overrides=d3d8,d3d9,d3d10core,d3d11,dxgi=n;d3d12,d3d12core=b`, no `Unhandled page fault` |
| 11 | With Show FPS on | the counter in the chosen corner, once the game shows its picture | `PW_WINE64 pc5 vulkan hud=1 tick=1 color=1` |
| 12 | Light bar: Game's colour, then Follows the screen | the cover's colour; then the screen's, changing with it | `PC5 light bar … rc=0` |
| 13 | Adaptive triggers: Rumble, then Resistance, then Rumble + resistance | L2/R2 buzz when the game rumbles; then push back; then push back and buzz while it rumbles | `PC5 triggers rumble mode=2 rc=0`, `PC5 triggers resistance mode=1 rc=0` |
| 14 | Trigger rumble and Trigger resistance at 25 %, then 100 % | a faint buzz and a light push at 25 %; strong at 100 % | `PC5 game … trigger_rumble=25 trigger_resistance=25 …` |
| 14b | Vibration 25 %, then Off | weaker rumble; then none | `PC5 game … vibration=25 …` |
| 15 | Hold the touchpad and Options for 2 seconds | a short buzz, the game closes (or is ended after 5 s), PC5 shows | `PW_WINE64 close requested by=combo` |
| 16 | A game that closes at once (or crashes) | PC5, when it opens, says so and names the kept log | `PC5 logs kept=1 newest=…` |

## Controllers

| # | Do | Expect | Expect in the log |
| --- | --- | --- | --- |
| 17 | △ on a game › Controller: Xbox controller, DualSense, Keyboard & mouse | the row changes, and stays after restarting PC5; Button mapping shows only with Keyboard & mouse | `prefixes/<game>/pc5/controls.txt` has `mode = …` |
| 18 | Keyboard & mouse › Button mapping: give Cross a key, move the pointer to the left stick, change its speed, start the game | the key, the pointer and its speed in the game; Reset to default brings W A S D back | `PW_WINE64 profile … input=keyboard …` |
| 19 | DualSense, then start a game with DualSense or SDL support (or a DirectInput game) | the game finds a controller and shows PlayStation buttons (an SDL game); the buttons, sticks and triggers work | `PW_WINE64 display … xinput=0 dualsense=1`, `PW_WINE64 dualsense output=1 motion=ok rc=0`, `xinput: the controller is a DualSense (054c:0ce6, …)` |
| 20 | In that game: something that rumbles, changes the light bar or sets the triggers | the motors at Vibration's strength; the game's colour on the light bar; the game's trigger effects | `PW_WINE64 dualsense output set=…`, `PC5 light bar the game's …`, `PC5 triggers game effects=… rc=0` |
| 21 | DualSense: the touchpad and tilting the controller in a game that uses them | the game follows the finger and the tilt | |

A non-zero `rc=` on the light bar or triggers lines means the console refused that call; send
that log.
