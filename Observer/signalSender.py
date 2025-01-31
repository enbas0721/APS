import numpy as np
import pyaudio
import signal
import time
import sys

class Sender:
    def __init__(self, amp=0.3, freq=1760, duration=0.1, rate=44100, send_cycle=1):
        self.amp = amp
        self.freq = freq
        self.duration = duration
        self.rate = rate
        self.samples = self.makeWave()
        self.stream = self.startStream()
        self.send_cycle = send_cycle

    def makeWave(self):
        samples = np.array([])
        for i in range(int(self.rate * self.duration)):
            n = i/self.rate
            samples =  np.append(samples, self.amp * np.sin(2*np.pi * n * (1700+((1800-1700)/(2*0.1))*n)))
            # g[n] = (int)((vol*1000) * sin(2*M_PI * t * (f0 + ((f1-f0)/(2*SIGNAL_L))*t)));
        print(samples[0:100])

        return samples

    def startStream(self):
        p = pyaudio.PyAudio()
        stream = p.open(format=pyaudio.paFloat32,
                        channels=1,
                        rate=self.rate,
                        frames_per_buffer=1024,
                        output=True)

        return stream

    def sendSignal(self, arg1, arg2):
        self.stream.write(self.samples.astype(np.float32).tostring())

    def startSending(self):
        if self.amp == 2:
            time.sleep(1)
        else:
            signal.signal(signal.SIGALRM, self.sendSignal)
            signal.setitimer(signal.ITIMER_REAL, self.send_cycle, self.send_cycle)

if __name__ == '__main__':
    amp = float(sys.argv[1])
    sender = Sender(amp=amp)
    sender.startSending()
    time.sleep(100)
