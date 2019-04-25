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

/*
Intended to logically exercise the following patents:

Computer processor employing explicit operations that support execution of software pipelined loops
and a compiler that utilizes such operations for scheduling software pipelined loops
Patent number: 9815669
Computer processor employing instructions with elided nop operations
Patent number: 9785441
Computer processor employing temporal addressing for storage of transient operands
Patent number: 9513921
Computer processor employing split-stream encoding
Patent number: 9513920

These are property of Mill Computing, Inc.
*/

#include <vector>
#include <pthread.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

typedef long long BELT_T;
typedef unsigned int MEM_T;

static const size_t FAST_BELT_SIZE = 32U; // If you change this, BAD! things will happen.
static const size_t SLOW_BELT_SIZE = 32U; // If you change this, BAD! things will happen.
static const size_t BIG_BELT_SIZE = 32U; // If you change this, BAD! things will happen.
static const size_t ALUNITS = 2U;
static const size_t ALU_RETIRE_SIZE = 2U;
static const size_t FLOW_UNITS = 1U;
static const size_t FLOW_RETIRE_SIZE = FAST_BELT_SIZE;
// A not-taken call with a full belt of returns requires this.

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

enum FlowBeltUse
 {
   NOT_IN_USE, // Because SOMEONE is probably using UNUSED
   CANON,
   SLOW_CANON,
   SIGNAL_CALL,
   SIGNAL_RETURN
 };

class ALURetire
 {
public:
   BELT_T fast [ALU_RETIRE_SIZE]; // Reinitialize to EMPTY after each op.
   BELT_T slow [ALU_RETIRE_SIZE];
   size_t nops; // Flow NOPs queued up by the ALUnits

   void flush()
    {
      for (size_t i = 0; i < ALU_RETIRE_SIZE; ++i) fast[i] = EMPTY;
      for (size_t i = 0; i < ALU_RETIRE_SIZE; ++i) slow[i] = EMPTY;
      nops = 0U;
    }

   void write(std::FILE * file)
    {
      std::fwrite(static_cast<void*>(fast), sizeof(BELT_T), ALU_RETIRE_SIZE, file);
      std::fwrite(static_cast<void*>(slow), sizeof(BELT_T), ALU_RETIRE_SIZE, file);
      std::fwrite(static_cast<void*>(&nops), sizeof(size_t), 1U, file);
    }

   void read(std::FILE * file)
    {
      std::fread(static_cast<void*>(fast), sizeof(BELT_T), ALU_RETIRE_SIZE, file);
      std::fread(static_cast<void*>(slow), sizeof(BELT_T), ALU_RETIRE_SIZE, file);
      std::fread(static_cast<void*>(&nops), sizeof(size_t), 1U, file);
    }
 };

class FlowRetire
 {
public:
   BELT_T fast [FLOW_RETIRE_SIZE];
   BELT_T slow;
   size_t nops; // ALU NOPs queued up by the flow instructions
   BELT_T belt [BIG_BELT_SIZE]; // Don't need it's filled size: just count not EMPTY
   FlowBeltUse use; // Is the belt used and how
   size_t next; // Size of the extra data to this flow instruction
   size_t jump; // The destination of a branch or call instruction (0 IS invalid)

   void flush()
    {
      for (size_t i = 0; i < FLOW_RETIRE_SIZE; ++i) fast[i] = EMPTY;
      slow = EMPTY;
      nops = 0U;
      for (size_t i = 0; i < BIG_BELT_SIZE; ++i) belt[i] = EMPTY;
      use = NOT_IN_USE;
      next = 0U;
      jump = 0U;
    }

   void write(std::FILE * file)
    {
      std::fwrite(static_cast<void*>(fast), sizeof(BELT_T), FLOW_RETIRE_SIZE, file);
      std::fwrite(static_cast<void*>(&slow), sizeof(BELT_T), 1U, file);
      std::fwrite(static_cast<void*>(&nops), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(belt), sizeof(BELT_T), BIG_BELT_SIZE, file);
      std::fwrite(static_cast<void*>(&use), sizeof(FlowBeltUse), 1U, file);
      std::fwrite(static_cast<void*>(&next), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(&jump), sizeof(size_t), 1U, file);
    }

   void read(std::FILE * file)
    {
      std::fread(static_cast<void*>(fast), sizeof(BELT_T), FLOW_RETIRE_SIZE, file);
      std::fread(static_cast<void*>(&slow), sizeof(BELT_T), 1U, file);
      std::fread(static_cast<void*>(&nops), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(belt), sizeof(BELT_T), BIG_BELT_SIZE, file);
      std::fread(static_cast<void*>(&use), sizeof(FlowBeltUse), 1U, file);
      std::fread(static_cast<void*>(&next), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&jump), sizeof(size_t), 1U, file);
    }
 };

