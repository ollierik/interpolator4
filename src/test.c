
#include <stdio.h>
#include <stdlib.h>
#include "interpolator4.h"

#define EPSILON_CMP(a, b) (fabs(a - b) > 1e-5)

/** Test src against dst with rate of 1.0 */
int naive_test(int ndst)
{
    int i, num_errors = 0;
    int nsrc = ndst + 2;
    float* src = (float*)malloc(sizeof(float) * nsrc);
    float* ref_dst = (float*)malloc(sizeof(float) * ndst);
    float* seg_dst = (float*)malloc(sizeof(float) * ndst);

    interpolator4f ref_interp;
    interpolator4f seg_interp;

    for (i = 0; i < ndst; ++i)
    {
        src[i] = (float)rand() / (float)RAND_MAX;
    }

    interpolator4f_init(&ref_interp, ndst, 0);
    interpolator4f_init(&seg_interp, ndst, 0);

    interpolator4f_process(&ref_interp, ref_dst, ndst, src, nsrc, 1.0);

    for (i = 0; i < ndst; ++i)
    {
        if (src[i] != ref_dst[i])
        {
            printf("ERROR: %i: %.20f %.20f\n", i, src[i], ref_dst[i]);
            num_errors++;
        }
    }
    printf("Linear RW test done, %i errors encountered.\n", num_errors);

    return 0;
}

/**
 Brute force test that splits both dst and src to segments with randomised
 lengths in range [1, 8], and compares those to linear interpolation.
 */
int segmented_rw_test(int ndst, float rate)
{
    int i, num_errors = 0;
    int nsrc = (int)ceil(ndst * rate) + 2;
    int nsrcseg = 0, ndstseg = 0;
    int isrc, idst;

    float* src = (float*)malloc(sizeof(float) * nsrc);
    float* dst = (float*)malloc(sizeof(float) * ndst);
    float* ref_dst = (float*)malloc(sizeof(float) * ndst);

    float* srcseg = NULL;
    float* dstseg = NULL;

    interpolator4f ref_interp = interpolator4f_create(ndst, 0);
    interpolator4f seg_interp = interpolator4f_create(ndst, 0);
    
    srand(1);

    for (i = 0; i < nsrc; ++i)
    {
        src[i] = (float)rand() / (float)RAND_MAX;
    }

    isrc = 0; idst = 0;

    do {
        /* copy from src to segment */
        if (srcseg == NULL || seg_interp.state == InterpolatorState_SrcDepleted) {
            
            nsrcseg = rand() % 7 + 1;
            if (nsrcseg + isrc >= nsrc) {
                nsrcseg = nsrc - isrc;
            }
            
            if (srcseg != NULL) free(srcseg);
            srcseg = (float*)malloc(sizeof(float) * nsrcseg);
            for (i = 0; i < nsrcseg; ++i) {
                srcseg[i] = src[isrc++];
            }
        }

        if (dstseg == NULL || seg_interp.state == InterpolatorState_DstDepleted) {

            /* check for output values */
            ndstseg = rand() % 7 + 1;
            
            if (ndstseg + idst >= ndst) {
                ndstseg = ndst - idst;
            }
            
            if (dstseg != NULL) free(dstseg);
            dstseg = (float*)malloc(sizeof(float) * ndstseg);
        }

        interpolator4f_process(&seg_interp, dstseg, ndstseg, srcseg, nsrcseg, rate);
        
        if (seg_interp.state == InterpolatorState_DstDepleted)
        {
            for (i = 0; i < ndstseg; ++i)
            {
                dst[idst] = dstseg[i];
                idst++;
            }
        }
        
    } while (seg_interp.state != InterpolatorState_Done);
    
    for (i = 0; i < ndstseg; ++i)
    {
        dst[idst] = dstseg[i];
        idst++;
    }
    
    /* reference dst */
    interpolator4f_process(&ref_interp, ref_dst, ndst, src, nsrc, rate);
    
    for (i = 0; i < ndst; ++i)
    {
        if (EPSILON_CMP(ref_dst[i], dst[i]))
        {
            printf("ERROR %i %.20f %.20f %.20f\n", i, ref_dst[i], dst[i], ref_dst[i] - dst[i]);
            num_errors++;
        }
        else
        {
            /*
            printf("OK %i %.20f %.20f %.20f\n", i, ref_dst[i], dst[i], ref_dst[i] - dst[i]);
             */
        }
    }
    
    printf("Segmented RW test done, %i errors encountered.\n", num_errors);

    return 0;
}

int main()
{
    float rate = 0.1;
    
    naive_test(32);
    
    while (rate < 8)
    {
        segmented_rw_test(512, 4.0);
        rate += 0.1;
    }
}
