LauchPlay MIDI Sequencer
========================

LauchPlay is a tiny sequencer for use with a Novation LaunchPad controller in any DAW supporting VST plugins. 
It currently runs on Mac OS X (10.5 or later) and Windows 7. Your DAW must support VST v2.4.

Content
-------

There currently are three plugins: 

* LaunchPlaySequencer: a VSTi sequencing notes using [Novation LauchPad](http://www.novationmusic.com/launchpad/) as a remote controller,
* LaunchPlayMidiFilter: a mandatory VST effect that can help you filtering MIDI messages to retain only one channel, only if needed,
* LaunchPlayVirtualCable: an useful VST effect that greatly simplify MIDI "wiring" between the sequencer and your virtual (or physical) instruments.

Yet another sequencer? My DAW already is a sequencer!
-----------------------------------------------------

LaunchPlay is a generative sequencer for use with your LaunchPad, inspired by Batuhan Bozkurt's work on [Otomata](http://www.earslap.com/). 
It turns your Novation LaunchPad into a fancy remote to generate random music. And you can generate notes for up to 8 virtual or physical instruments.
Please visit [sample-hold.com](http://sample-hold.com/) for a detailed explanation on how to use it.  

How to compile sources
----------------------

The available sources require two third-parties libraries : 

* [boost version 1.47 or later](http://boost.org/),
* [Steinberg VST SDK 2.4](http://www.steinberg.net/en/company/developer.html).

There are two projects ready for use : 

* LaunchPlayVST.xcodeproj: for Mac OS X, requires XCode 4,
* LaunchPlayMIDIEffect.sln: for Windows, requires Visual C++ 2010.

Please feel free to contact me for any help on installing and compiling these sources.

Licensing issue
---------------

This work belongs to Fred Ghilini and is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
Otomata is an intellectual property owned by [Batuhan Bozkurt](http://www.earslap.com/).
VST is an intellectual property owned by [Steinberg Media Technologies GmbH](http://www.steinberg.net/).
LaunchPad is intellectual property owned by [Focusrite Audio Engineering Limited](http://www.novationmusic.com/) and [Ableton AG](http://www.ableton.com/).

Contributing
------------

Want to contribute? Great! I look forward having news from you :)