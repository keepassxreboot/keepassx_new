/**********************************************************************************
 * C implementation of the zxcvbn password strength estimation method.
 * Copyright (c) 2015-2017 Tony Evans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **********************************************************************************/

#include <zxcvbn.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <float.h>

/* printf */
#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif

#ifdef USE_DICT_FILE
#if defined(USE_FILE_IO) || !defined(__cplusplus)
#include <stdio.h>
#else
#include <fstream>
#endif
#endif


/* Minimum number of characters in a incrementing/decrementing sequence match */
#define MIN_SEQUENCE_LEN 3

/* Year range for data matching */
#define MIN_YEAR 1901
#define MAX_YEAR 2050

/* Minimum number of characters in a spatial matching sequence */
#define MIN_SPATIAL_LEN 3

/* Minimum number of characters in a repeat sequence match */
#define MIN_REPEAT_LEN 2

/* Additional entropy to add when password is made of multiple matches. Use different
 * amounts depending on whether the match is at the end of the password, or in the
 * middle. If the match is at the begining then there is no additional entropy.
 */
#define MULTI_END_ADDITION 1.0
#define MULTI_MID_ADDITION 1.75

/*################################################################################*
 *################################################################################*
 * Begin utility function code
 *################################################################################*
 *################################################################################*/

/**********************************************************************************
 * Binomial coefficient function. Uses method described at
 *      http://blog.plover.com/math/choose.html
 */
static double nCk(int n, int k)
{
    int d;
    double r;
    if (k > n)
        return 0.0;
    if (!k)
        return 1.0;
    r = 1.0;
    for(d = 1; d <= k; ++d)
    {
        r *= n--;
        r /= d;
    }
    return r;
}

/**********************************************************************************
 * Binary search function to find a character in a string.
 * Parameters:
 *  Ch      The character to find
 *  Ents    The string to search
 *  NumEnts The number character groups in the string Ents
 *  SizeEnt The size of each character group.
 * Returns a pointer to the found character, or null if not found.
 */
static const uint8_t *CharBinSearch(uint8_t Ch, const uint8_t *Ents, unsigned int NumEnts, unsigned int SizeEnt)
{
    while(NumEnts > 0)
    {
        const uint8_t *Mid = Ents + (NumEnts >> 1) * SizeEnt;
        int Dif = Ch - *Mid;
        if (!Dif)
        {
            return Mid;
        }
        if (Dif > 0)
        {
            Ents = Mid + SizeEnt;
            --NumEnts;
        }
        NumEnts /= 2;
    }
    return 0;
}

/**********************************************************************************
 * Calculate potential number of different characters in the passed string.
 * Parameters:
 *  Str     The string of characters
 *  Len     The maximum number of characters to scan
 * Returns the potential number of different characters in the string.
 */
static int Cardinality(const uint8_t *Str, int Len)
{
    int Card=0, Types=0;
    int c;
    while(Len > 0)
    {
        c = *Str++ & 0xFF;
        if (!c)
            break;
        if (islower(c))      Types |= 1;    /* Lowercase letter */
        else if (isupper(c)) Types |= 2;    /* Uppercase letter */
        else if (isdigit(c)) Types |= 4;    /* Numeric digit */
        else if (c <= 0x7F)  Types |= 8;    /* Punctuation character */
        else                 Types |= 16;   /* Other (Unicode?) */
        --Len;
    }
    if (Types & 1)  Card += 26;
    if (Types & 2)  Card += 26;
    if (Types & 4)  Card += 10;
    if (Types & 8)  Card += 33;
    if (Types & 16) Card += 100;
    return Card;
}

/**********************************************************************************
 * Allocate a ZxcMatch_t struct, clear it to zero
 */
static ZxcMatch_t *AllocMatch()
{
    ZxcMatch_t *p = MallocFn(ZxcMatch_t, 1);
    memset(p, 0, sizeof *p);
    return p;
}

/**********************************************************************************
 * Add new match struct to linked list of matches. List ordered with shortest at
 * head of list. Note: passed new match struct in parameter Nu may be de allocated.
 */
static void AddResult(ZxcMatch_t **HeadRef, ZxcMatch_t *Nu, int MaxLen)
{
    /* Adjust the entropy to be used for calculations depending on whether the passed match is
     * at the begining, middle or end of the password
     */
    if (Nu->Begin)
    {
        if (Nu->Length >= MaxLen)
            Nu->MltEnpy = Nu->Entrpy + MULTI_END_ADDITION * log(2.0);
        else
            Nu->MltEnpy = Nu->Entrpy + MULTI_MID_ADDITION * log(2.0);
    }
    else
        Nu->MltEnpy = Nu->Entrpy;

    /* Find the correct insert point */
    while(*HeadRef && ((*HeadRef)->Length < Nu->Length))
        HeadRef = &((*HeadRef)->Next);

    /* Add new entry or replace existing */
    if (*HeadRef && ((*HeadRef)->Length == Nu->Length))
    {
        /* New entry has same length as existing, so one of them needs discarding */
        if ((*HeadRef)->MltEnpy <= Nu->MltEnpy)
        {
            /* Existing entry has lower entropy - keep it, discard new entry */
            FreeFn(Nu);
        }
        else
        {
            /* New entry has lower entropy - replace existing entry */
            Nu->Next = (*HeadRef)->Next;
            FreeFn(*HeadRef);
            *HeadRef = Nu;
        }
    }
    else
    {
        /* New entry has different length, so add it */
        Nu->Next = *HeadRef;
        *HeadRef = Nu;
    }
}

/**********************************************************************************
 * See if the match is repeated. If it is then add a new repeated match to the results.
 */
static void AddMatchRepeats(ZxcMatch_t **Result, ZxcMatch_t *Match, const uint8_t *Passwd, int MaxLen)
{
    int Len = Match->Length;
    const uint8_t *Rpt = Passwd + Len;
    int RepeatCount = 2;

    while(MaxLen >= (Len * RepeatCount))
    {
        if (strncmp((const char *)Passwd, (const char *)Rpt, Len) == 0)
        {
            /* Found a repeat */
            ZxcMatch_t *p = AllocMatch();
            p->Entrpy = Match->Entrpy + log(RepeatCount);
            p->Type = (ZxcTypeMatch_t)(Match->Type + MULTIPLE_MATCH);
            p->Length = Len * RepeatCount;
            p->Begin = Match->Begin;
            AddResult(Result, p, MaxLen);
        }
        else
            break;
        ++RepeatCount;
        Rpt += Len;
    }
}

/*################################################################################*
 *################################################################################*
 * Begin dictionary matching code
 *################################################################################*
 *################################################################################*/

#ifdef USE_DICT_FILE
/* Use dictionary data from file */

#if defined(USE_FILE_IO) || !defined(__cplusplus)
/* Use the FILE streams from stdio.h */

typedef FILE *FileHandle;

#define MyOpenFile(f, name)       (f = fopen(name, "rb"))
#define MyReadFile(f, buf, bytes) (fread(buf, 1, bytes, f) == (bytes))
#define MyCloseFile(f)            fclose(f)

#else

/* Use the C++ iostreams */
typedef std::ifstream FileHandle;

static inline void MyOpenFile(FileHandle & f, const char *Name)
{
    f.open(Name, std::ifstream::in | std::ifstream::binary);
}
static inline bool MyReadFile(FileHandle & f, void *Buf, unsigned int Num)
{
    return (bool)f.read((char *)Buf, Num);
}
static inline void MyCloseFile(FileHandle & f)
{
    f.close();
}

#endif

/* Include file contains the CRC of the dictionary data file. Used to detect corruption */
/* of the file. */
#include "dict-crc.h"

#define MAX_DICT_FILE_SIZE  (100+WORD_FILE_SIZE)
#define CHK_INIT 0xffffffffffffffffULL

