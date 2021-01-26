# Photon Audio Example Code

*Code examples for doing audio on the Particle Photon*

[Example 1](https://github.com/rickkas7/photonAudio/tree/master/audio1/) uses an [Adafruit 1713] (https://www.adafruit.com/products/1713) Electret Microphone Amplifier - MAX9814 with Auto Gain Control to capture audio on the Photon, then sends the data via TCP to a server written in node.js. It uses a fairly straightforward method of reading the ADC using a hardware timer using the [SparkIntervalTimer] (https://github.com/pkourany/SparkIntervalTimer) library.

[Example 3](https://github.com/rickkas7/photonAudio/tree/master/audio3/) does the same thing, but is more experimental. It does all of the sampling in hardware using the ADC, hardware timer and DMA, storing the samples in RAM at precise intervals without using the main CPU. It's very efficient and this example works at a 32000 Hz sample rate with 16-bit samples. The node.js server program saves the data to wav files.

[Example 4](https://github.com/rickkas7/photonAudio/tree/master/audio4/) is like Example 3, except it uploads 6 channels of 44.1KHz 16-bit samples to a server. The Javascript can save one of the streams to a wav file.  

[Example 5](https://github.com/rickkas7/photonAudio/tree/master/audio5/) is like Example 3, except it captures samples into RAM. Once the output buffer is filled, the samples are printed to USB serial as decimal values. 

