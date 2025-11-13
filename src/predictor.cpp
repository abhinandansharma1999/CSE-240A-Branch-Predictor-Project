
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
const char *bpName[4] = {"Static", "Gshare", "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 15; // Number of bits used for Global History

int ghistoryBits_tournament = 12; // Number of bits used for Global History
int lhistoryBits_tournament = 12; // Number of bits used for Local History

int ghistoryBits_tage_T4 = 64; // long history
int ghistoryBits_tage_T3 = 32;
int ghistoryBits_tage_T2 = 16;
int ghistoryBits_tage_T1 = 8;

int tage_tag_bits_T4 = 9;
int tage_tag_bits_T3 = 9;
int tage_tag_bits_T2 = 9;
int tage_tag_bits_T1 = 9;

int tage_u_bits = 2;           // usefulness counter bits
int tage_predictor_bits = 3;   // predictor counter bits (3-bit folded states 0..7)

int tage_pred_table_length_T0 = 11; // index bits for base predictor (T0)
int tage_pred_table_length_T1 = 10;
int tage_pred_table_length_T2 = 10;
int tage_pred_table_length_T3 = 10;
int tage_pred_table_length_T4 = 10;

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
static uint8_t  *tage_bht_T0 = NULL; // base predictor (3-bit counters)
static uint8_t  *tage_bht_T1 = NULL;
static uint8_t  *tage_bht_T2 = NULL;
static uint8_t  *tage_bht_T3 = NULL;
static uint8_t  *tage_bht_T4 = NULL;

static uint16_t *tage_tag_T1 = NULL;
static uint16_t *tage_tag_T2 = NULL;
static uint16_t *tage_tag_T3 = NULL;
static uint16_t *tage_tag_T4 = NULL;

static uint8_t  *tage_u_T1 = NULL;
static uint8_t  *tage_u_T2 = NULL;
static uint8_t  *tage_u_T3 = NULL;
static uint8_t  *tage_u_T4 = NULL;

static uint64_t ghistory_tage = 0ULL; // global history (up to 64 bits supported)

static uint64_t tage_global_reset_counter = 0;      // counts number of trained branches
static const uint64_t TAGE_RESET_PERIOD = 2097152ULL; // total reset period

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

// Helper: compute entries (1 << bits)
static inline uint32_t entries_from_len(int len_bits) {
    return (1u << len_bits);
}

static uint32_t fold_xor(uint64_t val, int len, int out_bits) {
    if (out_bits == 0) return 0;
    uint32_t res = 0;
    int pos = 0;
    int remaining = len;
    while (remaining > 0) {
        int take = (remaining < out_bits) ? remaining : out_bits;
        uint32_t chunk = (uint32_t)((val >> pos) & ((1ULL << take) - 1));
        res ^= chunk;
        pos += take;
        remaining -= take;
    }
    return res & ((1u << out_bits) - 1);
}

// A slightly different folding used to compute tags (mix with offset)
static uint32_t fold_xor_with_stride(uint64_t val, int len, int out_bits, int stride, int offset) {
    if (out_bits == 0) return 0;
    uint32_t res = 0;
    for (int i = 0; i < len; ++i) {
        uint32_t bit = (uint32_t)((val >> i) & 1ULL);
        int pos = (i * stride + offset) % out_bits;
        res ^= (bit << pos);
    }
    return res & ((1u << out_bits) - 1);
}

// -----------------------------
// Index & tag computation for each bank
// -----------------------------
static inline uint32_t compute_index(uint32_t pc, int ghistoryBits, int table_len_bits) {
    // take pc bits and folded GHR, xor them -> index
    int pc_bits = 32; // PC passed as 32-bit value
    uint32_t folded_pc = fold_xor(pc, pc_bits, table_len_bits);
    uint32_t folded_ghr = fold_xor(ghistory_tage, ghistoryBits, table_len_bits);
    return (folded_pc ^ folded_ghr) & ((1u << table_len_bits) - 1);
}

static inline uint32_t compute_tag(uint32_t pc, int ghistoryBits, int tag_bits) {
    // produce a small tag by mixing PC and GHR with different strides
    int pc_bits = 32;
    uint32_t p = fold_xor_with_stride(pc, pc_bits, tag_bits, 7, 5);
    uint32_t g = fold_xor_with_stride(ghistory_tage, ghistoryBits, tag_bits, 13, 3);
    return (p ^ g) & ((1u << tag_bits) - 1);
}

// -----------------------------
// State machine: 3-bit counter update (values 0..7, N11..T11 in predictor.h)
// Value >= T00 (4) => predict TAKEN
// -----------------------------
static void update_3bit_counter(uint8_t *table, uint32_t idx, uint8_t outcome) {
    uint8_t v = table[idx];
    switch (v) {
        case N11: table[idx] = (outcome == TAKEN) ? N10 : N11; break;
        case N10: table[idx] = (outcome == TAKEN) ? N01 : N11; break;
        case N01: table[idx] = (outcome == TAKEN) ? N00 : N10; break;
        case N00: table[idx] = (outcome == TAKEN) ? T00 : N01; break;
        case T00: table[idx] = (outcome == TAKEN) ? T01 : N00; break;
        case T01: table[idx] = (outcome == TAKEN) ? T10 : T00; break;
        case T10: table[idx] = (outcome == TAKEN) ? T11 : T01; break;
        case T11: table[idx] = (outcome == TAKEN) ? T11 : T10; break;
        default:
            // Shouldn't happen — clamp to a valid state
            if (outcome == TAKEN) table[idx] = T00; else table[idx] = N00;
            break;
    }
}

// -----------------------------
// Usefulness counter update
// -----------------------------
static void update_u_counter(uint8_t *u_table, uint32_t idx, uint8_t pred, uint8_t outcome) {
    uint8_t max_u = (1u << tage_u_bits) - 1u;
    if (pred == outcome) {
        if (u_table[idx] < max_u) u_table[idx]++;
    } else {
        if (u_table[idx] > 0) u_table[idx]--;
    }
}

// Initialize new entry when allocating
static void initialize_entry(uint8_t *bht, uint16_t *tag_arr, uint8_t *u_arr, uint32_t idx, uint16_t tag) {
    // set predictor to weakly taken (N00 in your scheme), usefulness = 0
    bht[idx] = N00;
    tag_arr[idx] = tag;
    u_arr[idx] = 0;
}

static void tage_clear_usefulness_bits(bool clear_msb)
{
    uint32_t eT1 = entries_from_len(tage_pred_table_length_T1);
    uint32_t eT2 = entries_from_len(tage_pred_table_length_T2);
    uint32_t eT3 = entries_from_len(tage_pred_table_length_T3);
    uint32_t eT4 = entries_from_len(tage_pred_table_length_T4);

    for (uint32_t i = 0; i < eT1; ++i)
        tage_u_T1[i] &= clear_msb ? 0x01 : 0x02; // because 2-bit usefulness counters
    for (uint32_t i = 0; i < eT2; ++i)
        tage_u_T2[i] &= clear_msb ? 0x01 : 0x02;
    for (uint32_t i = 0; i < eT3; ++i)
        tage_u_T3[i] &= clear_msb ? 0x01 : 0x02;
    for (uint32_t i = 0; i < eT4; ++i)
        tage_u_T4[i] &= clear_msb ? 0x01 : 0x02;
}

// -----------------------------
// Initialization
// -----------------------------
static void init_tage()
{
    // allocate tables sized by "tage_pred_table_length_Tn" bits
    uint32_t eT0 = entries_from_len(tage_pred_table_length_T0);
    uint32_t eT1 = entries_from_len(tage_pred_table_length_T1);
    uint32_t eT2 = entries_from_len(tage_pred_table_length_T2);
    uint32_t eT3 = entries_from_len(tage_pred_table_length_T3);
    uint32_t eT4 = entries_from_len(tage_pred_table_length_T4);

    // free existing (if any) for safety
    if (tage_bht_T0) free(tage_bht_T0);
    if (tage_bht_T1) free(tage_bht_T1);
    if (tage_bht_T2) free(tage_bht_T2);
    if (tage_bht_T3) free(tage_bht_T3);
    if (tage_bht_T4) free(tage_bht_T4);
    if (tage_tag_T1) free(tage_tag_T1);
    if (tage_tag_T2) free(tage_tag_T2);
    if (tage_tag_T3) free(tage_tag_T3);
    if (tage_tag_T4) free(tage_tag_T4);
    if (tage_u_T1) free(tage_u_T1);
    if (tage_u_T2) free(tage_u_T2);
    if (tage_u_T3) free(tage_u_T3);
    if (tage_u_T4) free(tage_u_T4);

    tage_bht_T0 = (uint8_t*) calloc(eT0, sizeof(uint8_t));
    tage_bht_T1 = (uint8_t*) calloc(eT1, sizeof(uint8_t));
    tage_bht_T2 = (uint8_t*) calloc(eT2, sizeof(uint8_t));
    tage_bht_T3 = (uint8_t*) calloc(eT3, sizeof(uint8_t));
    tage_bht_T4 = (uint8_t*) calloc(eT4, sizeof(uint8_t));

    tage_tag_T1 = (uint16_t*) calloc(eT1, sizeof(uint16_t));
    tage_tag_T2 = (uint16_t*) calloc(eT2, sizeof(uint16_t));
    tage_tag_T3 = (uint16_t*) calloc(eT3, sizeof(uint16_t));
    tage_tag_T4 = (uint16_t*) calloc(eT4, sizeof(uint16_t));

    tage_u_T1 = (uint8_t*) calloc(eT1, sizeof(uint8_t));
    tage_u_T2 = (uint8_t*) calloc(eT2, sizeof(uint8_t));
    tage_u_T3 = (uint8_t*) calloc(eT3, sizeof(uint8_t));
    tage_u_T4 = (uint8_t*) calloc(eT4, sizeof(uint8_t));

    // initialize default states
    for (uint32_t i = 0; i < eT0; ++i) tage_bht_T0[i] = N00;
    for (uint32_t i = 0; i < eT1; ++i) { tage_bht_T1[i] = N00; tage_tag_T1[i] = 0; tage_u_T1[i] = 0; }
    for (uint32_t i = 0; i < eT2; ++i) { tage_bht_T2[i] = N00; tage_tag_T2[i] = 0; tage_u_T2[i] = 0; }
    for (uint32_t i = 0; i < eT3; ++i) { tage_bht_T3[i] = N00; tage_tag_T3[i] = 0; tage_u_T3[i] = 0; }
    for (uint32_t i = 0; i < eT4; ++i) { tage_bht_T4[i] = N00; tage_tag_T4[i] = 0; tage_u_T4[i] = 0; }

    ghistory_tage = 0ULL;
}

// -----------------------------
// Prediction: try T4..T1 then T0
// -----------------------------
uint8_t tage_predict(uint32_t pc)
{
    // compute indices & tags
    uint32_t idx4 = compute_index(pc, ghistoryBits_tage_T4, tage_pred_table_length_T4);
    uint32_t idx3 = compute_index(pc, ghistoryBits_tage_T3, tage_pred_table_length_T3);
    uint32_t idx2 = compute_index(pc, ghistoryBits_tage_T2, tage_pred_table_length_T2);
    uint32_t idx1 = compute_index(pc, ghistoryBits_tage_T1, tage_pred_table_length_T1);
    uint32_t idx0 = pc & ((1u << tage_pred_table_length_T0) - 1u);

    uint32_t tag4 = compute_tag(pc, ghistoryBits_tage_T4, tage_tag_bits_T4);
    uint32_t tag3 = compute_tag(pc, ghistoryBits_tage_T3, tage_tag_bits_T3);
    uint32_t tag2 = compute_tag(pc, ghistoryBits_tage_T2, tage_tag_bits_T2);
    uint32_t tag1 = compute_tag(pc, ghistoryBits_tage_T1, tage_tag_bits_T1);

    // read predictor values
    uint8_t val4 = tage_bht_T4[idx4];
    uint8_t val3 = tage_bht_T3[idx3];
    uint8_t val2 = tage_bht_T2[idx2];
    uint8_t val1 = tage_bht_T1[idx1];
    uint8_t val0 = tage_bht_T0[idx0];

    // Predict TAKEN/NOTTAKEN if tag matches; assign 2 to indicate no valid prediction otherwise.
    uint8_t pred4 = (tage_tag_T4[idx4] == (uint16_t)tag4) ? ((val4 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred3 = (tage_tag_T3[idx3] == (uint16_t)tag3) ? ((val3 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred2 = (tage_tag_T2[idx2] == (uint16_t)tag2) ? ((val2 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred1 = (tage_tag_T1[idx1] == (uint16_t)tag1) ? ((val1 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred0 = (val0 >= T00) ? TAKEN : NOTTAKEN; // base predictor always valid

    // choose provider (longest matching) and compute alt_pred exactly as in train_tage
    int provider = -1; // 4,3,2,1,0
    uint8_t provider_pred = pred0;
    uint8_t alt_pred = pred0;

    if (tage_tag_T4[idx4] == (uint16_t)tag4) {
        provider = 4;
        provider_pred = pred4;
        // alt_pred = next longest matching: T3 -> T2 -> T1 -> T0
        if (tage_tag_T3[idx3] == (uint16_t)tag3) alt_pred = pred3;
        else if (tage_tag_T2[idx2] == (uint16_t)tag2) alt_pred = pred2;
        else if (tage_tag_T1[idx1] == (uint16_t)tag1) alt_pred = pred1;
        else alt_pred = pred0;
    } else if (tage_tag_T3[idx3] == (uint16_t)tag3) {
        provider = 3;
        provider_pred = pred3;
        // alt_pred = next longest matching: T2 -> T1 -> T0
        if (tage_tag_T2[idx2] == (uint16_t)tag2) alt_pred = pred2;
        else if (tage_tag_T1[idx1] == (uint16_t)tag1) alt_pred = pred1;
        else alt_pred = pred0;
    } else if (tage_tag_T2[idx2] == (uint16_t)tag2) {
        provider = 2;
        provider_pred = pred2;
        // alt_pred = next longest matching: T1 -> T0
        if (tage_tag_T1[idx1] == (uint16_t)tag1) alt_pred = pred1;
        else alt_pred = pred0;
    } else if (tage_tag_T1[idx1] == (uint16_t)tag1) {
        provider = 1;
        provider_pred = pred1;
        alt_pred = pred0;
    } else {
        provider = 0;
        provider_pred = pred0;
        alt_pred = pred0;
    }

    // If the provider is a freshly initialized entry (usefulness == 0 && counter == N00),
    // fall back to the alternate prediction instead of trusting the fresh entry.
    if (provider >= 1 && provider <= 4) {
        bool newly_allocated = false;
        switch (provider) {
            case 1: newly_allocated = (tage_u_T1[idx1] == 0 && val1 == N00); break;
            case 2: newly_allocated = (tage_u_T2[idx2] == 0 && val2 == N00); break;
            case 3: newly_allocated = (tage_u_T3[idx3] == 0 && val3 == N00); break;
            case 4: newly_allocated = (tage_u_T4[idx4] == 0 && val4 == N00); break;
        }
        if (newly_allocated) {
            // return alternate as the trusted prediction
            // (alt_pred is computed above using the same logic as train_tage)
            return alt_pred;
        }
    }

    // Otherwise trust the provider's prediction
    return provider_pred;
}


// -----------------------------
// Train the predictor: update counters, usefulness, allocate entries on mispred
// -----------------------------
void train_tage(uint32_t pc, uint8_t outcome)
{
    uint32_t idx4 = compute_index(pc, ghistoryBits_tage_T4, tage_pred_table_length_T4);
    uint32_t idx3 = compute_index(pc, ghistoryBits_tage_T3, tage_pred_table_length_T3);
    uint32_t idx2 = compute_index(pc, ghistoryBits_tage_T2, tage_pred_table_length_T2);
    uint32_t idx1 = compute_index(pc, ghistoryBits_tage_T1, tage_pred_table_length_T1);
    uint32_t idx0 = pc & ((1u << tage_pred_table_length_T0) - 1u);

    uint32_t tag4 = compute_tag(pc, ghistoryBits_tage_T4, tage_tag_bits_T4);
    uint32_t tag3 = compute_tag(pc, ghistoryBits_tage_T3, tage_tag_bits_T3);
    uint32_t tag2 = compute_tag(pc, ghistoryBits_tage_T2, tage_tag_bits_T2);
    uint32_t tag1 = compute_tag(pc, ghistoryBits_tage_T1, tage_tag_bits_T1);

    // read values
    uint8_t val4 = tage_bht_T4[idx4];
    uint8_t val3 = tage_bht_T3[idx3];
    uint8_t val2 = tage_bht_T2[idx2];
    uint8_t val1 = tage_bht_T1[idx1];
    uint8_t val0 = tage_bht_T0[idx0];

    // Predict TAKEN/NOTTAKEN if tag matches; assign 2 to indicate no valid prediction otherwise.
    uint8_t pred4 = (tage_tag_T4[idx4] == (uint16_t)tag4) ? ((val4 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred3 = (tage_tag_T3[idx3] == (uint16_t)tag3) ? ((val3 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred2 = (tage_tag_T2[idx2] == (uint16_t)tag2) ? ((val2 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred1 = (tage_tag_T1[idx1] == (uint16_t)tag1) ? ((val1 >= T00) ? TAKEN : NOTTAKEN) : 2;
    uint8_t pred0 = (val0 >= T00) ? TAKEN : NOTTAKEN;

    // choose provider (longest matching)
    int provider = -1; // 4,3,2,1,0
    uint8_t provider_pred = pred0;
    if (tage_tag_T4[idx4] == (uint16_t)tag4) { provider = 4; provider_pred = pred4; }
    else if (tage_tag_T3[idx3] == (uint16_t)tag3) { provider = 3; provider_pred = pred3; }
    else if (tage_tag_T2[idx2] == (uint16_t)tag2) { provider = 2; provider_pred = pred2; }
    else if (tage_tag_T1[idx1] == (uint16_t)tag1) { provider = 1; provider_pred = pred1; }
    else { provider = 0; provider_pred = pred0; }

    // alt provider: next longest matching (for usefulness update)
    int alt_provider = 0;
    uint8_t alt_pred = pred0;
    if (provider == 4) {
        if (tage_tag_T3[idx3] == (uint16_t)tag3) { alt_provider = 3; alt_pred = pred3; }
        else if (tage_tag_T2[idx2] == (uint16_t)tag2) { alt_provider = 2; alt_pred = pred2; }
        else if (tage_tag_T1[idx1] == (uint16_t)tag1) { alt_provider = 1; alt_pred = pred1; }
        else { alt_provider = 0; alt_pred = pred0; }
    } else if (provider == 3) {
        if (tage_tag_T2[idx2] == (uint16_t)tag2) { alt_provider = 2; alt_pred = pred2; }
        else if (tage_tag_T1[idx1] == (uint16_t)tag1) { alt_provider = 1; alt_pred = pred1; }
        else { alt_provider = 0; alt_pred = pred0; }
    } else if (provider == 2) {
        if (tage_tag_T1[idx1] == (uint16_t)tag1) { alt_provider = 1; alt_pred = pred1; }
        else { alt_provider = 0; alt_pred = pred0; }
    } else if (provider == 1) {
        alt_provider = 0; alt_pred = pred0;
    } else {
        alt_provider = 0; alt_pred = pred0;
    }

    // Update provider table counter
    switch (provider) {
        case 4: update_3bit_counter(tage_bht_T4, idx4, outcome); break;
        case 3: update_3bit_counter(tage_bht_T3, idx3, outcome); break;
        case 2: update_3bit_counter(tage_bht_T2, idx2, outcome); break;
        case 1: update_3bit_counter(tage_bht_T1, idx1, outcome); break;
        case 0: update_3bit_counter(tage_bht_T0, idx0, outcome); break;
        default: break;
    }

    // If provider != alt_provider and we have a component provider (1..4), update usefulness
    if (provider >= 1 && provider <= 4) {
      uint8_t *u_table = NULL;
      uint32_t u_idx = 0;
      if (provider_pred != alt_pred) {
        if (provider == 1) { u_table = tage_u_T1; u_idx = idx1; }
        if (provider == 2) { u_table = tage_u_T2; u_idx = idx2; }
        if (provider == 3) { u_table = tage_u_T3; u_idx = idx3; }
        if (provider == 4) { u_table = tage_u_T4; u_idx = idx4; }
        if (u_table) update_u_counter(u_table, u_idx, provider_pred, outcome);
      }
    }

    // If misprediction by provider -> try allocate entries in longer tables than provider
    if (provider_pred != outcome) {
        // collect eligible banks (higher priority = longer histories) whose usefulness == 0
        int eligible[4];
        int eligible_count = 0;

        if (provider < 1) {
            if (tage_u_T1[idx1] == 0) eligible[eligible_count++] = 1;
            if (tage_u_T2[idx2] == 0) eligible[eligible_count++] = 2;
            if (tage_u_T3[idx3] == 0) eligible[eligible_count++] = 3;
            if (tage_u_T4[idx4] == 0) eligible[eligible_count++] = 4;
        } else if (provider < 2) {
            if (tage_u_T2[idx2] == 0) eligible[eligible_count++] = 2;
            if (tage_u_T3[idx3] == 0) eligible[eligible_count++] = 3;
            if (tage_u_T4[idx4] == 0) eligible[eligible_count++] = 4;
        } else if (provider < 3) {
            if (tage_u_T3[idx3] == 0) eligible[eligible_count++] = 3;
            if (tage_u_T4[idx4] == 0) eligible[eligible_count++] = 4;
        } else if (provider < 4) {
            if (tage_u_T4[idx4] == 0) eligible[eligible_count++] = 4;
        }

        if (eligible_count == 0) {
            // Gradually age out less useful entries by decrementing the usefulness counters of all tables beyond the provider, if their value is greater than 0.
            if (provider < 1 && tage_u_T1[idx1] > 0) tage_u_T1[idx1]--;
            if (provider < 2 && tage_u_T2[idx2] > 0) tage_u_T2[idx2]--;
            if (provider < 3 && tage_u_T3[idx3] > 0) tage_u_T3[idx3]--;
            if (provider < 4 && tage_u_T4[idx4] > 0) tage_u_T4[idx4]--;
        } else {
            // choose a bank to allocate: prefer shorter history among eligible (i.e., smallest number)
            int chosen = eligible[0];
            if (eligible_count >= 2) {
                // random bias towards shorter history (2/3)
                int r = rand() % 3; // 0,1,2
                chosen = (r < 2) ? eligible[0] : eligible[1];
            }
            switch (chosen) {
                case 1: initialize_entry(tage_bht_T1, tage_tag_T1, tage_u_T1, idx1, (uint16_t)tag1); break;
                case 2: initialize_entry(tage_bht_T2, tage_tag_T2, tage_u_T2, idx2, (uint16_t)tag2); break;
                case 3: initialize_entry(tage_bht_T3, tage_tag_T3, tage_u_T3, idx3, (uint16_t)tag3); break;
                case 4: initialize_entry(tage_bht_T4, tage_tag_T4, tage_u_T4, idx4, (uint16_t)tag4); break;
                default: break;
            }
        }
    }

    // Finally update the global history
    ghistory_tage = ((ghistory_tage << 1) | (uint64_t)(outcome & 1u));
    // ensure we don't overflow 64-bit when histories are smaller — but keep full 64-bit history (masking done on use)

    // -----------------------------
    // Periodic usefulness reset mechanism
    // -----------------------------

    tage_global_reset_counter++;

    if (tage_global_reset_counter == (TAGE_RESET_PERIOD / 2)) {
        // After half period: clear MSB of all usefulness counters
        tage_clear_usefulness_bits(true);
    } else if (tage_global_reset_counter >= TAGE_RESET_PERIOD) {
        // After full period: clear LSB of all usefulness counters
        tage_clear_usefulness_bits(false);
        tage_global_reset_counter = 0;
    }
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
      return train_tage(pc, (uint8_t)(outcome & 1u));
    default:
      break;
    }
  }
}