/* Static table used for the crc implementation. */
static const uint64_t CrcTable[16] =
{
    0x0000000000000000ULL, 0x7d08ff3b88be6f81ULL, 0xfa11fe77117cdf02ULL, 0x8719014c99c2b083ULL,
    0xdf7adabd7a6e2d6fULL, 0xa2722586f2d042eeULL, 0x256b24ca6b12f26dULL, 0x5863dbf1e3ac9decULL,
    0x95ac9329ac4bc9b5ULL, 0xe8a46c1224f5a634ULL, 0x6fbd6d5ebd3716b7ULL, 0x12b5926535897936ULL,
    0x4ad64994d625e4daULL, 0x37deb6af5e9b8b5bULL, 0xb0c7b7e3c7593bd8ULL, 0xcdcf48d84fe75459ULL
};

static const unsigned int MAGIC = 'z' + ('x'<< 8) + ('c' << 16) + ('v' << 24);

static unsigned int NumNodes, NumChildLocs, NumRanks, NumWordEnd, NumChildMaps;
static unsigned int SizeChildMapEntry, NumLargeCounts, NumSmallCounts, SizeCharSet;

static unsigned int   *DictNodes;
static uint8_t        *WordEndBits;
static unsigned int   *ChildLocs;
static unsigned short *Ranks;
static uint8_t        *ChildMap;
static uint8_t        *EndCountLge;
static uint8_t        *EndCountSml;
static char           *CharSet;

/**********************************************************************************
 * Calculate the CRC-64 of passed data.
 * Parameters:
 *  Crc     The initial or previous CRC value
 *  v       Pointer to the data to add to CRC calculation
 *  Len     Length of the passed data
 * Returns the updated CRC value.
 */
static uint64_t CalcCrc64(uint64_t Crc, const void *v, unsigned int Len)
{
    const uint8_t *Data = (const unsigned char *)v;
    while(Len--)
    {
        Crc = CrcTable[(Crc ^ (*Data >> 0)) & 0x0f] ^ (Crc >> 4);
        Crc = CrcTable[(Crc ^ (*Data >> 4)) & 0x0f] ^ (Crc >> 4);
        ++Data;
    }
    return Crc;
}

/**********************************************************************************
 * Read the dictionary data from file.
 * Parameters:
 *  Filename    Name of the file to read.
 * Returns 1 on success, 0 on error
 */
int ZxcvbnInit(const char *Filename)
{
    FileHandle f;
    uint64_t Crc = CHK_INIT;
    if (DictNodes)
        return 1;
    MyOpenFile(f, Filename);
    if (f)
    {
        unsigned int i, DictSize;

        /* Get magic number */
        if (!MyReadFile(f, &i, sizeof i))
            i = 0;

        /* Get header data */
        if (!MyReadFile(f, &NumNodes, sizeof NumNodes))
            i = 0;
        if (!MyReadFile(f, &NumChildLocs, sizeof NumChildLocs))
            i = 0;
        if (!MyReadFile(f, &NumRanks, sizeof NumRanks))
            i = 0;
        if (!MyReadFile(f, &NumWordEnd, sizeof NumWordEnd))
            i = 0;
        if (!MyReadFile(f, &NumChildMaps, sizeof NumChildMaps))
            i = 0;
        if (!MyReadFile(f, &SizeChildMapEntry, sizeof SizeChildMapEntry))
            i = 0;
        if (!MyReadFile(f, &NumLargeCounts, sizeof NumLargeCounts))
            i = 0;
        if (!MyReadFile(f, &NumSmallCounts, sizeof NumSmallCounts))
            i = 0;
        if (!MyReadFile(f, &SizeCharSet, sizeof SizeCharSet))
            i = 0;

        /* Validate the header data */
        if (NumNodes >= (1<<17))
            i = 1;
        if (NumChildLocs >= (1<<BITS_CHILD_MAP_INDEX))
            i = 2;
        if (NumChildMaps >= (1<<BITS_CHILD_PATT_INDEX))
            i = 3;
        if ((SizeChildMapEntry*8) < SizeCharSet)
            i = 4;
        if (NumLargeCounts >= (1<<9))
            i = 5;
        if (NumSmallCounts != NumNodes)
            i = 6;

        if (i != MAGIC)
        {
            MyCloseFile(f);
            return 0;
        }
        Crc = CalcCrc64(Crc, &i,    sizeof i);
        Crc = CalcCrc64(Crc, &NumNodes,     sizeof NumNodes);
        Crc = CalcCrc64(Crc, &NumChildLocs, sizeof NumChildLocs);
        Crc = CalcCrc64(Crc, &NumRanks,     sizeof NumRanks);
        Crc = CalcCrc64(Crc, &NumWordEnd,   sizeof NumWordEnd);
        Crc = CalcCrc64(Crc, &NumChildMaps, sizeof NumChildMaps);
        Crc = CalcCrc64(Crc, &SizeChildMapEntry, sizeof SizeChildMapEntry);
        Crc = CalcCrc64(Crc, &NumLargeCounts,   sizeof NumLargeCounts);
        Crc = CalcCrc64(Crc, &NumSmallCounts,   sizeof NumSmallCounts);
        Crc = CalcCrc64(Crc, &SizeCharSet,      sizeof SizeCharSet);

        DictSize = NumNodes*sizeof(*DictNodes) + NumChildLocs*sizeof(*ChildLocs) + NumRanks*sizeof(*Ranks) +
                   NumWordEnd + NumChildMaps*SizeChildMapEntry + NumLargeCounts + NumSmallCounts + SizeCharSet;
        if (DictSize < MAX_DICT_FILE_SIZE)
        {
            DictNodes = MallocFn(unsigned int, DictSize / sizeof(unsigned int) + 1);
            if (!MyReadFile(f, DictNodes, DictSize))
            {
                FreeFn(DictNodes);
                DictNodes = 0;
            }
        }
        MyCloseFile(f);

        if (!DictNodes)
            return 0;
        /* Check crc */
        Crc = CalcCrc64(Crc, DictNodes, DictSize);
        if (memcmp(&Crc, WordCheck, sizeof Crc))
        {
            /* File corrupted */
            FreeFn(DictNodes);
            DictNodes = 0;
            return 0;
        }
        fflush(stdout);
        /* Set pointers to the data */
        ChildLocs = DictNodes + NumNodes;
        Ranks = (unsigned short *)(ChildLocs + NumChildLocs);
        WordEndBits = (unsigned char *)(Ranks + NumRanks);
        ChildMap = (unsigned char*)(WordEndBits + NumWordEnd);
        EndCountLge = ChildMap + NumChildMaps*SizeChildMapEntry;
        EndCountSml = EndCountLge + NumLargeCounts;
        CharSet = (char *)EndCountSml + NumSmallCounts;
        CharSet[SizeCharSet] = 0;
        return 1;
    }
    return 0;
}
/**********************************************************************************
 * Free the data allocated by ZxcvbnInit().
 */
void ZxcvbnUnInit()
{
    if (DictNodes)
        FreeFn(DictNodes);
    DictNodes = 0;
}

#else

/* Include the source file containing the dictionary data */
#include "dict-src.h"

#endif

/**********************************************************************************
 * Leet conversion strings
 */
/* String of normal chars that could be given as leet chars in the password */
static const uint8_t L33TChr[] = "abcegilostxz";

/* String of leet,normal,normal char triples. Used to convert supplied leet char to normal. */
static const uint8_t L33TCnv[] = "!i $s %x (c +t 0o 1il2z 3e 4a 5s 6g 7lt8b 9g <c @a [c {c |il";
#define LEET_NORM_MAP_SIZE 3

/* Struct holding additional data on the word match */
typedef struct
{
    int Rank;                        /* Rank of word in dictionary */
    int  Caps;                       /* Number of capital letters */
    int  Lower;                      /* Number of lower case letters */
    int NumLeet;                     /* Total number of leeted characters */
    uint8_t  Leeted[sizeof L33TChr]; /* Number of leeted chars for each char found in L33TChr */
    uint8_t  UnLeet[sizeof L33TChr]; /* Number of normal chars for each char found in L33TChr */
} DictMatchInfo_t;

/* Struct holding working data for the word match */
typedef struct
{
    uint32_t StartLoc;
    int     Ordinal;
    int     PwdLength;
    int     Begin;
    int     Caps;
    int     Lower;
    int     NumPossChrs;
    uint8_t Leeted[sizeof L33TChr];
    uint8_t UnLeet[sizeof L33TChr];
    uint8_t LeetCnv[sizeof L33TCnv / LEET_NORM_MAP_SIZE + 1];
    uint8_t First;
    uint8_t PossChars[CHARSET_SIZE];
} DictWork_t;

