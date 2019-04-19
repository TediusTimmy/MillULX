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