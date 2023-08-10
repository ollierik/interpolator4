/******************************************************************************
interpolator4.h

Copyright 2023 Olli Erik Keskinen

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
******************************************************************************/

#ifndef INTERPOLATOR_H
#define INTERPOLATOR_H

#include <math.h>

typedef enum {
    InterpolatorState_Done = 0,
    InterpolatorState_Init,
    InterpolatorState_SrcDepleted,
    InterpolatorState_DstDepleted,
} InterpolatorState;

#ifndef INTERPOLATOR4_CONTEXT_SIZE
#   define INTERPOLATOR4_CONTEXT_SIZE 9
#endif

/***********************
 * float implementation
 ***********************/

typedef struct t_interpolator4f
{
    float context [INTERPOLATOR4_CONTEXT_SIZE]; /* overlap context memory */
    int context_index;                          /* index of the next free slot */
    int context_position;                       /* position of the first context element */
    float position;                             /* local position, gets reset with every src depletion */
    int dst_index;                              /* dst output index, gets reset with every dst depletion */
    int num_remaining;                          /* number of samples total to interpolate */
    
    InterpolatorState state;                    /* both src and dst can't deplete on the same pass.
                                                   src depletion takes priority */
} interpolator4f;
    
/** Create and return a new interpolator4f. */
static interpolator4f interpolator4f_create(int num_to_write, float initial_state);

/** (Re)initialise an existing interpolator4f */
static void interpolator4f_init(interpolator4f* interp, int num_to_write, float initial_state);

/** Write interpolated values to dst. The process stops when either dst or src are depleted.
    The reason for stopping can be queried from interpolator4f.state variable.
    */
static void interpolator4f_process(interpolator4f* interp, float* dst, int ndst, const float* src, int nsrc, float rate);



static void interpolator4f_init(interpolator4f* interp, int num_to_write, float initial_state)
{
    interp->context[0] = initial_state;
    interp->context_index = 1;
    interp->context_position = -1;
    interp->position = 0.0;
    interp->dst_index = 0;
    interp->num_remaining = num_to_write;
    interp->state = InterpolatorState_Init;
}

static interpolator4f interpolator4f_create(int num_to_write, float initial_state)
{
    interpolator4f interp;
    interpolator4f_init(&interp, num_to_write, initial_state);
    return interp;
}

static float interpolator4f__cubic_interp(const float* x, float fract)
{
    const float x21_diff = x[2] - x[1];
    const float common_term = (x[3] - x[0] - (float)(3.0) * x21_diff) * fract
                        + (x[3] + (float)(2.0) * x[0] - (float)(3.0) * x[1]);
    
    const float value = x[1] + fract * (x21_diff - (float)(0.1666667) * ((float)(1.0) - fract) * common_term);
    
    /*
    printf("%i: [%.20f, %.20f, %.20f, %.20f] (fract: %.20f) -> %.20f\n", interp_counter++, x[0], x[1], x[2], x[3], fract, value);
     */

    return value;
}
    
static int interpolator4f__read_from_context(interpolator4f* interp, float* dst, float rate, int n)
{
    int num_read = n; /* init to n, substract after loop */

    int ipos, index;
    float fract, pos;
    const int maxpos = interp->context_position + interp->context_index - 3;

    pos = interp->position;
    dst = dst + interp->dst_index; /* dst is incremented as ptr */

    ipos = (int)(floor(interp->position));
    
    while (ipos <= maxpos && n > 0)
    {
        index = ipos - interp->context_position - 1;
        fract = pos - ipos;
        
        *dst++ = interpolator4f__cubic_interp(&interp->context[index], fract);
        
        pos += rate;
        n--;
        ipos = (int)(floor(pos));
    }
    num_read = num_read - n;

    /* store */
    interp->position = pos;
    interp->num_remaining -= num_read;
    interp->dst_index += num_read;

    return n;
}

static void interpolator4f__read_from_src(interpolator4f* interp, float* dst, const float* src, float rate, int n)
{
    int num_read = n; /* init to n, substract after loop*/
    float pos = interp->position;

    /* temps */
    int ipos, index;
    float fract;

    dst = dst + interp->dst_index;

    while (n > 0) {

        ipos = (int)(pos);
        index = ipos - 1;
        fract = pos - ipos;
        
        *dst++ = interpolator4f__cubic_interp(&src[index], fract);
        
        pos += rate;
        n--;
    }
    
    num_read -= n;

    /* store */
    interp->position = pos;
    interp->dst_index += num_read;
    interp->num_remaining -= num_read;
}

