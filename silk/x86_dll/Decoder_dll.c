/***********************************************************************
Copyright (c) 2006-2012, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, (subject to the limitations in the disclaimer below)
are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Skype Limited, nor the names of specific
contributors, may be used to endorse or promote products derived from
this software without specific prior written permission.
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/


/*****************************/
/* Silk decoder test program */
/*****************************/

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE    1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SKP_Silk_SDK_API.h"
#include "SKP_Silk_SigProc_FIX.h"

/* Define codec specific settings should be moved to h file */
#define MAX_BYTES_PER_FRAME     1024
#define MAX_INPUT_FRAMES        5
#define MAX_FRAME_LENGTH        480
#define FRAME_LENGTH_MS         20
#define MAX_API_FS_KHZ          48
#define MAX_LBRR_DELAY          2

#ifdef _SYSTEM_IS_BIG_ENDIAN
/* Function to convert a little endian int16 to a */
/* big endian int16 or vica verca                 */
void swap_endian(
    SKP_int16       vec[],
    SKP_int         len
)
{
    SKP_int i;
    SKP_int16 tmp;
    SKP_uint8 *p1, *p2;

    for( i = 0; i < len; i++ ){
        tmp = vec[ i ];
        p1 = (SKP_uint8 *)&vec[ i ]; p2 = (SKP_uint8 *)&tmp;
        p1[ 0 ] = p2[ 1 ]; p1[ 1 ] = p2[ 0 ];
    }
}
#endif

#if (defined(_WIN32) || defined(_WINCE))
#include <windows.h>	/* timer */
#else    // Linux or Mac
#include <sys/time.h>
#endif

#ifdef _WIN32

unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    /* Returns a time counter in microsec	*/
    /* the resolution is platform dependent */
    /* but is typically 1.62 us resolution  */
    LARGE_INTEGER lpPerformanceCount;
    LARGE_INTEGER lpFrequency;
    QueryPerformanceCounter(&lpPerformanceCount);
    QueryPerformanceFrequency(&lpFrequency);
    return (unsigned long)((1000000*(lpPerformanceCount.QuadPart)) / lpFrequency.QuadPart);
}
#else    // Linux or Mac
unsigned long GetHighResolutionTime() /* O: time in usec*/
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return((tv.tv_sec*1000000)+(tv.tv_usec));
}
#endif // _WIN32

/* Seed for the random number generator, which is used for simulating packet loss */
static SKP_int32 rand_seed = 1;

size_t readFileAsByteArray(void *ptr, size_t lenOfChar, FILE *fp)
{
    /*
    fseek(fp, 0, SEEK_END); 
    size = ftell(fp); 
    fseek(fp, 0, SEEK_SET); 
    */
    size_t fileSize;
    fileSize =  fread(ptr, sizeof(SKP_uint8), lenOfChar, fp);
    return fileSize;
}

void init()
{
}

size_t ConCatOutArr(SKP_int16 *arr, size_t len, SKP_int16 **outArr, size_t *outArrLen){
    size_t s = sizeof(SKP_int16);
    if(*outArr == NULL){
        SKP_int16 *p = malloc(s * (len));
        memcpy(p, arr, len * s);
        *outArrLen = len;
        //printf( "done!");
        *outArr = p;
    }else{
        SKP_int16 *p = malloc(s * (len + *outArrLen));
        memcpy(p, *outArr, *outArrLen * s);
        memcpy(p + *outArrLen, arr, len * s);
        *outArrLen = *outArrLen + len;
        //printf( "done!!");
        //free(arr);
        free(*outArr);
        *outArr = p;
    }

    return *outArrLen;
}

size_t getCharBufferFromCurrentPos(char *buffer, size_t len, size_t *currentPos, SKP_uint8 *jBuffer)
{
    if(jBuffer !=NULL){
        //lenForRead = 1;
        size_t byteLen = len * sizeof(char);
        memcpy(buffer, &jBuffer[*currentPos], byteLen);
        *currentPos += byteLen;
        return len;
    }
    return 0;
}