/**********************************************************************************
 * Given a map entry create a string of all possible characters for following to
 * a child node
 */
static int ListPossibleChars(uint8_t *List, const uint8_t *Map)
{
    unsigned int i, j, k;
    int Len = 0;
    for(k = i = 0; i < SizeChildMapEntry; ++i, ++Map)
    {
        if (!*Map)
        {
            k += 8;
            continue;
        }
        for(j = 0; j < 8; ++j)
        {
            if (*Map & (1<<j))
            {
                *List++ = CharSet[k];
                ++Len;
            }
            ++k;
        }
    }
    *List=0;
    return Len;
}

/**********************************************************************************
 * Increment count of each char that could be leeted.
 */
static void AddLeetChr(uint8_t c, int IsLeet, uint8_t *Leeted, uint8_t *UnLeet)
{
    const uint8_t *p = CharBinSearch(c, L33TChr, sizeof L33TChr - 1, 1);
    if (p)
    {
        int i = p - L33TChr;
        if (IsLeet > 0)
        {
            Leeted[i] += 1;
        }
        else if (IsLeet < 0)
        {
            Leeted[i] += 1;
            UnLeet[i] -= 1;
        }
        else
        {
            UnLeet[i] += 1;
        }
    }
}

/**********************************************************************************
 * Given details of a word match, update it with the entropy (as natural log of
 * number of possiblities)
 */
static void DictionaryEntropy(ZxcMatch_t *m, DictMatchInfo_t *Extra, const uint8_t *Pwd)
{
    double e = 0.0;
    /* Add allowance for uppercase letters */
    if (Extra->Caps)
    {
        if (Extra->Caps == m->Length)
        {
            /* All uppercase, common case so only 1 bit */
            e += log(2.0);
        }
        else if ((Extra->Caps == 1) && (isupper(*Pwd) || isupper(Pwd[m->Length - 1])))
        {
            /* Only first or last uppercase, also common so only 1 bit */
            e += log(2.0);
        }
        else
        {
            /* Get number of combinations of lowercase, uppercase letters */
            int Up = Extra->Caps;
            int Lo = Extra->Lower;
            int i = Up;
            if (i > Lo)
                i = Lo;
            for(Lo += Up; i >= 0; --i)
                e += nCk(Lo, i);
            if (e > 0.0)
                e = log(e);
        }
    }
    /* Add allowance for using leet substitution */
    if (Extra->NumLeet)
    {
        int i;
        double d = 0.0;
        for(i = sizeof Extra->Leeted - 1; i >= 0; --i)
        {
            int Sb = Extra->Leeted[i];
            if (Sb)
            {
                int Un = Extra->UnLeet[i];
                int j = m->Length - Extra->NumLeet;
                if ((j >= 0) && (Un > j))
                    Un = j;
                j = Sb;
                if (j > Un)
                    j = Un;
                for(Un += Sb; j >= 0; --j)
                {
                    double z = nCk(Un, j);
                    d += z;
                }
            }
        }
        if (d > 0.0)
            d = log(d);
        if (d < log(2.0))
            d = log(2.0);
        e += d;
    }
    /* Add entropy due to word's rank */
    e += log((double)Extra->Rank);
    m->Entrpy = e;
}

/**********************************************************************************
 * Function that does the word matching
 */
static void DoDictMatch(const uint8_t *Passwd, int Start, int MaxLen, DictWork_t *Wrk, ZxcMatch_t **Result, DictMatchInfo_t *Extra, int Lev)
{
    int Len;
    uint8_t TempLeet[LEET_NORM_MAP_SIZE];
    int Ord = Wrk->Ordinal;
    int Caps = Wrk->Caps;
    int Lower = Wrk->Lower;
    unsigned int NodeLoc = Wrk->StartLoc;
    uint8_t *PossChars = Wrk->PossChars;
    int NumPossChrs = Wrk->NumPossChrs;
    const uint8_t *Pwd = Passwd;
    uint32_t NodeData = DictNodes[NodeLoc];
    Passwd += Start;
    for(Len = 0; *Passwd && (Len < MaxLen); ++Len, ++Passwd)
    {
        uint8_t c;
        int w, x, y, z;
        const uint8_t *q;
        z = 0;
        if (!Len && Wrk->First)
        {
            c = Wrk->First;
        }
        else
        {
            /* Get char and set of possible chars at current point in word. */
            const uint8_t *Bmap;
            c = *Passwd;
            Bmap = ChildMap + (NodeData & ((1<<BITS_CHILD_PATT_INDEX)-1)) * SizeChildMapEntry;
            NumPossChrs = ListPossibleChars(PossChars, Bmap);

            /* Make it lowercase and update lowercase, uppercase counts */
            if (isupper(c))
            {
                c = tolower(c);
                ++Caps;
            }
            else if (islower(c))
            {
                ++Lower;
            }
            /* See if current char is a leet and can be converted  */
            q = CharBinSearch(c, L33TCnv, sizeof L33TCnv / LEET_NORM_MAP_SIZE, LEET_NORM_MAP_SIZE);
            if (q)
            {
                /* Found, see if used before */
                unsigned int j;
                unsigned int i = (q - L33TCnv ) / LEET_NORM_MAP_SIZE;
                if (Wrk->LeetCnv[i])
                {
                    /* Used before, so limit characters to try */
                    TempLeet[0] = c;
                    TempLeet[1] = Wrk->LeetCnv[i];
                    TempLeet[2] = 0;
                    q = TempLeet;
                }
                for(j = 0; (*q > ' ') && (j < LEET_NORM_MAP_SIZE); ++j, ++q)
                {
                    const uint8_t *r = CharBinSearch(*q, PossChars, NumPossChrs, 1);
                    if (r)
                    {
                        /* valid conversion from leet */
                        DictWork_t w;
                        w = *Wrk;
                        w.StartLoc = NodeLoc;
                        w.Ordinal = Ord;
                        w.PwdLength += Len;
                        w.Caps = Caps;
                        w.Lower = Lower;
                        w.First = *r;
                        w.NumPossChrs = NumPossChrs;
                        memcpy(w.PossChars, PossChars, sizeof w.PossChars);
                        if (j)
                        {
                            w.LeetCnv[i] = *r;
                            AddLeetChr(*r, -1, w.Leeted, w.UnLeet);
                        }
                        DoDictMatch(Pwd, Passwd - Pwd, MaxLen - Len, &w, Result, Extra, Lev+1);
                    }
                }
                return;
            }
        }
        q = CharBinSearch(c, PossChars, NumPossChrs, 1);
        if (q)
        {
            /* Found the char as a normal char */
            if (CharBinSearch(c, L33TChr, sizeof L33TChr - 1, 1))
            {
                /* Char matches, but also a normal equivalent to a leet char */
                AddLeetChr(c, 0,  Wrk->Leeted, Wrk->UnLeet);
            }
        }
        if (!q)
        {
            /* No match for char - return */
            return;
        }
        /* Add all the end counts of the child nodes before the one that matches */
        x = (q - Wrk->PossChars);
        y = (NodeData >> BITS_CHILD_PATT_INDEX) & ((1 << BITS_CHILD_MAP_INDEX) - 1);
        NodeLoc = ChildLocs[x+y];
        for(w=0; w<x; ++w)
        {
            unsigned int Cloc = ChildLocs[w+y];
            z = EndCountSml[Cloc];
            if (Cloc < NumLargeCounts)
                z += EndCountLge[Cloc]*256;
            Ord += z;
        }

        /* Move to next node */
        NodeData = DictNodes[NodeLoc];
        if (WordEndBits[NodeLoc >> 3] & (1<<(NodeLoc & 7)))
        {
            /* Word matches, save result */
            unsigned int v;
            ZxcMatch_t *p;
            v = Ranks[Ord];
            if (v & (1<<15))
                v = (v & ((1 << 15) - 1)) * 4 + (1 << 15);
            Extra->Caps = Caps;
            Extra->Rank = v;
            Extra->Lower = Lower;
            for(x = 0, y = sizeof Extra->Leeted - 1; y >= 0; --y)
                x += Wrk->Leeted[y];
            Extra->NumLeet = x;

            memcpy(Extra->UnLeet, Wrk->UnLeet, sizeof Extra->UnLeet);
            memcpy(Extra->Leeted, Wrk->Leeted, sizeof Extra->Leeted);

            p = AllocMatch();
            if (x)
                p->Type = DICT_LEET_MATCH;
            else
                p->Type = DICTIONARY_MATCH;
            p->Length = Wrk->PwdLength + Len + 1;
            p->Begin = Wrk->Begin;
            DictionaryEntropy(p, Extra, Pwd);
            AddMatchRepeats(Result, p, Pwd, MaxLen);
            AddResult(Result, p, MaxLen);
            ++Ord;
        }
    }
}

