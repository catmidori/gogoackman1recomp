"""Compile the unchanged production resampler/ring without unrelated CPU linkage."""
from pathlib import Path
import sys
runner, output = map(Path, sys.argv[1:])
rtl = (runner / 'common_rtl.c').read_text()
dsp = (runner / 'snes/dsp.c').read_text()
globals_ = rtl[rtl.index('#define RTL_AUDIO_RECOVERY_RAMP'):rtl.index('static void rtl_sync_apu_frame_boundary(void);')]
resampler = rtl[rtl.index('#define RTL_AUDIO_NATIVE_RATE'):rtl.index('void RtlRenderAudio(')]
ring = dsp[dsp.index('uint32_t dsp_available('):dsp.index('uint32_t dsp_getSamples(')]
output.write_text(globals_ + ring + resampler)
