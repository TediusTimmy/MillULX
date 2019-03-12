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

#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <set>
#include <cstdio>
#include <cstring>

// A form of the input closest to lexical form.
class Op
 {
public:
   char type; // + > [ , . 0
   int run; // + >   (- and < are negative runs)
   std::vector<Op> loop; // [

   Op(int type) : type(type), run(0) { }

   // Convert the Op form to a condensed lexical form, for determining unique blocks
   std::string toString() const
    {
      switch (type)
       {
      case '+':
         switch (run)
          {
         case 1:
            return "+";
         case 2:
            return "++";
         case -1:
            return "-";
         case -2:
            return "--";
         default:
            if (run > 0)
               return "+" + std::to_string(run);
            else
               return "-" + std::to_string(-run);
          }
      case '>':
         switch (run)
          {
         case 1:
            return ">";
         case 2:
            return ">>";
         case -1:
            return "<";
         case -2:
            return "<<";
         default:
            if (run > 0)
               return ">" + std::to_string(run);
            else
               return "<" + std::to_string(-run);
          }
      case '.':
         return ".";
      case ',':
         return ",";
      case '0':
         return "[-]";
      case '[':
         std::string result = "[";
         for (auto op : loop)
          {
            result += op.toString();
          }
         return result + "]";
       }
      std::cerr << "If you see this message, don't trust the output." << std::endl;
      return "";
    }
 };

// Read in the program, byte by byte, into Op form.
// Do not be fooled by whitespace when counting run lengths.
bool recRead(std::vector<Op>& dest, bool isMain)
 {
   int next = std::cin.get();
   while (false == std::cin.eof())
    {
      switch (next)
       {
      case '+':
      case '-':
      case '>':
      case '<':
         if (true == dest.empty())
          {
            dest.push_back(Op(next));
            dest.back().run = 1;
          }
         else
          {
            if (dest.back().type == next)
             {
               ++dest.back().run;
             }
            else
             {
               dest.push_back(Op(next));
               dest.back().run = 1;
             }
          }
         break;
      case ',':
         dest.push_back(Op(next));
         break;
      case '.':
         dest.push_back(Op(next));
         break;
      case '[':
         dest.push_back(Op(next));
         if (!recRead(dest.back().loop, false))
          {
            return false;
          }
         break;
      case ']':
         return !isMain;
      default:
         break;
       }
      next = std::cin.get();
    }
   return isMain;
 }

bool cleaner(std::vector<Op>& dest, bool isMain)
 {
   if (false == dest.empty())
    {
      // Remove comments at the beginning of main.
      while ((true == isMain) && ('[' == dest[0].type))
       {
         dest.erase(dest.begin());
       }
      for (size_t i = 0U; i < dest.size(); ++i)
       {
         if ('[' == dest[i].type)
          {
            if (!cleaner(dest[i].loop, false))
             {
               return false;
             }
            // Remove loops-after-loops. Lost Kingdom's code generator did this a lot.
            while (((i + 1U) < dest.size()) && ('[' == dest[i + 1].type))
             {
               dest.erase(dest.begin() + i + 1U);
             }
          }
       }
    }
   else
    {
      return false; // This is bad.
    }
   return true;
 }

// Convert - to + with a negative run.
// Convert < to > with a negative run.
void canon (std::vector<Op>& dest)
 {
   for (size_t i = 0U; i < dest.size(); ++i)
    {
      if ('-' == dest[i].type)
       {
         dest[i].type = '+';
         dest[i].run = -dest[i].run;
       }
      else if ('<' == dest[i].type)
       {
         dest[i].type = '>';
         dest[i].run = -dest[i].run;
       }
      else if ('[' == dest[i].type)
       {
         canon(dest[i].loop);
       }
    }
 }

