//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "predictor.h"
// #include <iostream>
// using namespace std;

//
// TODO:Student Information
//
const char *studentName = "Abhinandan Sharma";
const char *studentID = "A69041851";
const char *email = "abs021@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 15; // Number of bits used for Global History

int ghistoryBits_tournament = 12; // Number of bits used for Global History
int lhistoryBits_tournament = 16; // Number of bits used for Local History

int ghistoryBits_tage_T4 = 64; // Number of bits used for Global History for T4
int ghistoryBits_tage_T3 = 32; // Number of bits used for Global History for T3
int ghistoryBits_tage_T2 = 16; // Number of bits used for Global History for T2
int ghistoryBits_tage_T1 = 8; // Number of bits used for Global History for T1

int tage_tag_bits_T4 = 8; // Number of bits used for Tag for T4
int tage_tag_bits_T3 = 8; // Number of bits used for Tag for T3
int tage_tag_bits_T2 = 8; // Number of bits used for Tag for T2
int tage_tag_bits_T1 = 8; // Number of bits used for Tag for T1

int tage_u_bits = 2; // Number of bits used for usefulness counter

int tage_predictor_bits = 3; // Number of bits used for predictor counter

int tage_pred_table_length_T0 = 10; // Length of the base predictor table T0 in bits
int tage_pred_table_length_T1 = 10; // Length of the component table T1 in bits
int tage_pred_table_length_T2 = 10; // Length of the component table T2 in bits
int tage_pred_table_length_T3 = 10; // Length of the component table T3 in bits
int tage_pred_table_length_T4 = 10; // Length of the component table T4 in bits

int bpType;            // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//


// gshare
uint8_t *bht_gshare;
uint64_t ghistory;

// tournament
uint8_t   *bht_local_tournament;
uint8_t   *bht_global_tournament;
uint8_t   *choice_bht_tournament;
uint16_t  *lht_tournament;
uint32_t  ghistory_tournament;

//TAGE
uint8_t  *tage_bht_T0;
uint8_t  *tage_bht_T1;
uint8_t  *tage_bht_T2;
uint8_t  *tage_bht_T3;
uint8_t  *tage_bht_T4;

uint16_t  *tage_tag_T1;
uint16_t  *tage_tag_T2;
uint16_t  *tage_tag_T3;
uint16_t  *tage_tag_T4;

uint8_t   *tage_u_T1;
uint8_t   *tage_u_T2;
uint8_t   *tage_u_T3;
uint8_t   *tage_u_T4;

uint64_t  ghistory_tage;
// uint8_t   ghistory_tage_num_blks = 4;
// uint64_t  ghistory_tage[ghistory_tage_num_blks];   // Global History Register for TAGE with 256 bits storage

uint32_t  u_reset_preiod = 512000; // Period after which usefulness counters are reset
uint32_t  usefulness_counter_reset_count = 0;

int oc = 0;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor

