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

#include <cstdio>
#include <cstring>

typedef unsigned int MEM_T;

//// Condition codes
enum COND
 {
   C_ALWAYS             =  0,
   C_DEFINITE           =  1,
   C_CARRY              =  2,
   C_NO_CARRY           =  3,
   C_SIGNED_OVERFLOW    =  4,
   C_NO_SIGNED_OVERFLOW =  5,
   C_NEGATIVE           =  6,
   C_NOT_NEGATIVE       =  7,
   C_ZERO               =  8,
   C_NOT_ZERO           =  9,
   C_NOT_POSITIVE       = 10,
   C_POSITIVE           = 11,
   C_INVALID            = 12,
   C_NOT_INVALID        = 13,
   C_TRANSIENT          = 14,
   C_NOT_TRANSIENT      = 15
 };

enum DEST_BELT
 {
   BELT_FAST = 0,
   FLOW_SLOW = 16,
   BELT_SLOW = 32
 };

static const char* endian()
 {
   const short var = 0x454C;
   return (0 == std::strncmp("LE", static_cast<const char*>(static_cast<const void*>(&var)), 2U)) ? "LE" : "BE";
 }

//// ALU ops
MEM_T nop(int elide = 0) // use args() for a flow NOP
 {
   return (elide << 28);
 }

MEM_T addc(int lhs, int rhs, int carry, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 1 | static_cast<int>(belt) | (lhs << 10) | (rhs << 16) | (carry << 22) | (elide << 28);
 }

MEM_T subb(int lhs, int rhs, int borrow, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 2 | static_cast<int>(belt) | (lhs << 10) | (rhs << 16) | (borrow << 22) | (elide << 28);
 }

MEM_T mull(int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 3 | static_cast<int>(belt) | (lhs << 10) | (rhs << 16) | (elide << 28);
 }