/**********************************************************************************
 * Try to match password part with user supplied dictionary words
 * Parameters:
 *  Result  Pointer head of linked list used to store results
 *  Words   Array of pointers to dictionary words
 *  Passwd  The start of the password
 *  Start   Where in the password to start attempting to match
 *  MaxLen  Maximum number characters to consider
 */
static void UserMatch(ZxcMatch_t **Result, const char *Words[], const uint8_t *Passwd, int Start, int MaxLen)
{
    int Rank;
    if (!Words)
        return;
    Passwd += Start;
    for(Rank = 0; Words[Rank]; ++Rank)
    {
        DictMatchInfo_t Extra;
        uint8_t LeetChr[sizeof L33TCnv / LEET_NORM_MAP_SIZE + 1];
        uint8_t TempLeet[3];
        int Len = 0;
        int Caps = 0;
        int Lowers = 0;
        int Leets = 0;
        const uint8_t *Wrd = (const uint8_t *)(Words[Rank]);
        const uint8_t *Pwd = Passwd;
        memset(Extra.Leeted, 0, sizeof Extra.Leeted);
        memset(Extra.UnLeet, 0, sizeof Extra.UnLeet);
        memset(LeetChr, 0, sizeof LeetChr);
        while(*Wrd)
        {
            const uint8_t *q;
            uint8_t d = tolower(*Wrd++);
            uint8_t c = *Pwd++;
            if (isupper(c))
            {
                c = tolower(c);
                ++Caps;
            }
            else if (islower(c))
            {
                ++Lowers;
            }
            /* See if current char is a leet and can be converted  */
            q = CharBinSearch(c, L33TCnv, sizeof L33TCnv / LEET_NORM_MAP_SIZE, LEET_NORM_MAP_SIZE);
            if (q)
            {
                /* Found, see if used before */
                unsigned int j;
                unsigned int i = (q - L33TCnv ) / LEET_NORM_MAP_SIZE;
                if (LeetChr[i])
                {
                    /* Used before, so limit characters to try */
                    TempLeet[0] = c;
                    TempLeet[1] = LeetChr[i];
                    TempLeet[2] = 0;
                    q = TempLeet;
                }
                c = d+1;
                for(j = 0; (*q > ' ') && (j < LEET_NORM_MAP_SIZE); ++j, ++q)
                {
                    if (d == *q)
                    {
                        c = d;
                        if (i)
                        {
                            LeetChr[i] = c;
                            AddLeetChr(c, 1, Extra.Leeted, Extra.UnLeet);
                            ++Leets;
                        }
                        break;
                    }
                }
                if (c != d)
                {
                    Len = 0;
                    break;
                }
            }
            else if (c == d)
            {
                /* Found the char as a normal char */
                if (CharBinSearch(c, L33TChr, sizeof L33TChr - 1, 1))
                {
                    /* Char matches, but also a normal equivalent to a leet char */
                    AddLeetChr(c, 0,  Extra.Leeted, Extra.UnLeet);
                }
            }
            else
            {
                /* No Match */
                Len = 0;
                break;
            }
            if (++Len > MaxLen)
            {
                Len = 0;
                break;
            }
        }
        if (Len)
        {
            ZxcMatch_t *p = AllocMatch();
            if (!Leets)
                p->Type = USER_MATCH;
            else
                p->Type = USER_LEET_MATCH;
            p->Length = Len;
            p->Begin = Start;
            /* Add Entrpy */
            Extra.Caps = Caps;
            Extra.Lower = Lowers;
            Extra.NumLeet = Leets;
            Extra.Rank = Rank+1;
            DictionaryEntropy(p, &Extra, Passwd);
            AddMatchRepeats(Result, p, Passwd, MaxLen);
            AddResult(Result, p, MaxLen);
        }
    }
}

/**********************************************************************************
 * Try to match password part with the dictionary words
 * Parameters:
 *  Result  Pointer head of linked list used to store results
 *  Passwd  The start of the password
 *  Start   Where in the password to start attempting to match
 *  MaxLen  Maximum number characters to consider
 */
static void DictionaryMatch(ZxcMatch_t **Result, const uint8_t *Passwd, int Start, int MaxLen)
{
    DictWork_t Wrk;
    DictMatchInfo_t Extra;

    memset(&Extra, 0, sizeof Extra);
    memset(&Wrk, 0, sizeof Wrk);
    Wrk.Ordinal = 1;
    Wrk.StartLoc = ROOT_NODE_LOC;
    Wrk.Begin = Start;
    DoDictMatch(Passwd+Start, 0, MaxLen, &Wrk, Result, &Extra, 0);
}


/*################################################################################*
 *################################################################################*
 * Begin keyboard spatial sequence matching code
 *################################################################################*
 *################################################################################*/

/* Struct to hold information on a keyboard layout */
typedef struct Keyboard
{
    const uint8_t *Keys;
    const uint8_t *Shifts;
    int NumKeys;
    int NumNear;
    int NumShift;
    int NumBlank;
} Keyboard_t;

/* Struct to hold information on the match */
typedef struct
{
    int Keyb;
    int Turns;
    int Shifts;
} SpatialMatchInfo_t;

/* Shift mapping, characters in pairs: first is shifted, second un-shifted. */
static const uint8_t UK_Shift[] = "!1\"2$4%5&7(9)0*8:;<,>.?/@'AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz^6_-{[|\\}]~#�4�3�`";
static const uint8_t US_Shift[] = "!1\"'#3$4%5&7(9)0*8:;<,>.?/@2AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz^6_-{[|\\}]~`";


/* Neighbour tables */
static const uint8_t UK_Qwerty[48*7] =
{
/* key, left, up-left, up-right, right, down-right, down-left */
    '#', '\'',']',   0,   0,   0,   0,    '\'',';', '[', ']', '#',   0, '/',
    ',', 'm', 'k', 'l', '.',   0,   0,    '-', '0',   0,   0, '=', '[', 'p',
    '.', ',', 'l', ';', '/',   0,   0,    '/', '.', ';', '\'',  0,   0,   0, 
    '0', '9',   0,   0, '-', 'p', 'o',    '1', '`',   0,   0, '2', 'q',   0, 
    '2', '1',   0,   0, '3', 'w', 'q',    '3', '2',   0,   0, '4', 'e', 'w',
    '4', '3',   0,   0, '5', 'r', 'e',    '5', '4',   0,   0, '6', 't', 'r',
    '6', '5',   0,   0, '7', 'y', 't',    '7', '6',   0,   0, '8', 'u', 'y',
    '8', '7',   0,   0, '9', 'i', 'u',    '9', '8',   0,   0, '0', 'o', 'i',
    ';', 'l', 'p', '[','\'', '/', '.',    '=', '-',   0,   0,   0, ']', '[',
    '[', 'p', '-', '=', ']', '\'',';',    '\\',  0,   0, 'a', 'z',   0,   0,
    ']', '[', '=',   0,   0, '#','\'',    '`',   0,   0,   0, '1',   0,   0,
    'a',   0, 'q', 'w', 's', 'z','\\',    'b', 'v', 'g', 'h', 'n',   0,   0,
    'c', 'x', 'd', 'f', 'v',   0,   0,    'd', 's', 'e', 'r', 'f', 'c', 'x',
    'e', 'w', '3', '4', 'r', 'd', 's',    'f', 'd', 'r', 't', 'g', 'v', 'c',
    'g', 'f', 't', 'y', 'h', 'b', 'v',    'h', 'g', 'y', 'u', 'j', 'n', 'b',
    'i', 'u', '8', '9', 'o', 'k', 'j',    'j', 'h', 'u', 'i', 'k', 'm', 'n',
    'k', 'j', 'i', 'o', 'l', ',', 'm',    'l', 'k', 'o', 'p', ';', '.', ',',
    'm', 'n', 'j', 'k', ',',   0,   0,    'n', 'b', 'h', 'j', 'm',   0,   0,
    'o', 'i', '9', '0', 'p', 'l', 'k',    'p', 'o', '0', '-', '[', ';', 'l',
    'q',   0, '1', '2', 'w', 'a',   0,    'r', 'e', '4', '5', 't', 'f', 'd',
    's', 'a', 'w', 'e', 'd', 'x', 'z',    't', 'r', '5', '6', 'y', 'g', 'f',
    'u', 'y', '7', '8', 'i', 'j', 'h',    'v', 'c', 'f', 'g', 'b',   0,   0,
    'w', 'q', '2', '3', 'e', 's', 'a',    'x', 'z', 's', 'd', 'c',   0,   0,
    'y', 't', '6', '7', 'u', 'h', 'g',    'z', '\\','a', 's', 'x',   0,   0
};

