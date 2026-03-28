<CsoundSynthesizer>
<CsOptions>
-o sine_sweep.wav -d
</CsOptions>
<CsInstruments>

sr     = 44100
ksmps  = 64
nchnls = 1
0dbfs  = 1

instr 1
  out oscil(0.5, line(20, p3, 20000), ftgen(0, 0, 16384, 10, 1))
endin

</CsInstruments>
<CsScore>

i 1 0 10

e
</CsScore>
</CsoundSynthesizer>
