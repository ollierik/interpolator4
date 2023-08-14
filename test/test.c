
#include <stdio.h>
#include <stdlib.h>
#include "interp4ff.h"

#define EPSILON_CMP(a, b) (fabs(a - b) > 1e-5)

/** Test src against dst with rate of 1.0 */
int naive_test(int ndst)
{
    int i, num_errors = 0;
    int nsrc = ndst + 2;
    float* src = (float*)malloc(sizeof(float) * nsrc);
    float* ref_dst = (float*)malloc(sizeof(float) * ndst);
    float* seg_dst = (float*)malloc(sizeof(float) * ndst);

    interp4ff ref_interp = interp4ff_create(ndst, 0);

    for (i = 0; i < ndst; ++i)
    {
        src[i] = (float)rand() / (float)RAND_MAX;
    }

    interp4ff_process(&ref_interp, ref_dst, ndst, src, nsrc, 1.0);

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

    interp4ff ref_interp = interp4ff_create(ndst, 0);
    interp4ff seg_interp = interp4ff_create(ndst, 0);
    
    srand(1);

    for (i = 0; i < nsrc; ++i)
    {
        src[i] = (float)rand() / (float)RAND_MAX;
    }

    isrc = 0; idst = 0;

    do {
        /* copy from src to segment */
        if (srcseg == NULL || seg_interp.state == Interp4State_SrcDepleted) {
            
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

        if (dstseg == NULL || seg_interp.state == Interp4State_DstDepleted) {

            /* check for output values */
            ndstseg = rand() % 7 + 1;
            
            if (ndstseg + idst >= ndst) {
                ndstseg = ndst - idst;
            }
            
            if (dstseg != NULL) free(dstseg);
            dstseg = (float*)malloc(sizeof(float) * ndstseg);
        }

        interp4ff_process(&seg_interp, dstseg, ndstseg, srcseg, nsrcseg, rate);
        
        if (seg_interp.state == Interp4State_DstDepleted)
        {
            for (i = 0; i < ndstseg; ++i)
            {
                dst[idst] = dstseg[i];
                idst++;
            }
        }
        
    } while (seg_interp.state != Interp4State_Done);
    
    for (i = 0; i < ndstseg; ++i)
    {
        dst[idst] = dstseg[i];
        idst++;
    }
    
    /* reference dst */
    interp4ff_process(&ref_interp, ref_dst, ndst, src, nsrc, rate);
    
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

/** Test src against dst with rate of 1.0 */
int linear_drift_test(int nsrc, float rate)
{
    int i, num_errors = 0;
    int ndst = (int)floor(nsrc / rate);
    float* src;
    float* dst = (float*)malloc(sizeof(float) * ndst);
    
    nsrc += 2;
    src = (float*)malloc(sizeof(float) * nsrc);
    

    interp4ff interp = interp4ff_create(ndst, 0);

    for (i = 0; i < ndst; ++i)
    {
        src[i] = i+1;
    }

    interp4ff_process(&interp, dst, ndst, src, nsrc, rate);

    for (i = 0; i < ndst; ++i)
    {
        printf("TEST %i: %.20f\n", i, dst[i]);
    }

    return 0;
}

/** Test src against dst with rate of 1.0 */
int drift_test(int nsrc, float rate)
{
    int i, num_errors = 0;
    int ndst = (int)floor(nsrc / rate);
    float* src;
    float* srcseg = (float*)malloc(sizeof(float) * 8);
    float* lindst = (float*)malloc(sizeof(float) * ndst);
    float* segdst = (float*)malloc(sizeof(float) * ndst);
    int isrc = 0;
    
    nsrc += 2;
    src = (float*)malloc(sizeof(float) * nsrc);

    interp4ff seg_interp = interp4ff_create(ndst, 0);
    interp4ff lin_interp = interp4ff_create(ndst, 0);

    for (i = 0; i < ndst; ++i)
    {
        src[i] = i+1;
    }
    
    /* segmented */
    do {
        for (i = 0; i < 8; ++i)
        {
            srcseg[i] = src[isrc];
            isrc++;
        }
        interp4ff_process(&seg_interp, lindst, ndst, srcseg, 8, rate);
        
    } while (seg_interp.state != Interp4State_Done);

    /* linear */
    interp4ff_process(&lin_interp, segdst, ndst, src, nsrc, rate);
    
    for (i = 0; i < ndst; ++i)
    {
        printf("TEST %i: %.20f %.20f\n", i, lindst[i], segdst[i]);
    }
    
    free(src); free(srcseg); free(lindst); free(segdst);

    return 0;
}

int main()
{
#if 0
    float rate = 0.1;
    
    naive_test(32);
    
    //while (rate < 8)
    {
        segmented_rw_test(4096, rate);
        rate += 0.1;
    }
#endif
    //linear_drift_test(512, 0.1);
    drift_test(512, 0.1);
}