static const uint8_t US_Qwerty[47*7] =
{
/* key, left, up-left, up-right, right, down-right, down-left */
    '\'',';', '[', ']',   0,   0, '/',    ',', 'm', 'k', 'l', '.',   0,   0,
    '-', '0',   0,   0, '=', '[', 'p',    '.', ',', 'l', ';', '/',   0,   0,
    '/', '.', ';','\'',   0,   0,   0,    '0', '9',   0,   0, '-', 'p', 'o',
    '1', '`',   0,   0, '2', 'q',   0,    '2', '1',   0,   0, '3', 'w', 'q',
    '3', '2',   0,   0, '4', 'e', 'w',    '4', '3',   0,   0, '5', 'r', 'e',
    '5', '4',   0,   0, '6', 't', 'r',    '6', '5',   0,   0, '7', 'y', 't',
    '7', '6',   0,   0, '8', 'u', 'y',    '8', '7',   0,   0, '9', 'i', 'u',
    '9', '8',   0,   0, '0', 'o', 'i',    ';', 'l', 'p', '[','\'', '/', '.',
    '=', '-',   0,   0,   0, ']', '[',    '[', 'p', '-', '=', ']','\'', ';',
    '\\',']',   0,   0,   0,   0,   0,    ']', '[', '=',   0,'\\',   0,'\'',
    '`',   0,   0,   0, '1',   0,   0,    'a',   0, 'q', 'w', 's', 'z',   0,
    'b', 'v', 'g', 'h', 'n',   0,   0,    'c', 'x', 'd', 'f', 'v',   0,   0,
    'd', 's', 'e', 'r', 'f', 'c', 'x',    'e', 'w', '3', '4', 'r', 'd', 's',
    'f', 'd', 'r', 't', 'g', 'v', 'c',    'g', 'f', 't', 'y', 'h', 'b', 'v',
    'h', 'g', 'y', 'u', 'j', 'n', 'b',    'i', 'u', '8', '9', 'o', 'k', 'j',
    'j', 'h', 'u', 'i', 'k', 'm', 'n',    'k', 'j', 'i', 'o', 'l', ',', 'm',
    'l', 'k', 'o', 'p', ';', '.', ',',    'm', 'n', 'j', 'k', ',',   0,   0,
    'n', 'b', 'h', 'j', 'm',   0,   0,    'o', 'i', '9', '0', 'p', 'l', 'k',
    'p', 'o', '0', '-', '[', ';', 'l',    'q',   0, '1', '2', 'w', 'a',   0,
    'r', 'e', '4', '5', 't', 'f', 'd',    's', 'a', 'w', 'e', 'd', 'x', 'z',
    't', 'r', '5', '6', 'y', 'g', 'f',    'u', 'y', '7', '8', 'i', 'j', 'h',
    'v', 'c', 'f', 'g', 'b',   0,   0,    'w', 'q', '2', '3', 'e', 's', 'a',
    'x', 'z', 's', 'd', 'c',   0,   0,    'y', 't', '6', '7', 'u', 'h', 'g',
    'z',   0, 'a', 's', 'x',   0,   0,
};
static const uint8_t Dvorak[47*7] =
{
    '\'',  0, '1', '2', ',', 'a',   0,    ',','\'', '2', '3', '.', 'o', 'a',
    '-', 's', '/', '=',   0,   0, 'z',    '.', ',', '3', '4', 'p', 'e', 'o',
    '/', 'l', '[', ']', '=', '-', 's',    '0', '9',   0,   0, '[', 'l', 'r',
    '1', '`',   0,   0, '2','\'',   0,    '2', '1',   0,   0, '3', ',','\'',
    '3', '2',   0,   0, '4', '.', ',',    '4', '3',   0,   0, '5', 'p', '.',
    '5', '4',   0,   0, '6', 'y', 'p',    '6', '5',   0,   0, '7', 'f', 'y',
    '7', '6',   0,   0, '8', 'g', 'f',    '8', '7',   0,   0, '9', 'c', 'g',
    '9', '8',   0,   0, '0', 'r', 'c',    ';',   0, 'a', 'o', 'q',   0,   0,
    '=', '/', ']',   0,'\\',   0, '-',    '[', '0',   0,   0, ']', '/', 'l',
    '\\','=',   0,   0,   0,   0,   0,    ']', '[',   0,   0,   0, '=', '/',
    '`',   0,   0,   0, '1',   0,   0,    'a',   0,'\'', ',', 'o', ';',   0,
    'b', 'x', 'd', 'h', 'm',   0,   0,    'c', 'g', '8', '9', 'r', 't', 'h',
    'd', 'i', 'f', 'g', 'h', 'b', 'x',    'e', 'o', '.', 'p', 'u', 'j', 'q',
    'f', 'y', '6', '7', 'g', 'd', 'i',    'g', 'f', '7', '8', 'c', 'h', 'd',
    'h', 'd', 'g', 'c', 't', 'm', 'b',    'i', 'u', 'y', 'f', 'd', 'x', 'k',
    'j', 'q', 'e', 'u', 'k',   0,   0,    'k', 'j', 'u', 'i', 'x',   0,   0,
    'l', 'r', '0', '[', '/', 's', 'n',    'm', 'b', 'h', 't', 'w',   0,   0,
    'n', 't', 'r', 'l', 's', 'v', 'w',    'o', 'a', ',', '.', 'e', 'q', ';',
    'p', '.', '4', '5', 'y', 'u', 'e',    'q', ';', 'o', 'e', 'j',   0,   0,
    'r', 'c', '9', '0', 'l', 'n', 't',    's', 'n', 'l', '/', '-', 'z', 'v',
    't', 'h', 'c', 'r', 'n', 'w', 'm',    'u', 'e', 'p', 'y', 'i', 'k', 'j',
    'v', 'w', 'n', 's', 'z',   0,   0,    'w', 'm', 't', 'n', 'v',   0,   0,
    'x', 'k', 'i', 'd', 'b',   0,   0,    'y', 'p', '5', '6', 'f', 'i', 'u',
    'z', 'v', 's', '-',   0,   0,   0
};

static const uint8_t PC_Keypad[15*9] =
{
/*Key, left, up-left, up, up-right, right, down-right, down, down-left */
    '*', '/',   0,   0,   0, '-', '+', '9', '8',
    '+', '9', '*', '-',   0,   0,   0,   0, '6',
    '-', '*',   0,   0,   0,   0,   0, '+', '9',
    '.', '0', '2', '3',   0,   0,   0,   0,   0,
    '/',   0,   0,   0,   0, '*', '9', '8', '7',
    '0',   0, '1', '2', '3', '.',   0,   0,   0,
    '1',   0,   0, '4', '5', '2', '0',   0,   0,
    '2', '1', '4', '5', '6', '3', '.', '0',   0,
    '3', '2', '5', '6',   0,   0,   0, '.', '0',
    '4',   0,   0, '7', '8', '5', '2', '1',   0,
    '5', '4', '7', '8', '9', '6', '3', '2', '1',
    '6', '5', '8', '9', '+',   0,   0, '3', '2',
    '7',   0,   0,   0, '/', '8', '5', '4',   0,
    '8', '7',   0, '/', '*', '9', '6', '5', '4',
    '9', '8', '/', '*', '-', '+',   0, '6', '5'
};