bool cleanse (std::vector<Op>& dest, bool isMain)
 {
   for (size_t i = 0U; i < dest.size(); ++i)
    {
      // Fold + followed by - (or vice-versa) together.
      while (((i + 1U) < dest.size()) && ('+' == dest[i].type) && ('+' == dest[i + 1U].type))
       {
         dest[i].run += dest[i + 1U].run;
         dest.erase(dest.begin() + i + 1U);
         if (0 == dest[i].run)
          {
            dest.erase(dest.begin() + i);
            if (true == dest.empty())
             {
               return isMain;
             }
          }
       }
      // Fold > followed by < (or vice-versa) together.
      while (((i + 1U) < dest.size()) && ('>' == dest[i].type) && ('>' == dest[i + 1U].type))
       {
         dest[i].run += dest[i + 1U].run;
         dest.erase(dest.begin() + i + 1U);
         if (0 == dest[i].run)
          {
            dest.erase(dest.begin() + i);
            if (true == dest.empty())
             {
               return isMain;
             }
          }
       }
      if ('[' == dest[i].type)
       {
         if (!cleanse(dest[i].loop, false))
          {
            return false;
          }
         // Convert [-] idiom to 0 pseudo-instruction.
         if ((1U == dest[i].loop.size()) && ('+' == dest[i].loop[0].type) && (-1 == dest[i].loop[0].run))
          {
            dest[i].type = '0';
            dest[i].loop.clear();
          }
       }
    }
   for (size_t i = 0U; i < dest.size(); ++i)
    {
      // Remove + before 0 : it will be removed anyway.
      while (((i + 1U) < dest.size()) && ('+' == dest[i].type) && ('0' == dest[i + 1U].type))
       {
         dest.erase(dest.begin() + i);
         if (true == dest.empty())
          {
            return isMain;
          }
       }
    }
   return true;
 }

const int MAKE_ZERO = 0x80000000;

// Form2 takes advantage of the fact that a data operation (+ - 0) is independent of
// a pointer operation (> <), and thus both can be issued together.
// FUTURE : intermingle the code of two Form2s for better density.
// SPACE AGE : Do actual dependency analysis and hoisting of operations into available NOPs.
//    This might be easier in Op form....
class Form2
 {
public:
   char type; // + [ , .
   int d_run; // if MAKE_ZERO, then this is [-]
   int p_run;
   size_t loop; // index into array

   Form2(char type) : type(type) { }
 };

// Convert Op form into Form2, an intermediate representation between Op form and Dispatches.
// Here is where we log dependencies between blocks, and force block uniqueness (reuse duplicated blocks).
void convert (const std::vector<Op>& src, size_t dest,
   std::vector<std::vector<Form2> >& converts, std::vector<std::set<size_t> >& deps, std::map<std::string, size_t>& reps)
 {
   for (size_t i = 0U; i < src.size(); ++i)
    {
      switch (src[i].type)
       {
      case '+':
      case '0':
         if (((i + 1U) < src.size()) && ('>' == src[i + 1U].type))
          {
            converts[dest].push_back(Form2('+'));
            converts[dest].back().d_run = ('+' == src[i].type) ? src[i].run : MAKE_ZERO;
            converts[dest].back().p_run = src[i + 1U].run;
            ++i;
          }
         else
          {
            converts[dest].push_back(Form2('+'));
            converts[dest].back().d_run = ('+' == src[i].type) ? src[i].run : MAKE_ZERO;
            converts[dest].back().p_run = 0;
          }
         break;
      case '>':
// I can't make this optimization, as the data modification requires the new pointer, and I can't know to hoist the pointer computation.
/*         if (((i + 1U) < src.size()) && ('+' == src[i + 1U].type))
          {
            converts[dest].push_back(Form2('+'));
            converts[dest].back().d_run = src[i + 1U].run;
            converts[dest].back().p_run = src[i].run;
            ++i;
          }
         else if (((i + 1U) < src.size()) && ('0' == src[i + 1U].type))
          {
            converts[dest].push_back(Form2('+'));
            converts[dest].back().d_run = MAKE_ZERO;
            converts[dest].back().p_run = src[i].run;
            ++i;
          }
         else
          {*/
            converts[dest].push_back(Form2('+'));
            converts[dest].back().d_run = 0;
            converts[dest].back().p_run = src[i].run;
//          }
         break;
      case '.':
         converts[dest].push_back(Form2('.'));
         break;
      case ',':
         converts[dest].push_back(Form2(','));
         break;
      case '[':
         converts[dest].push_back(Form2('['));
         std::string finger = src[i].toString();
         if (reps.end() == reps.find(finger))
          {
            converts[dest].back().loop = converts.size();
            deps[dest].insert(converts[dest].back().loop);
            reps.insert(std::make_pair(finger, converts[dest].back().loop));
            converts.push_back(std::vector<Form2>());
            deps.push_back(std::set<size_t>());
            convert(src[i].loop, converts[dest].back().loop, converts, deps, reps);
          }
         else
          {
            converts[dest].back().loop = reps[finger];
            deps[dest].insert(converts[dest].back().loop);
          }
         break;
       }
    }
 }

