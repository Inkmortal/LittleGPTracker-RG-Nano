"""Copy the parts of Mutable Instruments Plaits (MIT, Emilie Gillet) that the
synth's DRUM and PHYS engines use into sources/Externals/Plaits.

    python tools/import_plaits.py <eurorack checkout> <stmlib checkout>

(github.com/pichenettes/eurorack, github.com/pichenettes/stmlib; imported from
eurorack 08460a6 and stmlib d18def8.) Changes on the way in:
  - stmlib's namespace becomes pstmlib, so it can't clash with the older
    stmlib that Braids brings (sources/Externals/Braids/stmlib)
  - includes point at Externals/Plaits/...
  - the models run at the mixer's 44.1 kHz (kSampleRate)
  - resources.cc keeps only the three tables these models read
"""
import os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
eurorack, stm = sys.argv[1], sys.argv[2]
dst = os.path.join(here, '..', 'sources', 'Externals', 'Plaits')


def source(path):
    # stmlib files are named relative to the stmlib checkout
    if path.startswith('plaits/'):
        return os.path.join(eurorack, path)
    return os.path.join(stm, path)
headers = {
    'plaits/dsp/dsp.h': None,
    'plaits/dsp/drums/analog_bass_drum.h': None, 'plaits/dsp/drums/analog_snare_drum.h': None,
    'plaits/dsp/drums/hi_hat.h': None, 'plaits/dsp/drums/synthetic_bass_drum.h': None,
    'plaits/dsp/drums/synthetic_snare_drum.h': None,
    'plaits/dsp/oscillator/sine_oscillator.h': None, 'plaits/dsp/oscillator/oscillator.h': None,
    'plaits/dsp/noise/dust.h': None,
    'plaits/dsp/fx/overdrive.h': None,
    'dsp/limiter.h': 'stmlib/dsp/limiter.h',
    'plaits/dsp/physical_modelling/delay_line.h': None, 'plaits/dsp/physical_modelling/modal_voice.h': None,
    'plaits/dsp/physical_modelling/resonator.h': None, 'plaits/dsp/physical_modelling/string.h': None,
    'plaits/dsp/physical_modelling/string_voice.h': None,
    'stmlib.h': 'stmlib/stmlib.h', 'dsp/dsp.h': 'stmlib/dsp/dsp.h', 'dsp/filter.h': 'stmlib/dsp/filter.h',
    'dsp/units.h': 'stmlib/dsp/units.h', 'dsp/parameter_interpolator.h': 'stmlib/dsp/parameter_interpolator.h',
    'dsp/polyblep.h': 'stmlib/dsp/polyblep.h', 'dsp/rsqrt.h': 'stmlib/dsp/rsqrt.h',
    'dsp/cosine_oscillator.h': 'stmlib/dsp/cosine_oscillator.h',
    'utils/random.h': 'stmlib/utils/random.h', 'utils/buffer_allocator.h': 'stmlib/utils/buffer_allocator.h',
    'LICENSE': 'stmlib/LICENSE',
}
sources = {
    'plaits/dsp/physical_modelling/modal_voice.cc': 'PlaitsModalVoice.cpp',
    'plaits/dsp/physical_modelling/resonator.cc': 'PlaitsResonator.cpp',
    'plaits/dsp/physical_modelling/string.cc': 'PlaitsString.cpp',
    'plaits/dsp/physical_modelling/string_voice.cc': 'PlaitsStringVoice.cpp',
    'dsp/units.cc': 'PlaitsUnits.cpp', 'utils/random.cc': 'PlaitsRandom.cpp',
}


def fix(t):
    t = re.sub(r'#include "stmlib/', '#include "Externals/Plaits/stmlib/', t)
    t = re.sub(r'#include "plaits/', '#include "Externals/Plaits/plaits/', t)
    t = re.sub(r'\bnamespace stmlib\b', 'namespace pstmlib', t)
    t = re.sub(r'\bstmlib::', 'pstmlib::', t)
    t = re.sub(r'STMLIB_', 'PSTMLIB_', t)
    # stmlib.h's compile-time assertion helpers live outside the namespace
    t = re.sub(r'\bnamespace impl\b', 'namespace pstmlib_impl', t)
    t = re.sub(r'\bimpl::', 'pstmlib_impl::', t)
    t = re.sub(r'\bStaticAssertImplementation\b', 'PStaticAssertImplementation', t)
    # The module's ARM assembly (ssat, usat, vsqrt) only on ARM; the portable
    # versions (its TEST build) elsewhere, e.g. the Windows simulator
    t = t.replace('#ifdef TEST', '#if defined(TEST) || !defined(__arm__)')
    # One random generator per thread (as in the Braids port), so the
    # instrument screen's hit picture never races the audio thread's voices
    t = t.replace('  static uint32_t rng_state_;',
                  '  // LittleGPTracker port: one generator per thread, so the instrument\n'
                  "  // screen's hit picture never races the audio thread's voices.\n"
                  '  static __thread uint32_t rng_state_;')
    t = t.replace('uint32_t Random::rng_state_ = 0x21;', '__thread uint32_t Random::rng_state_ = 0x21;')
    # No RAM code section on Linux or Windows
    t = t.replace('#ifndef TEST\n#define IN_RAM __attribute__ ((section (".ramtext")))\n#else\n'
                  '#define IN_RAM\n#endif  // TEST', '#define IN_RAM')
    return t


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, 'w', newline='\n').write(text)


for s, d in headers.items():
    write(os.path.join(dst, d or s), fix(open(source(s)).read()))
for s, d in sources.items():
    write(os.path.join(dst, d), fix(open(source(s)).read()))

# resources: only the three tables these models read
r = open(source('plaits/resources.cc')).read()
out = [fix(r[:r.index('namespace plaits')]), 'namespace plaits {\n\n']
for name in ['lut_sine', 'lut_stiffness', 'lut_svf_shift']:
    out.append(re.search(r'const float %s\[\] = \{.*?\};\n' % name, r, re.S).group(0) + '\n')
out.append('}  // namespace plaits\n')
write(os.path.join(dst, 'PlaitsResources.cpp'), ''.join(out))
h = open(source('plaits/resources.h')).read()
body = ('namespace plaits {\n\nextern const float lut_sine[];\nextern const float lut_stiffness[];\n'
        'extern const float lut_svf_shift[];\n\n}  // namespace plaits\n\n#endif  // PLAITS_RESOURCES_H_\n')
write(os.path.join(dst, 'plaits/resources.h'), fix(h[:h.index('namespace plaits')]) + body)
# The mixer's rate on every platform
p = os.path.join(dst, 'plaits/dsp/dsp.h')
t = open(p).read()
start = t.index('static const float kSampleRate')
end = t.index('const float a0')
NOTE = """// LittleGPTracker: the models run straight at the mixer's rate (44.1 kHz on
// every platform), so kSampleRate is that and times and pitches written in
// terms of it come out right. (The module ran at 47872.34 Hz: an I2S clock
// divider.) Only the module's DSP classes are used; see NOTICE.md.
static const float kSampleRate = 44100.0f;
static const float kCorrectedSampleRate = 44100.0f;
"""
t = t[:start] + NOTE + t[end:]
open(p, 'w', newline='\n').write(t)
print('imported into', os.path.normpath(dst))