static const uint8_t MacKeypad[16*9] =
{
    '*', '/',   0,   0,   0,   0,   0, '-', '9',
    '+', '6', '9', '-',   0,   0,   0,   0, '3',
    '-', '9', '/', '*',   0,   0,   0, '+', '6',
    '.', '0', '2', '3',   0,   0,   0,   0,   0,
    '/', '=',   0,   0,   0, '*', '-', '9', '8',
    '0',   0, '1', '2', '3', '.',   0,   0,   0,
    '1',   0,   0, '4', '5', '2', '0',   0,   0,
    '2', '1', '4', '5', '6', '3', '.', '0',   0,
    '3', '2', '5', '6', '+',   0,   0, '.', '0',
    '4',   0,   0, '7', '8', '5', '2', '1',   0,
    '5', '4', '7', '8', '9', '6', '3', '2', '1',
    '6', '5', '8', '9', '-', '+',   0, '3', '2',
    '7',   0,   0,   0, '=', '8', '5', '4',   0,
    '8', '7',   0, '=', '/', '9', '6', '5', '4',
    '9', '8', '=', '/', '*', '-', '+', '6', '5',
    '=',   0,   0,   0,   0, '/', '9', '8', '7'
};

static const Keyboard_t Keyboards[] =
{
    { US_Qwerty, US_Shift, sizeof US_Qwerty / 7, 7, sizeof US_Shift / 2, 66 },
    { Dvorak,    US_Shift, sizeof Dvorak / 7,    7, sizeof US_Shift / 2, 66 },
    { UK_Qwerty, UK_Shift, sizeof UK_Qwerty / 7, 7, sizeof UK_Shift / 2, 66 },
    { MacKeypad, 0, sizeof MacKeypad / 9, 9, 0, 44 },
    { PC_Keypad, 0, sizeof PC_Keypad / 9, 9, 0, 44 }
};

/**********************************************************************************
 * Match password for the given keyboard layout
 */
static int DoSptlMatch(const uint8_t *Passwd, int MaxLen, const Keyboard_t *Keyb, SpatialMatchInfo_t *Extra)
{
    int i;
    int ShiftCount = 0;
    int Turns = 0;
    int Dir = -1;
    int Len = 0;
    uint8_t PrevChar = 0;
    for( ; *Passwd && (Len < MaxLen); ++Passwd, ++Len)
    {
        const uint8_t *Key;
        int s = 0;
        uint8_t CurChar = *Passwd;
        /* Try to unshift the character */
        if (Keyb->Shifts)
        {
            Key = CharBinSearch(CurChar, Keyb->Shifts, Keyb->NumShift, 2);
            if (Key)
            {
                /* Shifted char */
                CurChar = Key[1];
                s = 1;
            }
        }
        if (PrevChar)
        {
            /* See if the pattern can be extended */
            i = 0;
            Key = CharBinSearch(PrevChar, Keyb->Keys, Keyb->NumKeys, Keyb->NumNear);
            if (Key)
            {
                for(i = Keyb->NumNear - 1; i > 0; --i)
                {
                    if (Key[i] == CurChar)
                        break;
                }
            }
            if (i)
            {
                Turns += (i != Dir);
                Dir = i;
                ShiftCount += s;
            }
            else
            {
                break;
            }
        }
        PrevChar = CurChar;
    }
    if (Len >= MIN_SPATIAL_LEN)
    {
        Extra->Turns = Turns;
        Extra->Shifts = ShiftCount;
        return Len;
    }
    return 0;
}

/**********************************************************************************
 *  Try to match spatial patterns on the keyboard
 * Parameters:
 *  Result  Pointer head of linked list used to store results
 *  Passwd  The start of the password
 *  Start   Where in the password to start attempting to match
 *  MaxLen  Maximum number characters to consider
 */
static void SpatialMatch(ZxcMatch_t **Result, const uint8_t *Passwd, int Start, int MaxLen)
{
    unsigned int Indx;
    int Len, CurLen;
    SpatialMatchInfo_t Extra;
    const Keyboard_t *k;
    Passwd += Start;

    for(CurLen = MaxLen; CurLen >= MIN_SPATIAL_LEN;CurLen = Len - 1)
    {
        Len = 0;
        for(k = Keyboards, Indx = 0; Indx < (sizeof Keyboards / sizeof Keyboards[0]); ++Indx, ++k)
        {
            memset(&Extra, 0, sizeof Extra);
            Len = DoSptlMatch(Passwd, CurLen, k, &Extra);
            if (Len > 0)
            {
                /* Got a sequence of required length so add to result list */
                int i, j, s;
                double Degree, Entropy;
                ZxcMatch_t *p;
                Degree = (k->NumNear-1) - (double)k->NumBlank / (double)k->NumKeys;
                s = k->NumKeys;
                if (k->Shifts)
                    s *= 2;

                /* Estimate the number of possible patterns with length ranging 2 to match length and */
                /* with turns ranging from 0 to match turns */
                Entropy = 0.0;
                for(i = 2; i <= Len; ++i)
                {
                    int PossTurns = Extra.Turns;
                    if (PossTurns >= i)
                        PossTurns = i-1;
                    for(j = 1; j <= PossTurns; ++j)
                        Entropy += nCk(i-1, j-1) * pow(Degree, j) * s;
                }
                if (Entropy > 0.0)
                    Entropy = log(Entropy);
                if (Extra.Shifts)
                {
                    /* Add extra entropy for shifted keys. (% instead of 5, A instead of a etc.) */
                    /* Math is similar to extra entropy from uppercase letters in dictionary matches. */
                    int Shift = Extra.Shifts;
                    int Unshift = Len - Shift;

                    Degree = 0.0;
                    j = Shift;
                    if (j > Unshift)
                        j = Unshift;
                    for(i = 0; i <= j; ++i)
                    {
                        Degree += nCk(Len, i);
                    }
                    if (Degree > 0.0)
                        Entropy += log(Degree);
                }
                p = AllocMatch();
                p->Type = SPATIAL_MATCH;
                p->Begin = Start;
                p->Entrpy = Entropy;
                p->Length = Len;
                AddMatchRepeats(Result, p, Passwd, MaxLen);
                AddResult(Result, p, MaxLen);
            }
        }
    }
}


/*################################################################################*
 *################################################################################*
 * Begin date matching code
 *################################################################################*
 *################################################################################*/

/* The possible date formats ordered by length (d for day, m for month, */
/*  y for year, ? for separator) */
static const char *Formats[] =
{
    "yyyy",
    "d?m?yy",
    "ddmmyy",
    "dmyyyy",
    "dd?m?yy",
    "d?mm?yy",
    "ddmyyyy",
    "dmmyyyy",
    "yyyymmd",
    "yyyymdd",
    "d?m?yyyy",
    "dd?mm?yy",
    "ddmmyyyy",
    "yyyy?m?d",
    "yyyymmdd",
    "dd?m?yyyy",
    "d?mm?yyyy",
    "yyyy?mm?d",
    "yyyy?m?dd",
    "dd?mm?yyyy",
    "yyyy?mm?dd",
    0
};
/* Possible separator characters that could be used */
static const char DateSeperators[] = "/\\-_. ";

/**********************************************************************************
 * Try to match the password with the formats above.
 */