static void interpolator4f_process(interpolator4f* interp, float* dst, int ndst, const float* src, int nsrc, float rate)
{
    int i, j, n, m;
    int last_index;

    /* Check that we're not continuing with the same src */
    if (interp->state != InterpolatorState_DstDepleted) {

        /* We're either starting up or continuing with a new block. Copy
           newly available samples to the end of the context buffer. */
        m = nsrc < 3 ? nsrc : 3;
        
        for (i = 0; i < m; ++i) {

            /* If we're about to overflow, shift tail to the start of
               the context. This is a fairly unlikely case. */
            if (interp->context_index == INTERPOLATOR4_CONTEXT_SIZE) {

                for (j = 0; j < 5; ++j) {
                    interp->context[j] = interp->context[interp->context_index - 5 + j];
                }
                interp->context_index = 5;
                interp->context_position += INTERPOLATOR4_CONTEXT_SIZE - 5;
            }
                
            interp->context[interp->context_index++] = src[i];
        }
    }
    
    /* Clear out old state flags */
    interp->state = InterpolatorState_Done;
    
    /* n is how many samples we may at most write to dst */
    n = ndst - interp->dst_index;
    
    if (n < interp->num_remaining) {
        /* dst is shorter than the number of requested samples */
        interp->state = InterpolatorState_DstDepleted;
    } else {
        /* dst is equal or too long, truncate */
        n = interp->num_remaining;
    }
    
    n = interpolator4f__read_from_context(interp, dst, rate, n);
    
    /* Context reading may have depleted all available space in dst. */
    if (n > 0) {
        
        /* Check if we will overflow, i.e. if src doesn't have enough samples
           available. */
        last_index = (int)(interp->position + n * rate);
        
        if (last_index > nsrc - 3) {
            
            /* src is going to get depleted with this call */
            interp->state = InterpolatorState_SrcDepleted;
            
            /* Adjust how many samples we're still able to write */
            n = (int)ceil(((float)(nsrc - 2) - interp->position) / rate);
        }
        
        /* do the main interpolation loop */
        interpolator4f__read_from_src(interp, dst, src, rate, n);
    }
    
    if (interp->state == InterpolatorState_SrcDepleted) {

        interp->position -= nsrc;
        
        /* Fill the context by copying the 2 last and by reading 3 new elements
           to the beginning of the context. If nsrc is not greater than we've
           already pushed all available samples before the interpolation. */
        if (nsrc > 3) {

            interp->context[0] = interp->context[interp->context_index - 2];
            interp->context[1] = interp->context[interp->context_index - 1];
            interp->context[2] = src[nsrc - 3];
            interp->context[3] = src[nsrc - 2];
            interp->context[4] = src[nsrc - 1];
            
            interp->context_index = 5;
            interp->context_position = -5;
            
        } else {
            interp->context_position -= nsrc;
        }
    }
    else if (interp->state == InterpolatorState_DstDepleted)
    {
        interp->dst_index = 0;
    }
}



/***********************
 * double implementation
 ***********************/

typedef struct t_interpolator4d
{
    double context [INTERPOLATOR4_CONTEXT_SIZE];    /* overlap context memory */
    int context_index;                              /* index of the next free slot */
    int context_position;                           /* position of the first context element */
    double position;                                /* local position, gets reset with every src depletion */
    int dst_index;                                  /* dst output index, gets reset with every dst depletion */
    int num_remaining;                              /* number of samples total to interpolate */
    
    InterpolatorState state;                        /* both src and dst can't deplete on the same pass.
                                                       src depletion takes priority */
} interpolator4d;

static void interpolator4d_init(interpolator4d* interp, int num_to_write, double initial_state)
{
    interp->context[0] = initial_state;
    interp->context_index = 1;
    interp->context_position = -1;
    interp->position = 0.0;
    interp->dst_index = 0;
    interp->num_remaining = num_to_write;
    interp->state = InterpolatorState_Init;
}

static interpolator4d interpolator4d_create(int num_to_write, double initial_state)
{
    interpolator4d interp;
    interpolator4d_init(&interp, num_to_write, initial_state);
    return interp;
}

static double interpolator4d__cubic_interp(const double* x, double fract)
{
    const double x21_diff = x[2] - x[1];
    const double common_term = (x[3] - x[0] - (double)(3.0) * x21_diff) * fract
                        + (x[3] + (double)(2.0) * x[0] - (double)(3.0) * x[1]);
    
    const double value = x[1] + fract * (x21_diff - (double)(0.1666667) * ((double)(1.0) - fract) * common_term);
    
    /*
    printf("%i: [%.20f, %.20f, %.20f, %.20f] (fract: %.20f) -> %.20f\n", interp_counter++, x[0], x[1], x[2], x[3], fract, value);
     */

    return value;
}
    
