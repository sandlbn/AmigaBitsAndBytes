#include <devices/ahi.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include <stdlib.h>

#include "include/libraries/mpega.h"
#include "include/clib/mpega_protos.h"

#define BUFFER_SIZE 32768
#define MP3_CHUNK_SIZE 4096
#define FREQUENCY 44100
#define TYPE AHIST_M16S

// AHI structures
struct MsgPort *AHImp = NULL;
struct AHIRequest *AHIios[2] = {NULL, NULL};
struct AHIRequest *AHIio = NULL;
APTR AHIiocopy = NULL;
BYTE AHIDevice = -1;

// MPEGA structures
struct Library *MPEGABase = NULL;
MPEGA_STREAM *mpegaStream = NULL;
WORD *pcmBuffers[MPEGA_MAX_CHANNELS];

// Audio buffers
WORD *buffer1 = NULL;
WORD *buffer2 = NULL;
UBYTE *errorBuffer = NULL;

static const char *UNABLE_TO_OPEN = "Unable to open %s/0 version 4\n";
static const char *USAGE = "Usage: %s <mp3file>\n";
static const char *FAILED_TO_INIT = "Failed to initialize AHI\n";
static const char *NO_MEMORY = "Not enough memory\n";
static const char *MPEGA_FAILED = "Failed to initialize MPEGA library\n";

void cleanup(LONG rc) {
    if (!AHIDevice) {
        CloseDevice((struct IORequest *)AHIio);
    }
    if (AHIio) {
        DeleteIORequest((struct IORequest *)AHIio);
    }
    if (AHIiocopy) {
        FreeMem(AHIiocopy, sizeof(struct AHIRequest));
    }
    if (AHImp) {
        DeleteMsgPort(AHImp);
    }
    if (buffer1) {
        FreeMem(buffer1, BUFFER_SIZE * sizeof(WORD));
    }
    if (buffer2) {
        FreeMem(buffer2, BUFFER_SIZE * sizeof(WORD));
    }
    if (errorBuffer) {
        FreeMem(errorBuffer, 256);
    }

    // Cleanup MPEGA
    if (mpegaStream) {
        MPEGA_close(mpegaStream);
    }
    if (MPEGABase) {
        CloseLibrary(MPEGABase);
    }

    // Free PCM buffers
    for (int i = 0; i < MPEGA_MAX_CHANNELS; i++) {
        if (pcmBuffers[i]) {
            FreeMem(pcmBuffers[i], MPEGA_PCM_SIZE * sizeof(WORD));
        }
    }

    exit(rc);
}

BOOL allocateBuffers(void) {
    buffer1 = AllocMem(BUFFER_SIZE * sizeof(WORD), MEMF_ANY|MEMF_CLEAR);
    if (!buffer1) return FALSE;

    buffer2 = AllocMem(BUFFER_SIZE * sizeof(WORD), MEMF_ANY|MEMF_CLEAR);
    if (!buffer2) return FALSE;

    errorBuffer = AllocMem(256, MEMF_ANY|MEMF_CLEAR);
    if (!errorBuffer) return FALSE;

    // Allocate PCM buffers for MPEGA
    for (int i = 0; i < MPEGA_MAX_CHANNELS; i++) {
        pcmBuffers[i] = AllocMem(MPEGA_PCM_SIZE * sizeof(WORD), MEMF_ANY|MEMF_CLEAR);
        if (!pcmBuffers[i]) return FALSE;
    }

    return TRUE;
}

BOOL initMPEGA(const char *filename) {
    MPEGABase = OpenLibrary("mpega.library", 0);
    if (!MPEGABase) return FALSE;

    // Setup MPEGA control structure
    MPEGA_CTRL ctrl = {
        NULL,    // Default file access
        // Layers I & II settings
        { FALSE, { 1, 2, FREQUENCY }, { 1, 2, FREQUENCY } },
        // Layer III settings
        { FALSE, { 1, 2, FREQUENCY }, { 1, 2, FREQUENCY } },
        0,      // Don't check MPEG validity
        32768   // Stream buffer size
    };

    // Open MP3 stream
    mpegaStream = MPEGA_open((char *)filename, &ctrl);

    if (!mpegaStream) {
        CloseLibrary(MPEGABase);
        return FALSE;
    }

    return TRUE;
}

