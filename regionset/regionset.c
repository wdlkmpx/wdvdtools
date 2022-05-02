/*
 * regionset: Region Code Manager
 * Copyright (C) 1999 Christian Wolff for convergence integrated media GmbH
 * 1999-12-13
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 
 * The author can be reached at scarabaeus@convergence.de, 
 * the project's page is at http://linuxtv.org/dvd/
 *
 * Several changes (see debian/changelog) by Mirko Doelle <cooper@linvdr.org>
 */

#include <stdio.h>
#include "dvd_udf.h"

#define DEFAULTDEVICE "/dev/dvd"
#define VERSION "0.2"

int main (int argc, char* argv[]) {
  int i;
  int rpc_status;
  int vendor_resets;
  int user_resets;
  int region_mask;
  int rpc_scheme;
  char ch[3];

  // setbuf(stdin,NULL);

  if (argc>2) {
      fprintf(stderr, "ERROR: Wrong parameters. Use -h for help.\n");
      return -1;
  } else {
    if(argc==2 && argv[1][0]=='-' && argv[1][1]=='h' && argv[1][2]=='\0') {
      printf("regionset version %s -- reads/sets region code on DVD drives\n", VERSION);
      printf("Usage: %s [device]\n", argv[0]);
      printf("       where default device is /dev/dvd\n");
      return -1;
    }
    if(UDFOpenDisc(((argc==2)?argv[1]:DEFAULTDEVICE)) < 0) {
      fprintf(stderr, "ERROR: Could not open drive \"%s\".\n", ((argc>=1)?argv[1]:DEFAULTDEVICE));
      fprintf(stderr, "Ensure that there is a (readable) CD or DVD in the drive.\n");
      return -1;
    }
    if (UDFRPCGet(&rpc_status,&vendor_resets,&user_resets,&region_mask,&rpc_scheme)) {
      printf("ERROR: Could not retrieve region code settings from drive.\n");
    } else {
      printf("Current drive parameters for %s:\n", ((argc==2)?argv[1]:DEFAULTDEVICE));
      printf("  RPC Type: %s\n",((rpc_scheme==0)?"Phase I (Software)":((rpc_scheme==1)?"Phase II (Hardware)":"unknown")));
      printf("  RPC Status: %s",((rpc_status==0)?"no region code set":((rpc_status==1)?"active region code":((rpc_status==2)?"active region code, last change":"active region code, permanent"))));
      printf(" (bitmask=0x%02X)\n", region_mask);
      if(rpc_status>0) {
        printf("  Drive plays discs from this region(s):");
        for (i=0; i<8; i++) {
          if (!(region_mask&(1<<i))) printf(" %d",i+1);
        }
        printf("\n");
      }
      printf("  Vendor may reset the RPC %d times\n", vendor_resets);
      printf("  User is allowed change the region setting %d times\n", user_resets);
    }
    printf("Would you like to change the region setting for this drive? [y/n]: ");
    fgets(ch,3,stdin);
    if ((ch[0]=='y') || (ch[0]=='Y')) {
      printf("Enter the new region number for your drive [1..8]: ");
      fgets(ch,3,stdin);
      if ((ch[0]<'0') || (ch[0]>'8')) {
        printf("ERROR: Illegal region code.\n");
        goto ERROR;
      }
      i=255;
      if (ch[0]>='1')
        i^=(1<<(ch[0]-'1'));
      if (i==region_mask) {
        printf("The drive is already configured for this region, aborting.\n");
        goto ERROR;
      }
      printf("New RPC bitmask is 0x%02X, ok? [y/n]: ",i);
      fgets(ch,3,stdin);
      if ((ch[0]=='y') || (ch[0]=='Y')) {
        if (UDFRPCSet(i)) {
          printf("ERROR: Can't set region code.\n");
          goto ERROR;
        } else {
          printf("Region code set successfully.\n");
        }
      }
    }
    UDFCloseDisc();
  }
  return 0;

  ERROR:
  UDFCloseDisc();
  return -1;
}
