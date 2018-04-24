# Photon Audio Sample 4
*Sample program for streaming audio from a Particle Photon to a node.js server over TCP using DMA!*

This sample project saves 6 channels of 44.1 KHz 16-bit samples to a local server running node.js to receive and process the files in this case. As the data rate is 441000 bytes per second, this works best over local network only.

Also, this is kind of at the bleeding edge of what the Photon can do. I wouldn't really recommend doing this, it's more of a proof-of-concept.

## Photon Side

The code has many comments and should be self-explanatory.

The main thing is that the code uses two ADCs (ADC1 and ADC2). They're run in dual-simultaneous mode, so both are sampled at the same time.

They're also run in scan mode so multiple pins are sample sequentially, and very rapidly, 5 cycles of the ADC clock (30 MHz).

ADC1 is set to read A0, A2, and A4. ADC2 is set to read A1, A3, and A5.

44100 times per second, controlled by a hardware timer, the ADCs are set off do do their thing, and store the data in a DMA buffer, so the inter-frame spacing is rock-solid.

The DMA buffer is 1020 bytes, because something around 1024 bytes is ideal with 0.6.x and TCP, but the buffer size should be a multiple of 12 (the number of channels * 2 bytes per channel).

There are two DMA buffers, and the interrupt flag is set when the buffer is half-full so it can be sent out from loop while the other half is being filled.

From the main loop we poll the IRQ flag to see if there's a buffer ready to be transmitted. The good part is that this is not particularly timing-sensitive.

Because all of the sampling is done via hardware, there's no interrupt latency to worry about.


## Server Side

The server-side code is written in [node.js] (https://nodejs.org). You'll want to copy the [audioserver.js] (https://github.com/rickkas7/photonAudio/blob/master/audio4/audioserver.js) file to your computer. 

It requires the standard Node 4.5.0 features plus the [wav package] (https://github.com/TooTallNate/node-wav) to output the audio files. You install it using:

```
npm install wav
```

Then you run the server:

```
node audioserver.js
```

A directory called out will be created in the same directory and will contain audio files like 00001.wav, 00002.wav, etc.. You should be able to play these on your computer.


## Using It

With the code loaded on the Photon and the server running, hit the SETUP button to being recording. The blue D7 LED turns on after a connection is made to the server. Recording stops when you hit the SETUP button again, or the maximum recording time is reached. That's currently 30 seconds, but there's really nothing on either side that requires that limit. It could be hours if you wanted it to be.

You should only use this code on system firmware 0.6.x. The network stack changed in 0.7.x and appears to be slower. I had a hard time getting the 517 Kbytes/sec. off the Photon using 0.7.0. It works most of the time on 0.6.3.