class Frame
 {
public:
   // ALU/FLOW read-only
   BELT_T fast [FAST_BELT_SIZE];
   BELT_T slow [SLOW_BELT_SIZE];
   size_t ffront, fsize; // The front and size of the fast belt
   size_t sfront, ssize; // The front and size of the slow belt
   size_t alunop;
   size_t flownop;
   size_t alupc;
   size_t flowpc;
   size_t entryPoint; // What alupc and flowpc were set to at the creation of this Frame.
   size_t nextpc; // The winning branch instruction.
   size_t index; // For a call, the flow unit that initiated it.

   // ALU/FLOW write-only
   ALURetire alu_retire [ALUNITS];
   FlowRetire flow_retire [FLOW_UNITS];

   void init (void)
    {
      for (size_t i = 0U; i < FAST_BELT_SIZE; ++i) fast[i] = INVALID;
      for (size_t i = 0U; i < SLOW_BELT_SIZE; ++i) slow[i] = INVALID;
      ffront = 0U;
      fsize = 0U;
      sfront = 0U;
      ssize = 0U;
      alunop = 0U;
      flownop = 0U;
      alupc = 0U;
      flowpc = 0U;
      entryPoint = 0U;
      nextpc = 0U;
    }

   void write(std::FILE * file)
    {
      std::fwrite(static_cast<void*>(fast), sizeof(BELT_T), FAST_BELT_SIZE, file);
      std::fwrite(static_cast<void*>(slow), sizeof(BELT_T), SLOW_BELT_SIZE, file);
      std::fwrite(static_cast<void*>(&ffront), sizeof(size_t), 1U, file); //std::printf("ffront %lu\n", ffront);
      std::fwrite(static_cast<void*>(&fsize), sizeof(size_t), 1U, file); //std::printf("fsize %lu\n", fsize);
      std::fwrite(static_cast<void*>(&sfront), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(&ssize), sizeof(size_t), 1U, file);
      std::fwrite(static_cast<void*>(&alunop), sizeof(size_t), 1U, file); //std::printf("alunop %lu\n", alunop);
      std::fwrite(static_cast<void*>(&flownop), sizeof(size_t), 1U, file); //std::printf("flownop %lu\n", flownop);
      std::fwrite(static_cast<void*>(&alupc), sizeof(size_t), 1U, file); //std::printf("alupc %lu\n", alupc);
      std::fwrite(static_cast<void*>(&flowpc), sizeof(size_t), 1U, file); //std::printf("flowpc %lu\n", flowpc);
      std::fwrite(static_cast<void*>(&entryPoint), sizeof(size_t), 1U, file); //std::printf("entryPoint %lu\n", entryPoint);
      std::fwrite(static_cast<void*>(&nextpc), sizeof(size_t), 1U, file); //std::printf("nextpc %lu\n", nextpc);
      std::fwrite(static_cast<void*>(&index), sizeof(size_t), 1U, file);
      for (size_t i = 0U; i < ALUNITS; ++i) alu_retire[i].write(file);
      for (size_t i = 0U; i < FLOW_UNITS; ++i) flow_retire[i].write(file);
    }

   void read(std::FILE * file)
    {
      std::fread(static_cast<void*>(fast), sizeof(BELT_T), FAST_BELT_SIZE, file);
      std::fread(static_cast<void*>(slow), sizeof(BELT_T), SLOW_BELT_SIZE, file);
      std::fread(static_cast<void*>(&ffront), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&fsize), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&sfront), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&ssize), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&alunop), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&flownop), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&alupc), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&flowpc), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&entryPoint), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&nextpc), sizeof(size_t), 1U, file);
      std::fread(static_cast<void*>(&index), sizeof(size_t), 1U, file);
      for (size_t i = 0U; i < ALUNITS; ++i) alu_retire[i].read(file);
      for (size_t i = 0U; i < FLOW_UNITS; ++i) flow_retire[i].read(file);
    }
 };