// A Dispatch is one clock cycle. It is three instructions that are executed together.
class Dispatch
 {
public:
   int alu1, alu2, flow;

   Dispatch(int alu1, int alu2, int flow) : alu1(alu1), alu2(alu2), flow(flow) { }
 };

int nop(int elide = 0) // elide is for ALU side only
 {
   return (elide << 28);
 }

int pick(int cond, int source, int _true, int _false)
 {
   return 5 | (cond << 6) | (source << 10) | (_true << 16) | (_false << 22);
 }

int addi(int lhs, int imm)
 {
   if (((imm < 0) && (-imm > 0x10000)) || (imm > 0xFFFF))
    {
      std::cout << "Bad compile: immediate overflow (" << imm << ")." << std::endl;
    }
   return 22 | (lhs << 6) | ((imm & 0x1FFFF) << 12);
 }

int subi(int lhs, int imm)
 {
   if (((imm < 0) && (-imm > 0x10000)) || (imm > 0xFFFF))
    {
      std::cout << "Bad compile: immediate overflow (" << imm << ")." << std::endl;
    }
   return 23 | (lhs << 6) | ((imm & 0x1FFFF) << 12);
 }

int ldb(int mem)
 {
   return 4 | (mem << 15);
 }

int stb(int mem, int val)
 {
   return 7 | (mem << 15) | (val << 21);
 }

int ret(int cond, int source, int numargs)
 {
   return 9 | (cond << 5) | (source << 9) | (numargs << 15);
 }

int jmpi(int dest = 0)
 {
   return 10 | ((dest & 0x7FFF) << 14);
 }

int call(int cond, int source, int dest, int numargs, int numrets)
 {
   return 12 | (cond << 4) | (source << 8) | (dest << 14) | (numargs << 20) | (numrets << 25);
 }

int _int(int numargs, int numrets)
 {
   return 13 | (numargs << 20) | (numrets << 25);
 }

int args(int first, int second = 0, int third = 0, int fourth = 0)
 {
   return 16 | (first << 5) | (second << 11) | (third << 17) | (fourth << 23);
 }

int changeDP (int dp, int inc, size_t i, size_t j)
 {
   dp += inc;
   if (dp > 29)
    {
      std::cout << "Bad compile: lost data pointer in block " << i << " at " << j << std::endl;
    }
   return dp;
 }

