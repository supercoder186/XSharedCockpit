# XSharedCockpit

This is a new plugin for xplane that aims to allow simmers to share a cockpit
The plugin achieves this by syncing datarefs and commands between a "master" and a "slave" computer. 
# INSTALLATION
To install, simply unzip the file and paste the folder named "XSharedCockpit" inside the Resources/plugins directory of your X-Plane folder
# WHAT DOES THIS PLUGIN DO?
This plugin syncs datarefs and commands, to give the feeling of flying in one cockpit. It is similar to smartcopilot in functionality, syncing the flight between two pilots and allowing both pilots to interact with the aircraft.
# VIDEO TUTORIAL SERIES
https://www.youtube.com/playlist?list=PLjahzB_xjsquR12hirZM15XZ1GwHiXeoe
# USAGE
An xsharedcockpit.cfg file must be present in the aircraft's root directory. You can simply rename the aircraft's smartcopilot.cfg file, and it will work. Additionally, unlike the XSharedCockpit lua, the sim does not do anything except sync, so the syncing will not be perfect on things such as flap deflections and engine throttle % until the cfg file is updated.
This plugin is still buggy and slightly unstable. Do not use it if you want a perfect experience like smartcopilot. I have also released videos detailing the plugin's usage
# KNOWN ISSUES
Currently, the linux build crashes when slave mode is activated. I will try to fix this soon
# PERFORMANCE
I am including this because I feel it is important (and imo a lot more plugins should do this). The performance impact of this plugin on my PC was negligible (and my PC is quite slow - i5-6200U, Geforce 940M)
# DEVELOPMENT OF CFG FILES
Details on how these are developed can be found on the smartcopilot website over here -> https://sky4crew.com/hangar/, as well as in the video tutorial series above ^
Note that XSharedCockpit needs you to mention literally everything you want to sync, apart from aircraft position, velocity and orientation
# STATE DATAREF
There is now a dataref named "xsharedcockpit/state". It is an int dataref
0 - Disabled
1 - Running in master mode
2 - Running in slave mode
# IMPORTANT NOTE
I haven't had the opportunity to test version 1.1 - it might be unstable / non-functional (though I doubt it, since I went through the code thoroughly, and the actual change to the codebase was small). I would love it if someone would take the time to understand how the plugin works through the video series, and test it for me. Note that it uses the UDP protocol for transmission. Anyways, the added features in 1.1 should offset this potential instability, so give it a try. If it is unstable, this is the link to the older V1.0 files -> https://github.com/supercoder186/XSharedCockpit/releases/download/1.0/XSharedCockpit.zip
# DEVELOPMENT FORUM
I have created a new forum in order to discuss feature requests, etc. It can be found over here -> 