size_t  getShortBufferFromCurrentPos(SKP_int16 *buffer, size_t len, size_t *currentPos, SKP_uint8 *jBuffer)
{
    if(jBuffer !=NULL){
        // lenForRead = 2;
        size_t byteLen = len * sizeof(SKP_int16);
        SKP_int8 jBuf8[ len * sizeof(SKP_int16) ];
        //SKP_int16 jBuf16;
        memcpy(jBuf8, &jBuffer[*currentPos], byteLen);
        *currentPos += byteLen;
        for( int i = 0; i < len; i++ ) {
             buffer[i] = (SKP_int16)((jBuf8[ 2*i +1 ] << 8 ) | jBuf8[ 2 * i ]);
        }   
        return len;
    }
    return 0;
}


size_t getByteBufferFromCurrentPos(SKP_uint8 *buffer, size_t len, size_t *currentPos, SKP_uint8 *jBuffer)
{ 
    if(jBuffer !=NULL){
        // lenForRead = 1;
        size_t byteLen = len * sizeof(SKP_uint8);
        memcpy(buffer, &jBuffer[*currentPos], byteLen);
        *currentPos += byteLen;
        //printf( "j-test:   *currentPos:  %d\n",  *currentPos);
        return len;
    }
    return 0;
}

static void print_usage(char* argv[]) {
    printf( "\nVersion:20160922    Build By kn007 (kn007.net)");
    printf( "\nGithub: https://github.com/kn007/silk-v3-decoder\n");
    printf( "\nusage: %s in.bit out.pcm [settings]\n", argv[ 0 ] );
    printf( "\nin.bit       : Bitstream input to decoder" );
    printf( "\nout.pcm      : Speech output from decoder" );
    printf( "\n   settings:" );
    printf( "\n-Fs_API <Hz> : Sampling rate of output signal in Hz; default: 24000" );
    printf( "\n-loss <perc> : Simulated packet loss percentage (0-100); default: 0" );
    printf( "\n-quiet       : Print out just some basic values" );
    printf( "\n" );
}

