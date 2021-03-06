// Copyright (C) 1985-1998 by Symantec
// Copyright (C) 2000-2009 by Digital Mars
// All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in /dmd/src/dmd/backendlicense.txt
 * or /dm/src/dmd/backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#if !SPP

#include        <stdio.h>
#include        <time.h>

#include        "cc.h"
#include        "oper.h"
#include        "global.h"
#include        "code.h"
#include        "type.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

/*********************************
 * Struct for each elem:
 *      Helem   pointer to elem
 *      Hhash   hash value for the elem
 */

typedef struct HCS
        {       elem    *Helem;
                unsigned Hhash;
        } hcs;

static hcs *hcstab = NULL;              /* array of hcs's               */
static unsigned hcsmax = 0;             /* max index into hcstab[]      */
static unsigned hcstop;                 /* # of entries in hcstab[]     */
static unsigned touchstari;
static unsigned touchfunci[2];

// Use a bit vector for quick check if expression is possibly in hcstab[].
// This results in much faster compiles when hcstab[] gets big.
static vec_t csvec;                     // vector of used entries
#define CSVECDIM        16001 //8009 //3001     // dimension of csvec (should be prime)

STATIC void ecom(elem **);
STATIC unsigned cs_comphash(elem *);
STATIC void addhcstab(elem *,int);
STATIC void touchlvalue(elem *);
STATIC void touchfunc(int);
STATIC void touchstar(void);
STATIC void touchaccess(elem *);

/*******************************
 * Eliminate common subexpressions across extended basic blocks.
 * String together as many blocks as we can.
 */

void comsubs()
{ register block *bl,*blc,*bln;
  register int n;                       /* # of blocks to treat as one  */

//static int xx;
//printf("comsubs() %d\n", ++xx);
//debugx = (xx == 37);

#ifdef DEBUG
  if (debugx) dbg_printf("comsubs(%p)\n",startblock);
#endif

  // No longer do we just compute Bcount. We now eliminate unreachable
  // blocks.
  block_compbcount();                   // eliminate unreachable blocks
#if SCPP
  if (errcnt)
        return;
#endif

  if (!csvec)
  {
        csvec = vec_calloc(CSVECDIM);
  }

  for (bl = startblock; bl; bl = bln)
  {
        bln = bl->Bnext;
        if (!bl->Belem)
                continue;       /* if no expression or no parents       */

        // Count up n, the number of blocks in this extended basic block (EBB)
        n = 1;                          // always at least one block in EBB
        blc = bl;
        while (bln && list_nitems(bln->Bpred) == 1 &&
                ((blc->BC == BCiftrue &&
                  list_block(list_next(blc->Bsucc)) == bln) ||
                 (blc->BC == BCgoto && list_block(blc->Bsucc) == bln)
                ) &&
               bln->BC != BCasm         // no CSE's extending across ASM blocks
              )
        {
                n++;                    // add block to EBB
                blc = bln;
                bln = blc->Bnext;
        }
        vec_clear(csvec);
        hcstop = 0;
        touchstari = 0;
        touchfunci[0] = 0;
        touchfunci[1] = 0;
        bln = bl;
        while (n--)                     // while more blocks in EBB
        {
#ifdef DEBUG
                if (debugx)
                        dbg_printf("cses for block %p\n",bln);
#endif
                if (bln->Belem)
                    ecom(&bln->Belem);  // do the tree
                bln = bln->Bnext;
        }
  }

#ifdef DEBUG
  if (debugx)
        dbg_printf("done with comsubs()\n");
#endif
}

/*******************************
 */

void cgcs_term()
{
    vec_free(csvec);
    csvec = NULL;
#ifdef DEBUG
    debugw && dbg_printf("freeing hcstab\n");
#endif
#if TX86
    util_free(hcstab);
#else
    MEM_PARF_FREE(hcstab);
#endif
    hcstab = NULL;
    hcsmax = 0;
}

