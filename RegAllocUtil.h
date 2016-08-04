/*
 * Copyright 2011 Christoph Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef REGALLOC_UTIL_H
#define REGALLOC_UTIL_H

#include "globals.h"

// macro to return the larger of two numbers
#define MAX2(a, b) (((b)>(a)) ? (b):(a))

class Interval
{
public:
   Interval() : head(0), tail(0) { }
   ~Interval();

   bool extend(int, int);
   void unify(Interval*); // clears source interval
   void clear();

   inline int begin() const { return head ? head->bgn : -1; }
   inline int end() const { checkTail(); return tail ? tail->end : -1; }
   inline bool isEmpty() const { return !head; }
   bool overlaps(const Interval&) const;
   bool contains(int pos) const;

   inline int extent() const { return end() - begin(); }
   int length() const;

   void print() const;

   inline void checkTail() const;

private:
   class Range
   {
   public:
      Range(int a, int b) : next(0), bgn(a), end(b) { }

      Range *next;
      int bgn;
      int end;

      void coalesce(Range **ptail)
      {
         Range *rnn;

         while (next && end >= next->bgn) {
            assert(bgn <= next->bgn);
            rnn = next->next;
            end = MAX2(end, next->end);
            delete next;
            next = rnn;
         }
         if (!next)
            *ptail = this;
      }
   };

   Range *head;
   Range *tail;
};

class BitSet
{
public:
   BitSet() : marker(false), data(0), size(0) { }
   BitSet(unsigned int nBits, bool zero) : marker(false), data(0), size(0)
   {
      allocate(nBits, zero);
   }
   ~BitSet()
   {
      if (data)
         free(data);
   }

   // allocate will keep old data iff size is unchanged
   bool allocate(unsigned int nBits, bool zero);
   bool resize(unsigned int nBits); // keep old data, zero additional bits

   inline unsigned int getSize() const { return size; }

   void fill(uint32_t val);

   void setOr(BitSet *, BitSet *); // second BitSet may be NULL

   inline void set(unsigned int i)
   {
      assert(i < size);
      data[i / 32] |= 1 << (i % 32);
   }
   // NOTE: range may not cross 32 bit boundary (implies n <= 32)
   inline void setRange(unsigned int i, unsigned int n)
   {
      assert((i + n) <= size && (((i % 32) + n) <= 32));
      data[i / 32] |= ((1 << n) - 1) << (i % 32);
   }
   inline void setMask(unsigned int i, uint32_t m)
   {
      assert(i < size);
      data[i / 32] |= m;
   }

   inline void clr(unsigned int i)
   {
      assert(i < size);
      data[i / 32] &= ~(1 << (i % 32));
   }
   // NOTE: range may not cross 32 bit boundary (implies n <= 32)
   inline void clrRange(unsigned int i, unsigned int n)
   {
      assert((i + n) <= size && (((i % 32) + n) <= 32));
      data[i / 32] &= ~(((1 << n) - 1) << (i % 32));
   }
   
   inline void clrAll()
   {
      memset(data, 0, (size + 7) / 8);
   }

   inline bool test(unsigned int i) const
   {
      assert(i < size);
      return data[i / 32] & (1 << (i % 32));
   }
   // NOTE: range may not cross 32 bit boundary (implies n <= 32)
   inline bool testRange(unsigned int i, unsigned int n) const
   {
      assert((i + n) <= size && (((i % 32) + n) <= 32));
      return data[i / 32] & (((1 << n) - 1) << (i % 32));
   }

   // Find a range of size (<= 32) clear bits aligned to roundup_pow2(size).
   int findFreeRange(unsigned int size) const;

   BitSet& operator|=(const BitSet&);

   BitSet& operator=(const BitSet& set)
   {
      assert(data && set.data);
      assert(size == set.size);
      memcpy(data, set.data, (set.size + 7) / 8);
      return *this;
   }

   void andNot(const BitSet&);

   // bits = (bits | setMask) & ~clrMask
   inline void periodicMask32(uint32_t setMask, uint32_t clrMask)
   {
      for (unsigned int i = 0; i < (size + 31) / 32; ++i)
         data[i] = (data[i] | setMask) & ~clrMask;
   }

   //unsigned int popCount() const;

   void print() const;

public:
   bool marker; // for user

private:
   uint32_t *data;
   unsigned int size;
};

void Interval::checkTail() const
{
#if COMPILER_DEBUG & COMPILER_DEBUG_RA
   Range *r = head;
   while (r->next)
      r = r->next;
   assert(tail == r);
#endif
}

#endif // !defined REGALLOC_UTIL_H
