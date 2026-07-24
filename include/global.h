#ifndef GLOBAL_H
#define GLOBAL_H

#include "PR/ultratypes.h"
#include "PR/mbi.h"
#include "functions.h"
#include "ek_functions.h"
#include "variables.h"
#include "macros.h"
#include "sys.h"
#include "controller.h"
#include "fzx_math.h"
#include "unk_structs.h"
#include "leo/leo_internal.h"
#include "sfx.h"

#ifndef PORT
#define GDX_CK(x)
#else
extern void *memset(void *, int, unsigned long long);
extern void gdx_ck(const char* s);
extern void gdx_cki(const char* s, int v);
extern void gdx_ckp(const char* s, void* p);
extern int gdx_diag_verbose(void);
extern int gdx_unlock_diag_enabled(void);
extern void gdx_unlock_diagf(const char* fmt, ...);
extern void gdx_unlock_audio_expect_note(void);
extern void gdx_unlock_audio_cancel_note(void);
extern void gdx_unlock_audio_trace_dsp_begin(void);
extern int gdx_unlock_audio_trace_dsp_active(void);
extern unsigned int gdx_unlock_audio_trace_generation(void);
extern int gdx_unlock_audio_trace_note_index(int noteIndex);
extern void gdx_unlock_audio_capture_ai_buffer(const s16* buffer, unsigned int frameCount,
                                               unsigned int sampleRate);
extern void gdx_unlock_audio_stage_begin_command_list(unsigned int generation, const void* commandList,
                                                      unsigned int sampleRate);
extern void gdx_unlock_audio_stage_register_command_range(
    unsigned int generation, int noteIndex, const void* commandStart, const void* commandEnd,
    const void* adpcmState, const void* resampleState, unsigned int sampleRate);
#define GDX_CK(x) gdx_ck(#x)
#endif

#endif // GLOBAL_H
