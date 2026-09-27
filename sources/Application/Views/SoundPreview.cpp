#include "SoundPreview.h"
#include "Application/AppWindow.h"
#include "Application/Instruments/InstrumentBank.h"
#include "Application/Instruments/MacroInstrument.h"
#include "Application/Instruments/SampleInstrument.h"
#include "Application/Instruments/SamplePool.h"
#include "Application/Instruments/SynthInstrument.h"
#include "Application/Model/Phrase.h"
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
#include "Adapters/SDL/GUI/SDLGUIWindowImp.h"
#endif
#include <string.h>

SoundPreview::SoundPreview() : for_(0), columns_(0) {
    memset(min_, 0, sizeof(min_));
    memset(max_, 0, sizeof(max_));
}

const char *InstrumentTypeTag(I_Instrument *instr) {
    switch (instr->GetType()) {
    case IT_SYNTH:
        return "SYN";
    case IT_MACRO:
        return "MAC";
    case IT_MIDI:
        return "MID";
    default:
        return instr->IsEmpty() ? "---" : "SMP";
    }
}

void CountInstrumentUsage(Song *song, int usage[MAX_INSTRUMENT_COUNT]) {
    memset(usage, 0, sizeof(int) * MAX_INSTRUMENT_COUNT);
    Phrase *phrase = song->phrase_;
    for (int p = 0; p < PHRASE_COUNT; p++) {
        bool seen[MAX_INSTRUMENT_COUNT];
        memset(seen, 0, sizeof(seen));
        for (int s = 0; s < 16; s++) {
            unsigned char i = phrase->instr_[16 * p + s];
            if (i < MAX_INSTRUMENT_COUNT && !seen[i]) {
                seen[i] = true;
                usage[i]++;
            }
        }
    }
}

// Min/max per column: two cycles of a synth, the whole recording of a sample
void SoundPreview::cache(I_Instrument *instr, int columns) {
    for_ = instr;
    columns_ = columns > 200 ? 200 : columns;
    memset(min_, 0, sizeof(min_));
    memset(max_, 0, sizeof(max_));
    if (instr->GetType() == IT_SYNTH) {
        float cycle[100];
        ((SynthInstrument *)instr)->RenderCycle(cycle, 100);
        int prev = 0;
        for (int c = 0; c < columns_; c++) {
            float v = cycle[(c * 200 / columns_) % 100];
            int y = (int)(v * 100.0f);
            if (y > 100)
                y = 100;
            if (y < -100)
                y = -100;
            if (c == 0)
                prev = y;
            // Join to the previous column so steep edges (square, saw) show
            min_[c] = (signed char)(y < prev ? y : prev);
            max_[c] = (signed char)(y > prev ? y : prev);
            prev = y;
        }
        return;
    }
    if (instr->GetType() == IT_MACRO) {
        // The model's own output, as on its SOUND page
        float minv[200], maxv[200];
        ((MacroInstrument *)instr)->RenderPreview(minv, maxv, columns_);
        for (int c = 0; c < columns_; c++) {
            int lo = (int)(minv[c] * 100.0f);
            int hi = (int)(maxv[c] * 100.0f);
            min_[c] = (signed char)(lo < -100 ? -100 : lo);
            max_[c] = (signed char)(hi > 100 ? 100 : hi);
        }
        return;
    }
    if (instr->GetType() != IT_SAMPLE || instr->IsEmpty())
        return;
    int index = ((SampleInstrument *)instr)->GetSampleIndex();
    SamplePool *pool = SamplePool::GetInstance();
    if (index < 0 || index >= pool->GetNameListSize())
        return;
    SoundSource *source = pool->GetSource(index);
    if (!source)
        return;
    int size = source->GetSize(-1);
    int channels = source->GetChannelCount(-1);
    short *samples = (short *)source->GetSampleBuffer(-1);
    if (!samples || size <= 0)
        return;
    if (channels <= 0)
        channels = 1;
    int peak = 1;
    int lo[200], hi[200];
    for (int c = 0; c < columns_; c++) {
        int start = (int)(((long long)c * size) / columns_);
        int end = (int)(((long long)(c + 1) * size) / columns_);
        if (end <= start)
            end = start + 1;
        if (end > size)
            end = size;
        // Long samples: a strided scan per column is plenty for a picture
        int stride = (end - start) / 64 + 1;
        lo[c] = hi[c] = 0;
        for (int i = start; i < end; i += stride) {
            int s = samples[i * channels];
            if (s < lo[c])
                lo[c] = s;
            if (s > hi[c])
                hi[c] = s;
        }
        if (-lo[c] > peak)
            peak = -lo[c];
        if (hi[c] > peak)
            peak = hi[c];
    }
    for (int c = 0; c < columns_; c++) {
        min_[c] = (signed char)((lo[c] * 100) / peak);
        max_[c] = (signed char)((hi[c] * 100) / peak);
    }
}

void SoundPreview::Draw(SDLGUIWindowImp *imp, InstrumentBank *bank, int slot,
                        int bx, int by, int bw, int bh) {
#if defined(PLATFORM_RGNANO) || defined(PLATFORM_RGNANO_SIM)
    I_Instrument *instr = bank->GetInstrument(slot);
    if (instr != for_ || columns_ != bw - 4)
        cache(instr, bw - 4);
    GUIColor frame = AppWindow::ThemeBlend(CD_BACKGROUND, CD_BORDER, 45);
    GUIColor panel = AppWindow::ThemeColor(CD_BACKGROUND);
    GUIColor trace = AppWindow::ThemeColor(CD_HILITE2);
    GUIRect outer(bx, by, bx + bw, by + bh);
    imp->SetColor(frame);
    imp->DrawRect(outer);
    GUIRect inner(bx + 1, by + 1, bx + bw - 1, by + bh - 1);
    imp->SetColor(panel);
    imp->DrawRect(inner);
    int mid = by + bh / 2;
    int half = bh / 2 - 3;
    imp->SetColor(trace);
    for (int c = 0; c < columns_ && c < bw - 4; c++) {
        int y0 = mid - (max_[c] * half) / 100;
        int y1 = mid - (min_[c] * half) / 100;
        if (y1 < y0) {
            int t = y0;
            y0 = y1;
            y1 = t;
        }
        GUIRect col(bx + 2 + c, y0, bx + 3 + c, y1 + 1);
        imp->DrawRect(col);
    }
#endif
}
