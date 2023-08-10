# interpolator4

interpolator4 is an ANSI-C cubic interpolation mostly intended for audio use.

It is able to read from and write to segmented memory, and handles the onerous
cesspool of of-by-one errors and required paddings by holding an internal state
and context buffer to carryover from one block to another.

This can make it particularily useful when working with ring buffers or chained
memory blocks, especially in real-time audio applications where the
interpolation can't be easily performed before-hand by more efficient and
accurate block-based algorithms. It should also be simple and efficient enough
to use even if no segmented memory is used.

I hate interpolation. I've had to implement it numerous times with slight
changes of corner cases. With these 150 lines of code, I hope I'll never have
to do it ever again.

The implementation is held in a single file, and uses static functions for
simplicity. Both float and double variant available.

## TODO:

- Better documentation
- Varirate implementation where rate may change per sample
