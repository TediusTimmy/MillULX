# LINEAR B : the interpretation-friendly cousin of MillULX

I was thinking of ways to simplify the mechanism of MillULX. Threads are expensive to synchronize.
In trying to balance my various desires, I came up with this compromise: the TickTock unit.
I like the entropy of having two instruction sets: I get a free bit. So, originally,
the Tick and Tock executed simultaneously, with the NOP bit delaying the other unit, as in the Mill.
But, I linearized the code by interpolating Tick and Tock instructions. So, the NOP bit now
suppresses the next operation of the other functional unit. That's why it's backwards.
Instead of a "switch functional units" bit, the switch is implicit and it is a "suppress switch"
bit.

### What are the differences?

1. ALU (Tick) instructions with an immediate operand have two extra bits of immediate.
2. The NOP bit is now uniformly the most significant bit, for both instruction sets.
3. All Flow (Tock) instructions have a destination belt bit in the same location (0x20 for Tick and 0x10 for Tock).
4. JMPI gets one extra bit of immediate.
5. If the slow belt destination bit is set in JMPI, then the immediate field is ignored and it is followed by an ARGS instruction with a 26-bit immediate.
6. CALLI is now conditional: it has the form of INT and is always followed by an ARGS instruction with a 26-bit immediate.
7. The bits in INT and CALL have moved: one bit of NOP has moved to after the instruction (the belt selector bit).
8. The belt destination bit is now honored for CALL, CALLI, and INT. (So the machine really has two general-purpose belts.)

# Note
I make mistakes and there are undoubtedly bugs in the VM. The "executable" format is poor and vulnerable to attack. Remember that this is a toy.

## Notes on interpretation:
I compiled both with g++ 7.4.0 with flags -O6 and -s. Yes, I know that -O6 isn't a thing, but that's what I like to use: the documentation states that any number higher than 3 is a synonym for 3.
The 1000 extra lines produce a 48KB larger executable (for reference, the big switch executable is only 29 KB).
I compiled Daniel Cristofani's bf interpreter in bf with my compiler and executed it, giving it Daniel's numwarp program with the input "123456789".
Over three runs, it took the big switch an average of 24.4 seconds, while the jump tables took an average of 19.5 seconds. Three runs isn't a large sample size, but the samples were all within one second of another.
When given no optimization options, these averages were 2 minutes, 44.9 seconds for the big switch, and 2 minutes, 34.4 seconds for the jump tables.

I'll note that while debugging the bf compiler (and the interpreter), I modified the big switch to determine what was going on in the generated code. The big switch is easier to read, easier to understand, and easier to maintain.
