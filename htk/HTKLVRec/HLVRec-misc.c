/* ----------------------------------------------------------- */
/*                                                             */
/*                          ___                                */
/*                       |_| | |_/   SPEECH                    */
/*                       | | | | \   RECOGNITION               */
/*                       =========   SOFTWARE                  */ 
/*                                                             */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                             */
/*      Machine Intelligence Laboratory                        */
/*      Department of Engineering                              */
/*      University of Cambridge                                */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright:                                          */
/*         2002-2003  Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*         File: HLVRec-misc.c Miscellaneous functions for     */
/*                             HTK LV Decoder                  */
/* ----------------------------------------------------------- */

/* CheckTokenSetOrder

     check whether the relTokens are sorted by LMState order 
*/
void Decoder::CheckTokenSetOrder (TokenSet *ts)
{
   int i;
   RelToken *prevTok;
   bool ok = TRUE;

   prevTok = &ts->relTok[0];
   for (i = 1; i < ts->n; ++i) {
      if (TOK_LMSTATE_LT(&ts->relTok[i], prevTok) || TOK_LMSTATE_EQ(&ts->relTok[i], prevTok))
         ok = FALSE;
      prevTok = &ts->relTok[i];
   }

   if (!ok) {
      printf ("XXXXX CheckTokenSetOrder \n");
      PrintTokSet (ts);
      abort();
   }
}

/* CheckTokenSetId

     check whether two TokenSets that have the same id are in fact equal, i.e.
     have the same set of RelToks
*/
void Decoder::CheckTokenSetId (TokenSet *ts1, TokenSet *ts2)
{
   int i1, i2;
   RelToken *tok1, *tok2;
   bool ok=TRUE;

   abort ();    /* need to convert to use TOK_LMSTATE_ */

   assert (ts1 != ts2);
   assert (ts1->id == ts2->id);

   if (ts2->score < _dec->beamLimit || ts1->score < _dec->beamLimit)
      return;

   i1 = i2 = 0;
   while (ts1->score + ts1->relTok[i1].delta < _dec->beamLimit && i1 < ts1->n)
      ++i1;
   while (ts2->score + ts2->relTok[i2].delta < _dec->beamLimit && i2 < ts2->n)
      ++i2;
   
   while (i1 < ts1->n && i2 < ts2->n) {
      tok1 = &ts1->relTok[i1];
      tok2 = &ts2->relTok[i2];

      if ((tok1->lmState != tok2->lmState) ||
          (tok1->lmscore != tok2->lmscore) ||
          (tok1->delta != tok2->delta))
         ok = FALSE;
      ++i1;
      ++i2;
      
      while (ts1->score + ts1->relTok[i1].delta < _dec->beamLimit && i1 < ts1->n)
         ++i1;
      while (ts2->score + ts2->relTok[i2].delta < _dec->beamLimit && i2 < ts2->n)
         ++i2;
   };
   for ( ; i1 < ts1->n; ++i1)
      if (ts1->score + ts1->relTok[i1].delta > _dec->beamLimit)
         ok = FALSE;
   for ( ; i2 < ts2->n; ++i2)
      if (ts2->score + ts2->relTok[i2].delta > _dec->beamLimit)
         ok = FALSE;

   if (!ok) {
      printf ("XXXXX CheckTokenSetId  difference in tokensets \n");
      PrintTokSet (ts1);
      PrintTokSet (ts2);
      abort();
   }
}


/* CombinePaths

     incorporate the traceback info from loser token into winner token

     diff = T_l - T_w
*/
WordendHyp *Decoder::CombinePaths (RelToken *winner, RelToken *loser, LogFloat diff)
{
   WordendHyp *weHyp;
   AltWordendHyp *alt, *newalt;
   AltWordendHyp **p;
   
   abort();
   assert (diff < 0.1);
   assert (winner->path != loser->path);

   /*   assert (winner->path->score > loser->path->score);  */

   weHyp = (WordendHyp *) New (&_dec->weHypHeap, sizeof (WordendHyp));
   *weHyp = *winner->path;

   weHyp->frame = _dec->frame;

   p = &weHyp->alt;
   for (alt = winner->path->alt; alt; alt = alt->next) {
      newalt = (AltWordendHyp *) New (&_dec->altweHypHeap, sizeof (AltWordendHyp));
      *newalt = *alt;
      newalt->next = NULL;
      *p = newalt;
      p = &newalt->next;
   }

   /* add info from looser */

   newalt = (AltWordendHyp *) New (&_dec->altweHypHeap, sizeof (AltWordendHyp));
   newalt->prev = loser->path->prev;
   newalt->score = diff;
   newalt->lm = loser->path->lm;
   newalt->next = NULL;

   assert (newalt->score < 0.1);
   /*    assert (winner->path->pron->word == loser->path->pron->word); */

   *p = newalt;
   p = &newalt->next;
   for (alt = loser->path->alt; alt; alt = alt->next) {
      /* only add if in main Beam, otherwise we will prune 
         it anyway later on */
      /* should be latprunebeam? */
      if (diff + alt->score > -_dec->beamWidth) {
         newalt = (AltWordendHyp *) New (&_dec->altweHypHeap, sizeof (AltWordendHyp));
         *newalt = *alt;
         newalt->score = diff + alt->score;
         newalt->next = NULL;
         *p = newalt;
         p = &newalt->next;
         assert (newalt->score < 0.1);
      }
   }

#ifndef NDEBUG
   assert (weHyp->prev->frame <= weHyp->frame);
   for (alt = weHyp->alt; alt; alt = alt->next) {
      assert (alt->prev->frame <= weHyp->frame);
      assert (alt->score <= 0.1);
   }
#endif

   return weHyp;
}