__declspec(dllexport) int __cdecl SilkDecoderToPcm( SKP_uint8 *jBuffers, size_t jbuffersSize, SKP_int16 **outBuffer, size_t *outSize, int sampleRate)
{
    unsigned long tottime, starttime;
    double    filetime;
    size_t    counter;
    SKP_int32 totPackets, i, k;
    SKP_int16 ret, len, tot_len;
    SKP_int16 nBytes;
    SKP_uint8 payload[    MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES * ( MAX_LBRR_DELAY + 1 ) ];
    SKP_uint8 *payloadEnd = NULL, *payloadToDec = NULL;
    SKP_uint8 FECpayload[ MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES ], *payloadPtr;
    SKP_int16 nBytesFEC;
    SKP_int16 nBytesPerPacket[ MAX_LBRR_DELAY + 1 ], totBytes;
    SKP_int16 out[ ( ( FRAME_LENGTH_MS * MAX_API_FS_KHZ ) << 1 ) * MAX_INPUT_FRAMES ], *outPtr;
    SKP_int32 packetSize_ms=0, API_Fs_Hz = 0;
    SKP_int32 decSizeBytes;
    void      *psDec;
    SKP_float loss_prob;
    SKP_int32 frames, lost, quiet;
    SKP_SILK_SDK_DecControlStruct DecControl;


    SKP_uint8 *jBuffer = NULL;
    size_t jSize = 0;
    size_t  currentPos = 0;

    SKP_int16 *outArr = NULL;
    size_t outArrLen = 0;

    /* default settings */
    quiet     = 1;
    loss_prob = 0.0f;
    API_Fs_Hz = sampleRate;

    /* get arguments */
    
    if( !quiet ) {
        printf("********** Silk Decoder (Fixed Point) v %s ********************\n", SKP_Silk_SDK_get_version());
        printf("********** Compiled for %d bit cpu *******************************\n", (int)sizeof(void*) * 8 );
    }


    jBuffer = jBuffers;
    jSize = jbuffersSize;
    printf( "j-test:  file read    bufer first:%i  file len: %d\n", jBuffer[0], jSize);

    /* Check Silk header */
    {
        char header_buf[ 50 ];
        getCharBufferFromCurrentPos(header_buf, 1, &currentPos, jBuffer);

        header_buf[ strlen( "" ) ] = '\0'; /* Terminate with a null character */
        if( strcmp( header_buf, "" ) != 0 ) {
           // for test from jin
           getCharBufferFromCurrentPos(header_buf, strlen( "!SILK_V3" ), &currentPos, jBuffer);

           header_buf[ strlen( "!SILK_V3" ) ] = '\0'; /* Terminate with a null character */
           if( strcmp( header_buf, "!SILK_V3" ) != 0 ) {
               /* Non-equal strings */
               printf( "Error: Wrong Header %s\n", header_buf );
               exit( 0 );
           }
        } else {

           // for test from jin
           getCharBufferFromCurrentPos(header_buf, strlen( "#!SILK_V3" ), &currentPos, jBuffer);
           header_buf[ strlen( "#!SILK_V3" ) ] = '\0'; /* Terminate with a null character */
           if( strcmp( header_buf, "#!SILK_V3" ) != 0 ) {
               /* Non-equal strings */
               printf( "Error: Wrong Header %s\n", header_buf );
               exit( 0 );
           }
        }
    }

    /* Set the samplingrate that is requested for the output */
    if( API_Fs_Hz == 0 ) {
        DecControl.API_sampleRate = 16000;
    } else {
        DecControl.API_sampleRate = API_Fs_Hz;
    }

    /* Initialize to one frame per packet, for proper concealment before first packet arrives */
    DecControl.framesPerPacket = 1;

    /* Create decoder */
    ret = SKP_Silk_SDK_Get_Decoder_Size( &decSizeBytes );
    if( ret ) {
        printf( "\nSKP_Silk_SDK_Get_Decoder_Size returned %d", ret );
    }
    psDec = malloc( decSizeBytes );

    /* Reset decoder */
    ret = SKP_Silk_SDK_InitDecoder( psDec );
    if( ret ) {
        printf( "\nSKP_Silk_InitDecoder returned %d", ret );
    }

    totPackets = 0;
    tottime    = 0;
    payloadEnd = payload;

    /* Simulate the jitter buffer holding MAX_FEC_DELAY packets */
    for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
        /* Read payload size */
        counter = getShortBufferFromCurrentPos(&nBytes, 1, &currentPos, jBuffer);
        if( !quiet ) {
            printf( "j-test:       counter    %i\n", (int)counter );
        }

#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes, 1 );
#endif
        /* Read payload */
        // for test from jin
        counter = getByteBufferFromCurrentPos(payloadEnd, nBytes, &currentPos, jBuffer);
        //payloadEnd = jBuf8_u;
        //printf( "j-test:       jBuf8_u     %hu\n", jBuf8_u[0] );
        if( !quiet ) {
            printf( "j-test:       counter     %i\n", counter );
        }

        if( ( SKP_int16 )counter < nBytes ) {
            break;
        }
        nBytesPerPacket[ i ] = nBytes;
        payloadEnd          += nBytes;
        totPackets++;
    }

    while( 1 ) {
        /* Read payload size */
        counter = getShortBufferFromCurrentPos(&nBytes, 1, &currentPos, jBuffer);
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( &nBytes, 1 );
#endif
        if( nBytes < 0 || counter < 1 ) {
            break;
        }

        if(currentPos >= jSize){
            break;
        }

        counter = getByteBufferFromCurrentPos(payloadEnd, nBytes, &currentPos, jBuffer);
        //printf( "j-test:       &currentPos     %i\n", currentPos );
        if( ( SKP_int16 )counter < nBytes ) {
            break;
        }

        if(currentPos >= jSize){
            break;
        }

        /* Simulate losses */
        rand_seed = SKP_RAND( rand_seed );
        if( ( ( ( float )( ( rand_seed >> 16 ) + ( 1 << 15 ) ) ) / 65535.0f >= ( loss_prob / 100.0f ) ) && ( counter > 0 ) ) {
            nBytesPerPacket[ MAX_LBRR_DELAY ] = nBytes;
            payloadEnd                       += nBytes;
        } else {
            nBytesPerPacket[ MAX_LBRR_DELAY ] = 0;
        }

        if( nBytesPerPacket[ 0 ] == 0 ) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
                if( nBytesPerPacket[ i + 1 ] > 0 ) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR( payloadPtr, nBytesPerPacket[ i + 1 ], ( i + 1 ), FECpayload, &nBytesFEC );
                    tottime += GetHighResolutionTime() - starttime;
                    if( nBytesFEC > 0 ) {
                        payloadToDec = FECpayload;
                        nBytes = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[ i + 1 ];
            }
        } else {
            lost = 0;
            nBytes = nBytesPerPacket[ 0 ];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if( lost == 0 ) {
            /* No Loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 0, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_SDK_Decode returned %d", ret );
                }

                frames++;
                outPtr  += len;
                tot_len += len;
                if( frames > MAX_INPUT_FRAMES ) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr  = out;
                    tot_len = 0;
                    frames  = 0;
                }
                /* Until last 20 ms frame of packet has been decoded */
            } while( DecControl.moreInternalDecoderFrames );
        } else {
            /* Loss: Decode enough frames to cover one packet duration */
            for( i = 0; i < DecControl.framesPerPacket; i++ ) {
                /* Generate 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 1, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_Decode returned %d", ret );
                }
                outPtr  += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / ( DecControl.API_sampleRate / 1000 );
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        //size_t jlen1 = fwrite( out, sizeof( SKP_int16 ), tot_len, speechOutFile );
        //printf( "j-test:       out len    %i\n", (int)jlen );
        ConCatOutArr(out, tot_len, &outArr, &outArrLen);
        //printf( "j-test:       out2 len    %i\n", (int)jlen );

        /* Update buffer */
        totBytes = 0;
        for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
            totBytes += nBytesPerPacket[ i + 1 ];
        }
        SKP_memmove( payload, &payload[ nBytesPerPacket[ 0 ] ], totBytes * sizeof( SKP_uint8 ) );
        payloadEnd -= nBytesPerPacket[ 0 ];
        SKP_memmove( nBytesPerPacket, &nBytesPerPacket[ 1 ], MAX_LBRR_DELAY * sizeof( SKP_int16 ) );

        if( !quiet ) {
            fprintf( stderr, "\rPackets decoded:             %d", totPackets );
        }
    }

    /* Empty the recieve buffer */
    for( k = 0; k < MAX_LBRR_DELAY; k++ ) {
        if( nBytesPerPacket[ 0 ] == 0 ) {
            /* Indicate lost packet */
            lost = 1;

            /* Packet loss. Search after FEC in next packets. Should be done in the jitter buffer */
            payloadPtr = payload;
            for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
                if( nBytesPerPacket[ i + 1 ] > 0 ) {
                    starttime = GetHighResolutionTime();
                    SKP_Silk_SDK_search_for_LBRR( payloadPtr, nBytesPerPacket[ i + 1 ], ( i + 1 ), FECpayload, &nBytesFEC );
                    tottime += GetHighResolutionTime() - starttime;
                    if( nBytesFEC > 0 ) {
                        payloadToDec = FECpayload;
                        nBytes = nBytesFEC;
                        lost = 0;
                        break;
                    }
                }
                payloadPtr += nBytesPerPacket[ i + 1 ];
            }
        } else {
            lost = 0;
            nBytes = nBytesPerPacket[ 0 ];
            payloadToDec = payload;
        }

        /* Silk decoder */
        outPtr  = out;
        tot_len = 0;
        starttime = GetHighResolutionTime();

        if( lost == 0 ) {
            /* No loss: Decode all frames in the packet */
            frames = 0;
            do {
                /* Decode 20 ms */
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 0, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_SDK_Decode returned %d", ret );
                }

                frames++;
                outPtr  += len;
                tot_len += len;
                if( frames > MAX_INPUT_FRAMES ) {
                    /* Hack for corrupt stream that could generate too many frames */
                    outPtr  = out;
                    tot_len = 0;
                    frames  = 0;
                }
            /* Until last 20 ms frame of packet has been decoded */
            } while( DecControl.moreInternalDecoderFrames );
        } else {
            /* Loss: Decode enough frames to cover one packet duration */

            /* Generate 20 ms */
            for( i = 0; i < DecControl.framesPerPacket; i++ ) {
                ret = SKP_Silk_SDK_Decode( psDec, &DecControl, 1, payloadToDec, nBytes, outPtr, &len );
                if( ret ) {
                    printf( "\nSKP_Silk_Decode returned %d", ret );
                }
                outPtr  += len;
                tot_len += len;
            }
        }

        packetSize_ms = tot_len / ( DecControl.API_sampleRate / 1000 );
        tottime += GetHighResolutionTime() - starttime;
        totPackets++;

        /* Write output to file */