// Convert Form2 into the actual instructions that will be issued.
void compile1(const std::vector<std::vector<Form2> >& converts, std::vector<std::vector<Dispatch> >& compiledBlocks)
 {
   for (size_t i = 0U; i < converts.size(); ++i)
    {
      compiledBlocks.push_back(std::vector<Dispatch>());

      // Initialize the Data Pointer.
      if (0U == i)
       { // Add zero to zero to put a zero on the belt.
         compiledBlocks.back().push_back(Dispatch(addi(30, 0), nop(), nop()));
       }
      int dp = 0; // Where is the data pointer.

      for (size_t j = 0U; j < converts[i].size(); ++j)
       {
         switch (converts[i][j].type)
          {
         case '+':
            if (MAKE_ZERO == converts[i][j].d_run)
             {
               if (0 == converts[i][j].p_run)
                {
                  compiledBlocks.back().push_back(Dispatch(nop(), nop(), stb(dp, 30)));
                }
               else
                {
                  compiledBlocks.back().push_back(Dispatch(nop(), addi(dp, converts[i][j].p_run), stb(dp, 30)));
                  dp = changeDP(dp, -dp, i, j);
                }
             }
            else if (0 == converts[i][j].d_run)
             {
               compiledBlocks.back().push_back(Dispatch(nop(), addi(dp, converts[i][j].p_run), nop()));
               dp = changeDP(dp, -dp, i, j);
             }
            else
             {
               if (0 == converts[i][j].p_run)
                {
                  compiledBlocks.back().push_back(Dispatch(nop(), nop(), ldb(dp)));
                  compiledBlocks.back().push_back(Dispatch(addi(0, converts[i][j].d_run), nop(), nop()));
                  dp = changeDP(dp, 2, i, j);
                  compiledBlocks.back().push_back(Dispatch(nop(), nop(), stb(dp, 0)));
                }
               else
                {
                  compiledBlocks.back().push_back(Dispatch(nop(), nop(), ldb(dp)));
                  dp = changeDP(dp, 1, i, j);
                  compiledBlocks.back().push_back(Dispatch(addi(0, converts[i][j].d_run), addi(dp, converts[i][j].p_run), nop()));
                  dp = changeDP(dp, 2, i, j);
                  compiledBlocks.back().push_back(Dispatch(nop(), nop(), stb(dp, 1)));
                  dp = changeDP(dp, -dp, i, j);
                }
             }
            break;
         case '.':
            compiledBlocks.back().push_back(Dispatch(nop(), nop(), ldb(dp)));
            dp = changeDP(dp, 1, i, j);
            compiledBlocks.back().push_back(Dispatch(nop(1), nop(), _int(2, 0)));
            compiledBlocks.back().push_back(Dispatch(nop(), nop(), args(31, 0)));
            break;
         case ',':
            compiledBlocks.back().push_back(Dispatch(addi(31, 1), nop(), nop()));
            dp = changeDP(dp, 1, i, j);
            compiledBlocks.back().push_back(Dispatch(nop(1), nop(), _int(1, 1)));
            compiledBlocks.back().push_back(Dispatch(nop(), nop(), args(0)));
            dp = changeDP(dp, 1, i, j);
            compiledBlocks.back().push_back(Dispatch(nop(), nop(), stb(dp, 0)));
            break;
         case '[':
            compiledBlocks.back().push_back(Dispatch(subi(30, converts[i][j].loop), nop(), ldb(dp))); // Flag the offset for calls as special
            dp = changeDP(dp, 2, i, j);
            compiledBlocks.back().push_back(Dispatch(nop(1), nop(), call(8, 0, 1, 1, 1)));
            compiledBlocks.back().push_back(Dispatch(nop(), nop(), args(dp)));
            dp = changeDP(dp, 1, i, j);
            compiledBlocks.back().push_back(Dispatch(nop(), pick(14, 0, 0, dp), nop()));
            dp = changeDP(dp, -dp, i, j);
            break;
          }

         // FUTURE : insert check for dp > 20 here and rescue the dp if so.
       }

      if (0U == i)
       {
         compiledBlocks.back().push_back(Dispatch(nop(), nop(), ret(0, 0, 0)));
       }
      else
       {
         compiledBlocks.back().push_back(Dispatch(nop(), nop(), ldb(dp)));
         dp = changeDP(dp, 1, i, 0xFFFFFFFF);
         compiledBlocks.back().push_back(Dispatch(addi(dp, 0), nop(), ret(7, 0, 1)));
         compiledBlocks.back().push_back(Dispatch(nop(), nop(), args(dp)));
         compiledBlocks.back().push_back(Dispatch(nop(), nop(), jmpi()));
       }
    }
 }

// Are the dependencies of block complete?
bool canAdd (const std::set<size_t>& block, const std::set<size_t>& complete)
 {
   for (std::set<size_t>::const_iterator iter = block.begin(); block.end() != iter; ++iter)
    {
      if (complete.end() == complete.find(*iter))
       {
         return false;
       }
    }
   return true;
 }

