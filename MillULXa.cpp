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
      // This implementation retains stale values on the belt that get hidden. I don't like it.
      switch (beltLocation)
       {
         case 30:
            return ZERO;
         case 31:
            return 1;
         case 62:
            return INVALID;
         case 63:
            return TRANSIENT;
       }
      size_t front = frame.ffront;
      size_t size = frame.fsize;
      size_t wrap = BELT_SIZE;
      BELT_T* belt = frame.fast;
      if (beltLocation >= BELT_SIZE)
       {
         front = frame.sfront;
         size = frame.ssize;
         wrap = BELT_SIZE;
         belt = frame.slow;
         beltLocation -= BELT_SIZE;
       }
      if (beltLocation > size)
       {
         return INVALID;
       }
      size_t phys = front + beltLocation;
      if (phys >= wrap)
       {
         phys -= wrap;
       }
      return belt[phys];
    }

   BELT_T getBeltContent(size_t beltLocation)
    {
      return getBeltContent(machine->frames.back(), beltLocation);
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
      switch (cond)
       {
         case 0: // ALWAYS
            return true;
         case 1: // CARRY
            if (0U != (flags & CARRY))
            {
               return true;
            }
            return false;
         case 2: // NO CARRY
            if (0U == (flags & CARRY))
            {
               return true;
            }
            return false;
         case 3: // Signed Overflow
            if (0U != (flags & OVERFLOW))
            {
               return true;
            }
            return false;
         case 4: // No Signed Overflow
            if (0U == (flags & OVERFLOW))
            {
               return true;
            }
            return false;
         case 5: // NEGATIVE (Less)
            if (0U != (flags & NEGATIVE))
            {
               return true;
            }
            return false;
         case 6: // NOT NEGATIVE (Greater than or equal) (Zero or Positive)
            if (0U == (flags & NEGATIVE))
            {
               return true;
            }
            return false;
         case 7: // ZERO
            if (0U != (flags & ZERO))
            {
               return true;
            }
            return false;
         case 8: // NOT ZERO
            if (0U == (flags & ZERO))
            {
               return true;
            }
            return false;
         case 9: // POSITIVE (Greater) (Not Zero and Not Negative)
            if (0U == (flags & (ZERO | NEGATIVE)))
            {
               return true;
            }
            return false;
         case 10: // NOT POSITIVE (Less Than or Equal) (Zero or Negative)
            if (0U != (flags & (ZERO | NEGATIVE)))
            {
               return true;
            }
            return false;
         case 11: // INVALID
            if (0U != (flags & INVALID))
            {
               return true;
            }
            return false;
         case 12: // NOT INVALID
            if (0U == (flags & INVALID))
            {
               return true;
            }
            return false;
         case 13: // TRANSIENT
            if (0U != (flags & TRANSIENT))
            {
               return true;
            }
            return false;
         case 14: // NOT TRANSIENT
            if (0U == (flags & TRANSIENT))
            {
               return true;
            }
            return false;
         case 15: // DEFINITE : NOT INVALID AND NOT TRANSIENT
            if (0U == (flags & (INVALID | TRANSIENT)))
            {
               return true;
            }
            return false;
       }
      std::printf("Arrived in conditionTrue with invalid condition code.\nThis is a bug.\n");
      machine->invalidOp = true;
      return false;
    }

   static void retire(Frame& frame, BELT_T value)
    {
      if (0U != frame.ffront)
       {
         --frame.ffront;
       }
      else
       {
         frame.ffront = BELT_SIZE - 1;
       }
      frame.fast[frame.ffront] = value;
      if (frame.fsize != BELT_SIZE)
       {
         ++frame.fsize;
       }
    }

   static void slowretire(Frame& frame, BELT_T value)
    {
      if (0U != frame.sfront)
       {
         --frame.sfront;
       }
      else
       {
         frame.sfront = BELT_SIZE - 1;
       }
      frame.slow[frame.sfront] = value;
      if (frame.ssize != BELT_SIZE)
       {
         ++frame.ssize;
       }
    }

   void fillBelt(int num, BELT_T* rets)
    {
      BELT_T cur = 0U;
      Frame& frame = machine->frames.back();
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
         rets[i] = getBeltContent((cur >> (5 + 6 * (i % 4))) & 0x3F);
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
      BELT_T curOp;
      Frame* frame = &machine->frames.back();
      bool tick = true;
      void (*the_retire)(Frame&, BELT_T);
#define RETIRE(x) \
   x |= getZero(x); \
   the_retire(*frame, x);

      curOp = getMemory(frame->pc);
      if (0U != (curOp & INVALID))
       {
         std::printf("Terminate initiated due to invalid program counter: %d\n", static_cast<int>(frame->pc));
         machine->invalidOp = true;
       }
      while (false == (machine->invalidOp | machine->stop))
       {
         the_retire = retire;
         if (true == tick)
          {
            if (0U != (curOp & 0x20))
             {
               the_retire = slowretire;
             }
            if ((curOp & 0xF) > 5)
             {
               BELT_T cond, src, op1, op2, temp;
               if (0U == (curOp & 0x10))
                {
                  cond = (curOp >> 6) & 0xF;
                  src = getBeltContent((curOp >> 10) & 0x3F);
                  op1 = getBeltContent((curOp >> 16) & 0x3F);
                  op2 = getBeltContent((curOp >> 22) & 0x3F);
                }
               else
                {
                  cond = 0; // Unconditional
                  src = 0;
                  op1 = getBeltContent((curOp >> 6) & 0x3F);
                  op2 = (curOp >> 12) & 0x7FFFF;
                  if (op2 & 0x40000)
                   {
                     op2 |= 0xFFF80000;
                   }
                }

               if (false == conditionTrue(cond, src))
                {
                  the_retire(*frame, TRANSIENT | frame->pc);
                  if ((9U == (curOp & 0xF)) || (10U == (curOp & 0xF)))
                   {
                     the_retire(*frame, TRANSIENT | frame->pc);
                   }
                }
               else if (true == extraNumerical(op1, op2, temp))
                {
                  the_retire(*frame, temp);
                  if ((9U == (curOp & 0xF)) || (10U == (curOp & 0xF)))
                   {
                     the_retire(*frame, temp);
                   }
                }
               else
                {
                  switch (curOp & 0xF)
                   {
                     case 6: // ADD
                        temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, 0U);
                        RETIRE(temp)
                        break;
                     case 7: // SUB
                        temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, CARRY) ^ CARRY;
                        RETIRE(temp)
                        break;
                     case 8: // MUL
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
                        break;
                     case 9: // DIV
                        if (0U == (op2 & 0xFFFFFFFFLL))
                         {
                           temp = INVALID | frame->pc;
                           the_retire(*frame, temp);
                           the_retire(*frame, temp);
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
                        break;
                     case 10: // UDIV
                        if (0U == (op2 & 0xFFFFFFFFLL))
                         {
                           temp = INVALID | frame->pc;
                           the_retire(*frame, temp);
                           the_retire(*frame, temp);
                         }
                        else
                         {
                           temp = ((op1 & 0xFFFFFFFFLL) / (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                           BELT_T temp2 = ((op1 & 0xFFFFFFFFLL) % (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                           RETIRE(temp)
                           RETIRE(temp2)
                         }
                        break;
                     case 11: // SHR
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
                        break;
                     case 12: // ASHR
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
                        break;
                     case 13: // AND
                        temp = (op1 & op2) & 0xFFFFFFFFLL;
                        RETIRE(temp)
                        break;
                     case 14: // OR
                        temp = (op1 | op2) & 0xFFFFFFFFLL;
                        RETIRE(temp)
                        break;
                     case 15: // XOR
                        temp = (op1 ^ op2) & 0xFFFFFFFFLL;
                        RETIRE(temp)
                        break;
                   }
                }
             }
            else
             {
               BELT_T op1 = getBeltContent((curOp >> 10) & 0x3F);
               BELT_T op2 = getBeltContent((curOp >> 16) & 0x3F);
               BELT_T op3 = getBeltContent((curOp >> 22) & 0x3F);
               BELT_T temp;
               switch (curOp & 0x1F)
                {
                  case 0: // NOP
                     break;
                  case 1: // ADDC
                     if (false == extraNumerical(op1, op2, temp))
                      {
                        temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, op3);
                        temp |= getZero(temp);
                      }
                     the_retire(*frame, temp);
                     break;
                  case 2: // SUBB
                     if (false == extraNumerical(op1, op2, temp))
                      {
                        temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, op3 ^ CARRY) ^ CARRY;
                        temp |= getZero(temp);
                      }
                     the_retire(*frame, temp);
                     break;
                  case 3: // MULL
                     if (true == extraNumerical(op1, op2, temp))
                      {
                        the_retire(*frame, temp);
                        the_retire(*frame, temp);
                      }
                     else
                      {
                        temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
                        BELT_T temp1 = temp & 0xFFFFFFFFLL;
                        BELT_T temp2 = (temp >> 32) & 0xFFFFFFFFLL;
                        RETIRE(temp1)
                        RETIRE(temp2)
                      }
                     break;
                  case 4: // DIVL
                     if (true == extraNumerical(op1, op2, op3, temp))
                      {
                        the_retire(*frame, temp);
                        the_retire(*frame, temp);
                      }
                     else
                      {
                        if (0U == (op3 & 0xFFFFFFFFLL))
                         {
                           temp = INVALID | frame->pc;
                           the_retire(*frame, temp);
                           the_retire(*frame, temp);
                         }
                        else
                         {
                           temp = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) / (op3 & 0xFFFFFFFFLL);
                           BELT_T temp2 = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) % (op3 & 0xFFFFFFFFLL);
                           if (temp > 0xFFFFFFFFLL)
                            {
                              temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
                            }
                           RETIRE(temp);
                           RETIRE(temp2);
                         }
                      }
                     break;
                  case 5: // PICK?
                     if (conditionTrue((curOp >> 6) & 0xF, op1))
                      {
                        the_retire(*frame, op2);
                      }
                     else
                      {
                        the_retire(*frame, op3);
                      }
                     break;
                  case 16: // RAISE INVALID OPERATION
                  case 17: // RAISE INVALID OPERATION
                  case 18: // RAISE INVALID OPERATION
                  case 19: // RAISE INVALID OPERATION
                  case 20: // RAISE INVALID OPERATION
                  case 21: // RAISE INVALID OPERATION
                     std::printf("Terminate initiated due to invalid operation: %d\n", static_cast<int>(frame->pc));
                     machine->invalidOp = true;
                     break;
                }
             }
          }
         else
          {
            if (0U != (curOp & 0x10))
             {
               the_retire = slowretire;
             }
            BELT_T cond, src, num, op1, op2, temp;
            cond = (curOp >> 5) & 0xF;
            src = getBeltContent((curOp >> 9) & 0x3F);
            num = (curOp >> 15) & 0x3F;
            op1 = getBeltContent(num);
            op2 = getBeltContent((curOp >> 21) & 0x3F);
            switch (curOp & 0xF)
             {
               case 0: // NOP
                  if (0U != (curOp & 0x10)) // Executing ARGS is invalid.
                   {
                     std::printf("Terminate initiated due to invalid operation: %d\n", static_cast<int>(frame->pc));
                     machine->invalidOp = true;
                   }
                  break;
               case 1: // JMP
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
                      }
                   }
                  break;
               case 2: // LD
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
                     the_retire(*frame, temp);
                   }
                  else
                   {
                     the_retire(*frame, TRANSIENT | frame->pc);
                   }
                  break;
               case 3: // LDH
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
                     the_retire(*frame, temp);
                   }
                  else
                   {
                     the_retire(*frame, TRANSIENT | frame->pc);
                   }
                  break;
               case 4: // LDB
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
                     the_retire(*frame, temp);
                   }
                  else
                   {
                     the_retire(*frame, TRANSIENT | frame->pc);
                   }
                  break;
               case 5: // ST
                  if ((0U == ((op1 | op2) & TRANSIENT)) && conditionTrue(cond, src))
                   {
                     if (0U == ((op1 | op2) & INVALID))
                      {
                        if (INVALID == setMemory(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL))
                         {
                           std::printf("Terminate initiated due to store to invalid: %d\n", static_cast<int>(frame->pc));
                           machine->invalidOp = true;
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to store of invalid: %d %d\n", static_cast<int>(frame->pc), static_cast<int>(op2));
                        machine->invalidOp = true;
                      }
                   }
                  break;
               case 6: // STH
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
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to store of invalid: %d %d\n", static_cast<int>(frame->pc), static_cast<int>(op2));
                        machine->invalidOp = true;
                      }
                   }
                  break;
               case 7: // STB
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
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to store of invalid: %d %d\n", static_cast<int>(frame->pc), static_cast<int>(op2));
                        machine->invalidOp = true;
                      }
                   }
                  break;
               case 8: // CANON
                  if (conditionTrue(cond, src))
                   {
                     BELT_T belt [BELT_SIZE];
                     for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
                     fillBelt(num, belt);
                     if (0U == (curOp & 0x10))
                      {
                        frame->ffront = 0U;
                        frame->fsize = 0U;
                      }
                     else
                      {
                        frame->sfront = 0U;
                        frame->ssize = 0U;
                      }
                     for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & belt[i])); ++i)
                      {
                        the_retire(*frame, belt[i]);
                      }
                   }
                  break;
               case 9: // RET
                  if (conditionTrue(cond, src))
                   {
                     BELT_T belt [BELT_SIZE];
                     for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
                     fillBelt(num, belt);

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
                      }
                   }
                  else
                   {
                     frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
                   }
                  break;
               case 10: // JMPI
                  if (conditionTrue(cond, src))
                   {
                     if (0U == (curOp & 0x10))
                      {
                        temp = (curOp >> 15) & 0xFFFF;
                        if (0U != (temp & 0x8000))
                         {
                           temp |= 0xFFFFFFFFFFFF0000LL;
                         }
                      }
                     else
                      {
                        ++frame->pc;
                        temp = getMemory(frame->pc);
                        if (0U != (temp & 0x10))
                         {
                           std::printf("Terminate initiated due to bad branch: %d\n", static_cast<int>(frame->pc));
                           machine->invalidOp = true;
                         }
                        temp = (temp >> 5) & 0x3FFFFFF;
                        if (0U != (temp & 0x2000000))
                         {
                           temp |= 0xFFFFFFFFFC000000LL;
                         }
                      }
                     frame->entryPoint = frame->entryPoint + temp;
                     frame->pc = frame->entryPoint - 1U;
                     curOp &= 0x7FFFFFFF; // The instruction after a branch taken is ALWAYS a Tick.
                   }
                  break;
               case 11: // CALLI
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
                      }
                     temp = (temp >> 5) & 0x3FFFFFF;
                     if (0U != (temp & 0x2000000))
                      {
                        temp |= 0xFFFFFFFFFC000000LL;
                      }

                     frame->callOp = curOp;
                     BELT_T belt [BELT_SIZE];
                     for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
                     fillBelt(num, belt);

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
                        the_retire(*frame, TRANSIENT | frame->pc); // ensure TRANSIENT
                      }
                     frame->pc += 1U + num / 4 + ((0 != (num % 4)) ? 1 : 0);
                   }
                  break;
               case 12: // CALL
                  num = (curOp >> 21) & 0x1F;
                  op2 = (curOp >> 26) & 0x1F;
                  if ((0U == (op1 & TRANSIENT)) && conditionTrue(cond, src))
                   {
                     if (0U == (op1 & INVALID))
                      {
                        frame->callOp = curOp;
                        BELT_T belt [BELT_SIZE];
                        for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
                        fillBelt(num, belt);

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
                        the_retire(*frame, op1); // ensure TRANSIENT
                      }
                     frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
                   }
                  break;
               case 13: // INT
                  num = (curOp >> 21) & 0x1F;
                  op2 = (curOp >> 26) & 0x1F;
                  if (conditionTrue(cond, src))
                   {
                     BELT_T belt [BELT_SIZE], rets [BELT_SIZE];
                     for (size_t i = 0U; i < BELT_SIZE; ++i) belt[i] = EMPTY;
                     for (size_t i = 0U; i < BELT_SIZE; ++i) rets[i] = EMPTY;
                     fillBelt(num, belt);
                     serviceInterrupt(*machine, op1, belt, rets);
                     for (size_t i = 0U; (i < BELT_SIZE) && (0U == (EMPTY & rets[i])); ++i)
                      {
                        the_retire(*frame, rets[i]);
                      }
                   }
                  else
                   {
                     for (int i = 0; i < op2; ++i)
                      {
                        the_retire(*frame, TRANSIENT | frame->pc);
                      }
                     frame->pc += num / 4 + ((0 != (num % 4)) ? 1 : 0);
                   }
                  break;
               case 14: // RAISE INVALID OPERATION
               case 15: // RAISE INVALID OPERATION
                  std::printf("Terminate initiated due to invalid operation: %d\n", static_cast<int>(frame->pc));
                  machine->invalidOp = true;
                  break;
             }
          }

         if (0U == (curOp & 0x80000000))
          {
            tick = !tick;
          }
         ++frame->pc;
         curOp = getMemory(frame->pc);
         if (0U != (curOp & INVALID))
          {
            std::printf("Terminate initiated due to invalid program counter: %d\n", static_cast<int>(frame->pc));
            machine->invalidOp = true;
          }
       }
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