#ifdef _SYSTEM_IS_BIG_ENDIAN
        swap_endian( out, tot_len );
#endif
        //size_t jlen1 = fwrite( out, sizeof( SKP_int16 ), tot_len, speechOutFile );
       
        // printf( "j-test:       out len    %i\n", (int)jlen );
        ConCatOutArr(out, tot_len, &outArr, &outArrLen);
        //printf( "j-test:       out len    %i\n", (int)jlen );
        /* Update Buffer */
        totBytes = 0;
        for( i = 0; i < MAX_LBRR_DELAY; i++ ) {
            totBytes += nBytesPerPacket[ i + 1 ];
        }
        SKP_memmove( payload, &payload[ nBytesPerPacket[ 0 ] ], totBytes * sizeof( SKP_uint8 ) );
        payloadEnd -= nBytesPerPacket[ 0 ];
        SKP_memmove( nBytesPerPacket, &nBytesPerPacket[ 1 ], MAX_LBRR_DELAY * sizeof( SKP_int16 ) );

        if( !quiet ) {
            fprintf( stderr, "\rPackets decoded:              %d", totPackets );
        }
    }

    //fwrite( outArr, sizeof( SKP_int16 ), outArrLen, speechOutFile );
    *outBuffer =  outArr;
    *outSize = outArrLen;
    
    if( !quiet ) {
        printf( "\nDecoding Finished \n" );
    }

    /* Free decoder */
    free( psDec );
    //free( jBuffer );
    //free( outArr );

    filetime = totPackets * 1e-3 * packetSize_ms;
    if( !quiet ) {
        printf("\nFile length:                 %.3f s", filetime);
        printf("\nTime for decoding:           %.3f s (%.3f%% of realtime)", 1e-6 * tottime, 1e-4 * tottime / filetime);
        printf("\n\n");
    } else {
        /* print time and % of realtime */
        printf( "%.3f %.3f %d\n", 1e-6 * tottime, 1e-4 * tottime / filetime, totPackets );
    }
    return 0;
}