// Load the instructions into the actual memory image. Return the entry point of block 0.
// Inefficient: Loop through the list until every block has been compiled.
size_t compile2(const std::vector<std::vector<Dispatch> >& compiledBlocks, std::vector<int>& memory, const std::vector<std::set<size_t> >& deps)
 {
   std::vector<size_t> entryPoints;
   entryPoints.resize(compiledBlocks.size());
   size_t done = 0U;
   std::set<size_t> complete;
   while (compiledBlocks.size() > done)
    {
      for (size_t i = 0U; i < compiledBlocks.size(); ++i)
       {
         if ((0U == entryPoints[i]) && (true == canAdd(deps[i], complete)))
          {
            entryPoints[i] = memory.size() + compiledBlocks[i].size();
            complete.insert(i);
            for (size_t j = compiledBlocks[i].size() - 1U; j > 0; --j) // No block should ever be of zero size.
             {
               memory.push_back(compiledBlocks[i][j].flow);
             }
            memory.push_back(compiledBlocks[i][0U].flow);
            for (size_t j = 0U; j < compiledBlocks[i].size(); ++j)
             {
               if (23 == (compiledBlocks[i][j].alu1 & 0x1F)) // Is this loading a block entry address?
                {
                  int index = ((compiledBlocks[i][j].alu1 >> 12) & 0x1FFFF);
                  int imm = entryPoints[i] - entryPoints[index]; // Should always be positive.
                  int op = subi(30, imm);
                  memory.push_back(op);
                }
               else
                {
                  memory.push_back(compiledBlocks[i][j].alu1);
                }
               memory.push_back(compiledBlocks[i][j].alu2);
             }
            ++done;
          }
       }
    }
   return entryPoints[0];
 }

static const char* endian()
 {
   const short var = 0x454C;
   return (0 == std::strncmp("LE", static_cast<const char*>(static_cast<const void*>(&var)), 2U)) ? "LE" : "BE";
 }

void dumpBin(size_t entry, const std::vector<int>& data)
 {
   std::FILE * file = std::fopen("prog.prog", "wb");
   std::fprintf(file, "Mill%s%d Prog    ", endian(), static_cast<int>(sizeof(size_t)));

   size_t memsize = data.size();
   size_t numBlocks = 1U;
   size_t blockEntry = 0U;

   std::fwrite(static_cast<void*>(&memsize), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&entry), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&numBlocks), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&blockEntry), sizeof(size_t), 1, file);
   std::fwrite(static_cast<void*>(&memsize), sizeof(size_t), 1, file);
   std::fwrite(static_cast<const void*>(&data[0U]), sizeof(int), memsize, file);

   std::fclose(file);
 }

int main (void)
 {
   Op program ('[');
   if (!recRead(program.loop, true))
    {
      std::cerr << "Error reading input. Probably unmatched '[' or ']'." << std::endl;
      return 1;
    }
   if (!cleaner(program.loop, true))
    {
      std::cerr << "Error reading input. You have an empty loop, which will either be ignored or hang the program." << std::endl
         << "We'll be safe, assume the latter, and not compile." << std::endl;
      return 1;
    }
   canon(program.loop);
   if (!cleanse(program.loop, true))
    {
      std::cerr << "Error reading input. You have an effectively-empty loop, which will either be ignored or hang the program." << std::endl
         << "We'll be safe, assume the latter, and not compile." << std::endl;
      return 1;
    }
//   std::cout << program.toString() << std::endl;

   std::vector<std::vector<Form2> > converts;
   std::vector<std::set<size_t> > deps;
   std::map<std::string, size_t> reps;

   converts.push_back(std::vector<Form2>());
   deps.push_back(std::set<size_t>());
   reps.insert(std::make_pair(program.toString(), 0U));

   convert(program.loop, 0U, converts, deps, reps);

//   std::cout << "Converts: " << converts.size() << " : ";
//   for (size_t i = 0U; i < converts.size(); ++i) std::cout << converts[i].size() << " ";
//   std::cout << std::endl << "Deps: " << deps.size() << " : ";
//   for (size_t i = 0U; i < deps.size(); ++i) std::cout << deps[i].size() << " ";
//   std::cout << std::endl << "Reps: " << reps.size() << std::endl;

//   for (std::map<std::string, size_t>::const_iterator iter = reps.begin(); reps.end() != iter; ++iter)
//    {
//      std::cout << iter->first << std::endl << '\t' << iter->second << '\t' << converts[iter->second].size() << " : ";
//      for (std::set<size_t>::const_iterator diter = deps[iter->second].begin(); deps[iter->second].end() != diter; ++diter)
//       {
//         std::cout << *diter << ' ';
//       }
//      std::cout << std::endl;
//    }

   std::vector<std::vector<Dispatch> > compiledBlocks;
   compile1(converts, compiledBlocks);

   std::vector<int> memory;
   // The first 8K words (32K bytes) are the tape of zeros.
   memory.resize(8192);
   size_t entry = compile2(compiledBlocks, memory, deps);

   dumpBin(entry, memory);

   return 0;
 }