/*************************
 * Eliminate common subexpressions for an element.
 */

STATIC void ecom(elem **pe)
{ int i,op,hcstopsave;
  unsigned hash;
  elem *e,*ehash;
  tym_t tym;

  e = *pe;
  assert(e);
  elem_debug(e);
#ifdef DEBUG
  assert(e->Ecount == 0);
  //assert(e->Ecomsub == 0);
#endif
  tym = tybasic(e->Ety);
  op = e->Eoper;
  switch (op)
  {
    case OPconst:
    case OPvar:
    case OPrelconst:
        break;
    case OPstreq:
    case OPpostinc:
    case OPpostdec:
    case OPeq:
    case OPaddass:
    case OPminass:
    case OPmulass:
    case OPdivass:
    case OPmodass:
    case OPshrass:
    case OPashrass:
    case OPshlass:
    case OPandass:
    case OPxorass:
    case OPorass:
#if TX86
        /* Reverse order of evaluation for double op=. This is so that  */
        /* the pushing of the address of the second operand is easier.  */
        /* However, with the 8087 we don't need the kludge.             */
        if (op != OPeq && tym == TYdouble && !config.inline8087)
        {       if (EOP(e->E1))
                        ecom(&e->E1->E1);
                ecom(&e->E2);
        }
        else
#endif
        {
            /* Don't mark the increment of an i++ or i-- as a CSE, if it */
            /* can be done with an INC or DEC instruction.               */
            if (!(OTpost(op) && elemisone(e->E2)))
                ecom(&e->E2);           /* evaluate 2nd operand first   */
    case OPnegass:
            if (EOP(e->E1))             /* if lvalue is an operator     */
            {
#ifdef DEBUG
                if (e->E1->Eoper != OPind)
                    elem_print(e);
#endif
                assert(e->E1->Eoper == OPind);
                ecom(&(e->E1->E1));
            }
        }
        touchlvalue(e->E1);
        if (!OTpost(op))                /* lvalue of i++ or i-- is not a cse*/
        {
            hash = cs_comphash(e->E1);
            vec_setbit(hash % CSVECDIM,csvec);
            addhcstab(e->E1,hash);              // add lvalue to hcstab[]
        }
        return;

    case OPbtc:
    case OPbts:
    case OPbtr:
        ecom(&e->E1);
        ecom(&e->E2);
        touchfunc(0);                   // indirect assignment
        return;

    case OPandand:
    case OPoror:
        ecom(&e->E1);
        hcstopsave = hcstop;
        ecom(&e->E2);
        hcstop = hcstopsave;            /* no common subs by E2         */
        return;                         /* if comsub then logexp() will */
                                        /* break                        */
    case OPcond:
        ecom(&e->E1);
        hcstopsave = hcstop;
        ecom(&e->E2->E1);               /* left condition               */
        hcstop = hcstopsave;            /* no common subs by E2         */
        ecom(&e->E2->E2);               /* right condition              */
        hcstop = hcstopsave;            /* no common subs by E2         */
        return;                         /* can't be a common sub        */
    case OPcall:
    case OPcallns:
        ecom(&e->E2);                   /* eval right first             */
        /* FALL-THROUGH */
    case OPucall:
    case OPucallns:
        ecom(&e->E1);
        touchfunc(1);
        return;
    case OPstrpar:                      /* so we don't break logexp()   */
#if TX86
    case OPinp:                 /* never CSE the I/O instruction itself */
#endif
    case OPdctor:
        ecom(&e->E1);
        /* FALL-THROUGH */
    case OPasm:
    case OPstrthis:             // don't CSE these
    case OPframeptr:
    case OPgot:
    case OPctor:
    case OPdtor:
    case OPmark:
        return;

    case OPddtor:
        return;

    case OPparam:
#if TX86
    case OPoutp:
#endif
        ecom(&e->E1);
    case OPinfo:
        ecom(&e->E2);
        return;
    case OPcomma:
    case OPremquo:
        ecom(&e->E1);
        ecom(&e->E2);
        break;
    case OPvptrfptr:
    case OPcvptrfptr:
        ecom(&e->E1);
        touchaccess(e);
        break;
    case OPind:
        ecom(&e->E1);
        /* Generally, CSEing a *(double *) results in worse code        */
        if (tyfloating(tym))
            return;
        break;
#if TX86
    case OPstrcpy:
    case OPstrcat:
    case OPmemcpy:
    case OPmemset:
        ecom(&e->E2);
    case OPsetjmp:
        ecom(&e->E1);
        touchfunc(0);
        return;
#endif
    default:                            /* other operators */
#if TX86
#ifdef DEBUG
        if (!EBIN(e)) WROP(e->Eoper);
#endif
        assert(EBIN(e));
    case OPadd:
    case OPmin:
    case OPmul:
    case OPdiv:
    case OPor:
    case OPxor:
    case OPand:
    case OPeqeq:
    case OPne:
    case OPscale:
    case OPyl2x:
    case OPyl2xp1:
        ecom(&e->E1);
        ecom(&e->E2);
        break;
#else
#ifdef DEBUG
        if (!EOP(e)) WROP(e->Eoper);
#endif
        assert(EOP(e));
        ecom(&e->E1);
        if (EBIN(e))
                ecom(&e->E2);           /* eval left first              */
        break;
#endif
    case OPstring:
    case OPaddr:
    case OPbit:
#ifdef DEBUG
        WROP(e->Eoper);
        elem_print(e);
#endif
        assert(0);              /* optelem() should have removed these  */
        /* NOTREACHED */

#if TX86
    // Explicitly list all the unary ops for speed
    case OPnot: case OPcom: case OPneg: case OPuadd:
    case OPabs: case OPsqrt: case OPrndtol: case OPsin: case OPcos: case OPrint:
    case OPpreinc: case OPpredec:
    case OPbool: case OPstrlen: case OPs16_32: case OPu16_32:
    case OPd_s32: case OPd_u32: case OPoffset:
    case OPs32_d: case OPu32_d: case OPdblint: case OPs16_d: case OPlngsht:
    case OPd_f: case OPf_d:
    case OPd_ld: case OPld_d:
    case OPc_r: case OPc_i:
    case OPu8int: case OPs8int: case OPint8:
    case OPu32_64: case OPlngllng: case OP64_32: case OPmsw:
    case OPu64_128: case OPs64_128: case OP128_64:
    case OPd_s64: case OPs64_d: case OPd_u64: case OPu64_d:
    case OPstrctor: case OPu16_d: case OPdbluns:
    case OPptrlptr: case OPtofar16: case OPfromfar16: case OParrow:
    case OPvoid: case OPnullcheck:
    case OPbsf: case OPbsr: case OPbswap:
    case OPld_u64:
        ecom(&e->E1);
        break;
#endif
    case OPhalt:
        return;
  }

  /* don't CSE structures or unions or volatile stuff   */
  if (tym == TYstruct ||
      tym == TYvoid ||
      e->Ety & mTYvolatile
#if TX86
    // don't CSE doubles if inline 8087 code (code generator can't handle it)
      || (tyfloating(tym) && config.inline8087)
#endif
     )
        return;

  hash = cs_comphash(e);                /* must be AFTER leaves are done */

  /* Search for a match in hcstab[].
   * Search backwards, as most likely matches will be towards the end
   * of the list.
   */

#ifdef DEBUG
  if (debugx) dbg_printf("elem: %p hash: %6d\n",e,hash);
#endif
  int csveci = hash % CSVECDIM;
  if (vec_testbit(csveci,csvec))
  {
    for (i = hcstop; i--;)
    {
#ifdef DEBUG
        if (debugx)
            dbg_printf("i: %2d Hhash: %6d Helem: %p\n",
                i,hcstab[i].Hhash,hcstab[i].Helem);
#endif
        if (hash == hcstab[i].Hhash && (ehash = hcstab[i].Helem) != NULL)
        {
            /* if elems are the same and we still have room for more    */
            if (el_match(e,ehash) && ehash->Ecount < 0xFF)
            {
                /* Make sure leaves are also common subexpressions
                 * to avoid false matches.
                 */
                if (!OTleaf(op))
                {
                    if (!e->E1->Ecount)
                        continue;
                    if (OTbinary(op) && !e->E2->Ecount)
                        continue;
                }
                ehash->Ecount++;
                *pe = ehash;
#ifdef DEBUG
                if (debugx)
                        dbg_printf("**MATCH** %p with %p\n",e,*pe);
#endif
                el_free(e);
                return;
            }
        }
    }
  }
  else
    vec_setbit(csveci,csvec);
  addhcstab(e,hash);                    // add this elem to hcstab[]
}