BOOL initAHI(void) {
    AHImp = CreateMsgPort();
    if (!AHImp) return FALSE;

    AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest));
    if (!AHIio) {
        DeleteMsgPort(AHImp);
        return FALSE;
    }

    AHIio->ahir_Version = 4;
    AHIDevice = OpenDevice((STRPTR)AHINAME, 0, (struct IORequest *)AHIio, 0);
    if (AHIDevice) {
        LONG args[] = {(LONG)AHINAME};
        VPrintf((STRPTR)UNABLE_TO_OPEN, (LONG *)args);
        return FALSE;
    }

    AHIiocopy = AllocMem(sizeof(struct AHIRequest), MEMF_ANY);
    if (!AHIiocopy) return FALSE;
    
    CopyMem(AHIio, AHIiocopy, sizeof(struct AHIRequest));
    AHIios[0] = AHIio;
    AHIios[1] = (struct AHIRequest *)AHIiocopy;
    
    return TRUE;
}

BOOL decodeMPEGAFrame(WORD *buffer, ULONG *length) {
    LONG pcm_count = MPEGA_decode_frame(mpegaStream, pcmBuffers);
    if (pcm_count <= 0) {
        *length = 0;
        return FALSE;
    }

    // Copy decoded PCM data to play buffer
    WORD *dst = buffer;
    for (int i = 0; i < pcm_count; i++) {
        for (int ch = 0; ch < mpegaStream->dec_channels; ch++) {
            *dst++ = pcmBuffers[ch][i];
        }
    }

    *length = pcm_count * mpegaStream->dec_channels;
    return TRUE;
}

BOOL playBuffer(WORD *buffer, ULONG length, struct AHIRequest *req, struct AHIRequest *link) {
    req->ahir_Std.io_Message.mn_Node.ln_Pri = 0;
    req->ahir_Std.io_Command = CMD_WRITE;
    req->ahir_Std.io_Data = buffer;
    req->ahir_Std.io_Length = length * sizeof(WORD);
    req->ahir_Std.io_Offset = 0;
    req->ahir_Frequency = FREQUENCY;
    req->ahir_Type = TYPE;
    req->ahir_Volume = 0x10000;
    req->ahir_Position = 0x8000;
    req->ahir_Link = link;
    
    SendIO((struct IORequest *)req);
    return TRUE;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        LONG args[] = {(LONG)argv[0]};
        VPrintf((STRPTR)USAGE, (LONG *)args);
        return RETURN_FAIL;
    }

    if (!allocateBuffers()) {
        PutStr((STRPTR)NO_MEMORY);
        return RETURN_FAIL;
    }

    if (!initMPEGA(argv[1])) {
        PutStr((STRPTR)MPEGA_FAILED);
        cleanup(RETURN_FAIL);
        return RETURN_FAIL;
    }

    if (!initAHI()) {
        PutStr((STRPTR)FAILED_TO_INIT);
        cleanup(RETURN_FAIL);
        return RETURN_FAIL;
    }

    WORD *p1 = buffer1;
    WORD *p2 = buffer2;
    void *tmp;
    struct AHIRequest *link = NULL;
    ULONG signals, length;

    // Main playback loop
    SetIoErr(0);
    for(;;) {
        // Decode MP3 frame
        if (!decodeMPEGAFrame(p1, &length)) break;

        // Play buffer
        if (!playBuffer(p1, length, AHIios[0], link)) break;

        if (link) {
            signals = Wait(SIGBREAKF_CTRL_C | (1L << AHImp->mp_SigBit));
            
            if (signals & SIGBREAKF_CTRL_C) {
                SetIoErr(ERROR_BREAK);
                break;
            }

            if (WaitIO((struct IORequest *)link)) {
                SetIoErr(ERROR_WRITE_PROTECTED);
                break;
            }
        }

        if (length == 0) {
            WaitIO((struct IORequest *)AHIios[0]);
            break;
        }

        link = AHIios[0];

        // Swap buffers and requests
        tmp = p1;
        p1 = p2;
        p2 = tmp;
        tmp = AHIios[0];
        AHIios[0] = AHIios[1];
        AHIios[1] = tmp;
    }

    // Cleanup
    AbortIO((struct IORequest *)AHIios[0]);
    WaitIO((struct IORequest *)AHIios[0]);
    
    if (link) {
        AbortIO((struct IORequest *)AHIios[1]);
        WaitIO((struct IORequest *)AHIios[1]);
    }

    if (IoErr()) {
        Fault(IoErr(), (STRPTR)argv[0], (STRPTR)errorBuffer, 256);
        cleanup(RETURN_ERROR);
    }

    cleanup(RETURN_OK);
    return RETURN_OK;
}