static int interpolator4d__read_from_context(interpolator4d* interp, double* dst, double rate, int n)
{
    int num_read = n; /* init to n, substract after loop */

    int ipos, index;
    double fract, pos;
    const int maxpos = interp->context_position + interp->context_index - 3;

    pos = interp->position;
    dst = dst + interp->dst_index; /* dst is incremented as ptr */

    ipos = (int)(floor(interp->position));
    
    while (ipos <= maxpos && n > 0)
    {
        index = ipos - interp->context_position - 1;
        fract = pos - ipos;
        
        *dst++ = interpolator4d__cubic_interp(&interp->context[index], fract);
        
        pos += rate;
        n--;
        ipos = (int)(floor(pos));
    }
    num_read = num_read - n;

    /* store */
    interp->position = pos;
    interp->num_remaining -= num_read;
    interp->dst_index += num_read;

    return n;
}

static void interpolator4d__read_from_src(interpolator4d* interp, double* dst, const double* src, double rate, int n)
{
    int num_read = n; /* init to n, substract after loop*/
    double pos = interp->position;

    /* temps */
    int ipos, index;
    double fract;

    dst = dst + interp->dst_index;

    while (n > 0) {

        ipos = (int)(pos);
        index = ipos - 1;
        fract = pos - ipos;
        
        *dst++ = interpolator4d__cubic_interp(&src[index], fract);
        
        pos += rate;
        n--;
    }
    
    num_read -= n;

    /* store */
    interp->position = pos;
    interp->dst_index += num_read;
    interp->num_remaining -= num_read;
}

static void interpolator4d_process(interpolator4d* interp, double* dst, int ndst, const double* src, int nsrc, double rate)
{
    int i, j, n, m;
    int last_index;

    /* Check that we're not continuing with the same src */
    if (interp->state != InterpolatorState_DstDepleted) {

        /* We're either starting up or continuing with a new block. Copy
           newly available samples to the end of the context buffer. */
        m = nsrc < 3 ? nsrc : 3;
        
        for (i = 0; i < m; ++i) {

            /* If we're about to overflow, shift tail to the start of
               the context. This is a fairly unlikely case. */
            if (interp->context_index == INTERPOLATOR4_CONTEXT_SIZE) {

                for (j = 0; j < 5; ++j) {
                    interp->context[j] = interp->context[interp->context_index - 5 + j];
                }
                interp->context_index = 5;
                interp->context_position += INTERPOLATOR4_CONTEXT_SIZE - 5;
            }
                
            interp->context[interp->context_index++] = src[i];
        }
    }
    
    /* Clear out old state flags */
    interp->state = InterpolatorState_Done;
    
    /* n is how many samples we may at most write to dst */
    n = ndst - interp->dst_index;
    
    if (n < interp->num_remaining) {
        /* dst is shorter than the number of requested samples */
        interp->state = InterpolatorState_DstDepleted;
    } else {
        /* dst is equal or too long, truncate */
        n = interp->num_remaining;
    }
    
    n = interpolator4d__read_from_context(interp, dst, rate, n);
    
    /* Context reading may have depleted all available space in dst. */
    if (n > 0) {
        
        /* Check if we will overflow, i.e. if src doesn't have enough samples
           available. */
        last_index = (int)(interp->position + n * rate);
        
        if (last_index > nsrc - 3) {
            
            /* src is going to get depleted with this call */
            interp->state = InterpolatorState_SrcDepleted;
            
            /* Adjust how many samples we're still able to write */
            n = (int)ceil(((double)(nsrc - 2) - interp->position) / rate);
        }
        
        interpolator4d__read_from_src(interp, dst, src, rate, n);
    }
    
    if (interp->state == InterpolatorState_SrcDepleted) {

        interp->position -= nsrc;
        
        /* Fill the context by copying the 2 last and by reading 3 new elements
           to the beginning of the context. If nsrc is not greater than we've
           already pushed all available samples before the interpolation. */
        if (nsrc > 3) {

            interp->context[0] = interp->context[interp->context_index - 2];
            interp->context[1] = interp->context[interp->context_index - 1];
            interp->context[2] = src[nsrc - 3];
            interp->context[3] = src[nsrc - 2];
            interp->context[4] = src[nsrc - 1];
            
            interp->context_index = 5;
            interp->context_position = -5;
            
        } else {
            interp->context_position -= nsrc;
        }
    }
    else if (interp->state == InterpolatorState_DstDepleted)
    {
        interp->dst_index = 0;
    }
}

#endif /* INTERPOLATOR_H */ 