static void DateMatch(ZxcMatch_t **Result, const uint8_t *Passwd, int Start, int MaxLen)
{
    int CurFmt;
    int YrLen = 0;
    int PrevLen = 0;
    uint8_t Sep = 0;
    Passwd += Start;

    for(CurFmt = 0; Formats[CurFmt]; ++CurFmt)
    {
        int Len = 0;
        int Year = 0;
        int Mon  = 0;
        int Day  = 0;
        int Fail = 0;
        const uint8_t *p = Passwd;
        const char *Fmt;
        YrLen = 0;
        Sep = 0;
        /* Scan along the format, trying to match the password */
        for(Fmt = Formats[CurFmt]; *Fmt && !Fail; ++Fmt)
        {
            if (*Fmt == '?')
            {
                if (!Sep && strchr(DateSeperators, *p))
                        Sep = *p;
                Fail = (*p != Sep);
            }
            else if (isdigit(*p))
            {
                if (*Fmt == 'd')
                {
                    Day = 10 * Day + *p - '0';
                }
                else if (*Fmt == 'm')
                {
                    Mon = 10 * Mon + *p - '0';
                }
                else
                {
                    Year = 10 * Year + *p - '0';
                    ++YrLen;
                }
            }
            else
            {
                Fail = 1;
            }
            ++p;
            ++Len;
            if (Len >= MaxLen)
                break;
        }
        if (Len < 4)
            Fail = 1;
        if (!Fail)
        {
            /* Character matching is OK, now check to see if valid date */
            if (((YrLen > 3) || (Len <= 4)) && ((Year < MIN_YEAR) || (Year > MAX_YEAR)))
                Fail = 1;
            else if (Len > 4)
            {
                if ((Mon > 12) && (Day < 13))
                {
                    /* Swap day,month to try to make both in range */
                    int i = Mon;
                    Mon = Day;
                    Day = i;
                }
                /* Check for valid day, month. Currently assumes all months have 31 days. */
                if ((Mon < 1) || (Mon > 12))
                    Fail = 1;
                else if ((Day < 1) || (Day > 31))
                    Fail = 1;
            }
        }
        if (!Fail && (Len > PrevLen))
        {
            /* String matched the date, store result */
            double e;
            ZxcMatch_t *p = AllocMatch();

            if (Len <= 4)
                e = log(MAX_YEAR - MIN_YEAR + 1.0);
            else if (YrLen > 3)
                e = log(31 * 12 * (MAX_YEAR - MIN_YEAR + 1.0));
            else
                e = log(31 * 12 * 100.0);
            if (Sep)
                e += log(4.0);  /* Extra 2 bits for separator */
            p->Entrpy = e;
            p->Type = DATE_MATCH;
            p->Length = Len;
            p->Begin = Start;
            AddMatchRepeats(Result, p, Passwd, MaxLen);
            AddResult(Result, p, MaxLen);
            PrevLen = Len;
        }
    }
}


/*################################################################################*
 *################################################################################*
 * Begin repeated character matching code
 *################################################################################*
 *################################################################################*/

/**********************************************************************************
 * Try to match password part as a set of repeated characters.
 * Parameters:
 *  Result  Pointer head of linked list used to store results
 *  Passwd  The start of the password
 *  Start   Where in the password to start attempting to match
 *  MaxLen  Maximum number characters to consider
 */
static void RepeatMatch(ZxcMatch_t **Result, const uint8_t *Passwd, int Start, int MaxLen)
{
    int Len, i;
    uint8_t c;
    Passwd += Start;
    /* Remember first char and the count its occurances */
    c = *Passwd;
    for(Len = 1; (Len < MaxLen) && (c == Passwd[Len]); ++Len)
    { }
    if (Len >= MIN_REPEAT_LEN)
    {
        /* Enough repeated char, so create results from number found down to min acceptable repeats */
        double Card = Cardinality(&c, 1);
        for(i = Len; i >= MIN_REPEAT_LEN; --i)
        {
            ZxcMatch_t *p = AllocMatch();
            p->Type = REPEATS_MATCH;
            p->Begin = Start;
            p->Length = i;
            p->Entrpy = log(Card * i);
            AddResult(Result, p, MaxLen);
        }
    }

    /* Try to match a repeated sequence e.g. qxno6qxno6 */
    for(Len = MaxLen/2; Len >= MIN_REPEAT_LEN; --Len)
    {
        const uint8_t *Rpt = Passwd + Len;
        int RepeatCount = 2;
        while(MaxLen >= (Len * RepeatCount))
        {
            if (strncmp((const char *)Passwd, (const char *)Rpt, Len) == 0)
            {
                /* Found a repeat */
                int c = Cardinality(Passwd, Len);
                ZxcMatch_t *p = AllocMatch();
                p->Entrpy = log((double)c) * Len + log(RepeatCount);
                p->Type = (ZxcTypeMatch_t)(BRUTE_MATCH + MULTIPLE_MATCH);
                p->Length = Len * RepeatCount;
                p->Begin = Start;
                AddResult(Result, p, MaxLen);
            }
            else
                break;
            ++RepeatCount;
            Rpt += Len;
        }
    }
}

/**********************************************************************************
 **********************************************************************************
 * Begin character sequence matching code
 **********************************************************************************
 *********************************************************************************/

#define MAX_SEQUENCE_STEP 5
/**********************************************************************************
 * Try to match password part as a set of incrementing or decrementing characters.
 * Parameters:
 *  Result  Pointer head of linked list used to store results
 *  Passwd  The start of the password
 *  Start   Where in the password to start attempting to match
 *  MaxLen  Maximum number characters to consider
 */
static void SequenceMatch(ZxcMatch_t **Result, const uint8_t *Passwd, int Start, int MaxLen)
{
    int Len=0;
    int SetLow, SetHigh, Dir;
    uint8_t First, Next, IsDigits;
    const uint8_t *Pwd;
    Passwd += Start;
    Pwd = Passwd;
    First = Passwd[0];
    Dir = Passwd[1] - First;
    Len = 0;
    IsDigits = 0;
    /* Decide on min and max character code for sequence */
    if (islower(*Passwd))
    {
        SetLow = 'a';
        SetHigh = 'z';
    }
    else if (isupper(*Passwd))
    {
        SetLow = 'A';
        SetHigh = 'Z';
    }
    else if (isdigit(*Passwd))
    {
        SetLow = '0';
        SetHigh = '9';
        if ((First == '0') && isdigit(Passwd[1]) && (Dir > MAX_SEQUENCE_STEP))
        {
            /* Special case for decrementing sequence of digits, treat '0 as a 'ten' character */
            Dir = Passwd[1] - ('9' + 1);
        }
        IsDigits = 1;
    }
    else
        return;

    /* Only consider it a sequence if the character increment is not too large */
    if (Dir && (Dir <= MAX_SEQUENCE_STEP) && (Dir >= -MAX_SEQUENCE_STEP))
    {
        ++Len;
        while(1)
        {
            Next = Passwd[0] + Dir;
            if (IsDigits && (Dir > 0) && (Next == ('9' + 1)) && (Passwd[1] == '0'))
            {
                /* Incrementing digits, consider '0' to be same as a 'ten' character */ 
                ++Len;
                ++Passwd;
                break;
            }
            if (IsDigits && (Dir < 0) && (Passwd[0] == '0') && (Passwd[1] == ('9'+1 + Dir)))
            {
                ++Len;
                ++Passwd;
                break;
            }
            if ((Next > SetHigh) || (Next < SetLow) || (Passwd[1] != Next))
                break;
            ++Len;
            ++Passwd;
            if (Len >= MaxLen)
                break;
        }
    }
    if (Len >= MIN_SEQUENCE_LEN)
    {
        /* Enough repeated char, so create results from number found down to min acceptable length */
        int i;
        double e;
        if ((First == 'a') || (First == 'A') || (First == 'z') || (First == 'Z') ||
            (First == '0') || (First == '1') || (First == '9'))
            e = log(2.0);
        else if (IsDigits)
            e = log(10.0);
        else if (isupper(First))
            e = log(26*2.0);
        else
            e = log(26.0);
        if (Dir < 0)
            e += log(2.0);

        for(i = Len; i >= MIN_SEQUENCE_LEN; --i)
        {
            ZxcMatch_t *p = AllocMatch();
            /* Add new result to head of list as it has lower entropy */
            p->Type = SEQUENCE_MATCH;
            p->Begin = Start;
            p->Length = i;
            p->Entrpy = e + log((double)i);
            AddMatchRepeats(Result, p, Pwd, MaxLen);
            AddResult(Result, p, MaxLen);
        }
    }
}

