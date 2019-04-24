/*
Copyright (c) 2019, Thomas DiModica
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the names of the copyright holders nor the names of other
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <vector>
#include <cstdio>
#include <cstring>

typedef long long BELT_T;
typedef unsigned int MEM_T;

static const size_t BELT_SIZE = 32U;

static const BELT_T TRANSIENT =  0x200000000LL;
static const BELT_T INVALID   =  0x400000000LL;
static const BELT_T OVERFLOW  =  0x800000000LL;
static const BELT_T ZERO      = 0x1000000000LL;
static const BELT_T EMPTY     = 0x2000000000LL;

static const BELT_T CARRY     =  0x100000000LL;
static const BELT_T NEGATIVE  =   0x80000000LL;

static const char* endian()
 {
   const short var = 0x454C;
   return (0 == std::strncmp("LE", static_cast<const char*>(static_cast<const void*>(&var)), 2U)) ? "LE" : "BE";
 }

class Frame
 {
public:
   BELT_T fast [BELT_SIZE];
   BELT_T slow [BELT_SIZE];
   size_t ffront, fsize; // The front and size of the fast belt
   size_t sfront, ssize; // The front and size of the slow belt
   size_t pc;
   size_t entryPoint; // The destination of the last branch or call.
   MEM_T callOp;

   void init (void)
    {
      for (size_t i = 0U; i < BELT_SIZE; ++i) fast[i] = INVALID;
      for (size_t i = 0U; i < BELT_SIZE; ++i) slow[i] = INVALID;
      ffront = 0U;
      fsize = 0U;
      sfront = 0U;
      ssize = 0U;
      pc = 0U;
      entryPoint = 0U;
    }

   void write(std::FILE * file)
    {
      std::fwrite(static_cast<void*>(fast), sizeof(BELT_T), BELT_SIZE, file);
      std::fwrite(static_cast<void*>(slow), sizeof(BELT_T), BELT_SIZE, file);
      std::fwrite(static_cast<void*>(&ffront), sizeof(size_t), 1U, file); //std::printf("ffront %lu\n", ffront);
      std::fwrite(static_cast<void*>(&fsize), sizeof(size_t), 1U, file); //std::printf("fsize %lu\n", fsize);
      std::fwrite(static_cast<void*>(&sfront), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(&ssize), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(&pc), sizeof(size_t), 1U, file); //std::printf("pc %lu\n", pc);
      std::fwrite(static_cast<void*>(&entryPoint), sizeof(size_t), 1U, file); //std::printf("entryPoint %lu\n", entryPoint);
    }

   void read(std::FILE * file)
    {
      std::fread(static_cast<void*>(fast), sizeof(BELT_T), BELT_SIZE, file);
      std::fread(static_cast<void*>(slow), sizeof(BELT_T), BELT_SIZE, file);
      std::fread(static_cast<void*>(&ffront), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&fsize), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&sfront), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&ssize), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&pc), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&entryPoint), sizeof(size_t), 1U, file);
    }
 };

class Machine
 {
public:
   std::vector<Frame> frames;
   MEM_T * memory;
   size_t memsize;
   bool invalidOp;
   bool stop;

   Machine() : memory(NULL), memsize(0U), invalidOp(false), stop(false)
    {
      frames.push_back(Frame());
    }

   void write(std::FILE * file)
    {
      std::fwrite(static_cast<void*>(&memsize), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(memory), sizeof(MEM_T), memsize, file);
      size_t framesSize = frames.size();
      std::fwrite(static_cast<void*>(&framesSize), sizeof(size_t), 1U, file);
      for (size_t i = 0U; i < framesSize; ++i)
       {
         frames[i].write(file);
       }
    }

   void read(std::FILE * file)
    {
      std::fread(static_cast<void*>(&memsize), sizeof(size_t), 1U, file);
      memory = new MEM_T [memsize];
      std::fread(static_cast<void*>(memory), sizeof(MEM_T), memsize, file);
      size_t framesSize;
      std::fread(static_cast<void*>(&framesSize), sizeof(size_t), 1U, file);
      frames.clear();
      for (size_t i = 0U; i < framesSize; ++i)
       {
         frames.push_back(Frame());
         frames.back().read(file);
       }
    }
 };

class FunctionalUnit
 {
public:
   Machine* machine;

   virtual void doStuff() = 0;

   BELT_T getMemory(size_t location)
    {
      if (location >= machine->memsize)
       {
         // TODO : would throwing a CPU cache invalidation op here mitigate Spectre?
         return INVALID;
       }
      return machine->memory[location];
    }

   BELT_T setMemory(size_t location, MEM_T value)
    {
      if (location >= machine->memsize)
       {
         // TODO : would throwing a CPU cache invalidation op here mitigate Spectre?
         return INVALID;
       }
      machine->memory[location] = value;
      return 0U;
    }

   static BELT_T getBeltContent(Frame& frame, size_t beltLocation)
    {
      if (0U == (beltLocation & 0x20))
       {
         if ((beltLocation > frame.fsize) && (beltLocation < 30))
          {
            return INVALID;
          }
         return frame.fast[(frame.ffront + beltLocation) & 0x1F];
       }
      else
       {
         if (((beltLocation & 0x1F) > frame.ssize) && (beltLocation < 62))
          {
            return INVALID;
          }
         return frame.slow[(frame.sfront + beltLocation) & 0x1F];
       }
    }

   bool extraNumerical(BELT_T op, BELT_T& res)
    {
      if (0L != (op & (TRANSIENT | INVALID)))
       {
         res = op;
         return true;
       }
      return false;
    }

   bool extraNumerical(BELT_T op1, BELT_T op2, BELT_T& res)
    {
      if (0L != ((op1 & op2) & TRANSIENT))
       {
         res = op1 > op2 ? op1 : op2; // Assume that the flow address of highest value is
         return true; // the chronologically earlier instruction
       }
      if (0L != (op1 & TRANSIENT)) // TRANSIENT takes precedence over INVALID
       {
         res = op1;
         return true;
       }
      if (0L != (op2 & TRANSIENT))
       {
         res = op2;
         return true;
       }
      if (0L != ((op1 & op2) & INVALID))
       {
         res = op1 > op2 ? op1 : op2;
         return true;
       }
      if (0L != (op1 & INVALID)) // Save the metadata for INVALID
       {
         res = op1;
         return true;
       }
      if (0L != (op2 & INVALID))
       {
         res = op2;
         return true;
       }
      return false;
    }

   bool extraNumerical(BELT_T op1, BELT_T op2, BELT_T op3, BELT_T& res)
    {
      BELT_T temp = TRANSIENT;
      bool result = false;
      if (0L != (op1 & TRANSIENT))
       {
         temp = temp > op1 ? temp : op1;
         result = true;
       }
      if (0L != (op2 & TRANSIENT))
       {
         temp = temp > op2 ? temp : op2;
         result = true;
       }
      if (0L != (op3 & TRANSIENT))
       {
         temp = temp > op3 ? temp : op3;
         result = true;
       }
      if (true == result)
       {
         res = temp;
         return true;
       }
      temp = INVALID;
      if (0L != (op1 & INVALID))
       {
         temp = temp > op1 ? temp : op1;
         result = true;
       }
      if (0L != (op2 & INVALID))
       {
         temp = temp > op2 ? temp : op2;
         result = true;
       }
      if (0L != (op3 & INVALID))
       {
         temp = temp > op3 ? temp : op3;
         result = true;
       }
      if (true == result)
       {
         res = temp;
       }
      return result;
    }

   static BELT_T getZero(BELT_T input)
    {
      if (0U == (input & 0xFFFFFFFFLL))
       {
         return ZERO;
       }
      return 0;
    }

   BELT_T getAdd(BELT_T op1, BELT_T op2, BELT_T op3)
    {
      BELT_T cb = (op3 & CARRY) ? 1 : 0;
      BELT_T result = (op1 + op2 + cb) & 0x1FFFFFFFFLL; // CARRY/BORROW is free
      if (0U != ((result ^ op1) & (result ^ op2) & 0x80000000))
       {
         result |= OVERFLOW;
       }
      return result;
    }

   bool conditionTrue(BELT_T cond, BELT_T flags)
    {
      if (0U != (cond & ~0xFLL))
       {
         std::printf("Arrived in conditionTrue with invalid condition code.\nThis is a bug.\n");
         machine->invalidOp = true;
         return false;
       }
/*
      switch (cond)
       {
         case 0: // ALWAYS
         case 1: // DEFINITE : NOT INVALID AND NOT TRANSIENT
         case 2: // CARRY
         case 3: // NO CARRY
         case 4: // Signed Overflow
         case 5: // No Signed Overflow
         case 6: // NEGATIVE (Less)
         case 7: // NOT NEGATIVE (Greater than or equal) (Zero or Positive)
         case 8: // ZERO
         case 9: // NOT ZERO
         case 10: // NOT POSITIVE (Less Than or Equal) (Zero or Negative)
         case 11: // POSITIVE (Greater) (Not Zero and Not Negative)
         case 12: // INVALID
         case 13: // NOT INVALID
         case 14: // TRANSIENT
         case 15: // NOT TRANSIENT
       }
*/
      BELT_T conds [] = { 0U, CARRY, OVERFLOW, NEGATIVE, ZERO, ZERO | NEGATIVE, INVALID, TRANSIENT };
      if (0 == cond)
       {
         return true;
       }
      else if (1 == cond)
       {
         return (0U == (flags & (INVALID | TRANSIENT)));
       }
      else
       {
         if (0U == (cond & 1))
          {
            return (0U != (flags & conds[cond >> 1]));
          }
         else
          {
            return (0U == (flags & conds[cond >> 1]));
          }
       }
    }

   static void retire(Frame& frame, BELT_T value)
    {
      frame.ffront = (frame.ffront - 1) & 0x1F;
      frame.fast[frame.ffront] = value;
      frame.fsize = frame.fsize < BELT_SIZE ? frame.fsize + 1 : BELT_SIZE;

      frame.fast[(frame.ffront + 30) & 0x1F] = ZERO;
      frame.fast[(frame.ffront + 31) & 0x1F] = 1;
    }

   static void slowretire(Frame& frame, BELT_T value)
    {
      frame.sfront = (frame.sfront - 1) & 0x1F;
      frame.slow[frame.sfront] = value;
      frame.ssize = frame.ssize < BELT_SIZE ? frame.ssize + 1 : BELT_SIZE;

      frame.slow[(frame.sfront + 30) & 0x1F] = INVALID;
      frame.slow[(frame.sfront + 31) & 0x1F] = TRANSIENT;
    }

   void fillBelt(Frame& frame, int num, BELT_T* rets)
    {
      BELT_T cur = 0U;
      for (int i = 0; i < num; ++i)
       {
         if (0 == (i % 4))
          {
            ++frame.pc;
            cur = getMemory(frame.pc);
            if (0U != (cur & INVALID))
             {
               machine->invalidOp = true;
               return;
             }
            if (0x10 != (cur & 0x1F)) // Make sure this is an ARGS NOP
             {
               machine->invalidOp = true;
               return;
             }
          }
         rets[i] = getBeltContent(frame, (cur >> (5 + 6 * (i % 4))) & 0x3F);
       }
    }

   static void serviceInterrupt(Machine& machine, int /*serviceCode*/, const BELT_T* args, BELT_T* rets)
    {
      switch (args[0] & 0xFFFFFFFFLL)
       {
         case 1: // request put character, TODO deprecate
            putchar(args[1]);
            break;
         case 2: // request get character, TODO deprecate
            rets[0] = getchar() & 0xFFFFFFFFLL;
            rets[0] |= getZero(rets[0]);
            break;
         case 3: // request stop
            machine.stop = true;
            break;
         case 4: // gestalt : currently return zero
            rets[0] = ZERO;
            break;
         default: // INVALID OPERATION
            std::printf("Terminate initiated due to invalid interrupt: %lld\n", args[0]);
            machine.invalidOp = true;
            break;
       }
    }
 };

