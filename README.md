# Photon Audio Samples

*Code samples for doing audio on the Particle Photon*

[Sample 1] (https://github.com/rickkas7/photonAudio/tree/master/audio1/) uses an [Adafruit 1713] (https://www.adafruit.com/products/1713) Electret Microphone Amplifier - MAX9814 with Auto Gain Control to capture audio on the Photon, then sends the data via TCP to a server written in node.js. It uses a fairly straightforward method of reading the ADC using a hardware timer using the [SparkIntervalTimer] (https://github.com/pkourany/SparkIntervalTimer) library.

[Sample 3] (https://github.com/rickkas7/photonAudio/tree/master/audio3/) does the same thing, but it uses DMA to very efficiently sample and store the data. This sample works at a 32000 Hz sample rate with 16-bit samples.




