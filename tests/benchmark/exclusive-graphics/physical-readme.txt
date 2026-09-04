MIGA-80 EXCLUSIVE GRAPHICS BENCHMARK - PHYSICAL TEST CANDIDATE

Target: PAL Amiga 1200, stock 68EC020, 2 MiB Chip RAM, no Fast RAM and
no accelerator. Kickstart 3.0 or 3.1 is accepted.

Keep this disk writable. Cold-boot the Amiga from it and do not press keys
while the benchmark is running. Screen changes and silence are expected.
The patterned screen and mouse pointer may remain responsive-looking while the
long fair/hog contention matrix is still running; this alone is not a hang.
The benchmark first writes RESULT.TXT with result=running, then replaces it
with the full report after cleanup. When measurements finish, the text screen
returns and announces that the buffered report is being written. A successful
run then prints:

MIGA-80 BENCHMARK RESULT: PASS

The full machine-readable report is saved as RESULT.TXT on this disk.
Wait for the floppy LED to stop before ejecting or powering off.

If the machine shows a Guru, persistent corrupted display, or does not finish
after ten minutes, photograph the screen before resetting and report the
elapsed time. Preserve the modified disk image even after a failed run. A
remaining result=running marker proves startup but means the run is incomplete.

This image contains no AmigaOS command files. It relies only on the
libraries and devices supplied by the machine's Kickstart ROM.