// gshare functions
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < bht_entries; i++)
  {
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch (bht_gshare[index])
  {
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}

// tournament functions

void init_tournament()
{
  int bht_global_entries = 1 << ghistoryBits_tournament;
  bht_global_tournament = (uint8_t *)malloc(bht_global_entries * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < bht_global_entries; i++)
  {
    bht_global_tournament[i] = WN;
  }

  int bht_local_entries = 1 << lhistoryBits_tournament;
  bht_local_tournament = (uint8_t *)malloc(bht_local_entries * sizeof(uint8_t));
  for (i = 0; i < bht_local_entries; i++)
  {
    bht_local_tournament[i] = N00;
  }

  int lht_tournament_entries = 1 << lhistoryBits_tournament;
  lht_tournament = (uint16_t *)malloc(lht_tournament_entries * sizeof(uint16_t));
  for (i = 0; i < lht_tournament_entries; i++)
  {
    lht_tournament[i] = 0;
  } 

  int choice_bht_entries = 1 << ghistoryBits_tournament;
  choice_bht_tournament = (uint8_t *)malloc(choice_bht_entries * sizeof(uint8_t));
  for (i = 0; i < choice_bht_entries; i++)
  {
    choice_bht_tournament[i] = WLP;
  }

  ghistory_tournament = 0;
}

uint8_t tournament_predict(uint32_t pc)
{
  uint32_t bht_global_entries = 1 << ghistoryBits_tournament;
  uint32_t bht_local_entries = 1 << lhistoryBits_tournament;
  uint32_t pc_lower_bits = pc & (bht_local_entries - 1);
  uint32_t bht_local_index = lht_tournament[pc_lower_bits];
  uint32_t bht_global_index = ghistory_tournament & (bht_global_entries - 1);
  
  switch (choice_bht_tournament[bht_global_index])
  {
    case SGP:
    case WGP:
      switch (bht_global_tournament[bht_global_index])
      {
        case WN:
          return NOTTAKEN;
        case SN:
          return NOTTAKEN;
        case WT:
          return TAKEN;
        case ST:
          return TAKEN;
        default:
          printf("Warning: Undefined state of entry in TOURNAMENT gobal BHT prediction!\n");
          return NOTTAKEN;
      }  

    case SLP:
    case WLP:
      switch (bht_local_tournament[bht_local_index])
      {
        case N11:
          return NOTTAKEN;
        case N10:
          return NOTTAKEN;
        case N01:
          return NOTTAKEN;
        case N00:
          return NOTTAKEN;
        case T00:
          return TAKEN;
        case T01:
          return TAKEN;
        case T10:
          return TAKEN;
        case T11:
          return TAKEN;
        default:
          printf("Warning: Undefined state of entry in TOURNAMENT local BHT prediction!\n");
          printf("value = %d\n", bht_local_tournament[bht_local_index]);
          printf("index = %d\n", bht_local_index);
          return NOTTAKEN;
      }
    
    default:
      printf("Warning: Undefined state of entry in TOURNAMENT choice BHT prediction!\n");
      return NOTTAKEN;  
  }
}

void update_bht_global_tournament(uint32_t bht_global_index, uint8_t outcome)
{
  // Update state of entry in global bht based on outcome
  switch (bht_global_tournament[bht_global_index])
  {
  case WN:
    bht_global_tournament[bht_global_index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_global_tournament[bht_global_index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_global_tournament[bht_global_index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_global_tournament[bht_global_index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in TOURNAMENT global BHT training!\n");
    break;
  } 
}

void update_bht_local_tournament(uint32_t bht_local_index, uint8_t outcome)
{
  // Update state of entry in local bht based on outcome
  switch (bht_local_tournament[bht_local_index])
  {
  case N11:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? N10 : N11;
    break;
  case N10:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? N01 : N11;
    break;
  case N01:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? N00 : N10;
    break;
  case N00:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? T00 : N01;
    break;
  case T00:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? T01 : N00;
    break;
  case T01:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? T10 : T00;
    break;
  case T10:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? T11 : T01;
    break;
  case T11:
    bht_local_tournament[bht_local_index] = (outcome == TAKEN) ? T11 : T10;
    break;
  default:
    printf("Warning: Undefined state of entry in TOURNAMENT local BHT training!\n");
    break;
  } 
}

void update_choice_bht_tournament(uint32_t bht_global_index, uint8_t outcome, uint8_t prefer_global_predictor)
{
  // Update state of entry in choice bht based on which predictor is preferred
  if (prefer_global_predictor == 1) {
    // prefer global predictor
    switch (choice_bht_tournament[bht_global_index])
    {
      case SGP:
        choice_bht_tournament[bht_global_index] = SGP;
        break;
      case WGP:
        choice_bht_tournament[bht_global_index] = SGP;
        break;
      case WLP:
        choice_bht_tournament[bht_global_index] = WGP;
        break;
      case SLP:
        choice_bht_tournament[bht_global_index] = WLP;
        break;
      default:
        printf("Warning: Undefined state of entry in TOURNAMENT choice BHT training!\n");
        break;
    } 
    return;
  }
  else {
    // prefer local predictor
    switch (choice_bht_tournament[bht_global_index])
    {
      case SGP:
        choice_bht_tournament[bht_global_index] = WGP;
        break;
      case WGP:
        choice_bht_tournament[bht_global_index] = WLP;
        break;
      case WLP:
        choice_bht_tournament[bht_global_index] = SLP;;
        break;
      case SLP:
        choice_bht_tournament[bht_global_index] = SLP;
        break;
      default:
        printf("Warning: Undefined state of entry in TOURNAMENT choice BHT training!\n");
        break;  
    }
  }
}

void update_ghistory_tournament(uint8_t outcome)
{
  // Update history register
  ghistory_tournament = ((ghistory_tournament << 1) | outcome) & ((1 << ghistoryBits_tournament) - 1);
}

void update_lht_tournament(uint32_t pc_lower_bits, uint8_t outcome)
{
  // Update local history table
  lht_tournament[pc_lower_bits] = ((lht_tournament[pc_lower_bits] << 1) | outcome) & ((1 << lhistoryBits_tournament) - 1);
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
  uint32_t bht_global_entries = 1 << ghistoryBits_tournament;
  uint32_t bht_local_entries = 1 << lhistoryBits_tournament;
  uint32_t pc_lower_bits = pc & (bht_local_entries - 1);
  uint32_t bht_local_index = lht_tournament[pc_lower_bits];
  uint32_t bht_global_index = ghistory_tournament & (bht_global_entries - 1);

  uint8_t bht_global_value = bht_global_tournament[bht_global_index];
  uint8_t bht_local_value = (bht_local_tournament[bht_local_index]);
  
  uint8_t bht_global_prediction = (bht_global_value > 1) ? TAKEN : NOTTAKEN;
  uint8_t bht_local_prediction = (bht_local_value > 3) ? TAKEN : NOTTAKEN;

  uint8_t prediction_match_condition = ((bht_global_prediction == TAKEN) & (bht_local_prediction == TAKEN)) || ((bht_global_prediction == NOTTAKEN) & (bht_local_prediction == NOTTAKEN));
  uint8_t global_predictor_chosen = (choice_bht_tournament[bht_global_index] > 1)? 1: 0;

  uint8_t prefer_global_predictor = 0;

  if (prediction_match_condition == 1) {
    // both predictors made the same prediction, no need to update choice predictor
    if (global_predictor_chosen == 1) {
      // If chooser had chosen global predictor, update global predictor
      // update_bht_global_tournament(bht_global_index, outcome);

    } else {
      // update local predictor
      // update_bht_local_tournament(bht_local_index, outcome);
    }

  } else {
    // predictors made different predictions, update choice predictor accordingly
    
    if (global_predictor_chosen == 1) {
      if (bht_global_prediction == outcome) {
        // global predictor was correct, update the Chooser to prefer global predictor more
        prefer_global_predictor = 1;
        update_choice_bht_tournament(bht_global_index, outcome, prefer_global_predictor);
        // update_bht_global_tournament(bht_global_index, outcome);

      } else {
        // global predictor was incorrect, update the Chooser to prefer local predictor more
        prefer_global_predictor = 0;
        update_choice_bht_tournament(bht_global_index, outcome, prefer_global_predictor);
        // update_bht_local_tournament(bht_local_index, outcome);
        // update_lht_tournament(pc_lower_bits, outcome);
      }

    } else {
      if (bht_local_prediction == outcome) {
        // local predictor was correct, update the Chooser to prefer local predictor more
        prefer_global_predictor = 0;
        update_choice_bht_tournament(bht_global_index, outcome, prefer_global_predictor);
        // update_bht_local_tournament(bht_local_index, outcome);
        // update_lht_tournament(pc_lower_bits, outcome);

      } else {
        // local predictor was incorrect, update the Chooser to prefer global predictor more
        prefer_global_predictor = 1;
        update_choice_bht_tournament(bht_global_index, outcome, prefer_global_predictor);
        // update_bht_global_tournament(bht_global_index, outcome);
      }
    }
  }

  update_bht_global_tournament(bht_global_index, outcome);
  update_bht_local_tournament(bht_local_index, outcome);
  update_lht_tournament(pc_lower_bits, outcome);
  update_ghistory_tournament(outcome);
}

void cleanup_tournament()
{
  free(bht_global_tournament);
  free(bht_local_tournament);
  free(choice_bht_tournament);
  free(lht_tournament);
}

// TAGE functions

void init_tage()
// TAGE Predictor Initialization
{ 
  uint8_t tage_pred_table_entries_t0 = 1 << tage_pred_table_length_T0;
  tage_bht_T0 = (uint8_t *)malloc(tage_pred_table_entries_t0 * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < tage_pred_table_entries_t0; i++)
  {
    tage_bht_T0[i] = N00;
  }
  uint8_t tage_pred_table_entries_t1 = 1 << tage_pred_table_length_T1;
  tage_bht_T1 = (uint8_t *)malloc(tage_pred_table_entries_t1 * sizeof(uint8_t));
  for (i = 0; i < tage_pred_table_entries_t1; i++)
  {
    tage_bht_T1[i] = N00;
  }
  uint8_t tage_pred_table_entries_t2 = 1 << tage_pred_table_length_T2;
  tage_bht_T2 = (uint8_t *)malloc(tage_pred_table_entries_t2 * sizeof(uint8_t));
  for (i = 0; i < tage_pred_table_entries_t2; i++)
  {
    tage_bht_T2[i] = N00;
  }
  uint8_t tage_pred_table_entries_t3 = 1 << tage_pred_table_length_T3;
  tage_bht_T3 = (uint8_t *)malloc(tage_pred_table_entries_t3 * sizeof(uint8_t));
  for (i = 0; i < tage_pred_table_entries_t3; i++)
  {
    tage_bht_T3[i] = N00;
  }
  uint8_t tage_pred_table_entries_t4 = 1 << tage_pred_table_length_T4;
  tage_bht_T4 = (uint8_t *)malloc(tage_pred_table_entries_t4 * sizeof(uint8_t));
  for (i = 0; i < tage_pred_table_entries_t4; i++)
  {
    tage_bht_T4[i] = N00;
  }

  // TAGE Tags Initialization
  uint8_t tage_tag_t1_entries = 1 << tage_pred_table_length_T1;
  tage_tag_T1 = (uint16_t *)malloc(tage_tag_t1_entries * sizeof(uint16_t));
  for (i = 0; i < tage_tag_t1_entries; i++)
  {
    tage_tag_T1[i] = 0;
  }
  uint8_t tage_tag_t2_entries = 1 << tage_pred_table_length_T2;
  tage_tag_T2 = (uint16_t *)malloc(tage_tag_t2_entries * sizeof(uint16_t));
  for (i = 0; i < tage_tag_t2_entries; i++)
  {
    tage_tag_T2[i] = 0;
  }
  uint8_t tage_tag_t3_entries = 1 << tage_pred_table_length_T3;
  tage_tag_T3 = (uint16_t *)malloc(tage_tag_t3_entries * sizeof(uint16_t));
  for (i = 0; i < tage_tag_t3_entries; i++)
  {
    tage_tag_T3[i] = 0;
  }
  uint8_t tage_tag_t4_entries = 1 << tage_pred_table_length_T4;
  tage_tag_T4 = (uint16_t *)malloc(tage_tag_t4_entries * sizeof(uint16_t));
  for (i = 0; i < tage_tag_t4_entries; i++)
  {
    tage_tag_T4[i] = 0;
  }

  // TAGE Usefulness Counters Initialization
  uint8_t tage_u_t1_entries = 1 << tage_pred_table_length_T1;
  tage_u_T1 = (uint8_t *)malloc(tage_u_t1_entries * sizeof(uint8_t));
  for (i = 0; i < tage_u_t1_entries; i++)
  {
    tage_u_T1[i] = 0;
  }
  uint8_t tage_u_t2_entries = 1 << tage_pred_table_length_T2;
  tage_u_T2 = (uint8_t *)malloc(tage_u_t2_entries * sizeof(uint8_t));
  for (i = 0; i < tage_u_t2_entries; i++) 
  {
    tage_u_T2[i] = 0;
  } 
  uint8_t tage_u_t3_entries = 1 << tage_pred_table_length_T3;
  tage_u_T3 = (uint8_t *)malloc(tage_u_t3_entries * sizeof(uint8_t));
  for (i = 0; i < tage_u_t3_entries; i++)
  {
    tage_u_T3[i] = 0;
  }
  uint8_t tage_u_t4_entries = 1 << tage_pred_table_length_T4;
  tage_u_T4 = (uint8_t *)malloc(tage_u_t4_entries * sizeof(uint8_t));
  for (i = 0; i < tage_u_t4_entries; i++)
  {
    tage_u_T4[i] = 0;
  }

  // TAGE Global History Initialization
  ghistory_tage = 0;
  // for(int i = 0; i < ghistory_tage_num_blks; ++i) ghistory_tage[i] = 0;
  // cout<<(int*)&tage_bht_T0[544]<<endl;
  if(tage_bht_T0[544] == 0){printf("T0 value after update in init: %d, %d\n", tage_bht_T0[544], oc++);}

}

// Folds 'val' (either PC or GHR) of 'len' bits into 'bits_out' bits
static uint32_t fold_bits_hash_1(uint64_t val, int len, int bits_out)
{
  uint32_t folded = 0;
  val = val & ((1ULL << len) - 1); // ensure only the lowest 'len' bits are used
  for (int i = 0; i < len; ++i)
      folded ^= ((val >> i) & 1U) << (i % bits_out);
  return folded & ((1U << bits_out) - 1);
}

// Folding function with distinct stride/offset for tag
static uint32_t fold_bits_hash_2(uint64_t val, int len, int bits_out, int stride, int offset)
{
  uint32_t folded = 0;
  val = val & ((1ULL << len) - 1);
  for (int i = 0; i < len; ++i)
      folded ^= ((val >> i) & 1U) << (((i * stride + offset) % bits_out));
  return folded & ((1U << bits_out) - 1);
}

// static uint64_t get_ghr_bits(uint64_t ghistory, int bits) {
//     // Works for up to 64 bits; for >64 use an array and process blocks, but your design uses 64b for max history now
//     if (bits >= 64) return ghistory;
//     return ghistory & ((1ULL << bits) - 1);
// }

// static uint32_t fold_bits_hash_1(uint64_t val, int len, int bits_out) {
//     uint32_t folded = 0;
//     val = val & ((1ULL << len) - 1); // mask to use only the lowest 'len' bits
//     for (int i = 0; i < len; ++i)
//         folded ^= ((val >> i) & 1U) << (i % bits_out);
//     return folded & ((1U << bits_out) - 1);
// }

// static uint32_t fold_bits_hash_2(uint64_t val, int len, int bits_out, int stride, int offset) {
//     uint32_t folded = 0;
//     val = val & ((1ULL << len) - 1);
//     for (int i = 0; i < len; ++i)
//         folded ^= ((val >> i) & 1U) << (((i * stride + offset) % bits_out));
//     return folded & ((1U << bits_out) - 1);
// }

// uint16_t compute_tag_index(uint32_t pc, int ghistoryBits_tage, int tage_pred_table_length) {
//     int pc_bits = sizeof(pc) * CHAR_BIT;
//     uint32_t folded_pc = fold_bits_hash_1(pc, pc_bits, tage_pred_table_length);
//     uint64_t effective_ghr = get_ghr_bits(ghistory_tage, ghistoryBits_tage);
//     uint32_t folded_ghr = fold_bits_hash_1(effective_ghr, ghistoryBits_tage, tage_pred_table_length);
//     return (folded_pc ^ folded_ghr) & ((1U << tage_pred_table_length) - 1);
// }

// uint16_t compute_tag(uint32_t pc, int ghistoryBits_tage, int tage_tag_bits) {
//     int pc_bits = sizeof(pc) * CHAR_BIT;
//     uint32_t folded_pc  = fold_bits_hash_2(pc, pc_bits, tage_tag_bits, 3, 1);
//     uint64_t effective_ghr = get_ghr_bits(ghistory_tage, ghistoryBits_tage);
//     uint32_t folded_ghr = fold_bits_hash_2(effective_ghr, ghistoryBits_tage, tage_tag_bits, 2, 2);
//     return (folded_pc ^ folded_ghr) & ((1U << tage_tag_bits) - 1);
// }


uint16_t compute_tag_index(uint32_t pc, int ghistoryBits_tage, int tage_pred_table_length)
{
  int pc_bits = sizeof(pc) * CHAR_BIT; // Number of bits in pc variable
  uint32_t folded_pc = fold_bits_hash_1(pc, pc_bits, tage_pred_table_length);
  uint32_t folded_ghr = fold_bits_hash_1(ghistory_tage, ghistoryBits_tage, tage_pred_table_length);
  return (folded_pc ^ folded_ghr) & ((1U << tage_pred_table_length) - 1);
}

uint16_t compute_tag(uint32_t pc, uint64_t ghistoryBits_tage, int tage_tag_bits)
{
  int pc_bits = sizeof(pc) * CHAR_BIT;
  // Fold PC and GHR with unique stride/offset for tag
  uint32_t folded_pc  = fold_bits_hash_2(pc, pc_bits, tage_tag_bits, 3, 1);
  uint32_t folded_ghr = fold_bits_hash_2(ghistory_tage, ghistoryBits_tage, tage_tag_bits, 2, 2);
  // Combine folded values to create the tag
  return (folded_pc ^ folded_ghr) & ((1U << tage_tag_bits) - 1);
}

uint8_t tage_predict(uint32_t pc)
{ 
  // cout<<oc<<endl;
  if(tage_bht_T0[544] != 0){printf("T0 value before update in predict: %d, %d\n", tage_bht_T0[544], oc++);}

  uint16_t tag_index_T4 = compute_tag_index(pc, ghistoryBits_tage_T4, tage_pred_table_length_T4);
  uint16_t tag_index_T3 = compute_tag_index(pc, ghistoryBits_tage_T3, tage_pred_table_length_T3);
  uint16_t tag_index_T2 = compute_tag_index(pc, ghistoryBits_tage_T2, tage_pred_table_length_T2);
  uint16_t tag_index_T1 = compute_tag_index(pc, ghistoryBits_tage_T1, tage_pred_table_length_T1);

  uint16_t tag_T4 = compute_tag(pc, ghistoryBits_tage_T4, tage_tag_bits_T4);
  uint16_t tag_T3 = compute_tag(pc, ghistoryBits_tage_T3, tage_tag_bits_T3);  
  uint16_t tag_T2 = compute_tag(pc, ghistoryBits_tage_T2, tage_tag_bits_T2);
  uint16_t tag_T1 = compute_tag(pc, ghistoryBits_tage_T1, tage_tag_bits_T1);

  if (tage_tag_T4[tag_index_T4] == tag_T4)
  {
    // T4 Hit
    return (tage_bht_T4[tag_index_T4] >= T00) ? TAKEN : NOTTAKEN;
  }
  else if (tage_tag_T3[tag_index_T3] == tag_T3)
  {
    // T3 Hit
    return (tage_bht_T3[tag_index_T3] >= T00) ? TAKEN : NOTTAKEN;
  }
  else if (tage_tag_T2[tag_index_T2] == tag_T2)
  {
    // T2 Hit
    return (tage_bht_T2[tag_index_T2] >= T00) ? TAKEN : NOTTAKEN;
  }
  else if (tage_tag_T1[tag_index_T1] == tag_T1)
  {
    // T1 Hit
    return (tage_bht_T1[tag_index_T1] >= T00) ? TAKEN : NOTTAKEN;
  }
  else
  {
    // T0 Prediction
    uint16_t pred_index_T0 = pc & ((1U << tage_pred_table_length_T0) - 1);
    return (tage_bht_T0[pred_index_T0] >= T00) ? TAKEN : NOTTAKEN;
  }
}

void update_tage_predictor(uint8_t *bht, uint16_t index, uint8_t outcome)
{
  // Update state of entry in bht based on outcome
  switch (bht[index])
  {
  case N11:
    bht[index] = (outcome == TAKEN) ? N10 : N11;
    break;
  case N10:
    bht[index] = (outcome == TAKEN) ? N01 : N11;
    break;
  case N01:
    bht[index] = (outcome == TAKEN) ? N00 : N10;
    break;
  case N00:
    bht[index] = (outcome == TAKEN) ? T00 : N01;
    break;
  case T00:
    bht[index] = (outcome == TAKEN) ? T01 : N00;
    break;
  case T01:
    bht[index] = (outcome == TAKEN) ? T10 : T00;
    break;
  case T10:
    bht[index] = (outcome == TAKEN) ? T11 : T01;
    break;
  case T11:
    bht[index] = (outcome == TAKEN) ? T11 : T10;
    break;
  default:
    printf("Warning: Undefined state of entry in TAGE BHT training!\n");
    printf("value = %d\n", bht[index]);
    printf("index = %d\n", index);  
    exit(1);
    break;
  } 
}

// void update_bimodal_predictor (uint8_t *bht, uint16_t index, uint8_t outcome)
// {
//   // Update state of entry in bht based on outcome
//   switch (bht[index])
//   {
//   case WN:
//     bht[index] = (outcome == TAKEN) ? WT : SN;
//     break;
//   case SN:
//     bht[index] = (outcome == TAKEN) ? WN : SN;
//     break;
//   case WT:
//     bht[index] = (outcome == TAKEN) ? ST : WN;
//     break;
//   case ST:
//     bht[index] = (outcome == TAKEN) ? ST : WT;
//     break;
//   default:
//     // printf("Warning: Undefined state of entry in Bimodal BHT training!\n");
//     // printf("value = %d\n", bht[index]);
//     // printf("index = %d\n", index);
//     break;
//   } 
// }

void update_usefulness_counter(uint8_t *u_table, uint16_t index, uint8_t pred, uint8_t outcome, int tage_pred_table_length)
{
  if (pred == outcome)
  {
    // prediction was correct, increment usefulness counter
    if (u_table[index] < ((1 << tage_u_bits) - 1))
    {
      u_table[index]++;
    }
  }
  else
  {
    // prediction was incorrect, decrement usefulness counter
    if (u_table[index] > 0)
    {
      u_table[index]--;
    }
  }
  
  // usefulness_counter_reset_count++;
  // // cout << sizeof(u_table) << sizeof(u_table[0]) << sizeof(u_table) / sizeof(u_table[0]) << endl;

  // if (usefulness_counter_reset_count == (u_reset_preiod / 2)) {
  //     int u_table_entries = 1 << tage_pred_table_length;
  //     // Assuming usefulness counters are unsigned and n bits wide:
  //     // Build a mask with all bits 1 except for the MSB
  //     unsigned mask = ~((unsigned)1 << 1);
  //     for (int i = 0; i < u_table_entries; i++) {
  //         u_table[i] &= mask; // Clear MSB
  //     }
  // }
  // else if (usefulness_counter_reset_count == u_reset_preiod) {
  //     int u_table_entries = 1 << tage_pred_table_length;
  //     // Build a mask with all bits 1 except for the LSB
  //     unsigned mask = ~1u;
  //     for (int i = 0; i < u_table_entries; i++) {
  //         u_table[i] &= mask; // Clear LSB
  //     }
  //     usefulness_counter_reset_count = 0; // Reset the period counter
  // }
}

void initialize_new_entry(uint8_t *bht, uint16_t *tag_array, uint8_t *u_array, uint16_t index, uint16_t tag)
{
  bht[index] = N00; // Set to weakly taken
  tag_array[index] = tag;
  u_array[index] = 0; // Strong not useful
}

void update_ghistory_tage(uint8_t outcome)
{
  // Update history register
  ghistory_tage = ((ghistory_tage << 1) | outcome);
  
  // const int elem_bits = sizeof(ghistory_tage[0]) * CHAR_BIT;
  // for (int i = ghistory_tage_num_blks - 1; i > 0; --i) {
  //   ghistory_tage[i] = (ghistory_tage[i] << 1) | (ghistory_tage[i-1] >> (elem_bits - 1));
  // }
  // ghistory_tage[0] = (ghistory_tage[0] << 1) | (outcome & 1);
}

void train_tage(uint32_t pc, uint8_t outcome)
{
  if(tage_bht_T0[544] == 48){printf("T0 value before update in train: %d, %d\n", tage_bht_T0[544], oc++);}

  uint16_t tag_index_T4 = compute_tag_index(pc, ghistoryBits_tage_T4, tage_pred_table_length_T4);
  uint16_t tag_index_T3 = compute_tag_index(pc, ghistoryBits_tage_T3, tage_pred_table_length_T3);
  uint16_t tag_index_T2 = compute_tag_index(pc, ghistoryBits_tage_T2, tage_pred_table_length_T2);
  uint16_t tag_index_T1 = compute_tag_index(pc, ghistoryBits_tage_T1, tage_pred_table_length_T1);
  uint16_t index_T0 = pc & ( (1U << tage_pred_table_length_T0) - 1);

  uint16_t tag_T4 = compute_tag(pc, ghistoryBits_tage_T4, tage_tag_bits_T4);
  uint16_t tag_T3 = compute_tag(pc, ghistoryBits_tage_T3, tage_tag_bits_T3);  
  uint16_t tag_T2 = compute_tag(pc, ghistoryBits_tage_T2, tage_tag_bits_T2);
  uint16_t tag_T1 = compute_tag(pc, ghistoryBits_tage_T1, tage_tag_bits_T1);
 
  uint8_t pred_value_T4 = tage_bht_T4[tag_index_T4];
  uint8_t pred_value_T3 = tage_bht_T3[tag_index_T3];
  uint8_t pred_value_T2 = tage_bht_T2[tag_index_T2];
  uint8_t pred_value_T1 = tage_bht_T1[tag_index_T1];
  uint8_t pred_value_T0 = tage_bht_T0[index_T0];

  uint8_t prediction_T4 = (pred_value_T4 >= T00) ? TAKEN : NOTTAKEN;
  uint8_t prediction_T3 = (pred_value_T3 >= T00) ? TAKEN : NOTTAKEN;
  uint8_t prediction_T2 = (pred_value_T2 >= T00) ? TAKEN : NOTTAKEN;
  uint8_t prediction_T1 = (pred_value_T1 >= T00) ? TAKEN : NOTTAKEN;
  uint8_t prediction_T0 = (pred_value_T0 >= T00) ? TAKEN : NOTTAKEN;

  uint8_t  pred, altpred;
  uint8_t *tage_u_table;
  uint16_t tage_u_table_index;
  int tage_pred_table_length;

  uint8_t eligible[4];
  uint32_t count = 0;
  uint8_t selected_table;

  if (tage_tag_T4[tag_index_T4] == tag_T4){
    // T4 hit, update only T4 predictor
    selected_table = 4;
    pred = prediction_T4;
    tage_u_table = tage_u_T4;
    tage_u_table_index = tag_index_T4;
    tage_pred_table_length = tage_pred_table_length_T4;
    // if (tage_bht_T4[tag_index_T4]>7) {printf("T4 value before update: %d, %d\n", tage_bht_T4[tag_index_T4], tag_index_T4);}
    update_tage_predictor(tage_bht_T4, tag_index_T4, outcome);
    // if (tage_bht_T4[tag_index_T4]>7) {printf("T4 value after update: %d, %d\n\n", tage_bht_T4[tag_index_T4], tag_index_T4);}

    if (tage_tag_T3[tag_index_T3] == tag_T3){
      altpred = prediction_T3;
    } else if ((tage_tag_T2[tag_index_T2] == tag_T2)){
      altpred = prediction_T2;
    } else if ((tage_tag_T1[tag_index_T1] == tag_T1)){
      altpred = prediction_T1;
    } else {
      altpred = prediction_T0;
    }
  } else if (tage_tag_T3[tag_index_T3] == tag_T3) {
    // T3 hit, update only T3 predictor
    selected_table = 3;
    pred = prediction_T3;
    tage_u_table = tage_u_T3;
    tage_u_table_index = tag_index_T3;
    tage_pred_table_length = tage_pred_table_length_T3;
    update_tage_predictor(tage_bht_T3, tag_index_T3, outcome);

    if (tage_tag_T2[tag_index_T2] == tag_T2){
      altpred = prediction_T2;
    } else if ((tage_tag_T1[tag_index_T1] == tag_T1)){
      altpred = prediction_T1;
    } else {
      altpred = prediction_T0;
    }
  } else if (tage_tag_T2[tag_index_T2] == tag_T2) {
    // T2 hit, update only T2 predictor
    selected_table = 2;
    pred = prediction_T2;
    tage_u_table = tage_u_T2;
    tage_u_table_index = tag_index_T2;
    tage_pred_table_length = tage_pred_table_length_T2;
    update_tage_predictor(tage_bht_T2, tag_index_T2, outcome);

    if (tage_tag_T1[tag_index_T1] == tag_T1){
      altpred = prediction_T1;
    } else {
      altpred = prediction_T0;
    }
  } else if (tage_tag_T1[tag_index_T1] == tag_T1) {
    // T1 hit, update only T1 predictor
    selected_table = 1;
    pred = prediction_T1;
    tage_u_table = tage_u_T1;
    tage_u_table_index = tag_index_T1;
    tage_pred_table_length = tage_pred_table_length_T1;
    update_tage_predictor(tage_bht_T1, tag_index_T1, outcome);
    altpred = prediction_T0;
  } else {
    // T0 hit, update only T0 predictor
    selected_table = 0;
    pred = prediction_T0;
    update_tage_predictor(tage_bht_T0, index_T0, outcome);
    altpred = prediction_T0;
  }

  if (pred != altpred) {
    // update usefulness counter of T4 predictor
    update_usefulness_counter(tage_u_table, tage_u_table_index, pred, outcome, tage_pred_table_length);
  }

  if (pred != outcome) {
    // misprediction handling and allocation of new entries

    // Check usefulness u==0 for each tables greater than the selected table
    if (selected_table < 1 && tage_u_T1[tag_index_T1] == 0) eligible[count++] = 1;
    if (selected_table < 2 && tage_u_T2[tag_index_T2] == 0) eligible[count++] = 2;
    if (selected_table < 3 && tage_u_T3[tag_index_T3] == 0) eligible[count++] = 3;
    if (selected_table < 4 && tage_u_T4[tag_index_T4] == 0) eligible[count++] = 4;

    if (count == 0) {
      // No allocation because all u's are non-zero. Decrement all u values for tables greater than the seleceted table
      if (selected_table < 1 && tage_u_T1[tag_index_T1] > 0) tage_u_T1[tag_index_T1]--;
      if (selected_table < 2 && tage_u_T2[tag_index_T2] > 0) tage_u_T2[tag_index_T2]--;
      if (selected_table < 3 && tage_u_T3[tag_index_T3] > 0) tage_u_T3[tag_index_T3]--;
      if (selected_table < 4 && tage_u_T4[tag_index_T4] > 0) tage_u_T4[tag_index_T4]--;
    } else {
      // Allocate to one eligible bank, applying ping-phenomenon avoidance
      int selected_bank = eligible[0];
      if (count >= 2) {
          // Prefer shorter history (smaller index) with 2x probability
          int r = rand() % 3; // values: 0,1,2
          // eligible[0]=shorter; eligible[1]=longer
          selected_bank = (r < 2) ? eligible[0] : eligible[1];
      }

      // Initialize the selected entry
      switch (selected_bank) {
          case 1:
              initialize_new_entry(tage_bht_T1, tage_tag_T1, tage_u_T1, tag_index_T1, tag_T1);
              break;
          case 2:
              initialize_new_entry(tage_bht_T2, tage_tag_T2, tage_u_T2, tag_index_T2, tag_T2);
              break;
          case 3:
              initialize_new_entry(tage_bht_T3, tage_tag_T3, tage_u_T3, tag_index_T3, tag_T3);
              break;
          case 4:
              // if(tage_bht_T4[tag_index_T4]>7){printf("T4 value before initialization: %d, %d\n", tage_bht_T4[tag_index_T4], tag_index_T4);}
              initialize_new_entry(tage_bht_T4, tage_tag_T4, tage_u_T4, tag_index_T4, tag_T4);
              // if(tage_bht_T4[tag_index_T4]>7){printf("T4 value after initialization: %d, %d\n\n", tage_bht_T4[tag_index_T4], tag_index_T4);}
              break;
          default:
              break;
      }
    }
  }

  update_ghistory_tage(outcome);
  if(tage_bht_T0[544] == 48){printf("T0 value after update in train: %d, %d\n", tage_bht_T0[544], oc++);}
}

void cleanup_tage()
{
  free(tage_bht_T0);
  free(tage_bht_T1);
  free(tage_bht_T2);
  free(tage_bht_T3);
  free(tage_bht_T4);
  free(tage_tag_T1);
  free(tage_tag_T2);
  free(tage_tag_T3);
  free(tage_tag_T4);
  free(tage_u_T1);
  free(tage_u_T2);
  free(tage_u_T3);
  free(tage_u_T4);
}

void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tournament();
    break;
  case CUSTOM:
    // cout << oc++<<endl;
    printf("%d\n", oc++);
    init_tage();
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tournament_predict(pc);
  case CUSTOM:
    // cout << oc++<<endl;
    printf("%d\n", oc++);
    return tage_predict(pc);
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc, outcome);
    case CUSTOM:
      // cout << oc++<<endl;
      printf("%d\n", oc++);
      return train_tage(pc, outcome);
    default:
      break;
    }
  }
}