__declspec(dllexport) int __cdecl GetResult(SKP_int32 num, SKP_uint8 **jBuffers){
    //jBuffers[0] =  99;
    *jBuffers = malloc(10);
    *jBuffers[0] = 88;
    
    return num * 2;
}

__declspec(dllexport) void __cdecl CleanBytePointer(SKP_uint8 *ptr){
    //jBuffers[0] =  99;
   free( ptr );   
}

__declspec(dllexport) void __cdecl CleanShortPointer(SKP_int16 *ptr){
    //jBuffers[0] =  99;
   free( ptr );   
}

__declspec(dllexport) void __cdecl CleanIntPointer(SKP_int32 *ptr){
    //jBuffers[0] =  99;
   free( ptr );   
}


int main(int argc, char* argv[] ){

    FILE      *bitInFile, *speechOutFile;
    SKP_int16 *out;
    char      speechOutFileName[ 150 ], bitInFileName[ 150 ];
    SKP_int32 args;
    SKP_uint8 *jBuffer = NULL;
    size_t jSize = 0;
    size_t outLen = 0;

    if( argc < 3 ) {
        print_usage( argv );
        exit( 0 );
    }

    args = 1;
    strcpy( bitInFileName, argv[ args ] );
    args++;
    strcpy( speechOutFileName, argv[ args ] );

    /* Open files */
    bitInFile = fopen( bitInFileName, "rb" );
    if( bitInFile == NULL ) {
        printf( "Error: could not open input file %s\n", bitInFileName );
        exit( 0 );
    }

    speechOutFile = fopen( speechOutFileName, "wb" );
    if( speechOutFile == NULL ) {
        printf( "Error: could not open output file %s\n", speechOutFileName );
        exit( 0 );
    }

    fseek(bitInFile, 0, SEEK_END); 
    size_t jFileSize = ftell(bitInFile); 
    fseek(bitInFile, 0, SEEK_SET);
    jBuffer = malloc(jFileSize);
    jSize = readFileAsByteArray(jBuffer, jFileSize, bitInFile);
    printf( "j-test:  file read    bufer first:%i  file len: %d\n", jBuffer[0], jFileSize);

    fseek(bitInFile, 0, SEEK_SET);

    SilkDecoderToPcm(jBuffer, jSize, &out, &outLen, 16000);
    printf( "j-test:  file read    out first:%i  outLen: %d\n", out[0], outLen);

    fwrite( out, sizeof( SKP_int16 ), outLen, speechOutFile);

    /* Close files */
    fclose( speechOutFile );
    fclose( bitInFile );

    /* Free decoder */
    free( jBuffer );
    free( out );


    return 0;
}