/****           debug functions */

/*
  Debug_DumpNet

*/
void Debug_DumpNet (LexNet *net)
{
   int i, j, k, N;
   LexNode *ln;
   LexNodeInst *inst;
   TokenSet *ts;
   RelToken *rtok;
   FILE *debugFile;
   bool isPipe;
   static char *debug_net_fn = "net.dump";

   debugFile = FOpen (debug_net_fn, NoOFilter, &isPipe);
   if (!debugFile) {
      printf ("fopen failed\n");
      return;
   }

   fprintf (debugFile, "(LexNet *) %p\n", net);
   fprintf (debugFile, "nNodes %d\n", net->nNodes);

   for (i = 0; i < net->nNodes; ++i) {
      ln = &net->node[i];
      inst = ln->inst;
      if (inst) {
         fprintf (debugFile, "node %d  (LexNode *) %p", i, ln);
         fprintf (debugFile, " type %d nfoll %d", ln->type, ln->nfoll);
         if (ln->type == LN_MODEL)
            fprintf (debugFile, " name %s", 
                    FindMacroStruct (net->hset, 'h', ln->data.hmm)->id->name);
         fprintf (debugFile, "\n");
         
         assert (inst->node == ln);
         fprintf (debugFile, " (LexNodeInst *) %p", inst);
         fprintf (debugFile, "  best %f\n", inst->best);
         N = (ln->type == LN_MODEL) ? ln->data.hmm->numStates : 1;
         for (j = 0; j < N; ++j) {
            ts = &inst->ts[j];
            if (ts->n > 0) {
               fprintf (debugFile, "  state %d (TokenSet *) %p", j+1, ts);
               fprintf (debugFile, "   score %f\n", ts->score);
               for (k = 0; k < ts->n; ++k) {
                  rtok = &ts->relTok[k];
                  fprintf (debugFile, "   (RelToken *) %p", rtok);
                  fprintf (debugFile, "    delta %f  lmstate %p lmscore %f\n", 
                          rtok->delta, rtok->lmState, rtok->lmscore);
               }
            }
         }
      }
   }

   FClose (debugFile, isPipe);
}

void Decoder::Debug_Check_Score ()
{
   int l, j, N;
   LexNode *ln;
   LexNodeInst *inst;
   TokenSet *ts;

   for (l = 0; l < _dec->nLayers; ++l) {
      int sumTok, maxTok, nTS;
      sumTok = maxTok = 0;
      nTS = 0;
      for (inst = _dec->instsLayer[l]; inst; inst = inst->next) {
         ln = inst->node;
         N = (ln->type == LN_MODEL) ? ln->data.hmm->numStates : 1;
         for (j = 0; j < N; ++j) {
            ts = &inst->ts[j];
            if (ts->n > 0) {
               sumTok += ts->n;
               ++nTS;
               if (ts->n > maxTok)
                  maxTok = ts->n;
            }
         }
      }
      printf ("l %d aveTok/TS %.3f maxTok %d\n",
              l, (float)sumTok/nTS, maxTok);
   }
}


/***************** phone posterior estimation *************************/

void Decoder::InitPhonePost ()
{
   HMMScanState hss;
   HLink hmm;
   MLink m;
   char buf[100];
   LabId phoneId;

   NewHMMScan (_dec->hset, &hss);
   do {
      hmm = hss.hmm;
      assert (!hmm->hook);
      m = FindMacroStruct (_dec->hset, 'h', hmm);
      assert (strlen (m->id->name) < 100);
      strcpy (buf, m->id->name);
      TriStrip (buf);
      phoneId = GetLabId (buf, TRUE);
      phoneId->aux = (Ptr) 0;
      hmm->hook = (Ptr) phoneId;
   } while(GoNextHMM(&hss));
   EndHMMScan(&hss);

   _dec->nPhone = 0;
   /* count monophones -- #### make this more efficent! */
   NewHMMScan (_dec->hset, &hss);
   do {
      hmm = hss.hmm;
      phoneId = (LabId) hmm->hook;
      if (!phoneId->aux) {
         ++_dec->nPhone;
         phoneId->aux = (Ptr) _dec->nPhone;

         assert (_dec->nPhone < 100);
         _dec->monoPhone[_dec->nPhone] = phoneId;
      }
   } while(GoNextHMM(&hss));
   EndHMMScan(&hss);

   printf ("found %d monophones\n", _dec->nPhone);

   _dec->phonePost = (LogDouble *) New (&gcheap, (_dec->nPhone+1) * sizeof (LogDouble));
   _dec->phoneFreq = (int *) New (&gcheap, (_dec->nPhone+1) * sizeof (int));
}