/**************************
 * Compute hash function for elem e.
 */

STATIC unsigned cs_comphash(elem *e)
{   register int hash;
    unsigned op;

    elem_debug(e);
    op = e->Eoper;
#if TX86
    hash = (e->Ety & (mTYbasic | mTYconst | mTYvolatile)) + (op << 8);
#else
    hash = e->Ety + op;
#endif
    if (!OTleaf(op))
    {   hash += (size_t) e->E1;
        if (OTbinary(op))
                hash += (size_t) e->E2;
    }
    else
    {   hash += e->EV.Vint;
        if (op == OPvar || op == OPrelconst)
                hash += (size_t) e->EV.sp.Vsym;
    }
    return hash;
}

/****************************
 * Add an elem to the common subexpression table.
 * Recompute hash if it is 0.
 */

STATIC void addhcstab(elem *e,int hash)
{ unsigned h = hcstop;

  if (h >= hcsmax)                      /* need to reallocate table     */
  {
        assert(h == hcsmax);
        // With 32 bit compiles, we've got memory to burn
        hcsmax += (__INTSIZE == 4) ? (hcsmax + 128) : 100;
        assert(h < hcsmax);
#if TX86
        hcstab = (hcs *) util_realloc(hcstab,hcsmax,sizeof(hcs));
#else
        hcstab = (hcs *) MEM_PARF_REALLOC(hcstab,hcsmax*sizeof(hcs));
#endif
        //printf("hcstab = %p; hcstop = %d, hcsmax = %d\n",hcstab,hcstop,hcsmax);
  }
  hcstab[h].Helem = e;
  hcstab[h].Hhash = hash;
  hcstop++;
}