/**********************************************************************************
 **********************************************************************************
 * Begin top level zxcvbn code
 **********************************************************************************
 *********************************************************************************/

/**********************************************************************************
 * Matching a password is treated as a problem of finding the minimum distance
 * between two vertexes in a graph. This is solved using Dijkstra's algorithm.
 *
 * There  are a series of nodes (or vertexes in graph terminology) which correspond
 * to points between each character of the password. Also there is a start node
 * before the first character and an end node after the last character.
 *
 * The paths between the nodes (or edges in graph terminology) correspond to the
 * matched parts of the password (e.g. dictionary word, repeated characters etc).
 * The distance of the path is equal to the entropy of the matched part. A default
 * single character bruteforce match path is also added for all nodes except the
 * end node.
 *
 * Dijkstra's algorithm finds the combination of these part matches (or paths)
 * which gives the lowest entropy (or smallest distance) from begining to end
 * of the password. 
 */

/* Struct to hold the data of a node (imaginary point between password characters) */
typedef struct
{
    ZxcMatch_t *Paths;  /* Partial matches that lead to a following node */
    double      Dist;   /* Distance (or entropy) from start of password to this node */
    ZxcMatch_t *From;   /* Which path was used to get to this node with lowest distance/entropy */
    int         Visit;  /* Non zero when node has been visited during Dijkstra evaluation */
} Node_t;

/**********************************************************************************
 * Main function of the zxcvbn password entropy estimation
 */
double ZxcvbnMatch(const char *Pwd, const char *UserDict[], ZxcMatch_t **Info)
{
    int i, j;
    ZxcMatch_t *Zp;
    Node_t *Np;
    double e;
    int Len = strlen(Pwd);
    const uint8_t *Passwd = (const uint8_t *)Pwd;
    uint8_t *RevPwd;
    /* Create the paths */
    Node_t *Nodes = MallocFn(Node_t, Len+1);
    memset(Nodes, 0, (Len+1) * sizeof *Nodes);
    i = Cardinality(Passwd, Len);
    e = log((double)i);

    /* Do matching for all parts of the password */
    for(i = 0; i < Len; ++i)
    {
        int MaxLen = Len - i;
        /* Add all the 'paths' between groups of chars in the password, for current starting char */
        UserMatch(&(Nodes[i].Paths), UserDict, Passwd, i, MaxLen);
        DictionaryMatch(&(Nodes[i].Paths), Passwd, i, MaxLen);
        DateMatch(&(Nodes[i].Paths), Passwd, i, MaxLen);
        SpatialMatch(&(Nodes[i].Paths), Passwd, i, MaxLen);
        SequenceMatch(&(Nodes[i].Paths), Passwd, i, MaxLen);
        RepeatMatch(&(Nodes[i].Paths), Passwd, i, MaxLen);

        /* Initially set distance to nearly infinite */
        Nodes[i].Dist = DBL_MAX;
    }

    /* Reverse dictionary words check */
    RevPwd = MallocFn(uint8_t, Len+1);
    for(i = Len-1, j = 0; i >= 0; --i, ++j)
        RevPwd[j] = Pwd[i];
    RevPwd[j] = 0;
    for(i = 0; i < Len; ++i)
    {
        ZxcMatch_t *Path = 0;
        int MaxLen = Len - i;
        DictionaryMatch(&Path, RevPwd, i, MaxLen);
        UserMatch(&Path, UserDict, RevPwd, i, MaxLen);

        /* Now transfer any reverse matches to the normal results */
        while(Path)
        {
            ZxcMatch_t *Nxt = Path->Next;
            Path->Next = 0;
            Path->Begin = Len - (Path->Begin + Path->Length);
            AddResult(&(Nodes[Path->Begin].Paths), Path, MaxLen);
            Path = Nxt;
        }
    }

    /* Add a set of brute force matches. Start by getting all the start points and all */
    /* points at character position after end of the matches.  */
    memset(RevPwd, 0, Len+1);
    for(i = 0; i < Len; ++i)
    {
        ZxcMatch_t *Path = Nodes[i].Paths;
        while(Path)
        {
            RevPwd[Path->Begin] |= 1;
            RevPwd[Path->Begin + Path->Length] |= 2;
            Path = Path->Next;
        }
    }
    RevPwd[0] = 1;
    RevPwd[Len] = 2;

    /* Add the brute force matches */
    for(i = 0; i < Len; ++i)
    {
        int MaxLen = Len - i;
        int j;
        if (!RevPwd[i])
            continue;
        for(j = i+1; j <= Len; ++j)
        {
            if (RevPwd[j])
            {
                Zp = AllocMatch();
                Zp->Type = BRUTE_MATCH;
                Zp->Begin = i;
                Zp->Length = j - i;
                Zp->Entrpy = e * (j - i);
                AddResult(&(Nodes[i].Paths), Zp, MaxLen);
            }
        }
    }
    FreeFn(RevPwd);
    /* End node has infinite distance/entropy, start node has 0 distance */
    Nodes[i].Dist = DBL_MAX;
    Nodes[0].Dist = 0.0;

    /* Reduce the paths using Dijkstra's algorithm */
    for(i = 0; i < Len; ++i)
    {
        int j;
        double MinDist = DBL_MAX;
        int MinIdx = 0;
        /* Find the unvisited node with minimum distance or entropy */
        for(Np = Nodes, j = 0; j < Len; ++j, ++Np)
        {
            if (!Np->Visit && (Np->Dist < MinDist))
            {
                MinIdx = j;
                MinDist = Np->Dist;
            }
        }
        /* Mark the minimum distance node as visited */
        Np = Nodes + MinIdx;
        Np->Visit = 1;
        e = Np->Dist;
        /* Update all unvisited neighbouring nodes with their new distance. A node is a */
        /* neighbour if there is a path/match from current node Np to it. The neighbour */
        /* distance is the current node distance plus the path distance/entropy. Only */
        /* update if the new distance is smaller. */
        for(Zp = Np->Paths; Zp; Zp = Zp->Next)
        {
            Node_t *Ep = Np + Zp->Length;
            double d = e + Zp->MltEnpy;
            if (!Ep->Visit &&  (d < Ep->Dist))
            {
                /* Update as lower dist, also remember the 'from' node */
                Ep->Dist = d;
                Ep->From = Zp;
            }
        }
        /* If we got to the end node stop early */
        /*if (Nodes[Len].Dist < DBL_MAX/2.0) */
          /*  break; */
    }
    /* Make e hold entropy result and adjust to log base 2 */
    e = Nodes[Len].Dist / log(2.0);

    if (Info)
    {
        /* Construct info on password parts */
        *Info = 0;
        for(Zp = Nodes[Len].From; Zp; )
        {
            ZxcMatch_t *Xp;
            i = Zp->Begin;

            /* Remove all but required path */
            Xp = Nodes[i].Paths;
            Nodes[i].Paths = 0;
            while(Xp)
            {
                ZxcMatch_t *p = Xp->Next;
                if (Xp == Zp)
                {
                    /* Adjust the entropy to log to base 2 */
                    Xp->Entrpy /= log(2.0);
                    Xp->MltEnpy /= log(2.0);

                    /* Put previous part at head of info list */
                    Xp->Next = *Info;
                    *Info = Xp;
                }
                else
                {
                    /* Not going on info list, so free it */
                    FreeFn(Xp);
                }
                Xp = p;
            }
            Zp = Nodes[i].From;
        }
    }
    /* Free all paths. Any being returned to caller have already been freed */
    for(i = 0; i <= Len; ++i)
    {
        Zp = Nodes[i].Paths;
        while(Zp)
        {
            ZxcMatch_t *p = Zp->Next;
            FreeFn(Zp);
            Zp = p;
        }
    }
    FreeFn(Nodes);
    return e;
}

/**********************************************************************************
 * Free the path info returned by ZxcvbnMatch().
 */
void ZxcvbnFreeInfo(ZxcMatch_t *Info)
{
    ZxcMatch_t *p;
    while(Info)
    {
        p = Info->Next;
        FreeFn(Info);
        Info = p;
    }
}