class TickTockUnit : public FunctionalUnit
 {
public:
   virtual void doStuff()
    {
/*
   This is ugly. What am I trying to achieve?

   To the best of my knowledge and understanding, modern processors are REALLY bad at indirect branch prediction.
   They are EXCEPTIONALLY bad at predicting the destination of a switch statement implementing a virtual machine.
   So, what we do here is use the GNU C extension of label pointers to create a jump table for each of the virtual
   instructions, and then have a jump follow each instruction. This supposedly helps the branch predictor in that
   instructions generally occur in runs, with specific instructions generally following other instructions. So, we
   duplicate all of the rote setup and teardown code in the hope that we assist the branch predictor in correctly
   deciding which instruction will be executed next, so that we do not branch mis-predict.

   Note: the jump MUST be duplicated for each virtual instruction, so that each jump has its own memory location,
   and thus is predicted separately from other jumps.

   The problem with this structure is that it greatly increases the code size. More code means more cache misses,
   which means slower execution. So the question really is about where the balancing point is between cache misses
   and branch mispredicts.
*/
      void * tickJumpTable [] = {
   &&NOPI, &&ADDCF, &&SUBBF, &&MULLF, &&DIVLF, &&PICKF, &&ADDF,  &&SUBF,  &&MULF,  &&DIVF,  &&UDIVF,  &&SHRF,  &&ASHRF,  &&ANDF,  &&ORF,  &&XORF,
   &&INV,  &&INV,   &&INV,   &&INV,   &&INV,   &&INV,   &&ADDIF, &&SUBIF, &&MULIF, &&DIVIF, &&UDIVIF, &&SHRIF, &&ASHRIF, &&ANDIF, &&ORIF, &&XORIF,
   &&NOPI, &&ADDCS, &&SUBBS, &&MULLS, &&DIVLS, &&PICKS, &&ADDS,  &&SUBS,  &&MULS,  &&DIVS,  &&UDIVS,  &&SHRS,  &&ASHRS,  &&ANDS,  &&ORS,  &&XORS,
   &&INV,  &&INV,   &&INV,   &&INV,   &&INV,   &&INV,   &&ADDIS, &&SUBIS, &&MULIS, &&DIVIS, &&UDIVIS, &&SHRIS, &&ASHRIS, &&ANDIS, &&ORIS, &&XORIS };
      void * tockJumpTable [] = {
   &&NOPO, &&JMP, &&LDF, &&LDHF, &&LDBF, &&ST, &&STH, &&STB, &&CANONF, &&RET, &&JMPIF, &&CALLI, &&CALL, &&INTF, &&INV, &&INV,
   &&INV,  &&JMP, &&LDS, &&LDHS, &&LDBS, &&ST, &&STH, &&STB, &&CANONS, &&RET, &&JMPIS, &&CALLI, &&CALL, &&INTS, &&INV, &&INV };
      BELT_T curOp, nextOp;
      Frame* frame = &machine->frames.back();

#define OP_INTRO(x) \
x: \
      { \
         /* Do we need to die? */ \
         if (true == (machine->invalidOp | machine->stop) ) \
          { \
            return; \
          }

#define REG_OP_INTRO \
         BELT_T cond, src, op1, op2, temp; \
         cond = (curOp >> 6) & 0xF; \
         src = getBeltContent(*frame, (curOp >> 10) & 0x3F); \
         op1 = getBeltContent(*frame, (curOp >> 16) & 0x3F); \
         op2 = getBeltContent(*frame, (curOp >> 22) & 0x3F);

#define IMM_OP_INTRO \
         BELT_T cond, src, op1, op2, temp; \
         cond = 0; /* Unconditional */ \
         src = 0; \
         op1 = getBeltContent(*frame, (curOp >> 6) & 0x3F); \
         op2 = (curOp >> 12) & 0x7FFFF; \
         if (op2 & 0x40000) \
          { \
            op2 |= 0xFFF80000; \
          }

#define LONG_OP_INTRO \
         BELT_T op1 = getBeltContent(*frame, (curOp >> 10) & 0x3F); \
         BELT_T op2 = getBeltContent(*frame, (curOp >> 16) & 0x3F); \
         BELT_T op3 = getBeltContent(*frame, (curOp >> 22) & 0x3F); \
         BELT_T temp;

#define TOCK_OP_INTRO \
         BELT_T cond, src, num, op1, op2, temp; \
         cond = (curOp >> 5) & 0xF; \
         src = getBeltContent(*frame, (curOp >> 9) & 0x3F); \
         num = (curOp >> 15) & 0x3F; \
         op1 = getBeltContent(*frame, num); \
         op2 = getBeltContent(*frame, (curOp >> 21) & 0x3F);

#define OP_BASE_CASES_FAST \
         if (false == conditionTrue(cond, src)) \
          { \
            retire(*frame, TRANSIENT | frame->pc); \
            if ((9 == (curOp & 0xF)) || (10 == (curOp & 0xF))) \
             { \
               retire(*frame, TRANSIENT | frame->pc); \
             } \
          } \
         else if (true == extraNumerical(op1, op2, temp)) \
          { \
            retire(*frame, temp); \
            if ((9 == (curOp & 0xF)) || (10 == (curOp & 0xF))) \
             { \
               retire(*frame, temp); \
             } \
          } \
         else \
          {

#define OP_BASE_CASES_SLOW \
         if (false == conditionTrue(cond, src)) \
          { \
            slowretire(*frame, TRANSIENT | frame->pc); \
            if ((9 == (curOp & 0xF)) || (10 == (curOp & 0xF))) \
             { \
               slowretire(*frame, TRANSIENT | frame->pc); \
             } \
          } \
         else if (true == extraNumerical(op1, op2, temp)) \
          { \
            slowretire(*frame, temp); \
            if ((9 == (curOp & 0xF)) || (10 == (curOp & 0xF))) \
             { \
               slowretire(*frame, temp); \
             } \
          } \
         else \
          {

#define MACHINE_START \
         nextOp = getMemory(frame->pc); \
         if (0U != (nextOp & INVALID)) \
          { \
            std::printf("Terminate initiated due to invalid program counter: %d\n", static_cast<int>(frame->pc)); \
            machine->invalidOp = true; \
            return; \
          }

#define LONG_OP_CASE_END \
         ++frame->pc; \
         MACHINE_START

#define OP_CASE_END \
          } \
         LONG_OP_CASE_END

#define DISPATCH_NEXT_TICK \
         curOp = nextOp; \
         goto *tickJumpTable[curOp & 0x3F];

#define DISPATCH_NEXT_TOCK \
         curOp = nextOp; \
         goto *tockJumpTable[curOp & 0x1F];

#define DISPATCH_NEXT_FROM_TICK \
         if (0U == (curOp & 0x80000000)) \
          { \
            DISPATCH_NEXT_TOCK \
          } \
         else \
          { \
            DISPATCH_NEXT_TICK \
          } \
       }

#define DISPATCH_NEXT_FROM_TOCK \
         if (0U == (curOp & 0x80000000)) \
          { \
            DISPATCH_NEXT_TICK \
          } \
         else \
          { \
            DISPATCH_NEXT_TOCK \
          } \
       }

#define RETIRE(x) \
            x |= getZero(x); \
            retire(*frame, x);

#define SLOWRETIRE(x) \
            x |= getZero(x); \
            slowretire(*frame, x);

         MACHINE_START
         DISPATCH_NEXT_TICK

OP_INTRO(NOPI)
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ADDCF)
LONG_OP_INTRO
         if (false == extraNumerical(op1, op2, temp))
          {
            temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, op3);
            temp |= getZero(temp);
          }
         retire(*frame, temp);
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SUBBF)
LONG_OP_INTRO
         if (false == extraNumerical(op1, op2, temp))
          {
            temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, op3 ^ CARRY) ^ CARRY;
            temp |= getZero(temp);
          }
         retire(*frame, temp);
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(MULLF)
LONG_OP_INTRO
         (void) op3;
         if (true == extraNumerical(op1, op2, temp))
          {
            retire(*frame, temp);
            retire(*frame, temp);
          }
         else
          {
            temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
            BELT_T temp1 = temp & 0xFFFFFFFFLL;
            BELT_T temp2 = (temp >> 32) & 0xFFFFFFFFLL;
            RETIRE(temp1)
            RETIRE(temp2)
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(DIVLF)
LONG_OP_INTRO
         if (true == extraNumerical(op1, op2, op3, temp))
          {
            retire(*frame, temp);
            retire(*frame, temp);
          }
         else
          {
            if (0U == (op3 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               retire(*frame, temp);
               retire(*frame, temp);
             }
            else
             {
               temp = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) / (op3 & 0xFFFFFFFFLL);
               BELT_T temp2 = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) % (op3 & 0xFFFFFFFFLL);
               if (temp > 0xFFFFFFFFLL)
                {
                  temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
                }
               RETIRE(temp)
               RETIRE(temp2)
             }
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(PICKF)
LONG_OP_INTRO
         (void) temp;
         if (conditionTrue((curOp >> 6) & 0xF, op1))
          {
            retire(*frame, op2);
          }
         else
          {
            retire(*frame, op3);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ADDF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, 0U);
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SUBF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, CARRY) ^ CARRY;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(MULF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
            if (0U != ((op1 ^ op2 ^ temp) & 0x80000000LL))
             {
               temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
             }
            else
             {
               temp &= 0xFFFFFFFFLL;
             }
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(DIVF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               retire(*frame, temp);
               retire(*frame, temp);
             }
            else
             {
               if (0U != (op1 & NEGATIVE))
                {
                  op1 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op1 &= 0xFFFFFFFFLL;
                }
               if (0U != (op2 & NEGATIVE))
                {
                  op2 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op2 &= 0xFFFFFFFFLL;
                }
               temp = (op1 / op2) & 0xFFFFFFFFLL;
               RETIRE(temp)
               BELT_T temp2 = (op1 % op2) & 0xFFFFFFFFLL;
               RETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(UDIVF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               retire(*frame, temp);
               retire(*frame, temp);
             }
            else
             {
               temp = ((op1 & 0xFFFFFFFFLL) / (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               BELT_T temp2 = ((op1 & 0xFFFFFFFFLL) % (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               RETIRE(temp)
               RETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SHRF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (0 != (op2 & 0x7FFFFFFFLL))
                {
                  if (33U <= (op2 & 0x7FFFFFFFLL))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = ((op1 & 0xFFFFFFFFLL) >> ((op2 & 0xFFFFFFFFLL) - 1)) & 0xFFFFFFFFLL;
                     BELT_T out = temp & 1;
                     temp >>= 1;
                     if (1 == out)
                      {
                        temp |= CARRY;
                      }
                   }
                }
               else
                {
                  temp = op1 & 0xFFFFFFFFLL;
                }
             }
            else
             {
               if (33U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0x1FFFFFFFFLL;
                }
             }
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ASHRF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (32U <= (op2 & 0x7FFFFFFFLL))
                {
                  if (0U == (op1 & NEGATIVE))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = 0xFFFFFFFFLL;
                   }
                }
               else
                {
                  if (0U != (op1 & NEGATIVE))
                   {
                     op1 |= 0xFFFFFFFF00000000LL;
                   }
                  else
                   {
                     op1 &= 0xFFFFFFFFLL;
                   }
                  temp = (op1 >> (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            else // Standard shift left.
             {
               if (32U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ANDF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 & op2) & 0xFFFFFFFFLL;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ORF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 | op2) & 0xFFFFFFFFLL;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(XORF)
REG_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 ^ op2) & 0xFFFFFFFFLL;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(INV)
         std::printf("Terminate initiated due to invalid operation: %d\n", static_cast<int>(frame->pc));
         machine->invalidOp = true;
         return;
       }

OP_INTRO(ADDIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, 0U);
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SUBIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, CARRY) ^ CARRY;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(MULIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
            if (0U != ((op1 ^ op2 ^ temp) & 0x80000000LL))
             {
               temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
             }
            else
             {
               temp &= 0xFFFFFFFFLL;
             }
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(DIVIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               retire(*frame, temp);
               retire(*frame, temp);
             }
            else
             {
               if (0U != (op1 & NEGATIVE))
                {
                  op1 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op1 &= 0xFFFFFFFFLL;
                }
               if (0U != (op2 & NEGATIVE))
                {
                  op2 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op2 &= 0xFFFFFFFFLL;
                }
               temp = (op1 / op2) & 0xFFFFFFFFLL;
               RETIRE(temp)
               BELT_T temp2 = (op1 % op2) & 0xFFFFFFFFLL;
               RETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(UDIVIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               retire(*frame, temp);
               retire(*frame, temp);
             }
            else
             {
               temp = ((op1 & 0xFFFFFFFFLL) / (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               BELT_T temp2 = ((op1 & 0xFFFFFFFFLL) % (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               RETIRE(temp)
               RETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SHRIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (0 != (op2 & 0x7FFFFFFFLL))
                {
                  if (33U <= (op2 & 0x7FFFFFFFLL))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = ((op1 & 0xFFFFFFFFLL) >> ((op2 & 0xFFFFFFFFLL) - 1)) & 0xFFFFFFFFLL;
                     BELT_T out = temp & 1;
                     temp >>= 1;
                     if (1 == out)
                      {
                        temp |= CARRY;
                      }
                   }
                }
               else
                {
                  temp = op1 & 0xFFFFFFFFLL;
                }
             }
            else
             {
               if (33U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0x1FFFFFFFFLL;
                }
             }
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ASHRIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (32U <= (op2 & 0x7FFFFFFFLL))
                {
                  if (0U == (op1 & NEGATIVE))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = 0xFFFFFFFFLL;
                   }
                }
               else
                {
                  if (0U != (op1 & NEGATIVE))
                   {
                     op1 |= 0xFFFFFFFF00000000LL;
                   }
                  else
                   {
                     op1 &= 0xFFFFFFFFLL;
                   }
                  temp = (op1 >> (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            else // Standard shift left.
             {
               if (32U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ANDIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 & op2) & 0xFFFFFFFFLL;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ORIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 | op2) & 0xFFFFFFFFLL;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(XORIF)
IMM_OP_INTRO
OP_BASE_CASES_FAST
            temp = (op1 ^ op2) & 0xFFFFFFFFLL;
            RETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ADDCS)
LONG_OP_INTRO
         if (false == extraNumerical(op1, op2, temp))
          {
            temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, op3);
            temp |= getZero(temp);
          }
         slowretire(*frame, temp);
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SUBBS)
LONG_OP_INTRO
         if (false == extraNumerical(op1, op2, temp))
          {
            temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, op3 ^ CARRY) ^ CARRY;
            temp |= getZero(temp);
          }
         slowretire(*frame, temp);
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(MULLS)
LONG_OP_INTRO
         (void) op3;
         if (true == extraNumerical(op1, op2, temp))
          {
            slowretire(*frame, temp);
            slowretire(*frame, temp);
          }
         else
          {
            temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
            BELT_T temp1 = temp & 0xFFFFFFFFLL;
            BELT_T temp2 = (temp >> 32) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp1)
            SLOWRETIRE(temp2)
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(DIVLS)
LONG_OP_INTRO
         if (true == extraNumerical(op1, op2, op3, temp))
          {
            slowretire(*frame, temp);
            slowretire(*frame, temp);
          }
         else
          {
            if (0U == (op3 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               slowretire(*frame, temp);
               slowretire(*frame, temp);
             }
            else
             {
               temp = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) / (op3 & 0xFFFFFFFFLL);
               BELT_T temp2 = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) % (op3 & 0xFFFFFFFFLL);
               if (temp > 0xFFFFFFFFLL)
                {
                  temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
                }
               SLOWRETIRE(temp)
               SLOWRETIRE(temp2)
             }
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(PICKS)
LONG_OP_INTRO
         (void) temp;
         if (conditionTrue((curOp >> 6) & 0xF, op1))
          {
            slowretire(*frame, op2);
          }
         else
          {
            slowretire(*frame, op3);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ADDS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, 0U);
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SUBS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, CARRY) ^ CARRY;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(MULS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
            if (0U != ((op1 ^ op2 ^ temp) & 0x80000000LL))
             {
               temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
             }
            else
             {
               temp &= 0xFFFFFFFFLL;
             }
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(DIVS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               slowretire(*frame, temp);
               slowretire(*frame, temp);
             }
            else
             {
               if (0U != (op1 & NEGATIVE))
                {
                  op1 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op1 &= 0xFFFFFFFFLL;
                }
               if (0U != (op2 & NEGATIVE))
                {
                  op2 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op2 &= 0xFFFFFFFFLL;
                }
               temp = (op1 / op2) & 0xFFFFFFFFLL;
               SLOWRETIRE(temp)
               BELT_T temp2 = (op1 % op2) & 0xFFFFFFFFLL;
               SLOWRETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(UDIVS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               slowretire(*frame, temp);
               slowretire(*frame, temp);
             }
            else
             {
               temp = ((op1 & 0xFFFFFFFFLL) / (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               BELT_T temp2 = ((op1 & 0xFFFFFFFFLL) % (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               SLOWRETIRE(temp)
               SLOWRETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SHRS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (0 != (op2 & 0x7FFFFFFFLL))
                {
                  if (33U <= (op2 & 0x7FFFFFFFLL))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = ((op1 & 0xFFFFFFFFLL) >> ((op2 & 0xFFFFFFFFLL) - 1)) & 0xFFFFFFFFLL;
                     BELT_T out = temp & 1;
                     temp >>= 1;
                     if (1 == out)
                      {
                        temp |= CARRY;
                      }
                   }
                }
               else
                {
                  temp = op1 & 0xFFFFFFFFLL;
                }
             }
            else
             {
               if (33U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0x1FFFFFFFFLL;
                }
             }
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ASHRS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (32U <= (op2 & 0x7FFFFFFFLL))
                {
                  if (0U == (op1 & NEGATIVE))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = 0xFFFFFFFFLL;
                   }
                }
               else
                {
                  if (0U != (op1 & NEGATIVE))
                   {
                     op1 |= 0xFFFFFFFF00000000LL;
                   }
                  else
                   {
                     op1 &= 0xFFFFFFFFLL;
                   }
                  temp = (op1 >> (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            else // Standard shift left.
             {
               if (32U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ANDS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 & op2) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ORS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 | op2) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(XORS)
REG_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 ^ op2) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ADDIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, 0U);
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SUBIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, CARRY) ^ CARRY;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(MULIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
            if (0U != ((op1 ^ op2 ^ temp) & 0x80000000LL))
             {
               temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
             }
            else
             {
               temp &= 0xFFFFFFFFLL;
             }
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(DIVIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               slowretire(*frame, temp);
               slowretire(*frame, temp);
             }
            else
             {
               if (0U != (op1 & NEGATIVE))
                {
                  op1 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op1 &= 0xFFFFFFFFLL;
                }
               if (0U != (op2 & NEGATIVE))
                {
                  op2 |= 0xFFFFFFFF00000000LL;
                }
               else
                {
                  op2 &= 0xFFFFFFFFLL;
                }
               temp = (op1 / op2) & 0xFFFFFFFFLL;
               SLOWRETIRE(temp)
               BELT_T temp2 = (op1 % op2) & 0xFFFFFFFFLL;
               SLOWRETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(UDIVIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & 0xFFFFFFFFLL))
             {
               temp = INVALID | frame->pc;
               slowretire(*frame, temp);
               slowretire(*frame, temp);
             }
            else
             {
               temp = ((op1 & 0xFFFFFFFFLL) / (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               BELT_T temp2 = ((op1 & 0xFFFFFFFFLL) % (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
               SLOWRETIRE(temp)
               SLOWRETIRE(temp2)
             }
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(SHRIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (0 != (op2 & 0x7FFFFFFFLL))
                {
                  if (33U <= (op2 & 0x7FFFFFFFLL))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = ((op1 & 0xFFFFFFFFLL) >> ((op2 & 0xFFFFFFFFLL) - 1)) & 0xFFFFFFFFLL;
                     BELT_T out = temp & 1;
                     temp >>= 1;
                     if (1 == out)
                      {
                        temp |= CARRY;
                      }
                   }
                }
               else
                {
                  temp = op1 & 0xFFFFFFFFLL;
                }
             }
            else
             {
               if (33U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0x1FFFFFFFFLL;
                }
             }
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ASHRIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            if (0U == (op2 & NEGATIVE)) // op2 is positive, so shift is right
             {
               if (32U <= (op2 & 0x7FFFFFFFLL))
                {
                  if (0U == (op1 & NEGATIVE))
                   {
                     temp = 0U;
                   }
                  else
                   {
                     temp = 0xFFFFFFFFLL;
                   }
                }
               else
                {
                  if (0U != (op1 & NEGATIVE))
                   {
                     op1 |= 0xFFFFFFFF00000000LL;
                   }
                  else
                   {
                     op1 &= 0xFFFFFFFFLL;
                   }
                  temp = (op1 >> (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            else // Standard shift left.
             {
               if (32U <= (-op2 & 0x7FFFFFFFLL))
                {
                  temp = 0U;
                }
               else
                {
                  temp = ((op1 & 0xFFFFFFFFLL) << (-op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                }
             }
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ANDIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 & op2) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(ORIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 | op2) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK

OP_INTRO(XORIS)
IMM_OP_INTRO
OP_BASE_CASES_SLOW
            temp = (op1 ^ op2) & 0xFFFFFFFFLL;
            SLOWRETIRE(temp)
OP_CASE_END
DISPATCH_NEXT_FROM_TICK


OP_INTRO(NOPO)
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(JMP)
TOCK_OP_INTRO
         (void) op2, (void) temp;
         if ((0U != (op1 & TRANSIENT)) && conditionTrue(cond, src))
          {
            if (0U == (op1 & INVALID))
             {
               frame->entryPoint = ((op1 & 0xFFFFFFFFLL) + frame->entryPoint) & 0xFFFFFFFFLL;
               frame->pc = frame->entryPoint - 1U;
               curOp &= 0x7FFFFFFF; // The instruction after a branch taken is ALWAYS a Tick.
             }
            else
             {
               std::printf("Terminate initiated due to branch to invalid: %d\n", static_cast<int>(frame->pc));
               machine->invalidOp = true;
               return;
             }
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(LDF)
TOCK_OP_INTRO
         (void) op2;
         if (conditionTrue(cond, src))
          {
            if (false == extraNumerical(op1, temp))
             {
               temp = getMemory(op1 & 0xFFFFFFFFLL);
               if (0U == (temp & INVALID))
                {
                  temp |= getZero(temp);
                }
               else
                {
                  temp |= frame->pc;
                }
             }
            retire(*frame, temp);
          }
         else
          {
            retire(*frame, TRANSIENT | frame->pc);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(LDHF)
TOCK_OP_INTRO
         (void) op2;
         if (conditionTrue(cond, src))
          {
            if (false == extraNumerical(op1, temp))
             {
               temp = getMemory((op1 & 0xFFFFFFFFLL) >> 1);
               if (0U == (temp & INVALID))
                {
                  temp >>= 16 * (op1 & 1);
                  if (0U != (temp & 0x8000))
                   {
                     temp |= 0xFFFF0000LL;
                   }
                  else
                   {
                     temp &= 0xFFFF;
                   }
                  temp |= getZero(temp);
                }
               else
                {
                  temp |= frame->pc;
                }
             }
            retire(*frame, temp);
          }
         else
          {
            retire(*frame, TRANSIENT | frame->pc);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(LDBF)
TOCK_OP_INTRO
         (void) op2;
         if (conditionTrue(cond, src))
          {
            if (false == extraNumerical(op1, temp))
             {
               temp = getMemory((op1 & 0xFFFFFFFFLL) >> 2);
               if (0U == (temp & INVALID))
                {
                  temp >>= 8 * (op1 & 3);
                  if (0U != (temp & 0x80))
                   {
                     temp |= 0xFFFFFF00LL;
                   }
                  else
                   {
                     temp &= 0xFF;
                   }
                  temp |= getZero(temp);
                }
               else
                {
                  temp |= frame->pc;
                }
             }
            retire(*frame, temp);
          }
         else
          {
            retire(*frame, TRANSIENT | frame->pc);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(ST)
TOCK_OP_INTRO
         (void) temp;
         if ((0U == ((op1 | op2) & TRANSIENT)) && conditionTrue(cond, src))
          {
            if (0U == ((op1 | op2) & INVALID))
             {
               if (INVALID == setMemory(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL))
                {
                  std::printf("Terminate initiated due to store to invalid: %d\n", static_cast<int>(frame->pc));
                  machine->invalidOp = true;
                  return;
                }
             }
            else
             {
               std::printf("Terminate initiated due to store of invalid: %d %d\n", static_cast<int>(frame->pc), static_cast<int>(op2));
               machine->invalidOp = true;
               return;
             }
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(STH)
TOCK_OP_INTRO
         if ((0U == ((op1 | op2) & TRANSIENT)) && conditionTrue(cond, src))
          {
            if (0U == ((op1 | op2) & INVALID))
             {
               temp = getMemory((op1 & 0xFFFFFFFFLL) >> 1);
               if (INVALID != temp)
                {
                  temp &= ~(0xFFFF << (16 * (op1 & 1)));
                  temp |= ((op2 & 0xFFFF) << (16 * (op1 & 1)));
                  setMemory((op1 & 0xFFFFFFFFLL) >> 1, temp);
                }
               else
                {
                  std::printf("Terminate initiated due to store to invalid: %d\n",static_cast<int>(frame->pc));
                  machine->invalidOp = true;
                  return;
                }
             }
            else
             {
               std::printf("Terminate initiated due to store of invalid: %d %d\n", static_cast<int>(frame->pc), static_cast<int>(op2));
               machine->invalidOp = true;
               return;
             }
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(STB)
TOCK_OP_INTRO
         if ((0U == ((op1 | op2) & TRANSIENT)) && conditionTrue(cond, src))
          {
            if (0U == ((op1 | op2) & INVALID))
             {
               temp = getMemory((op1 & 0xFFFFFFFFLL) >> 2);
               if (INVALID != temp)
                {
                  temp &= ~(0xFF << (8 * (op1 & 3)));
                  temp |= ((op2 & 0xFF) << (8 * (op1 & 3)));
                  setMemory((op1 & 0xFFFFFFFFLL) >> 2, temp);
                }
               else
                {
                  std::printf("Terminate initiated due to store to invalid: %d\n", static_cast<int>(frame->pc));
                  machine->invalidOp = true;
                  return;
                }
             }
            else
             {
               std::printf("Terminate initiated due to store of invalid: %d %d\n", static_cast<int>(frame->pc), static_cast<int>(op2));
               machine->invalidOp = true;
               return;
             }
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(CANONF)
TOCK_OP_INTRO
         (void) op1, (void) op2, (void) temp;
         if (conditionTrue(cond, src))
          {
            BELT_T belt [BELT_SIZE];
            for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
            fillBelt(*frame, num, belt);
            frame->ffront = 0U;
            frame->fsize = 0U;
            for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & belt[i])); ++i)
             {
               retire(*frame, belt[i]);
             }
          }
         else
          {
            frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(RET)
TOCK_OP_INTRO
         (void) op1, (void) op2, (void) temp;
         if (conditionTrue(cond, src))
          {
            BELT_T belt [BELT_SIZE];
            for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
            fillBelt(*frame, num, belt);

            if (1U != machine->frames.size())
             {
               Frame* prevFrame = &machine->frames[machine->frames.size() - 2U];
               for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & belt[i])); ++i)
                {
                  if (0U == (prevFrame->callOp & 0x10))
                   {
                     retire(*prevFrame, belt[i]);
                   }
                  else
                   {
                     slowretire(*prevFrame, belt[i]);
                   }
                }
               machine->frames.pop_back();
               frame = &machine->frames.back(); // Don't use prevFrame.
               curOp = frame->callOp;
             }
            else
             {
               // Returning from the bottommost frame exits.
               machine->stop = true;
               ++frame->pc;
               return;
             }
          }
         else
          {
            frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(JMPIF)
TOCK_OP_INTRO
         (void) op1, (void) op2, (void) temp;
         if (conditionTrue(cond, src))
          {
            temp = (curOp >> 15) & 0xFFFF;
            if (0U != (temp & 0x8000))
             {
               temp |= 0xFFFFFFFFFFFF0000LL;
             }
            frame->entryPoint = frame->entryPoint + temp;
            frame->pc = frame->entryPoint - 1U;
            curOp &= 0x7FFFFFFF; // The instruction after a branch taken is ALWAYS a Tick.
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(CALLI)
TOCK_OP_INTRO
         (void) op1;
         num = (curOp >> 21) & 0x1F;
         op2 = (curOp >> 26) & 0x1F;
         if (conditionTrue(cond, src))
          {
            ++frame->pc;
            temp = getMemory(frame->pc);
            if (0U == (temp & 0x10))
             {
               std::printf("Terminate initiated due to bad branch: %d\n", static_cast<int>(frame->pc));
               machine->invalidOp = true;
               return;
             }
            temp = (temp >> 5) & 0x3FFFFFF;
            if (0U != (temp & 0x2000000))
             {
               temp |= 0xFFFFFFFFFC000000LL;
             }

            frame->callOp = curOp;
            BELT_T belt [BELT_SIZE];
            for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
            fillBelt(*frame, num, belt);

            machine->frames.push_back(Frame());
            Frame* prevFrame = &machine->frames[machine->frames.size() - 2U]; // Don't use frame
            frame = &machine->frames.back();
            frame->init();
            for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & belt[i])); ++i)
             {
               retire(*frame, belt[i]);
             }
            frame->entryPoint = prevFrame->entryPoint + temp;
            frame->pc = frame->entryPoint - 1U;
            curOp &= 0x7FFFFFFF; // The instruction after a branch taken is ALWAYS a Tick.
          }
         else
          {
            for (int i = 0; i < op2; ++i)
             {
               retire(*frame, TRANSIENT | frame->pc); // ensure TRANSIENT
             }
            frame->pc += 1U + num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(CALL)
TOCK_OP_INTRO
         (void) temp;
         num = (curOp >> 21) & 0x1F;
         op2 = (curOp >> 26) & 0x1F;
         if ((0U == (op1 & TRANSIENT)) && conditionTrue(cond, src))
          {
            if (0U == (op1 & INVALID))
             {
               frame->callOp = curOp;
               BELT_T belt [BELT_SIZE];
               for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
               fillBelt(*frame, num, belt);

               machine->frames.push_back(Frame());
               Frame* prevFrame = &machine->frames[machine->frames.size() - 2U]; // Don't use frame
               frame = &machine->frames.back();
               frame->init();
               for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & belt[i])); ++i)
                {
                  retire(*frame, belt[i]);
                }
               frame->entryPoint = ((op1 & 0xFFFFFFFFLL) + prevFrame->entryPoint) & 0xFFFFFFFFLL;
               frame->pc = frame->entryPoint - 1U;
               curOp &= 0x7FFFFFFF; // The instruction after a branch taken is ALWAYS a Tick.
             }
            else
             {
               std::printf("Terminate initiated due to branch to invalid: %d\n", static_cast<int>(frame->pc));
               machine->invalidOp = true;
               return;
             }
          }
         else
          {
            if (0U == (op1 & TRANSIENT))
             {
               op1 = TRANSIENT | frame->pc;
             }
            for (int i = 0; i < op2; ++i)
             {
               retire(*frame, op1); // ensure TRANSIENT
             }
            frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(INTF)
TOCK_OP_INTRO
         (void) temp;
         num = (curOp >> 21) & 0x1F;
         op2 = (curOp >> 26) & 0x1F;
         if (conditionTrue(cond, src))
          {
            BELT_T belt [BELT_SIZE], rets [BELT_SIZE];
            for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
            for (size_t i = 0U; i < BELT_SIZE; ++i) rets[i] = EMPTY;
            fillBelt(*frame, num, belt);
            serviceInterrupt(*machine, op1, belt, rets);
            for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & rets[i])); ++i)
             {
               retire(*frame, rets[i]);
             }
          }
         else
          {
            for (int i = 0; i < op2; ++i)
             {
               retire(*frame, TRANSIENT | frame->pc);
             }
            frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(LDS)
TOCK_OP_INTRO
         (void) op2;
         if (conditionTrue(cond, src))
          {
            if (false == extraNumerical(op1, temp))
             {
               temp = getMemory(op1 & 0xFFFFFFFFLL);
               if (0U == (temp & INVALID))
                {
                  temp |= getZero(temp);
                }
               else
                {
                  temp |= frame->pc;
                }
             }
            slowretire(*frame, temp);
          }
         else
          {
            slowretire(*frame, TRANSIENT | frame->pc);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(LDHS)
TOCK_OP_INTRO
         (void) op2;
         if (conditionTrue(cond, src))
          {
            if (false == extraNumerical(op1, temp))
             {
               temp = getMemory((op1 & 0xFFFFFFFFLL) >> 1);
               if (0U == (temp & INVALID))
                {
                  temp >>= 16 * (op1 & 1);
                  if (0U != (temp & 0x8000))
                   {
                     temp |= 0xFFFF0000LL;
                   }
                  else
                   {
                     temp &= 0xFFFF;
                   }
                  temp |= getZero(temp);
                }
               else
                {
                  temp |= frame->pc;
                }
             }
            slowretire(*frame, temp);
          }
         else
          {
            slowretire(*frame, TRANSIENT | frame->pc);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(LDBS)
TOCK_OP_INTRO
         (void) op2;
         if (conditionTrue(cond, src))
          {
            if (false == extraNumerical(op1, temp))
             {
               temp = getMemory((op1 & 0xFFFFFFFFLL) >> 2);
               if (0U == (temp & INVALID))
                {
                  temp >>= 8 * (op1 & 3);
                  if (0U != (temp & 0x80))
                   {
                     temp |= 0xFFFFFF00LL;
                   }
                  else
                   {
                     temp &= 0xFF;
                   }
                  temp |= getZero(temp);
                }
               else
                {
                  temp |= frame->pc;
                }
             }
            slowretire(*frame, temp);
          }
         else
          {
            slowretire(*frame, TRANSIENT | frame->pc);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(CANONS)
TOCK_OP_INTRO
         (void) op1, (void) op2, (void) temp;
         if (conditionTrue(cond, src))
          {
            BELT_T belt [BELT_SIZE];
            for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
            fillBelt(*frame, num, belt);
            frame->sfront = 0U;
            frame->ssize = 0U;
            for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & belt[i])); ++i)
             {
               slowretire(*frame, belt[i]);
             }
          }
         else
          {
            frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(JMPIS)
TOCK_OP_INTRO
         (void) op1, (void) op2, (void) temp;
         if (conditionTrue(cond, src))
          {
            ++frame->pc;
            temp = getMemory(frame->pc);
            if (0U != (temp & 0x10))
             {
               std::printf("Terminate initiated due to bad branch: %d\n", static_cast<int>(frame->pc));
               machine->invalidOp = true;
               return;
             }
            temp = (temp >> 5) & 0x3FFFFFF;
            if (0U != (temp & 0x2000000))
             {
               temp |= 0xFFFFFFFFFC000000LL;
             }
            frame->entryPoint = frame->entryPoint + temp;
            frame->pc = frame->entryPoint - 1U;
            curOp &= 0x7FFFFFFF; // The instruction after a branch taken is ALWAYS a Tick.
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

OP_INTRO(INTS)
TOCK_OP_INTRO
         (void) temp;
         num = (curOp >> 21) & 0x1F;
         op2 = (curOp >> 26) & 0x1F;
         if (conditionTrue(cond, src))
          {
            BELT_T belt [BELT_SIZE], rets [BELT_SIZE];
            for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
            for (size_t i = 0U; i < BELT_SIZE; ++i) rets[i] = EMPTY;
            fillBelt(*frame, num, belt);
            serviceInterrupt(*machine, op1, belt, rets);
            for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & rets[i])); ++i)
             {
               slowretire(*frame, rets[i]);
             }
          }
         else
          {
            for (int i = 0; i < op2; ++i)
             {
               slowretire(*frame, TRANSIENT | frame->pc);
             }
            frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
          }
LONG_OP_CASE_END
DISPATCH_NEXT_FROM_TOCK

    }
 };

void HelloWorld (Machine& machine)
 {
   const size_t mem = 45U;
   machine.memory = new MEM_T[mem];
   machine.memsize = mem;
   machine.frames[0].init();

   // PROGRAM ENTRY POINT
   machine.memory[ 0] = 22 | (30 << 6) | ('H' << 12);
   machine.memory[ 1] = 13 | (2 << 21);
   machine.memory[ 2] = 16 | (31 << 5); // putchar
   machine.memory[ 3] = 22 | (30 << 6) | ('e' << 12);
   machine.memory[ 4] = 13 | (2 << 21);
   machine.memory[ 5] = 16 | (31 << 5); // putchar
   machine.memory[ 6] = 22 | (30 << 6) | ('l' << 12);
   machine.memory[ 7] = 13 | (2 << 21);
   machine.memory[ 8] = 16 | (31 << 5); // putchar
   machine.memory[ 9] = 22 | (30 << 6) | ('l' << 12);
   machine.memory[10] = 13 | (2 << 21);
   machine.memory[11] = 16 | (31 << 5); // putchar
   machine.memory[12] = 22 | (30 << 6) | ('o' << 12);
   machine.memory[13] = 13 | (2 << 21);
   machine.memory[14] = 16 | (31 << 5); // putchar
   machine.memory[15] = 22 | (30 << 6) | (',' << 12);
   machine.memory[16] = 13 | (2 << 21);
   machine.memory[17] = 16 | (31 << 5); // putchar
   machine.memory[18] = 22 | (30 << 6) | (' ' << 12);
   machine.memory[19] = 13 | (2 << 21);
   machine.memory[20] = 16 | (31 << 5); // putchar
   machine.memory[21] = 22 | (30 << 6) | ('W' << 12);
   machine.memory[22] = 13 | (2 << 21);
   machine.memory[23] = 16 | (31 << 5); // putchar
   machine.memory[24] = 22 | (30 << 6) | ('o' << 12);
   machine.memory[25] = 13 | (2 << 21);
   machine.memory[26] = 16 | (31 << 5); // putchar
   machine.memory[27] = 22 | (30 << 6) | ('r' << 12);
   machine.memory[28] = 13 | (2 << 21);
   machine.memory[29] = 16 | (31 << 5); // putchar
   machine.memory[30] = 22 | (30 << 6) | ('l' << 12);
   machine.memory[31] = 13 | (2 << 21);
   machine.memory[32] = 16 | (31 << 5); // putchar
   machine.memory[33] = 22 | (30 << 6) | ('d' << 12);
   machine.memory[34] = 13 | (2 << 21);
   machine.memory[35] = 16 | (31 << 5); // putchar
   machine.memory[36] = 22 | (30 << 6) | ('!' << 12);
   machine.memory[37] = 13 | (2 << 21);
   machine.memory[38] = 16 | (31 << 5); // putchar
   machine.memory[39] = 22 | (30 << 6) | ('\n' << 12);
   machine.memory[40] = 13 | (2 << 21) | (1 << 31);
   machine.memory[41] = 16 | (31 << 5); // putchar
   machine.memory[42] = 9; // return from bottommost frame : quit
   machine.memory[43] = 0; // TICK NOP : required to restart the machine from here, as it always starts on a tick.
   machine.memory[44] = 10; // Jump back to the beginning.

   machine.frames[0].pc = 0;
   machine.frames[0].entryPoint = 0;
 }

int main (int argc, char ** argv)
 {
   Machine machine;
   TickTockUnit CPU;
   CPU.machine = &machine;

   if (1 == argc)
    {
      HelloWorld(machine);
      CPU.doStuff();
    }
   else
    {
      std::FILE * file = std::fopen(argv[1], "rb");
      if (NULL == file)
       {
         std::printf("Cannot open file %s\n", argv[1]);
         return 1;
       }
      char mill [4U];
      std::fread(mill, 1U, 4U, file);
      if (0 != std::strncmp(mill, "LINB", 4U))
       {
         std::printf("Not an image.\n");
         std::fclose(file);
         return 1;
       }
      std::fread(mill, 1U, 4U, file);
      if (0 != std::strncmp(mill, endian(), 2U))
       {
         std::printf("Only images of the same endianness as the host machine are supported.\n");
         std::fclose(file);
         return 1;
       }
      if (sizeof(size_t) != (mill[2] - '0'))
       { // Add a check so that I can't execute my desktop progs on my Pi3 and vice-versa.
         std::printf("Image uses different size of a 'size' than is supported.\n");
         std::fclose(file);
         return 1;
       }
      std::fread(mill, 1U, 4U, file);
// "LINB" "LE? " "Core" "    " memory_size {data_word} num_frames { frames }
      if (0 == std::strncmp(mill, "Core", 4U))
       {
         std::fread(mill, 1U, 4U, file); // word-align the file
         // A better way to do this is to create a Strategy that is accepted by the class so that
         // knowledge of how to de/serialize a specific class hierarchy to a specific format is in one place.
         machine.read(file);
         std::fclose(file);
         CPU.doStuff();
       }
// "LINB" "LE? " "Prog" "    " memory_size entry_point num_blocks { block_entry block_size {data_word} }
      else if (0 == std::strncmp(mill, "Prog", 4U))
       {
         std::fread(mill, 1U, 4U, file); // word-align the file
         std::fread(static_cast<void*>(&machine.memsize), sizeof(size_t), 1U, file);
//         std::printf("Size: %lu\n", machine.memsize);
         machine.memory = new MEM_T [machine.memsize];
         std::fread(static_cast<void*>(&machine.frames[0].entryPoint), sizeof(size_t), 1U, file);
//         std::printf("Entry Point: %lu\n", machine.frames[0].entryPoint);
         machine.frames[0].pc = machine.frames[0].entryPoint;
         size_t numBlocks;
         std::fread(static_cast<void*>(&numBlocks), sizeof(size_t), 1U, file);
//         std::printf("Num Blocks: %lu\n", numBlocks);
         while (numBlocks > 0U)
          {
            size_t blockEntry;
            std::fread(static_cast<void*>(&blockEntry), sizeof(size_t), 1U, file);
//            std::printf("Block Entry: %lu\n", blockEntry);
            size_t blockSize;
            std::fread(static_cast<void*>(&blockSize), sizeof(size_t), 1U, file);
//            std::printf("Block Size: %lu\n", blockSize);
            std::fread(static_cast<void*>(machine.memory + blockEntry), sizeof(MEM_T), blockSize, file);
            --numBlocks;
          }
         std::fclose(file);
         CPU.doStuff();
       }
      else
       {
         std::printf("Image format not recognized.\n");
         std::fclose(file);
         return 1;
       }
    }

   {
      std::FILE * file = std::fopen("MillULX.core", "wb"); // Assume success
      std::fprintf(file, "LINB%s%d Core    ", endian(), static_cast<int>(sizeof(size_t)));
      machine.write(file); // As simple and elegant as this SEEMS, it is always a bad way to structure the code.
      std::fclose(file);
   }

   return 0;
 }