/***************************
 * "touch" the elem.
 * If it is a pointer, "touch" all the suspects
 * who could be pointed to.
 * Eliminate common subs that are indirect loads.
 */

STATIC void touchlvalue(elem *e)
{ register int i;

  if (e->Eoper == OPind)                /* if indirect store            */
  {
        /* NOTE: Some types of array assignments do not need
         * to touch all variables. (Like a[5], where a is an
         * array instead of a pointer.)
         */

        touchfunc(0);
        return;
  }

  for (i = hcstop; --i >= 0;)
  {     if (hcstab[i].Helem &&
            hcstab[i].Helem->EV.sp.Vsym == e->EV.sp.Vsym)
                hcstab[i].Helem = NULL;
  }

    assert(e->Eoper == OPvar || e->Eoper == OPrelconst);
    switch (e->EV.sp.Vsym->Sclass)
    {
        case SCregpar:
        case SCregister:
        case SCtmp:
        case SCpseudo:
            break;
        case SCauto:
        case SCparameter:
#if TX86
        case SCfastpar:
        case SCbprel:
#endif
            if (e->EV.sp.Vsym->Sflags & SFLunambig)
                break;
            /* FALL-THROUGH */
        case SCstatic:
        case SCextern:
        case SCglobal:
        case SClocstat:
        case SCcomdat:
        case SCinline:
        case SCsinline:
        case SCeinline:
#if TX86
        case SCcomdef:
#endif
            touchstar();
            break;
        default:
#ifdef DEBUG
            elem_print(e);
            symbol_print(e->EV.sp.Vsym);
#endif
            assert(0);
    }
}