class Machine
 {
public:
   std::vector<Frame> frames;
   MEM_T * memory;
   size_t memsize;
   bool terminate;
   bool invalidOp;
   bool stop;

   Machine() : memory(NULL), memsize(0U), terminate(false), invalidOp(false), stop(false)
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
   pthread_barrier_t* synchronizer;
   size_t slot;
   pthread_t thread;

   virtual void doStuff() = 0;

   BELT_T getMemory(size_t location)
    {
      if (location >= machine->memsize)
       {
         return INVALID;
       }
      return machine->memory[location];
    }

   BELT_T setMemory(size_t location, MEM_T value)
    {
      if (location >= machine->memsize)
       {
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

   static bool extraNumerical(BELT_T op, BELT_T& res)
    {
      if (0L != (op & (TRANSIENT | INVALID)))
       {
         res = op;
         return true;
       }
      return false;
    }

   static bool extraNumerical(BELT_T op1, BELT_T op2, BELT_T& res)
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

   static bool extraNumerical(BELT_T op1, BELT_T op2, BELT_T op3, BELT_T& res)
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
      if (0U == (input & 0xFFFFFFFF))
       {
         return ZERO;
       }
      return 0;
    }

   static BELT_T getAdd(BELT_T op1, BELT_T op2, BELT_T op3)
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
      static const BELT_T conds [] = { 0U, CARRY, OVERFLOW, NEGATIVE, ZERO, ZERO | NEGATIVE, INVALID, TRANSIENT };
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
 };

class ALUnit : public FunctionalUnit
 {
public:
   virtual void doStuff()
    {
      for (;;)
       {
         //Wait for the start of an instruction cycle
         pthread_barrier_wait(synchronizer);
         // Do we need to die?
         if (true == machine->terminate)
          {
//            std::printf("Terminating ALU slot: %lu\n", slot);
            break;
          }
         // Interpret and execute the next operation.
         Frame& frame = machine->frames.back();
         ALURetire& retire = frame.alu_retire[slot];
         retire.flush(); // Make retire station is clean.
         if (0U == frame.alunop)
          {
//            std::printf("Executing ALU slot: %lu %lu\n", slot, frame.alupc);
            BELT_T curOp = getMemory(frame.alupc + slot);
            if (0U != (curOp & INVALID))
             {
               std::printf("Terminate initiated due to invalid operation in ALU slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.alupc + slot));
               machine->invalidOp = true;
             }
            if ((curOp & 0xF) > 5)
             {
               BELT_T cond, src, op1, op2, temp;
               BELT_T* dest;
               if (0 == (curOp & 0x10))
                {
                  cond = (curOp >> 6) & 0xF;
                  src = getBeltContent(frame, (curOp >> 10) & 0x3F);
                  op1 = getBeltContent(frame, (curOp >> 16) & 0x3F);
                  op2 = getBeltContent(frame, (curOp >> 22) & 0x3F);
                  retire.nops = (curOp >> 28) & 0x7;
                }
               else
                {
                  cond = 0; // Unconditional
                  src = 0;
                  op1 = getBeltContent(frame, (curOp >> 6) & 0x3F);
                  op2 = (curOp >> 12) & 0x1FFFF;
                  if (op2 & 0x10000)
                   {
                     op2 |= 0xFFFE0000;
                   }
                  retire.nops = (curOp >> 29) & 0x7;
                }
               dest = retire.fast;
               if (curOp & 0x20)
                {
                  dest = retire.slow;
                }
               if (false == conditionTrue(cond, src))
                {
                  dest[0] = TRANSIENT | frame.alupc;
                  if ((9 == (curOp & 0xF)) || (10 == (curOp & 0xF)))
                   {
                     dest[1] = TRANSIENT | frame.alupc;
                   }
                }
               else if (true == extraNumerical(op1, op2, temp))
                {
                  dest[0] = temp;
                  if ((9 == (curOp & 0xF)) || (10 == (curOp & 0xF)))
                   {
                     dest[1] = temp;
                   }
                }
               else
                {
                  switch (curOp & 0xF)
                   {
                     case 6: // ADD
                        temp = getAdd(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL, 0U);
                        break;
                     case 7: // SUB
                        temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, CARRY) ^ CARRY;
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
                        break;
                     case 9: // DIV
                        if (0U == (op2 & 0xFFFFFFFFLL))
                         {
                           temp = INVALID | frame.alupc;
                           dest[1] = temp;
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
                           BELT_T temp2 = (op1 % op2) & 0xFFFFFFFFLL;
                           temp2 |= getZero(temp2);
                           dest[1] = temp2;
                         }
                        break;
                     case 10: // UDIV
                        if (0U == (op2 & 0xFFFFFFFFLL))
                         {
                           temp = INVALID | frame.alupc;
                           dest[1] = temp;
                         }
                        else
                         {
                           temp = ((op1 & 0xFFFFFFFFLL) / (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                           BELT_T temp2 = ((op1 & 0xFFFFFFFFLL) % (op2 & 0xFFFFFFFFLL)) & 0xFFFFFFFFLL;
                           temp2 |= getZero(temp2);
                           dest[1] = temp2;
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
                        break;
                     case 13: // AND
                        temp = (op1 & op2) & 0xFFFFFFFFLL;
                        break;
                     case 14: // OR
                        temp = (op1 | op2) & 0xFFFFFFFFLL;
                        break;
                     case 15: // XOR
                        temp = (op1 ^ op2) & 0xFFFFFFFFLL;
                        break;
                   }
                  temp |= getZero(temp);
                  dest[0] = temp;
                }
             }
            else
             {
               BELT_T* dest = retire.fast;
               if (curOp & 0x20)
                {
                  dest = retire.slow;
                }
               BELT_T op1 = getBeltContent(frame, (curOp >> 10) & 0x3F);
               BELT_T op2 = getBeltContent(frame, (curOp >> 16) & 0x3F);
               BELT_T op3 = getBeltContent(frame, (curOp >> 22) & 0x3F);
               BELT_T temp;
               retire.nops = (curOp >> 28) & 0x7;
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
                     dest[0] = temp;
                     break;
                  case 2: // SUBB
                     if (false == extraNumerical(op1, op2, temp))
                      {
                        temp = getAdd(op1 & 0xFFFFFFFFLL, (op2 & 0xFFFFFFFFLL) ^ 0xFFFFFFFFLL, op3 ^ CARRY) ^ CARRY;
                        temp |= getZero(temp);
                      }
                     dest[0] = temp;
                     break;
                  case 3: // MULL
                     if (true == extraNumerical(op1, op2, temp))
                      {
                        dest[0] = temp;
                        dest[1] = temp;
                      }
                     else
                      {
                        temp = (op1 & 0xFFFFFFFFLL) * (op2 & 0xFFFFFFFFLL);
                        BELT_T temp1 = temp & 0xFFFFFFFFLL;
                        BELT_T temp2 = (temp >> 32) & 0xFFFFFFFFLL;
                        temp1 |= getZero(temp1);
                        temp2 |= getZero(temp2);
                        dest[0] = temp1;
                        dest[1] = temp2;
                      }
                     break;
                  case 4: // DIVL
                     if (true == extraNumerical(op1, op2, op3, temp))
                      {
                        dest[0] = temp;
                        dest[1] = temp;
                      }
                     else
                      {
                        if (0U == (op3 & 0xFFFFFFFFLL))
                         {
                           temp = INVALID | frame.alupc;
                           dest[0] = temp;
                           dest[1] = temp;
                         }
                        else
                         {
                           temp = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) / (op3 & 0xFFFFFFFFLL);
                           BELT_T temp2 = static_cast<unsigned long long>((op1 << 32) | (op2 & 0xFFFFFFFFLL)) % (op3 & 0xFFFFFFFFLL);
                           if (temp > 0xFFFFFFFFLL)
                            {
                              temp = (temp & 0xFFFFFFFFLL) | OVERFLOW;
                            }
                           temp |= getZero(temp);
                           temp2 |= getZero(temp2);
                           dest[0] = temp;
                           dest[1] = temp2;
                         }
                      }
                     break;
                  case 5: // PICK?
                     if (conditionTrue((curOp >> 6) & 0xF, op1))
                      {
                        dest[0] = op2;
                      }
                     else
                      {
                        dest[0] = op3;
                      }
                     break;
                  case 16: // RAISE INVALID OPERATION
                  case 17: // RAISE INVALID OPERATION
                  case 18: // RAISE INVALID OPERATION
                  case 19: // RAISE INVALID OPERATION
                  case 20: // RAISE INVALID OPERATION
                  case 21: // RAISE INVALID OPERATION
                     std::printf("Terminate initiated due to invalid operation in ALU slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.alupc + slot));
                     machine->invalidOp = true;
                     break;
                }
             }
          }

         // Signal that we have ended this cycle.
         pthread_barrier_wait(synchronizer);
       }
    }
 };

class FlowUnit : public FunctionalUnit
 {
public:

   void fillBelt(Frame& frame, int num)
    {
      int memOff = -1; // Start at the current instruction
      BELT_T cur = 0U; // If cur is used uninitialized, that is a bug in the compiler.
      FlowRetire& retire = frame.flow_retire[slot];
      for (int i = 0; i < num; ++i)
       {
         if (0 == (i % 4)) // memOff is intentionally initialized for this to occur at zero
          {
            --memOff;
            cur = getMemory(frame.flowpc - slot + memOff);
            if (0U != (cur & INVALID))
             {
               machine->invalidOp = true;
             }
            if (0x10 != (cur & 0x1F)) // Make sure this is an ARGS NOP
             {
               machine->invalidOp = true;
             }
          }
         retire.belt[i] = getBeltContent(frame, (cur >> (5 + 6 * (i % 4))) & 0x3F);
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

   virtual void doStuff()
    {
      for (;;)
       {
         // Wait for the start of the instruction cycle
         pthread_barrier_wait(synchronizer);
         // Do we need to die?
         if (true == machine->terminate)
          {
//            std::printf("Terminating Flow slot: %lu\n", slot);
            break;
          }
         // Interpret and execute the next operation.
         Frame& frame = machine->frames.back();
         FlowRetire& retire = frame.flow_retire[slot];
         retire.flush(); // Make retire station is clean.
         if (0U == frame.flownop)
          {
//            std::printf("Executing Flow slot: %lu %lu\n", slot, frame.flowpc);
            BELT_T curOp = getMemory(frame.flowpc - slot - 1U);
            if (0U != (curOp & INVALID))
             {
               std::printf("Terminate initiated due to invalid operation in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
               machine->invalidOp = true;
             }
            BELT_T cond, src, num, op1, op2, temp;
            BELT_T* dest;
            cond = (curOp >> 5) & 0xF;
            src = getBeltContent(frame, (curOp >> 9) & 0x3F);
            num = (curOp >> 15) & 0x3F;
            op1 = getBeltContent(frame, num);
            op2 = getBeltContent(frame, (curOp >> 21) & 0x3F);
            dest = retire.fast;
            if (curOp & 0x10)
             {
               dest = &retire.slow;
             }
            switch (curOp & 0xF)
             {
               case 0: // NOP
                  retire.nops = (curOp >> 29) & 0x7;
                  break;
               case 1: // JMP
                  if ((0U != (op1 & TRANSIENT)) && conditionTrue(cond, src))
                   {
                     if (0U == (op1 & INVALID))
                      {
                        retire.jump = ((op1 & 0xFFFFFFFFLL) + frame.entryPoint) & 0xFFFFFFFFLL;
                        if (0U == retire.jump)
                         {
                           std::printf("Terminate initiated due to branch to zero in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                           machine->invalidOp = true;
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to branch to invalid in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                        machine->invalidOp = true;
                      }
                   }
                  retire.nops = (curOp >> 27) & 0x7;
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
                           temp |= frame.flowpc;
                         }
                      }
                     dest[0] = temp;
                   }
                  else
                   {
                     dest[0] = TRANSIENT | frame.flowpc;
                   }
                  retire.nops = (curOp >> 27) & 0x7;
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
                           temp |= frame.flowpc;
                         }
                      }
                     dest[0] = temp;
                   }
                  else
                   {
                     dest[0] = TRANSIENT | frame.flowpc;
                   }
                  retire.nops = (curOp >> 27) & 0x7;
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
                           temp |= frame.flowpc;
                         }
                      }
                     dest[0] = temp;
                   }
                  else
                   {
                     dest[0] = TRANSIENT | frame.flowpc;
                   }
                  retire.nops = (curOp >> 27) & 0x7;
                  break;
               case 5: // ST
                  if ((0U == ((op1 | op2) & TRANSIENT)) && conditionTrue(cond, src))
                   {
                     if (0U == ((op1 | op2) & INVALID))
                      {
                        if (INVALID == setMemory(op1 & 0xFFFFFFFFLL, op2 & 0xFFFFFFFFLL))
                         {
                           std::printf("Terminate initiated due to store to invalid in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                           machine->invalidOp = true;
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to store of invalid in Flow slot: %d %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U), static_cast<int>(op2));
                        machine->invalidOp = true;
                      }
                   }
                  retire.nops = (curOp >> 27) & 0x7;
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
                           std::printf("Terminate initiated due to store to invalid in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                           machine->invalidOp = true;
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to store of invalid in Flow slot: %d %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U), static_cast<int>(op2));
                        machine->invalidOp = true;
                      }
                   }
                  retire.nops = (curOp >> 27) & 0x7;
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
                           std::printf("Terminate initiated due to store to invalid in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                           machine->invalidOp = true;
                         }
                      }
                     else
                      {
                        std::printf("Terminate initiated due to store of invalid in Flow slot: %d %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U), static_cast<int>(op2));
                        machine->invalidOp = true;
                      }
                   }
                  retire.nops = (curOp >> 27) & 0x7;
                  break;
               case 8: // CANON
                  if (conditionTrue(cond, src))
                   {
                     retire.use = (0 == (curOp & 0x10)) ? CANON : SLOW_CANON;
                     fillBelt(frame, num);
                   }
                  retire.next = num / 4 + ((0 != (num % 4)) ? 1 : 0);
                  retire.nops = (curOp >> 27) & 0x7;
                  break;
               case 9: // RET
                  if (conditionTrue(cond, src))
                   {
                     retire.use = SIGNAL_RETURN;
                     fillBelt(frame, num);
                   }
                  retire.next = num / 4 + ((0 != (num % 4)) ? 1 : 0);
                  retire.nops = (curOp >> 27) & 0x7;
                  break;
               case 10: // JMPI
                  cond = (curOp >> 4) & 0xF;
                  src = getBeltContent(frame, (curOp >> 8) & 0x3F);
                  if (conditionTrue(cond, src))
                   {
                     temp = (curOp >> 14) & 0x7FFF;
                     if (0U != (temp & 0x4000))
                      {
                        temp |= 0xFFFFFFFFFFFF8000LL;
                      }
                     retire.jump = frame.entryPoint + temp;
                     if (0U == retire.jump)
                      {
                        std::printf("Terminate initiated due to branch to zero in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                        machine->invalidOp = true;
                      }
                   }
                  retire.nops = (curOp >> 29) & 0x7;
                  break;
               case 11: // CALLI
                  num = (curOp >> 4) & 0x1F;
                  retire.next = num / 4 + ((0 != (num % 4)) ? 1 : 0);
                  temp = (curOp >> 9) & 0xFFFFF;
                  if (0U != (temp & 0x80000))
                   {
                     temp |= 0xFFFFFFFFFFF00000LL;
                   }
                  retire.jump = frame.entryPoint + temp;
                  if (0U == retire.jump)
                   {
                     std::printf("Terminate initiated due to branch to zero in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                     machine->invalidOp = true;
                   }
                  retire.use = SIGNAL_CALL;
                  fillBelt(frame, num);
                  retire.nops = (curOp >> 29) & 0x7;
                  break;
               case 12: // CALL
                  cond = (curOp >> 4) & 0xF;
                  src = getBeltContent(frame, (curOp >> 8) & 0x3F);
                  op1 = getBeltContent(frame, (curOp >> 14) & 0x3F);
                  num = (curOp >> 20) & 0x1F;
                  op2 = (curOp >> 25) & 0x1F;
                  retire.nops = (curOp >> 30) & 0x3;
                  retire.next = num / 4 + ((0 != (num % 4)) ? 1 : 0);
                  if ((0U == (op1 & TRANSIENT)) && conditionTrue(cond, src))
                   {
                     if (0U == (op1 & INVALID))
                      {
                        retire.jump = ((op1 & 0xFFFFFFFFLL) + frame.entryPoint) & 0xFFFFFFFFLL;
                        if (0U == retire.jump)
                         {
                           std::printf("Terminate initiated due to branch to zero in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                           machine->invalidOp = true;
                         }
                        retire.use = SIGNAL_CALL;
                        fillBelt(frame, num);
                      }
                     else
                      {
                        std::printf("Terminate initiated due to branch to invalid in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                        machine->invalidOp = true;
                      }
                   }
                  else
                   {
                     if (0U == (op1 & TRANSIENT))
                      {
                        op1 = TRANSIENT | frame.flowpc;
                      }
                     for (int i = 0; i < op2; ++i)
                      {
                        retire.fast[i] = op1; // ensure TRANSIENT
                      }
                   }
                  break;
               case 13: // INT
                  cond = (curOp >> 4) & 0xF;
                  src = getBeltContent(frame, (curOp >> 8) & 0x3F);
                  op1 = (curOp >> 14) & 0x3F;
                  num = (curOp >> 20) & 0x1F;
                  op2 = (curOp >> 25) & 0x1F;
                  retire.nops = (curOp >> 30) & 0x3;
                  retire.next = num / 4 + ((0 != (num % 4)) ? 1 : 0);
                  if (conditionTrue(cond, src))
                   {
                     fillBelt(frame, num);
                     serviceInterrupt(*machine, op1, retire.belt, retire.fast);
                   }
                  else
                   {
                     for (int i = 0; i < op2; ++i)
                      {
                        retire.fast[i] = TRANSIENT | frame.flowpc;
                      }
                   }
                  break;
               case 14: // RAISE INVALID OPERATION
               case 15: // RAISE INVALID OPERATION
                  std::printf("Terminate initiated due to invalid operation in Flow slot: %d %d\n", static_cast<int>(slot), static_cast<int>(frame.flowpc - slot - 1U));
                  machine->invalidOp = true;
                  break;
             }
          }

         // Signal that we have ended this cycle.
         pthread_barrier_wait(synchronizer);
       }
    }
 };

