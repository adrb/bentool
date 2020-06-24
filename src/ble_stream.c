/*
 *
 * Adrian Brzezinski (2020) <adrian.brzezinski at adrb.pl>
 * License: GPLv2+
 *
 */

#include "bentool.h"

ble_bonding_t *ble_bonding = NULL;
ble_pkt_stream_t *ble_stream = NULL;

// BT Core 5.0 spec, section 2.2.1 and 2.2.2
//
// Verification example with openssl for 4a:a0:d4:ff:c8:57 against IRK key e2270523033eb8f92204cba9ea221cf3 :
//  # printf "\x00\x00\x00\x00\x0\x0\x0\x0\x0\x0\x0\x0\x0\x4a\xa0\xd4" | openssl enc -p -aes-128-ecb -K "e2270523033eb8f92204cba9ea221cf3" -nosalt -nopad -out /tmp/bin && hexdump /tmp/bin
int ble_resolve_rpa(bdaddr_t *bda, uint8_t irk[16]) {

  AES_KEY aes_ekey;
  uint8_t in[16], out[16];

  if ( !bda || (bda->b[5]&0xc0) != 0x40 ||                  // is it resolvable device address?
       !memcmp(irk,"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16)  // key needs to be set
       ) {
      return 1;
  }

  memset(in, 0, 16);
  memset(out, 0, 16);

  // set 24bit random value (prand)
  in[13] = bda->b[5];
  in[14] = bda->b[4];
  in[15] = bda->b[3];

  AES_set_encrypt_key(irk, 128, &aes_ekey);
  AES_encrypt(in, out, &aes_ekey);

  // hash matched?
  if ( out[13] == bda->b[2] && out[14] == bda->b[1] && out[15] == bda->b[0] )
    return 0;

return 1;
}

int ble_bonding_add(ble_bonding_t *new_bk) {

  ble_bonding_t *bk;

  if ( !new_bk || !new_bk->name ) return 1;

  for ( bk = ble_bonding ; bk ; bk = bk->next ) {
    if ( !strncmp(new_bk->name, bk->name, strlen(bk->name)-1 ) )
      break;
  }

  // reset existing bonding
  if ( bk ) {

    if ( memcmp(new_bk->bda_public.b, "\0\0\0\0\0\0", 6) ) {
      bacpy(&bk->bda_public, &new_bk->bda_public);
    }

    if ( memcmp(new_bk->irk, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) ) {
      memcpy(bk->irk, new_bk->irk, 16);
    }

    free(new_bk->name);
    free(new_bk);
  } else {

    new_bk->next = ble_bonding;
    ble_bonding = new_bk;
  }

return 0;
}

void ble_bonding_print() {
  ble_bonding_t *bk;
  char addr[18];

  for ( bk = ble_bonding ; bk ; bk = bk->next ) {

    if ( bk->name ) {
      printf("Name: %s, ", bk->name);
    }

    if ( memcmp(bk->bda_public.b, "\0\0\0\0\0\0", 6) ) {
      ba2str(&bk->bda_public, addr);
      printf("BDA: %s, ", addr);
    }

    printf("IRK: ");
    printhex((void*)bk->irk, 16);
    printf("\n");

  }
}

void ble_stream_free_p( ble_pkt_stream_t *bps ) {

  ble_pkt_stream_t *nexts;
  ble_pkt_t *pkt, *older;

  if ( !bps ) return;

  while ( bps ) {

    pkt = bps->pkt_latest;

    while ( pkt ) {

      ble_pkt_t *older = pkt->older;

      ble_pkt_free(pkt);

      pkt = older;
    }

    nexts = bps->next;

    free(bps);

    bps = nexts;
  }

}

// deallocate packets captured during previous scan
void ble_stream_free() {

  if ( ble_stream ) {
    ble_stream_free_p(ble_stream);
    ble_stream = NULL;
  }
}

int ble_stream_dump(char *filename) {

  ble_pkt_stream_t *bps;
  ble_pkt_t *pkt;

  if ( !filename ) return -1;

  FILE *f = fopen(filename, "w");

  for ( bps = ble_stream ; bps ; bps = bps->next ) {

    // Dump in order of receiptment
    for ( pkt = bps->pkt_head ; pkt ; pkt = pkt->newer ) {

      print_busyloop();

      char addr[18];
      ba2str(&(pkt->bda), addr);

      fprintf(f, "%ld,%ld,%s,%d,",
        pkt->recv_time.tv_sec, pkt->recv_time.tv_usec,
        addr, pkt->rssi);

      switch (pkt->data_type) {

      case BLE_ADV_INFO:

        for ( int d = 0 ; d < sizeof(le_advertising_info) ; d++ )
          fprintf(f, "%02x", ((uint8_t*)pkt->data.advinfo)[d]);

        for ( int d = 0 ; d < pkt->data.advinfo->length ; d++ )
          fprintf(f, "%02x", pkt->data.advinfo->data[d]);

      break;

      case BLE_GA_EN:

        ;

        ble_ga_adv_t *ga_info = pkt->data.ga;

        for ( int d = 0 ; d < sizeof(ble_ga_adv_t) ; d++ )
          fprintf(f, "%02x", ((uint8_t*)ga_info)[d]);

      break;
      }

      fprintf(f,"\n");
    }
  }

  fflush(f);
  fclose(f);

  // clear busy loop char
  printf("\b");
  fflush(stdout);

return 0;
}

int ble_stream_load(char *filename) {

  FILE *f;
  ble_pkt_t *pkt = NULL;
  char buf[4096];
  char *tok;
  unsigned int val;
  int ret = 0, col;

  if ( !filename ) return -1;

  if ( !(f = fopen(filename, "r")) ) {
    perror("Coudn't load file");
    return -1;
  }

  ble_stream_free();

  while ( fgets(buf, sizeof(buf), f)  ) {

//    print_busyloop();

    for ( col = 0, tok = strtok(buf, ",") ; tok && *tok ; tok = strtok(NULL, ",\n"), col++ ) {

//      printf("%i: %s\n", col, tok);

      // alocate new pkt on column 0
      if ( !pkt && (pkt = calloc(1, sizeof(ble_pkt_t) )) == NULL ) {
        goto badv_load_csv_enomem;
      }

      switch ( col % 5 ) {
      case 0:
        pkt->recv_time.tv_sec = atol(tok);
      break;
      case 1:
        pkt->recv_time.tv_usec = atol(tok);
      break;
      case 2:
        str2ba(tok, &pkt->bda);
      break;
      case 3:
         pkt->rssi = atoi(tok);
      break;
      case 4:
        if ( strncmp(tok, "17166ffd", 8) ) {
          pkt->data_type = BLE_ADV_INFO;

          le_advertising_info *info_tok = (le_advertising_info*)tok;

          if ( (pkt->data.advinfo = (le_advertising_info*) malloc(sizeof(le_advertising_info)+info_tok->length)) == NULL ) {
            goto badv_load_csv_enomem;
          }

          for ( int i = 0; i < sizeof(le_advertising_info) << 1 && tok[i] ; i += 2 ) {
            val = 0;
            sscanf(tok+i, "%02x", &val);

            ((uint8_t*)pkt->data.advinfo)[i >> 1] = val & 0xff;
          }

          for ( int i = 0; i < pkt->data.advinfo->length << 1 && tok[i] ; i += 2 ) {
            val = 0;
            sscanf(tok+i, "%02x", &val);

            pkt->data.advinfo->data[i >> 1] = val & 0xff;
          }

        } else {
          pkt->data_type = BLE_GA_EN;

          if ( (pkt->data.ga = (ble_ga_adv_t*) malloc(sizeof(ble_ga_adv_t))) == NULL ) {
            goto badv_load_csv_enomem;
          }

          for ( int i = 0; i < sizeof(ble_ga_adv_t) << 1 && tok[i] ; i += 2 ) {
            val = 0;
            sscanf(tok+i, "%02x", &val);

            ((uint8_t*)pkt->data.ga)[i >> 1] = val & 0xff;
          }

        }

        // Last column, push pkt to stream
        ble_pkt_print(pkt, 0);
        printf("\n");

        if ( (ret = ble_stream_pkt_add(pkt)) < 0 )
          goto badv_load_csv_exit;

        pkt = NULL;

      break;

      default:
        fprintf(stderr, "Unknown line format!");
        ret = -1;
        goto badv_load_csv_exit;
      }

    }
  }

badv_load_csv_exit:

  // clear busy loop char
//  printf("\b");
//  fflush(stdout);

  fclose(f);

return ret;

badv_load_csv_enomem:

  fclose(f);

  perror("Could not allocate packet");
  exit(ENOMEM);

return -1;
}

int ble_stream_pkt_add( ble_pkt_t *pkt ) {

  ble_pkt_stream_t *bps, *bps_free = NULL;

  if ( !pkt ) return -1;

  // Assign packet to stream, if no match then bps = NULL
  for ( bps = ble_stream ; bps ; bps = bps->next ) {

    // mark free stream
    if ( !bps->pkt_latest ) {
      bps_free = bps;
      continue;
    }

    // Same BT Address
    if ( !bacmp(&bps->pkt_latest->bda, &pkt->bda) ) {
      break;
    }

    // Same RPI and AEM
    if ( pkt->data_type == BLE_GA_EN && bps->pkt_latest->data_type == BLE_GA_EN &&
         !memcmp(pkt->data.ga->rpi, bps->pkt_latest->data.ga->rpi, 16) &&
         !memcmp(pkt->data.ga->aem, bps->pkt_latest->data.ga->aem, 4)
       ) {

      // BT Address changed, so set RPA change time
      bps->rpa_last_change.tv_sec = pkt->recv_time.tv_sec;
      bps->rpa_last_change.tv_usec = pkt->recv_time.tv_usec;

      break;
    }

  }

  // no match found
  if ( !bps ) {

    if ( bps_free ) {

      bps = bps_free;

    } else {

      // insert new stream
      if ( (bps = calloc( 1, sizeof(ble_pkt_stream_t) )) == NULL ) {
        perror("Error while creating pkt stream");
        exit(ENOMEM);
      }

      bps->next = ble_stream;
      bps->prev = NULL;
      if ( ble_stream ) ble_stream->prev = bps;
      ble_stream = bps;
    }

  }

  // add packet to selected chain
  ble_pkt_t *older = bps->pkt_latest;

  if ( older )
    older->newer = pkt;

  pkt->older = older;
  pkt->newer = NULL;

  bps->pkt_latest = pkt;
  if ( !bps->pkt_head )
    bps->pkt_head = pkt;

return 0;
}

// Gather stream metadata
void ble_stream_meta() {

  ble_pkt_stream_t *bps;
  ble_pkt_t *pkt;
  uint64_t gap;

  for ( bps = ble_stream ; bps ; bps = bps->next ) {

    bps->pkt_gap_usum = 0;
    bps->pkts = 0;

    for ( pkt = bps->pkt_latest ; pkt && pkt->older ; pkt = pkt->older ) {

      gap = (pkt->recv_time.tv_sec*1000000 + pkt->recv_time.tv_usec) - (pkt->older->recv_time.tv_sec*1000000 + pkt->older->recv_time.tv_usec);

      // Maximum allowed interval between packets is 10.24sec
      if ( gap > 10240000 ) continue;

      bps->pkt_gap_usum += (pkt->recv_time.tv_sec*1000000 + pkt->recv_time.tv_usec) - (pkt->older->recv_time.tv_sec*1000000 + pkt->older->recv_time.tv_usec);
      bps->pkts++;

      /* Now set inside of ble_stream_pkt_add
      // Set RPA change time if addresses don't match but RPI and AEM does
      if ( !bps->rpa_last_change.tv_sec && bacmp(&pkt->bda, &pkt->older->ba) ) {

        if ( !memcmp(pkt->data.ga->rpi, pkt->older->data.ga->rpi, 16) &&
            !memcmp(pkt->data.ga->aem, pkt->older->data.ga->aem, 4) ) {

          bps->rpa_last_change.tv_sec = pkt->recv_time.tv_sec;
          bps->rpa_last_change.tv_usec = pkt->recv_time.tv_usec;
        }
      }
      */

    }
  }

}

// Keep in mind that it process data in reverse order (from newest packet to oldest)
int ble_stream_track() {

  ble_bonding_t *bk;
  ble_pkt_stream_t *bps_older, *bps_newer;
  ble_pkt_t *pkt, *last_pkt, *next_pkt;
  double newer_avg_gap, older_avg_gap;
  uint64_t bps_rpa_gap;
  int merges = 0, older_index, newer_index, bonded;

  if ( !ble_stream ) {
    fprintf(stderr, "No data to track\n");
    return -1;
  }

  ble_stream_meta();

  // Merge streams
  for ( bps_older = ble_stream, older_index = 0 ;
      bps_older ;
      bps_older = bps_older->next, older_index++ ) {

    // Search for last GA packet in chain
    for ( pkt = bps_older->pkt_latest ; pkt && pkt->data_type != BLE_GA_EN ; pkt = pkt->older ) ;

    if ( !pkt ) continue;

    last_pkt = pkt;

    // Match last packet with first packet belonging to next EN stream
    for ( bps_newer = ble_stream, newer_index = 0 ;
        bps_newer ;
        bps_newer = bps_newer->next, newer_index++ ) {

      if ( bps_newer == bps_older ) continue;

      // Search for earliest GA packet in stream
      next_pkt = NULL;
      for ( pkt = bps_newer->pkt_latest ; pkt ; pkt = pkt->older ) {
        if ( pkt->data_type == BLE_GA_EN )
          next_pkt = pkt;
      }

      // No GA packets in this stream
      if ( !next_pkt ) continue;

      // check bonding
      for ( bonded = 0, bk = ble_bonding ; bk ; bk = bk->next ) {

        // Resolved to the same device
        if ( !ble_resolve_rpa( &next_pkt->bda, bk->irk) &&
            !ble_resolve_rpa( &last_pkt->bda, bk->irk) ) {

          bonded = 1;
          break;
        }
      }

      bps_rpa_gap = 0;

      // No bonding, so we guess
      if ( !bonded ) {
        // Next packet before our device last packet?
        if ( next_pkt->recv_time.tv_sec < last_pkt->recv_time.tv_sec ) {
          continue;
        }

        // Too long packet gap?
        if ( (next_pkt->recv_time.tv_sec - last_pkt->recv_time.tv_sec) > 11 ) {
          continue;
        }

        // Average stream gap too different?
        older_avg_gap = ( (double)bps_newer->pkt_gap_usum / (double)bps_newer->pkts )/1000000.0;
        newer_avg_gap = ( (double)bps_older->pkt_gap_usum / (double)bps_older->pkts )/1000000.0;
        if ( fabs(newer_avg_gap - older_avg_gap) > 50.0 ) { // allowed 50ms diff
          continue;
        }

        // Check RPA interval if it's set
        if ( bps_newer->rpa_last_change.tv_sec && bps_older->rpa_last_change.tv_sec ) {
          bps_rpa_gap = labs(tvusec(&bps_newer->rpa_last_change) - tvusec(&bps_older->rpa_last_change));

          // 15min with some variation
          if ( bps_rpa_gap > 910*1000000 && bps_rpa_gap < 890*1000000 ) continue;

        } else {
          bps_rpa_gap = 0;
        }

        // RSSI more or less the same?
        if ( abs(next_pkt->rssi - last_pkt->rssi) > 20 ) {
          continue;
        }
      }


      printf("Merging stream %d to %d (%s):\n\t newer - average gap between packets %.3lfs, last RPA change ",
        older_index, newer_index, bk ? bk->name : "not bonded",
        ( (double)bps_newer->pkt_gap_usum / (double)bps_newer->pkts )/1000000.0);
      print_tv(&bps_newer->rpa_last_change);
      printf(", RPA inverval %.3lfs\n\t  head ", bps_newer->rpa_interval_us/1000000.0 );
      ble_pkt_print(bps_newer->pkt_head, 0);
      printf("\n\t  tail ");
      ble_pkt_print(bps_newer->pkt_latest, 0);

      printf("\n\t older - average gap between packets %.3lfs, last RPA change ",
        ( (double)bps_older->pkt_gap_usum / (double)bps_older->pkts )/1000000.0);
      print_tv(&bps_older->rpa_last_change);
      printf(", RPA inverval %.3lfs\n\t  head ", bps_older->rpa_interval_us/1000000.0 );
      ble_pkt_print(bps_older->pkt_head, 0);
      printf("\n\t  tail ");
      ble_pkt_print(bps_older->pkt_latest, 0);

      printf("\n");

      // If it's the same device, merge older stream to newer
      pkt = bps_newer->pkt_head;

      pkt->older = bps_older->pkt_latest;
      pkt->older->newer = pkt;

      bps_newer->pkt_head = bps_older->pkt_head;
      bps_newer->pkts += bps_older->pkts;
      bps_newer->pkt_gap_usum += bps_older->pkt_gap_usum;
      if ( bps_rpa_gap ) bps_newer->rpa_interval_us = bps_rpa_gap;

      // we moving back in time, so copy over from older stream if it's set
      if ( bps_older->rpa_last_change.tv_sec ) {
        bps_newer->rpa_last_change.tv_sec = bps_older->rpa_last_change.tv_sec;
        bps_newer->rpa_last_change.tv_usec = bps_older->rpa_last_change.tv_usec;
      }

      // release older stream
      bps_older->pkt_head = NULL;
      bps_older->pkt_latest = NULL;
      bps_older->pkts = 0;
      bps_older->pkt_gap_usum = 0;
      bps_older->rpa_interval_us = 0;
      memset((void*)&bps_older->rpa_last_change,'0',sizeof(struct timeval));

      merges++;

      break;
    }
  }

return merges;
}

// Keep in mind that it process data in reverse order (from newest packet to oldest)
void ble_stream_print() {
// /*
  ble_pkt_stream_t *bps;
  ble_pkt_t *seen_pkt, *pkt;
  int i = 0;

  printf("\n");

  for ( bps = ble_stream ; bps ; bps = bps->next, i++ ) {

    // Print EN chain
    for ( pkt = bps->pkt_latest, seen_pkt = NULL ; pkt ; pkt = pkt->older ) {

      if ( pkt->data_type != BLE_GA_EN ) continue;

      // Print only not seen data
      if ( !seen_pkt ||
            memcmp(pkt->data.ga->rpi, seen_pkt->data.ga->rpi, 16) ||
            memcmp(pkt->data.ga->aem, seen_pkt->data.ga->aem, 4) ||
            bacmp(&pkt->bda, &seen_pkt->bda) ) {

        seen_pkt = pkt;

        printf("Device %d, ", i);
        ble_pkt_print(pkt, 0);
        printf("\n");

      }

    }
  }

/*

  ble_pkt_stream_t *bps;
  ble_pkt_t *tail_pkt, *head_pkt, *pkt;
  double time_sum;
  uint32_t pkts_num, i = 0;

  for ( bps = ble_stream ; bps ; bps = bps->next, i++ ) {

    // Print EN chain
    for ( pkt = bps->pkt_latest, tail_pkt = NULL ; pkt ; pkt = pkt->older ) {

      if ( pkt->data_type != BLE_GA_EN ) continue;

      if ( !tail_pkt ) {
        // Save lately received packet from notification stream for further comparison
        tail_pkt = pkt;
        head_pkt = pkt;

        // average gap
        time_sum = 0.0;
        pkts_num = 1;
        continue;
      }

      // Compare head with previously caught packet
      if (
          memcmp(pkt->data.ga->rpi, head_pkt->data.ga->rpi, 16) ||
            memcmp(pkt->data.ga->aem, head_pkt->data.ga->aem, 4) ||
            bacmp(&pkt->bda, &head_pkt->bda) ||
            !pkt->older // we are at begining of whole device stream
         ) {

        if ( !pkt->older ) {
          // Move head to first packet in the stream
          head_pkt = pkt;
        }

        printf("Stream %d, average gap between packets %.3lf, last RPA change ",
            i, ( (double)bps->pkt_gap_usum / (double)bps->pkts )/1000000.0);
        print_tv(&bps->rpa_last_change);
        printf("\n");

        printf("\tbroadcasted %.3lf seconds, average gap between packets %.3lf\n",
          (tail_pkt->recv_time.tv_sec + (tail_pkt->recv_time.tv_usec - head_pkt->recv_time.tv_usec)/1000000.0 - head_pkt->recv_time.tv_sec),
          ( (double)time_sum / (double)pkts_num ) );

        printf("\thead ");
        ble_pkt_print(head_pkt, 0);
        printf("\n\ttail ");
        ble_pkt_print(tail_pkt, 0);
        printf("\n");

        // We are already at start of whole stream
        if ( pkt->older ) {
          printf("\ttail of previous stream (gap %.3lf seconds)\n\t     ",
            (head_pkt->recv_time.tv_sec + (head_pkt->recv_time.tv_usec - pkt->recv_time.tv_usec)/1000000.0 - pkt->recv_time.tv_sec));
          ble_pkt_print(pkt, 0);
          printf("\n");
        }

        tail_pkt = pkt; // Mark changed data tail
      }

      time_sum += (head_pkt->recv_time.tv_sec + (head_pkt->recv_time.tv_usec - pkt->recv_time.tv_usec)/1000000.0 - pkt->recv_time.tv_sec);
      pkts_num++;

      // Move stream head
      head_pkt = pkt;

    }
  }
// */
}