/**************************
 * "touch" variables that could be changed by a function call or
 * an indirect assignment.
 * Eliminate any subexpressions that are "starred" (they need to
 * be recomputed).
 * Input:
 *      flag    If !=0, then this is a function call.
 *              If 0, then this is an indirect assignment.
 */

STATIC void touchfunc(int flag)
{ register hcs *pe,*petop;
  register elem *he;

  //printf("touchfunc(%d)\n", flag);
  petop = &hcstab[hcstop];
  //pe = &hcstab[0]; printf("pe = %p, petop = %p\n",pe,petop);
  for (pe = &hcstab[0]; pe < petop; pe++)
  //for (pe = &hcstab[touchfunci[flag]]; pe < petop; pe++)
  {     he = pe->Helem;
        if (!he)
                continue;
        switch (he->Eoper)
        {
            case OPvar:
                switch (he->EV.sp.Vsym->Sclass)
                {
                    case SCregpar:
                    case SCregister:
                    case SCtmp:
                        break;
                    case SCauto:
                    case SCparameter:
#if TX86
                    case SCfastpar:
                    case SCbprel:
#endif
                        //printf("he = '%s'\n", he->EV.sp.Vsym->Sident);
                        if (he->EV.sp.Vsym->Sflags & SFLunambig)
                            break;
                        /* FALL-THROUGH */
                    case SCstatic:
                    case SCextern:
#if TX86
                    case SCcomdef:
#endif
                    case SCglobal:
                    case SClocstat:
                    case SCcomdat:
                    case SCpseudo:
                    case SCinline:
                    case SCsinline:
                    case SCeinline:
                        if (!(he->EV.sp.Vsym->ty() & mTYconst))
                            goto L1;
                        break;
                    default:
                        debug(WRclass((enum SC)he->EV.sp.Vsym->Sclass));
                        assert(0);
                }
                break;
            case OPind:
#if TX86
            case OPstrlen:
            case OPstrcmp:
            case OPmemcmp:
            case OPbt:
#endif
                goto L1;
            case OPvptrfptr:
            case OPcvptrfptr:
                if (flag == 0)          /* function calls destroy vptrfptr's, */
                    break;              /* not indirect assignments     */
            L1:
                pe->Helem = NULL;
                break;
        }
  }
  touchfunci[flag] = hcstop;
}


/*******************************
 * Eliminate all common subexpressions that
 * do any indirection ("starred" elems).
 */

STATIC void touchstar()
{ register int i;
  register elem *e;

  for (i = touchstari; i < hcstop; i++)
  {     e = hcstab[i].Helem;
        if (e && (e->Eoper == OPind || e->Eoper == OPbt) /*&& !(e->Ety & mTYconst)*/)
                hcstab[i].Helem = NULL;
  }
  touchstari = hcstop;
}

/*****************************************
 * Eliminate any common subexpressions that could be modified
 * if a handle pointer access occurs.
 */

STATIC void touchaccess(elem *ev)
{ register int i;
  register elem *e;

  ev = ev->E1;
  for (i = 0; i < hcstop; i++)
  {     e = hcstab[i].Helem;
        /* Invalidate any previous handle pointer accesses that */
        /* are not accesses of ev.                              */
        if (e && (e->Eoper == OPvptrfptr || e->Eoper == OPcvptrfptr) &&
            e->E1 != ev)
            hcstab[i].Helem = NULL;
  }
}

#endif // !SPP