void Decoder::CalcPhonePost ()
{
   int l, N, i, j;
   LexNodeInst *inst;
   LabId phoneId;
   int phone;
   LogDouble *phonePost;
   int *phoneFreq;
   TokenSet *ts;
   RelToken *tok;
   LogDouble sum;

   phonePost = _dec->phonePost;
   phoneFreq = _dec->phoneFreq;
   for (i = 0; i <= _dec->nPhone; ++i) {
      phonePost[i] = LZERO;
      phoneFreq[i] = 0;
   }

   for (l = 0; l < _dec->nLayers; ++l) {
      for (inst = _dec->instsLayer[l]; inst; inst = inst->next) {
         if (inst->node->type == LN_MODEL) {
            phoneId = (LabId) inst->node->data.hmm->hook;
            phone = (int) phoneId->aux;
            assert (phone >= 1 && phone <= _dec->nPhone);
            
            N = inst->node->data.hmm->numStates;
            for (i = 1; i < N; ++i) {
               ts = &inst->ts[i];
               for (j = 0, tok = ts->relTok; j < ts->n; ++j, ++tok) {
                  phonePost[phone] = LAdd (phonePost[phone], ts->score + tok->delta);
                  ++phoneFreq[phone];
               }
            }
         }
      }
   }

   sum = LZERO;
   for (i = 0; i <= _dec->nPhone; ++i)
      sum = LAdd (sum, phonePost[i]);
   
   for (i = 0; i <= _dec->nPhone; ++i) {
      if (phonePost[i] > LSMALL) {
         phonePost[i] = phonePost[i] - sum;
      } else 
         phonePost[i] = LZERO;
      
   }
}

/******************** Token Statistics ********************/

struct _LayerStats {
   int nInst;
   int nTS;
   int nTok;
   TokScore bestScore;
   TokScore worstScore;
};

typedef struct _LayerStats LayerStats;

void Decoder::AccumulateStats ()
{
   MemHeap statsHeap;
   LayerStats *layerStats;
   int l;

   CreateHeap (&statsHeap, "Token Stats Heap", MSTAK, 1, 1.5, 10000, 100000);

   layerStats = (LayerStats *) New (&statsHeap, _dec->nLayers * sizeof (LayerStats));
   memset ((void *) layerStats, 0, _dec->nLayers * sizeof (LayerStats));

   for (l = 0; l < _dec->nLayers; ++l) {
      layerStats[l].nInst = layerStats[l].nTS = layerStats[l].nTok = 0;
      layerStats[l].bestScore = LZERO;
      layerStats[l].worstScore = - LZERO;
   }

   /* count inst/ts/tok, find best/worst scores */
   {
      int l, N, s, i;
      LayerStats *ls;
      LexNodeInst *inst;
      LexNode *ln;
      TokenSet *ts;
      RelToken *tok;
      TokScore score;
      TokScore instBest;

      for (l = 0; l < _dec->nLayers; ++l) {
         ls = &layerStats[l];
         for (inst = _dec->instsLayer[l]; inst; inst = inst->next) {
            ++ls->nInst;
            ln = inst->node;

            switch (ln->type) {
            case LN_MODEL:
               N = ln->data.hmm->numStates;
               break;
            case LN_CON:
            case LN_WORDEND:
               N = 1;
               break;
            default:
               abort ();
               break;
            }

            instBest = LZERO;
            for (s = 0; s < N; ++s) {   /* for each state/TokenSet */
               ts = &inst->ts[s];
               if (ts->n > 0) {
                  ++ls->nTS;
                  if (ts->score > instBest)
                     instBest = ts->score;
                  for (i = 0; i < ts->n; ++i) { /* for each token */
                     tok = &ts->relTok[i];
                     ++ls->nTok;
                     score = ts->score + tok->delta;
                     if (score > ls->bestScore)
                        ls->bestScore = score;
                     if (score < ls->worstScore)
                        ls->worstScore = score;
                  }
               }
            }
            assert (instBest == inst->best);
            
         } /* for each inst */
         printf ("STATS layer %d  ", l);
         printf ("%5d Insts   %5d TokenSets   %6d Tokens  %.2f Tok/TS ",
                 ls->nInst, ls->nTS, ls->nTok, (float) ls->nTok / ls->nTS);
         printf ("best:  %.4f   ", ls->bestScore);
         printf ("worst: %.4f\n", ls->worstScore);

      } /* for each layer */
   }
   
   ResetHeap (&statsHeap);
   DeleteHeap (&statsHeap);
}

