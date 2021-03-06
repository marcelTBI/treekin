/*=================================================================*/
/*=   main.c                                                      =*/
/*=   main file for treekin                                       =*/
/*=   ---------------------------------------------------------   =*/
/*=   Last changed Time-stamp: <2006-11-27 13:09:09 mtw>          =*/
/*=   $Id: main.c,v 1.24 2006/11/27 13:47:57 mtw Exp $            =*/
/*=   ---------------------------------------------------------   =*/
/*=                 (c) Michael Thomas Wolfinger                  =*/
/*=                      mtw@tbi.univie.ac.at                     =*/
/*=                             treekin                           =*/
/*=================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include "calc.h"
#include "mxccm.h"
#include "barparser.h"
#include "globals.h"

#include "calcpp.h"

int
main (int argc, char **argv)
{
  clock_t clck1 = clock();

  BarData *Data=NULL;
  /*  U - matrix (Q+I)^T, where Q is infetisimal generator (^T - transposed)
      S - eigenvectors of U
      p0 - distribution in the begining
      p8 - stable (end) distribution
      R - tmp rates matrix (transposed) */
  double *U=NULL, *S=NULL, *p0=NULL, *p8=NULL, *R=NULL;
  int dim;

  parse_commandline(argc, argv);

  switch (opt.method) {
    case 'F': dim = ParseInfile(opt.INFILE, opt.RATFILE, &R); break;
    case 'I':
      if (opt.binrates) dim = MxReadBinRates(opt.RATFILE, &R, opt.n, opt.max_decrease);
      else dim = ParseRatesFile(opt.RATFILE, &R, opt.n, opt.max_decrease);

      if (opt.INFILE) {
        ParseBarfile(opt.INFILE, &Data);
      }
      if (dim == 0) {
        fprintf(stderr, "ERROR: Rate file empty!\n");
        free_gengetopt();
        exit(EXIT_FAILURE);
      }
      break;
    case 'A': dim = ParseBarfile (opt.INFILE, &Data); break;
  }

  // initialise matrices, dont forget to release 'em
  MxInit (dim);

  // visualize the graph:
  if (opt.vis_file) {
    VisualizeRates(opt.vis_file, R, Data, dim);
  }

  // here we create the "almighty" rate matrix U which is actually only matrix needed for whole program
  U  = MxBar2Matrix(Data, R);

  // rescale if minimal_rate has been set:
  if (opt.hard_rescale != 1.0) MxRescaleH(U, dim, opt.hard_rescale);
  else if (opt.minimal_rate > 0.0) MxRescale(U, dim, opt.minimal_rate);

  // multiply if times was set:
  if (opt.times != 1.0) MxTimes(U, dim, opt.times);

  // create initial probability vector
  MxStartVec(&p0);

  // check for ergodicity + adjust to that
  MxEgro(&U, &p0, dim);
  if (opt.want_verbose) MxPrint(U, "Ergodic U", 'm');

  // allocate space for other matrices
  MxGetSpace(&p8);

  if (!opt.quiet) fprintf(stderr, "Time to initialize: %.2f secs.\n", (clock() - clck1)/(double)CLOCKS_PER_SEC);
  clck1 = clock();

  // calculate equilibrium distribution
  if(opt.method == 'F')
    MxEqDistrFULL (E, p8);
  else {
    //MxEqDistr (Data, &p8);  /* FIX THIS */
    //MxEqDistrFromLocalBalance(U, &p8);
    MxEqDistrFromLinSys(U, &p8);
  }

  // write the equilibrium if we should
  if (opt.equil_file) {
    FILE *equil = fopen(opt.equil_file, "w");
    if (equil) {
      MxFPrint(p8, "Equilibrium distribution", 'v', equil, 1);
      fclose(equil);
    }
  }

  // first passage times computation
  if(opt.fpt) {
    // output file?
    FILE *fpt = stderr;
    if (opt.fpt_file!=NULL) {
      fpt = fopen(opt.fpt_file, "w");
      if (fpt==NULL) {
        if (!opt.quiet) fprintf(stderr, "Cannot open file \"%s\" for FPT output, using \"stderr\" instead\n", opt.fpt_file);
        fpt = stderr;
      }
    }
    // all or only one state?
    if ((opt.fpt_num == -1) || opt.absrb) { // all ftp's
      MxFPT(U, p8, fpt);
    } else {                                // only to state opt.ftp_num
      double *res = MxFPTOneState(U, opt.fpt_num-1);  // starting from 0
      if (res != NULL) {
        if (fpt==stderr) fprintf(fpt, "First passage times to state number %d:\n", opt.fpt_num);
        int k;
        for (k = 0; k < dim; k++) fprintf(fpt, "%15.7f ", res[k]);
        if (fpt==stderr) fprintf(fpt,"\n---\n");
        else fprintf(fpt, "\n");
        free(res);
      }
    }
    fclose(fpt);
    if (!opt.quiet) fprintf(stderr, "Time to compute fpt: %.2f secs.\n", (clock() - clck1)/(double)CLOCKS_PER_SEC);
    clck1 = clock();
  }

  //TestExpokit(U, dim, p0, opt.t0, opt.t8, opt.tinc);


  if (!opt.just_sh) {
    // diagonalization + iteration
    if(opt.matexp) MxExponent(p0,p8,U);
    else {
      if(opt.rrecover) /* read pre-calculated eigenv{alues,ectors} */
        MxRecover(&S, p8);
      else {
        clck1 = clock();
        MxDiagonalize (U, &S, p8);
        if (!opt.quiet) fprintf(stderr, "Time to diagonalize: %.2f secs.\n", (clock() - clck1)/(double)CLOCKS_PER_SEC);
        clck1 = clock();
      }
      MxIterate (p0, p8, S);
      if (!opt.quiet) fprintf(stderr, "Time to iterate: %.2f secs.\n", (clock() - clck1)/(double)CLOCKS_PER_SEC);
      free(S);
      free(p8);
    }
  }

  // clean up the memory
  MxMemoryCleanUp();
  if (opt.pini != NULL) free(opt.pini);
  free(U);
  free(p0);
  if(Data != NULL) free(Data);
  return 0;
}

/* End of file */