class MillCore
 {
public:
   Machine* machine;

   static void * runMe(void * slot)
    {
      MillCore* unit = reinterpret_cast<MillCore*>(slot);
      unit->doStuff();
      return NULL;
    }

   static void * runOne(void * slot)
    {
      FunctionalUnit* unit = reinterpret_cast<FunctionalUnit*>(slot);
      unit->doStuff();
      return NULL;
    }

   void retire(Frame& frame, BELT_T value)
    {
      frame.ffront = (frame.ffront - 1) & 0x1F;
      frame.fast[frame.ffront] = value;
      frame.fsize = frame.fsize < FAST_BELT_SIZE ? frame.fsize + 1 : FAST_BELT_SIZE;

      frame.fast[(frame.ffront + 30) & 0x1F] = ZERO;
      frame.fast[(frame.ffront + 31) & 0x1F] = 1;
    }

   void slowretire(Frame& frame, BELT_T value)
    {
      frame.sfront = (frame.sfront - 1) & 0x1F;
      frame.slow[frame.sfront] = value;
      frame.ssize = frame.ssize < SLOW_BELT_SIZE ? frame.ssize + 1 : SLOW_BELT_SIZE;

      frame.slow[(frame.sfront + 30) & 0x1F] = INVALID;
      frame.slow[(frame.sfront + 31) & 0x1F] = TRANSIENT;
    }

   void doStuff()
    {
      pthread_barrier_t synchronizer;
      pthread_barrier_init(&synchronizer, NULL, ALUNITS + FLOW_UNITS + 1U);
      ALUnit aunits [ALUNITS];
      FlowUnit funits [FLOW_UNITS];

      for (size_t i = 0U; i < ALUNITS; ++i)
       {
         aunits[i].machine = machine;
         aunits[i].synchronizer = &synchronizer;
         aunits[i].slot = i;
         pthread_create(&aunits[i].thread, NULL, runOne, reinterpret_cast<void*>(&aunits[i]));
       }

      for (size_t i = 0U; i < FLOW_UNITS; ++i)
       {
         funits[i].machine = machine;
         funits[i].synchronizer = &synchronizer;
         funits[i].slot = i;
         pthread_create(&funits[i].thread, NULL, runOne, reinterpret_cast<void*>(&funits[i]));
       }

      for (;;)
       {
         // Signal the start of the instruction cycle
         pthread_barrier_wait(&synchronizer);
         // Wait for the end of this cycle.
         pthread_barrier_wait(&synchronizer);

         // Synthesize unit data.
//         std::printf("Instruction finished\n");
         Frame* frame = &machine->frames.back();
//  Dec NOP counters OR move PCs
//    IF we performed an instruction, move the PC while we have the data to do so.
         if (0U != frame->alunop)
          {
            --frame->alunop;
          }
         else
          {
            frame->alupc += ALUNITS;
          }
         if (0U != frame->flownop)
          {
            --frame->flownop;
          }
         else
          {
            //  Accumulate flow delta
            size_t addPC = 0U;
            for (size_t i = 0U; i < FLOW_UNITS; ++i)
             {
               addPC += frame->flow_retire[i].next;
             }
            frame->flowpc -= (FLOW_UNITS + addPC);
          }
//  Accumulate NOPs and add to counters
         size_t addNops = 0U;
         for (size_t i = 0U; i < FLOW_UNITS; ++i)
          {
            addNops += frame->flow_retire[i].nops;
          }
         frame->alunop += addNops;
         addNops = 0U;
         for (size_t i = 0U; i < ALUNITS; ++i)
          {
            addNops += frame->alu_retire[i].nops;
          }
         frame->flownop += addNops;
//  Retire ALUs
         for (size_t i = 0U; i < ALUNITS; ++i)
          {
            for (size_t j = 0U; (j < ALU_RETIRE_SIZE) && (0U == (EMPTY & frame->alu_retire[i].fast[j])); ++j)
             {
               retire(*frame, frame->alu_retire[i].fast[j]);
             }
            for (size_t j = 0U; (j < ALU_RETIRE_SIZE) && (0U == (EMPTY & frame->alu_retire[i].slow[j])); ++j)
             {
               slowretire(*frame, frame->alu_retire[i].slow[j]);
             }
          }
//  Retire Flows
         for (size_t i = 0U; i < FLOW_UNITS; ++i)
          {
            for (size_t j = 0U; (j < FLOW_RETIRE_SIZE) && (0U == (EMPTY & frame->flow_retire[i].fast[j])); ++j)
             {
               retire(*frame, frame->flow_retire[i].fast[j]);
             }
            if (0U == (EMPTY & frame->flow_retire[i].slow))
             {
               slowretire(*frame, frame->flow_retire[i].slow);
             }
            // The first non-call branch wins and stops flow unit processing
            if ((0U == frame->nextpc) && (0U != frame->flow_retire[i].jump) && (SIGNAL_CALL != frame->flow_retire[i].use))
             {
               frame->nextpc = frame->flow_retire[i].jump;
               break;
             }
            switch (frame->flow_retire[i].use)
             {
               case NOT_IN_USE:
                  break;
               case CANON:
                  frame->ffront = 0U;
                  frame->fsize = 0U;
                  for (size_t j = 0U; (j < FAST_BELT_SIZE) && (0U == (EMPTY & frame->flow_retire[i].belt[j])); ++j)
                   {
                     retire(*frame, frame->flow_retire[i].belt[j]);
                   }
                  break;
               case SLOW_CANON:
                  frame->sfront = 0U;
                  frame->ssize = 0U;
                  for (size_t j = 0U; (j < SLOW_BELT_SIZE) && (0U == (EMPTY & frame->flow_retire[i].belt[j])); ++j)
                   {
                     slowretire(*frame, frame->flow_retire[i].belt[j]);
                   }
                  break;
               case SIGNAL_CALL:
                {
                  frame->index = i; // When we return, we will return to this index.
                  // The retire phase has been carefully constructed so that (hopefully) we can treat a call as an instruction
                  // that retires a variable number of values.
                  // And that we can create and destroy frames in this loop without invalidating the machine state.
                  machine->frames.push_back(Frame());
                  Frame* prevFrame = &machine->frames[machine->frames.size() - 2U]; // Don't use frame
                  frame = &machine->frames.back();
                  frame->init();
                  for (size_t j = 0U; (j < FAST_BELT_SIZE) && (0U == (EMPTY & prevFrame->flow_retire[i].belt[j])); ++j)
                   {
                     retire(*frame, prevFrame->flow_retire[i].belt[j]);
                   }
                  frame->nextpc = prevFrame->flow_retire[i].jump;
                  i = FLOW_UNITS; // Don't process any of the new frame's flow retire stations.
                }
                  break;
               case SIGNAL_RETURN:
                  if (1U != machine->frames.size())
                   {
                     Frame* prevFrame = &machine->frames[machine->frames.size() - 2U];
                     for (size_t j = 0U; (j < FAST_BELT_SIZE) && (0U == (EMPTY & frame->flow_retire[i].belt[j])); ++j)
                      {
                        retire(*prevFrame, frame->flow_retire[i].belt[j]);
                      }
                     machine->frames.pop_back();
                     frame = &machine->frames.back(); // Don't use prevFrame.
                     i = frame->index;
                   }
                  else
                   {
                     // Returning from the bottommost frame exits.
                     machine->stop = true;
                   }
                  break;
             }
          }
         if (0U != frame->nextpc)
          {
            frame->alupc = frame->nextpc;
            frame->flowpc = frame->nextpc;
            frame->entryPoint = frame->nextpc;
            frame->nextpc = 0U;
          }
/*
// You know you're in deep when you have to uncomment this block.
std::printf("%x %x %x %x %x %x %x %x %x %x %x %x\n",
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 0)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 1)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 2)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 3)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 4)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 5)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 6)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 7)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 8)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 9)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 10)),
   static_cast<char>(FunctionalUnit::getBeltContent(*frame, 11)));
*/
         if ((true == machine->invalidOp) || (true == machine->stop))
          {
            if (true == machine->invalidOp)
             {
               std::printf("Terminating Core due to invalid operation\n");
             }
            machine->terminate = true;
            pthread_barrier_wait(&synchronizer);
            break;
          }
       }

      {
         std::FILE * file = std::fopen("MillULX.core", "wb"); // Assume success
         std::fprintf(file, "Mill%s%d Core    ", endian(), static_cast<int>(sizeof(size_t)));
         machine->write(file); // As simple and elegant as this SEEMS, it is always a bad way to structure the code.
         std::fclose(file);
      }

      for (size_t i = 0U; i < ALUNITS; ++i)
       {
         pthread_join(aunits[i].thread, NULL);
       }
      for (size_t i = 0U; i < FLOW_UNITS; ++i)
       {
         pthread_join(funits[i].thread, NULL);
       }
    }
 };

