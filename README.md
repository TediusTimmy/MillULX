# Quick and Dirty Mill-like Virtual Machine

I don't know when Witold is finally going to deliver, so I've made something myself.  
This is a Mill-like processor. Primarily, I wanted to explore some ideas of the Mill Computer, among these are:

1. The Belt
2. Split-stream encoding
3. Metadata attached to the Belt, including NaRs and Nones
4. Elided NOPs

This implementation executes three instructions per "clock cycle". Two Exu or ALU instructions and one Flow instruction. The ALU PC counts up, while the Flow PC counts down. All use RISC-like instructions, because I coded this by hand, except the flow instructions are variable-length. Flow instruction extra data is encoded into the spare bits of NOP. Memory is composed of 32 bit words. A Belt location is a 32 bit word plus metadata. All three instructions see the same constant view of the belt while operating. The three instruction units then retire data to the belt in a deterministic order.

Instead of a Belt and a Scratchpad, I elected for two Belts: a "Fast" Belt and a "Slow" Belt. The Fast Belt is where calls retire their results. The ALUs treat the belts as two general-purpose belts.

So, to get into programming the monstrosity, I made a poor tool that will compile hand-generated machine code into a loadable program: ProgWrite. It allows you to make toy programs for the toy VM.

An impetus for me doing this was figuring out a slick way to synchronize the three threads (this is exactly the code that barriers are for). As such, it only compiles with a pthreads implementation present. I use Cygwin on Windows: it builds on Windows. I've also tested it in Rasbian on a Pi3 (some of the commented-out code produces warnings due to size_t not being a long type : it should compile warning-free with -Wall -Wextra -Wpedantic).

Note: I make mistakes and there are undoubtedly bugs in the VM. The "executable" format is poor, to say the least, and vulnerable to attack. Remember that this is a toy.

#### Condition Codes (Metadata)

