//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

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
int lhistoryBits_tournament = 10; // Number of bits used for Local History
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
uint16_t  ghistory_tournament;


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
    init_tournament();
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
    return tournament_predict(pc);
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
      return train_tournament(pc, outcome);
    default:
      break;
    }
  }
}