void HelloWorld (Machine& machine)
 {
   const size_t mem = 45U;
   machine.memory = new MEM_T[mem];
   machine.memsize = mem;
   machine.frames[0].init();

   machine.memory[0] = 10; // Jump back to the begining.
   machine.memory[1] = 9; // return from bottommost frame : quit
   machine.memory[2] = 16 | (31 << 5); // putchar
   machine.memory[3] = 13 | (2 << 20);
   machine.memory[4] = 16 | (31 << 5) | (1 << 11); // putchar
   machine.memory[5] = 13 | (2 << 20) | (3 << 30);
   machine.memory[6] = 16 | (31 << 5) | (2 << 11); // putchar
   machine.memory[7] = 13 | (2 << 20);
   machine.memory[8] = 16 | (31 << 5) | (3 << 11); // putchar
   machine.memory[9] = 13 | (2 << 20);
   machine.memory[10] = 16 | (31 << 5) | (4 << 11); // putchar
   machine.memory[11] = 13 | (2 << 20) | (3 << 30);
   machine.memory[12] = 16 | (31 << 5) | (5 << 11); // putchar
   machine.memory[13] = 13 | (2 << 20);
   machine.memory[14] = 16 | (31 << 5) | (6 << 11); // putchar
   machine.memory[15] = 13 | (2 << 20);
   machine.memory[16] = 16 | (31 << 5) | (7 << 11); // putchar
   machine.memory[17] = 13 | (2 << 20) | (3 << 30);
   machine.memory[18] = 16 | (31 << 5) | (8 << 11); // putchar
   machine.memory[19] = 13 | (2 << 20);
   machine.memory[20] = 16 | (31 << 5) | (9 << 11); // putchar
   machine.memory[21] = 13 | (2 << 20);
   machine.memory[22] = 16 | (31 << 5) | (10 << 11); // putchar
   machine.memory[23] = 13 | (2 << 20) | (3 << 30);
   machine.memory[24] = 16 | (31 << 5) | (11 << 11); // putchar
   machine.memory[25] = 13 | (2 << 20);
   machine.memory[26] = 16 | (31 << 5) | (12 << 11); // putchar
   machine.memory[27] = 13 | (2 << 20);
   machine.memory[28] = 16 | (31 << 5) | (11 << 11); // putchar
   machine.memory[29] = 13 | (2 << 20) | (3 << 30);
   machine.memory[30] = 0; // nop
   // PROGRAM ENTRY POINT
   machine.memory[31] = 22 | (30 << 6) | ('H' << 12) | (5 << 29);
   machine.memory[32] = 22 | (30 << 6) | ('e' << 12); // 1
   machine.memory[33] = 22 | (30 << 6) | ('l' << 12);
   machine.memory[34] = 22 | (30 << 6) | ('l' << 12); // 2
   machine.memory[35] = 22 | (30 << 6) | ('o' << 12);
   machine.memory[36] = 22 | (30 << 6) | (',' << 12); // 3
   machine.memory[37] = 22 | (30 << 6) | (' ' << 12);
   machine.memory[38] = 22 | (30 << 6) | ('W' << 12); // 4
   machine.memory[39] = 22 | (30 << 6) | ('o' << 12);
   machine.memory[40] = 22 | (30 << 6) | ('r' << 12); // 5
   machine.memory[41] = 22 | (30 << 6) | ('l' << 12);
   machine.memory[42] = 22 | (30 << 6) | ('d' << 12); // 6
   machine.memory[43] = 22 | (30 << 6) | ('!' << 12);
   machine.memory[44] = 22 | (30 << 6) | ('\n' << 12); // 7

   machine.frames[0].alupc = 31;
   machine.frames[0].flowpc = 31;
   machine.frames[0].entryPoint = 31;
 }

