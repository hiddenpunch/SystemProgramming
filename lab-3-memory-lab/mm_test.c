//--------------------------------------------------------------------------------------------------
// System Programming                       Memory Lab                                   Fall 2020
//
/// @file
/// @brief dynamic memory manager test program
/// @author Bernhard Egger <bernhard@csap.snu.ac.kr>
/// @section changelog Change Log
/// 2020/09/27 Bernhard Egger created
///
/// @section license_section License
/// Copyright (c) 2020, Computer Systems and Platforms Laboratory, SNU
/// All rights reserved.
///
/// Redistribution and use in source and binary forms, with or without modification, are permitted
/// provided that the following conditions are met:
///
/// - Redistributions of source code must retain the above copyright notice, this list of condi-
///   tions and the following disclaimer.
/// - Redistributions in binary form must reproduce the above copyright notice, this list of condi-
///   tions and the following disclaimer in the documentation and/or other materials provided with
///   the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
/// IMPLIED WARRANTIES,  INCLUDING, BUT NOT LIMITED TO,  THE IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
/// CONTRIBUTORS BE LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR CONSE-
/// QUENTIAL DAMAGES (INCLUDING,  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
/// LOSS OF USE, DATA,  OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON ANY THEORY OF
/// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//--------------------------------------------------------------------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dataseg.h"
#include "memmgr.h"

void enter(void)
{
  printf("Press enter to continue.\n");
  getchar();
}

int main(int argc, char *argv[])
{
  void *ptr[100];
  unsigned int idx = 0;
  unsigned debug = 0;
  AllocationPolicy ap=ap_NextFit;

  ds_setloglevel(2);
  mm_setloglevel(2);

  printf("\n\n\n----------------------------------------\n"
         "  Initializing heap...\n"
         "\n\n");
  ds_allocate(32*1024*1024);
  mm_init(ap);
  mm_check();

  printf("\n\n\n----------------------------------------\n"
         "  Testing mm_malloc()...\n"
         "\n\n");
  debug = 1;
  ds_setloglevel(1);
  mm_setloglevel(2);


  
  void *p = mm_malloc(1000); mm_check(); enter();
  void *p1 = mm_malloc(1000); mm_check(); enter();
  void *p2 = mm_malloc(1000); mm_check(); enter();
  p = mm_malloc(100); mm_check(); enter();

  mm_free(p1); mm_check(); enter();
  mm_free(p); mm_check(); enter();
  mm_free(p); mm_check(); enter();

  p = mm_malloc(20); mm_check(); enter();
  p = mm_malloc(5000); mm_check(); enter();
  p = mm_malloc(1000); mm_check(); enter();
  p = mm_malloc(1000); mm_check(); enter();
  p = mm_malloc(1000); mm_check(); enter();

  return EXIT_SUCCESS;

  enter(); ptr[idx++] = mm_malloc(1);       if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(15);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(16);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(17);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(31);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(32);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(33);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(47);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(48);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(49);      if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(100);     if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(200);     if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(400);     if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(1024);    if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(0x1000);  if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(0x2000);  if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(0x4000);  if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(0x8000);  if (debug) mm_check();
  enter(); ptr[idx++] = mm_malloc(0x10000); if (debug) mm_check();

  printf("\n\n\n----------------------------------------\n"
         "  Testing mm_free()...\n"
         "\n\n");
  debug = 1;
  ds_setloglevel(0);
  mm_setloglevel(0);

  enter(); mm_free(ptr[0]);  if (debug) mm_check();
  enter(); mm_free(ptr[1]);  if (debug) mm_check();
  enter(); mm_free(ptr[3]);  if (debug) mm_check();
  enter(); mm_free(ptr[2]);  if (debug) mm_check();
  enter(); mm_free(ptr[7]);  if (debug) mm_check();
  enter(); mm_free(ptr[6]);  if (debug) mm_check();
  enter(); mm_free(ptr[5]);  if (debug) mm_check();
  enter(); mm_free(ptr[4]);  if (debug) mm_check();
  enter(); mm_free(ptr[10]); if (debug) mm_check();


  /*
  printf("\n\n\n----------------------------------------\n"
         "  Testing mm_realloc()...\n"
         "\n\n");
  debug = 1;
  ds_setloglevel(0);
  mm_setloglevel(2);

  enter(); mm_realloc(ptr[9], 50);  if (debug) mm_check();
  enter(); mm_realloc(ptr[9], 60);  if (debug) mm_check();
  enter(); mm_realloc(ptr[9], 48);  if (debug) mm_check();
  enter(); mm_realloc(ptr[9], 220); if (debug) mm_check();
  */

  return EXIT_SUCCESS;
}