The actual metadata that gets stored are these things:
* Carry/Borrow (Unsigned Overflow)
* Signed Overflow
* Zero
* Transient (my name for None, because I'm a pretentious tool)
* Invalid (NaR)

This gives these conditions for conditional branches:
* always branch
* not invalid and not transient : definite
* carry/borrow generated, unsigned overflow
* no carry/borrow/unsigned overflow
* signed overflow
* no signed overflow
* result negative, less
* result not negative, zero or positive, greater than or equal
* zero
* non-zero
* result not positive, zero or negative, less than or equal
* result positive, greater
* invalid
* not invalid
* transient
* not transient


#### Instructions
##### ALU

All ALU operations fall into one of two encodings: three operand and immediate operand. All allow for encoding up to 7 elided NOPs for the Flow stream per instruction. Note: unlike the Mill, elided NOPs do not occur until the NEXT instruction. In software, it isn't until retire that the other stream is notified of the NOPs. Most of the three operand opcodes are always conditional.

An operand is a value from 0 to 63: values 0 to 31 address the fast belt, and 32 to 63 address the slow belt. Both belts are 30 words in size. All operations can pull from either belt. Reading from a belt location that isn't a result of an operation results in INVALID. The last two entries in the fast belt always return 0 and 1 respectively. The last two entries in the slow belt return invalid and transient, respectively.

An immediate is a 17 bit signed value. (This is true for the ALU, not for Flow.)

###### nop
Can elide NOPs for the other stream.

###### addc (lhs, rhs, carry)
Uses the carry bit from operand 3. Carry/borrow out is stored with the RESULT that generated it, not as a machine global, not as an instruction trap. It's actually really elegant.

###### subb (lhs, rhs, borrow)
Uses the carry bit as the borrow bit from operand 3.

###### mull (lhs, rhs)
Drops two results: the low word and then the high word of the product.

###### divl (high, low, rhs)
Concatenates high and low and divides by operand 3. Drops two results: the quotient and the remainder. If the divisor is zero, drops two INVALIDs instead. If the quotient will not fit in a word, returns the quotient with the high bits truncated and sets the overflow flag on the quotient.

###### pick? (cond, source, true, false)
Pick either true or false depending on the condition from source.

###### add (cond, source, lhs, rhs)

###### sub (cond, source, lhs, rhs)

###### mul (cond, source, lhs, rhs)
Sets the overflow flag if the result had signed overflow.

###### div (cond, source, lhs, rhs)
Drops two results: the quotient and the remainder. Drops INVALIDs if the divisor is zero. LHS and RHS are treated as SIGNED.

###### udiv (cond, source, lhs, rhs)
Drops two results: the quotient and the remainder. Drops INVALIDs if the divisor is zero. LHS and RHS are treated as UNSIGNED.

###### shr (cond, source, lhs, rhs)
Logical. Shifts into CARRY. Defined as zero for rhs > bits in word. Treats shifts by negative amounts as a shift left.

###### ashr (cond, source, lhs, rhs)
Arithmetic. Defined as zero or negative one for rhs > bits in word (depending on the sign of lhs). Treats shifts by negative amounts as a shift left.

###### and (cond, source, lhs, rhs)

###### or (cond, source, lhs, rhs)

###### xor (cond, source, lhs, rhs)

###### addi (lhs, imm)

###### subi (lhs, imm)

###### muli (lhs, imm)

###### divi (lhs, imm)

###### udivi (lhs, imm)
Immediate is still sign-extended before division.

###### shri (lhs, imm)

###### ashri (lhs, imm)

###### andi (lhs, imm)

###### ori (lhs, imm)

###### xori (lhs, imm)


##### Flow

Note on addressing: the destinations for calls and branches are word addresses. They consider memory to be a sequence of words, as such the destination for a jump is necessarily word-aligned: the program counter lacks those low order bits. All branches to memory location zero are assumed to be erroneous and fault.

###### nop
Same as the ALU operation.

###### jmp (cond, source, dest)
The destination is relative to the entry point of the current block. A transient destination doesn't get taken, and an invalid destination faults.

###### ld (cond, source, mem)
Load memory location and drop it on the belt. This is a word address: the bottom two bits are implicitly zero. A load from transient results in transient. A load from invalid results in invalid. A load from outside of available memory results in invalid. A load whose condition is false drops a transient.

###### ldh (cond, source, mem)
Load a signed half word from the half-word address.

###### ldb (cond, source, mem)
Load a signed byte from the byte address.

###### st (cond, source, mem, val)
Store value to the word address. A store of transient is ignored. A store of invalid faults. A store outside of available memory faults.

###### sth (cond, source, mem, val)
Store value to the half-word address.

###### stb (cond, source, mem, val)
Store value to the byte address.

###### canon (dest, numargs)
Put the belt in canonical form. (Conform) The args become the new belt and the rest of the belt is invalidated. Dest chooses the belt to put in canonical form.

###### ret (numargs)
Return to the calling stack frame, dropping Numargs on its Fast belt. (Note: a return from the bottommost stack frame causes the VM to exit.) Note that numargs in the return does not have to match numrets in the call: that's why calli doesn't specify it.

###### jmpi (cond, source, dest)
The immediate is a signed 15 bit value and the destination is relative to the entry point of the current block. As the destination is immediate, it cannot be transient or invalid.

###### calli (dest, numargs)
Perform a call to destination. Numargs are placed on the belt of the callee. These are filled from belt positions specified in ganged NOPs that follow this instruction. As the destination is immediate, it cannot be transient or invalid. Immediate is a signed 20 bit value.

###### call (cond, source, dest, numargs, numrets)
Conditionally perform a call to destination. Numargs are placed on the belt of the callee. These are filled from belt positions specified in ganged NOPs that follow this instruction. If the destination is transient, or the call is not taken, place numrets transients on the belt. If the destination is invalid, fault.

###### int (cond, source, numargs, numrets)
Conditionally signal an interrupt. Numargs are placed on the belt of the callee. These are filled from belt positions specified in ganged NOPs that follow this instruction. If the call is not taken, place numrets transients on the belt.

###### args (first, second, third, fourth)
Not really an instruction: this is actually a nop with the destination belt set to the slow belt. These are the arguments to a call, return, canon, or interrupt.


##### Defined interrupts
These are here as a convenience in making the VM do anything useful. The interrupt number is the first argument to be pushed to the callee's belt. While I should be going right-to-left, inside the VM it makes more sense to go left-to-right. Interrupts should be Pascal called, despite taking a variable number of arguments.

###### 1 - putchar
As in C's putchar function. One extra argument, the character to put.

###### 2 - getchar
As in C's getchar function. Hangs the entire VM until input is given. Returns the input character, or -1 on EOF.

###### 3 - quit
Signals the VM to stop as if the bottommost stack frame were returned from.

###### 4 - gestalt
Currently returns zero. Gestalt without arguments should be interpreted as querying whether the interpreter has any extra features.

## Why is it called MillULX?
Well, I wanted to make a Mill-like Glulx. Glulx is a 32 bit virtual machine for running interactive fiction. It was built to overcome the limitations of Infocom's Z-Machine. There are some warts in the specification, due to how Inform compiles to Z-Machine. I don't know what the benefit of running three threads to implement the VM would be, though. So, I have a distant goal of building out the VM to support glk and have Inform 6 and 7 target it, but I should see if there is any benefit to this form of virtual machine.  
Postscript Note: Initial results from running compiled bf do not look good. Compiled bf is an order of magnitude slower than LINEAR_B.

Post-postscript: I don't think that I am going to bother working this out more than the initial investigation I have here. Glulx has been around for 20 years now, and even has an implementation that runs in a browser: Quixe. As much as it pains me, Javascript in the browser seems to be one of the best bets for the future of IF. This, as it stands, offers nothing that Glulx doesn't, and the multi-thread implementation does too little work between thread synchronizations to provide any performance benefit. So, I will cease for now on this.