int main (int argc, char ** argv)
 {
   Machine machine;
   MillCore core;
   core.machine = &machine;

   if (1 == argc)
    {
      HelloWorld(machine);
      core.doStuff();
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
      if (0 != std::strncmp(mill, "Mill", 4U))
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
// "Mill" "LE? " "Core" "    " memory_size {data_word} num_frames { frames }
      if (0 == std::strncmp(mill, "Core", 4U))
       {
         std::fread(mill, 1U, 4U, file); // word-align the file
         // A better way to do this is to create a Strategy that is accepted by the class so that
         // knowledge of how to de/serialize a specific class hierarchy to a specific format is in one place.
         machine.read(file);
         std::fclose(file);
         core.doStuff();
       }
// "Mill" "LE? " "Prog" "    " memory_size entry_point num_blocks { block_entry block_size {data_word} }
      else if (0 == std::strncmp(mill, "Prog", 4U))
       {
         std::fread(mill, 1U, 4U, file); // word-align the file
         std::fread(static_cast<void*>(&machine.memsize), sizeof(size_t), 1U, file);
//         std::printf("Size: %lu\n", machine.memsize);
         machine.memory = new MEM_T [machine.memsize];
         std::fread(static_cast<void*>(&machine.frames[0].entryPoint), sizeof(size_t), 1U, file);
//         std::printf("Entry Point: %lu\n", machine.frames[0].entryPoint);
         machine.frames[0].alupc = machine.frames[0].entryPoint;
         machine.frames[0].flowpc = machine.frames[0].entryPoint;
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
         core.doStuff();
       }
      else
       {
         std::printf("Image format not recognized.\n");
         std::fclose(file);
         return 1;
       }
    }

//   pthread_t thread;
//   pthread_create(&thread, NULL, MillCore::runMe, reinterpret_cast<void*>(&core));
//   pthread_detach(thread);

//   std::printf("Sleeping...\n");
//   sleep(5);
//   std::printf("Killing...\n");
//   machine.stop = true;
//   sleep(15);
//   std::printf("Done.\n");

   return 0;
 }