MEM_T divl(int high, int low, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 4 | static_cast<int>(belt) | (high << 10) | (low << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T pick(COND cond, int source, int _true, int _false, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 5 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (_true << 16) | (_false << 22) | (elide << 28);
 }

MEM_T add(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 6 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T sub(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 7 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T mul(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 8 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T div(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 9 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T udiv(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 10 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T shr(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 11 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T ashr(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 12 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T _and(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 13 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T _or(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 14 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

MEM_T _xor(COND cond, int source, int lhs, int rhs, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 15 | static_cast<int>(belt) | (static_cast<int>(cond) << 6) | (source << 10) | (lhs << 16) | (rhs << 22) | (elide << 28);
 }

// 16 to 21 are INVALID

MEM_T addi(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 22 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T subi(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 23 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T muli(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 24 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T divi(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 25 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T udivi(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 26 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T shri(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 27 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T ashri(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 28 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T _andi(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 29 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T _ori(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 30 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

MEM_T _xori(int lhs, int imm, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 31 | static_cast<int>(belt) | (lhs << 6) | ((imm & 0x1FFFF) << 12) | (elide << 29);
 }

//// FLOW ops
MEM_T fnop(int elide = 0)
 {
   return (elide << 29);
 }

MEM_T args(int first, int second = 0, int third = 0, int fourth = 0)
 {
   return static_cast<int>(FLOW_SLOW) | (first << 5) | (second << 11) | (third << 17) | (fourth << 23);
 }

MEM_T jmp(COND cond, int source, int dest)
 {
   return 1 | (static_cast<int>(cond) << 5) | (source << 9) | (dest << 15);
 }

MEM_T ld(COND cond, int source, int mem, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 2 | static_cast<int>(belt) | (static_cast<int>(cond) << 5) | (source << 9) | (mem << 15) | (elide << 27);
 }

MEM_T ldh(COND cond, int source, int mem, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 3 | static_cast<int>(belt) | (static_cast<int>(cond) << 5) | (source << 9) | (mem << 15) | (elide << 27);
 }

MEM_T ldb(COND cond, int source, int mem, int elide = 0, DEST_BELT belt = BELT_FAST)
 {
   return 4 | static_cast<int>(belt) | (static_cast<int>(cond) << 5) | (source << 9) | (mem << 15) | (elide << 27);
 }

MEM_T st(COND cond, int source, int mem, int val, int elide = 0)
 {
   return 5 | (static_cast<int>(cond) << 5) | (source << 9) | (mem << 15) | (val << 21) | (elide << 27);
 }

MEM_T sth(COND cond, int source, int mem, int val, int elide = 0)
 {
   return 6 | (static_cast<int>(cond) << 5) | (source << 9) | (mem << 15) | (val << 21) | (elide << 27);
 }

MEM_T stb(COND cond, int source, int mem, int val, int elide = 0)
 {
   return 7 | (static_cast<int>(cond) << 5) | (source << 9) | (mem << 15) | (val << 21) | (elide << 27);
 }

MEM_T canon(COND cond, int source, int numargs, int elide = 0)
 {
   return 8 | static_cast<int>(BELT_FAST) | (static_cast<int>(cond) << 5) | (source << 9) | (numargs << 15) | (elide << 27);
 }

MEM_T slow_canon(COND cond, int source, int numargs, int elide = 0)
 {
   return 8 | static_cast<int>(FLOW_SLOW) | (static_cast<int>(cond) << 5) | (source << 9) | (numargs << 15) | (elide << 27);
 }

MEM_T ret(COND cond = C_ALWAYS, int source = 0, int numargs = 0, int elide = 0)
 {
   return 9 | (static_cast<int>(cond) << 5) | (source << 9) | (numargs << 15) | (elide << 27);
 }

MEM_T jmpi(COND cond, int source, int dest, int elide = 0)
 {
   return 10 | (static_cast<int>(cond) << 4) | (source << 8) | ((dest & 0x7FFF) << 14) | (elide << 29);
 }

MEM_T calli(int dest, int numargs, int elide = 0)
 {
   return 11 | (numargs << 4) | ((dest & 0xFFFFF) << 9) | (elide << 29);
 }

MEM_T call(COND cond, int source, int dest, int numargs, int numrets, int elide = 0)
 {
   return 12 | (static_cast<int>(cond) << 4) | (source << 8) | (dest << 14) | (numargs << 20) | (numrets << 25) | (elide << 30);
 }

MEM_T _int(COND cond, int source, int numargs, int numrets, int elide = 0)
 {
   return 13 | (static_cast<int>(cond) << 4) | (source << 8) | (numargs << 20) | (numrets << 25) | (elide << 30);
 }

// 14 and 15 are INVALID

int main (void)
 {
   std::FILE * file = std::fopen("prog.prog", "wb");
   std::fprintf(file, "Mill%s%d Prog    ", endian(), static_cast<int>(sizeof(size_t)));

// MEMORY SIZE
   size_t memsize = 43U;
// ENTRY POINT
   size_t entry = 31U;
// NUMBER OF BLOCKS
   size_t numBlocks = 1U;

   std::fwrite(static_cast<void*>(&memsize), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&entry), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&numBlocks), sizeof(size_t), 1, file);

   size_t blockEntry;
   size_t blockSize;
   MEM_T * block;

/*******************************/
//   DO THIS FOR EACH BLOCK
/*******************************/
// BLOCK ENTRY
   blockEntry = 0U;
// BLOCK SIZE
   blockSize = 43U;

   block = new MEM_T [blockSize];

// FILL OUT BLOCK
   block[0] = jmpi(C_ALWAYS, 0, 0); // 17
   block[1] = ret(); // 16
   block[2] = args(31, 0); // 15
   block[3] = _int(C_ALWAYS, 0, 2, 0);
   block[4] = args(31, 1); // 14
   block[5] = _int(C_ALWAYS, 0, 2, 0, 1);
   block[6] = args(31, 2); // 13
   block[7] = _int(C_ALWAYS, 0, 2, 0, 3);
   block[8] = args(31, 8); // 12
   block[9] = _int(C_ALWAYS, 0, 2, 0);
   block[10] = args(31, 3); // 11
   block[11] = _int(C_ALWAYS, 0, 2, 0);
   block[12] = args(31, 7); // 10
   block[13] = _int(C_ALWAYS, 0, 2, 0);
   block[14] = args(31, 4); // 9
   block[15] = _int(C_ALWAYS, 0, 2, 0);
   block[16] = args(31, 6); // 8
   block[17] = _int(C_ALWAYS, 0, 2, 0, 1);
   block[18] = args(31, 5); // 7
   block[19] = _int(C_ALWAYS, 0, 2, 0, 3);
   block[20] = args(31, 5); // 6
   block[21] = _int(C_ALWAYS, 0, 2, 0, 3);
   block[22] = args(31, 4); // 5
   block[23] = _int(C_ALWAYS, 0, 2, 0);
   block[24] = args(31, 2); // 4
   block[25] = _int(C_ALWAYS, 0, 2, 0);
   block[26] = args(31, 1); // 3
   block[27] = _int(C_ALWAYS, 0, 2, 0);
   block[28] = args(31, 0); // 2
   block[29] = _int(C_ALWAYS, 0, 2, 0);
   block[30] = fnop(); // 1
   // ENTRY POINT FOR JUMPS IS HERE
   block[31] = _ori(30, 'H');
   block[32] = nop(); // 1
   block[33] = addi(0, 29);
   block[34] = addi(0, 36); // 2
   block[35] = addi(0, 3);
   block[36] = _xori(2, 'h'); // 3
   block[37] = addi(0, 12);
   block[38] = addi(4, 15); // 4
   block[39] = addi(3, 3);
   block[40] = subi(5, 1); // 5
   block[41] = addi(4, 1);
   block[42] = subi(4, 22); // 6

// ADMINISTRATIVE TASKS
   std::fwrite(static_cast<void*>(&blockEntry), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&blockSize), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(block), sizeof(MEM_T), blockSize, file);
   delete [] block;
/*******************************/
//   RINSE AND REPEAT
/*******************************/

   std::fclose(file);
   return 0;
 }