/*
 * hashFunction.c
 * 
 * Copyright (c) 2011-2013 BGI-Shenzhen <soap at genomics dot org dot cn>. 
 *
 * This file is part of SOAPdenovo-Trans.
 *
 * SOAPdenovo-Trans is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SOAPdenovo-Trans is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SOAPdenovo-Trans.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdinc.h>

#define KMER_HASH_MASK    0x0000000000ffffffULL

/*
 * Fast non-cryptographic hash for k-mers.  Hashing is on the hot path of
 * graph construction and the old byte-wise CRC32 implementation performed
 * a table lookup for every byte.  The graph does not depend on the exact
 * hash value, only on a good distribution, so a SplitMix-style finalizer
 * gives the same interface with substantially less work.
 */
static inline ubyte8 mix64 (ubyte8 x)
{
	x ^= x >> 30;
	x *= 0xbf58476d1ce4e5b9ULL;
	x ^= x >> 27;
	x *= 0x94d049bb133111ebULL;
	x ^= x >> 31;
	return x;
}

ubyte8 hash_kmer (Kmer kmer)
{
	ubyte8 hash;

#ifdef MER127
	hash = mix64 (kmer.low2);
	hash ^= mix64 (kmer.high2 + 0x9e3779b97f4a7c15ULL);
	hash ^= mix64 (kmer.low1 + 0x3c6ef372fe94f82aULL);
	hash ^= mix64 (kmer.high1 + 0xdaa66d2c7ddefd3bULL);
#elif defined(MER63)
	hash = mix64 (kmer.low);
	hash ^= mix64 (kmer.high + 0x9e3779b97f4a7c15ULL);
#else
	hash = mix64 (kmer);
#endif

	return hash & KMER_HASH_MASK;
}
