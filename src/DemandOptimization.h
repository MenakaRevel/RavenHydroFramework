/*----------------------------------------------------------------
  Raven Library Source Code
  Copyright (c) 2008-2024 the Raven Development Team
  ----------------------------------------------------------------
  DemandOptimization.h
  ----------------------------------------------------------------*/
#ifndef DEMAND_OPTIMIZATION_H
#define DEMAND_OPTIMIZATION_H

#include "RavenInclude.h"
#include <stdio.h>
#include "Model.h"
#include "LookupTable.h"

#ifdef _LPSOLVE_
namespace lp_lib  {
#include "../lib/lp_solve/lp_lib.h"
}
#endif 

///////////////////////////////////////////////////////////////////
/// \brief different expression types  
//
enum exptype 
{
  EXP,     //< default expression
  EXP_OP,  //< operator
  EXP_INV  //< inverse operator 
};
///////////////////////////////////////////////////////////////////
/// \brief different expression term types  
//
enum termtype
{
  TERM_DV,        //< decision variable !Q123 or named 
  TERM_TS,        //< time series @ts(name,n)  
  TERM_LT,        //< lookup table @lookup(x)
  TERM_HRU,       //< state variable @HRU_var(SNOW,2345)
  TERM_SB,        //< state variable @SB_var(SNOW,234)
  TERM_CONST,     //< constant 
  TERM_HISTORY,   //< bracketed - !Q123[-2] 
  TERM_MAX,       //< @max(x,y) 
  TERM_MIN,       //< @min(x,y)
  TERM_CONVERT,   //< @convert(x,units)
  TERM_CUMUL_TS,  //< @cumul(ts_name,duration) //MAY WANT @cumul(ts_name,duration,n) to handle time shift, e.g., 3 days to 10 days ago?
  TERM_CUMUL,     //< cumulative delivery !C123
  TERM_UNKNOWN    //< unknown 
};
///////////////////////////////////////////////////////////////////
/// \brief decision variable types 
//
enum dv_type 
{
  DV_QOUT,    //< outflow from reach
  DV_QOUTRES, //< outflow from reservoir 
  DV_STAGE,   //< reservoir stage
  DV_DELIVERY,//< delivery of water demand 
  DV_SLACK,   //< slack variable for goal satisfaction
  DV_USER     //< user specified decision variable 
};

// -------------------------------------------------------------------
// data structures used by CDemandOptimizer class:
//   -expressionTerm
//   -expressionStruct (built from expressionTerms)
//   -decision_var
//   -exp_condition (may be defined using expressionStruct)
//   -op_regime (has conditions)
//   -control_var (defined using expressionStruct)
//   -manConstraint (built using multiple operating regimes)
// -------------------------------------------------------------------

//////////////////////////////////////////////////////////////////
/// expression term 
///    individual term in expression
// 
struct expressionTerm 
{
  termtype      type;            //< type of expression 
  double        mult;            //< multiplier of expression (+/- 1, depending upon operator and location in exp)
  bool          reciprocal;      //< true if term is in denominator 
  
  double        value;           //< constant value or conversion multiplier
  CTimeSeries  *pTS;             //< pointer to time series, if this is a named time series
  CLookupTable *pLT;             //< pointer to lookup table, if this is a named time series
  bool          is_nested;       //< true if nested within (i.e., an argument to) another term 
  int           timeshift;       //< for time series (+ or -) or lookback value (+) 
  int           DV_ind;          //< index of decision variable 
  int           nested_ind1;     //< index k of first argument (e.g., for lookup table with term entry)
  int           nested_ind2;     //< index k of second argument (e.g., for min/max functions)
  string        nested_exp1;     //< contents of first argument to function - can be expression
  string        nested_exp2;     //< contents of second argument to function
 
  string        origexp;         //< original string expression 
  int           p_index;         //< subbasin index p (for history variables, e.g, !Q324[-2] or @SB_var() command )
  int           HRU_index;       //< HRU index k (for @HRU_var command)
  int           SV_index;        //< state variable index i (for @SB_var or @HRU_var command)
  
  expressionTerm(); //defined in DemandExpressionHandling.cpp
};

//////////////////////////////////////////////////////////////////
/// expression structure
///   abstraction of (A*B*C)+(D*E)-(F)+(G*H) <= 0  
///   parenthetical collections are groups of terms -example has 4 groups with [3,2,1,2] terms per group
//
struct expressionStruct //full expression 
{ 
  expressionTerm  ***pTerms;      //< 2D irregular array of pointers to expression terms size:[nGroups][nTermsPerGrp[j]]
  int                nGroups;     //< total number of terms groups in expression 
  int               *nTermsPerGrp;//< number of terms per group [size: nGroups]
  comparison         compare;     //< comparison operator (==, <, >)

  string             origexp;     //< original string expression 

  expressionStruct();
  ~expressionStruct();
};

//////////////////////////////////////////////////////////////////
// decision variable  
//
struct decision_var
{
  string  name;      //< decision variable names: Qxxxx or Dxxxx where xxxx is SBID 
  dv_type dvar_type; //< decision variable type: e.g., DV_QOUT or DV_DELIVERY
  int     p_index;   //< raw subbasin index p (not SBID) of decision variable (or DOESNT_EXIST if not linked to SB) 
  int     dem_index; //< demand index in subbasin p (or DOESNT_EXIST if type not DV_DELIVERY) (ii..nDemands in SB p_index, usually <=1)
  int     loc_index; //< local index (rescount or subbasincount or demand count) 
  double  value;     //< solution value for decision variable
  double  min;       //< minimum bound (default=0) 
  double  max;       //< maximum bound (default unbounded)

  decision_var(string nam, int p, dv_type typ, int loc_ind)
  {
    name=nam; 
    p_index=p; 
    loc_index=loc_ind; 
    value=0.0;min=0.0;max=ALMOST_INF;dvar_type=typ;dem_index=DOESNT_EXIST;
  }
};
//////////////////////////////////////////////////////////////////
// goal/constraint condition 
//
struct exp_condition
{
  string      dv_name;      //< decision variable name (e.g., Q1023) or "MONTH" or "DATE" or "DAY_OF_YEAR"
  double      value;        //< conditional value 
  double      value2;       //< second conditional (if COMPARE_BETWEEN)
  string      date_string;  //< conditional value (if date)
  string      date_string2; //< second conditional (if DATE COMPARE_BETWEEN)
  comparison  compare;      //> comparison operator, e.g., COMPARE_IS_EQUAL
  long        p_index;      //> subbasin or demand index of LHS of condition expression (or DOESNT_EXIST)

  expressionStruct *pExp;   //< condition expression (or NULL if not used)

  exp_condition(){
    dv_name="";
    value=value2=0.0;
    date_string=date_string2="";
    compare=COMPARE_IS_EQUAL;
    p_index=DOESNT_EXIST;
    pExp=NULL;
  }
};
//////////////////////////////////////////////////////////////////
// operating regime 
//
struct op_regime 
{
  string            reg_name;             //< regime name 

  expressionStruct *pExpression;          //< constraint expression

  int               nConditions;          //< number of conditional statments 
  exp_condition   **pConditions;          //< array of pointers to conditional statements

  op_regime(string name) {
    reg_name=name;
    pExpression=NULL;
    nConditions=0;
    pConditions=NULL;
  }
};
//////////////////////////////////////////////////////////////////
// management constraint or goal (soft constraint) or DV definition (a special constraint defining a user specified DV)
//
struct manConstraint
{
  string            name;          //< goal or constraint name 
  
  bool              is_goal;       //< true if constraint is soft (goal rather than constraint)
  int               priority;      //< priority (default==1, for goals only)
  double            penalty_under; //< penalty if under specified value (for goals only)
  double            penalty_over;  //< penalty if over value (for goals only)
  int               slack_ind1;    //< slack index (0.._nSlackVars) of under/over slack index for goal, or DOESNT_EXIST if constraint
  int               slack_ind2;    //< slack index (0.._nSlackVars) of over slack index for target goal, or DOESNT_EXIST if constraint

  double            penalty_value; //< (from solution) penalty incurred by not satisfying goal (or zero for constraint) 

  op_regime       **pOperRegimes;  //< array of pointers to operating regimes, which are chosen from conditionals and determine active expression [size:nOperRegimes]
  int               nOperRegimes;  //< size of operating regime array 

  int               active_regime;        //< currently active operating regime (or DOESNT_EXIST if none)
  bool              conditions_satisfied; //< true if any operating regime satisfied during current timestep
  bool              ever_satisfied;       //< true if any operating regime ever satisfied during simulation (for warning at end of sim)

  manConstraint();
  ~manConstraint();
  expressionStruct *GetCurrentExpression() const{return pOperRegimes[nOperRegimes-1]->pExpression; }
  void AddOperatingRegime(op_regime *pOR, bool first);        
  void AddOpCondition(exp_condition *pCondition); //adds to most recent operating regime
  void AddExpression (expressionStruct *pExp);   //adds to most recent operating regime
};

//////////////////////////////////////////////////////////////////
// control variable definition 
//
struct control_var
{
  string            name;          //< control variable name 
  expressionStruct *pExpression;   //< expression defining control variable 

  double            current_val;   //< current value of control variable (evaluated at start of time step)
};

///////////////////////////////////////////////////////////////////
/// \brief Data abstraction for demand optimization 
//
class CDemandOptimizer
{
private: /*------------------------------------------------------*/

  CModel          *_pModel;             //< pointer to model

  int              _nDecisionVars;      //< total number of decision variables considered
  decision_var   **_pDecisionVars;      //< array of pointers to decision variable structures [size:_nDecisionVars]

  int              _nControlVars;       //< total number of control variables considered
  control_var    **_pControlVars;       //< array of pointers to control variables [size: _nControlVars]

  int              _nConstraints;       //< number of user-defined enforced constraints/goals in management model 
  manConstraint  **_pConstraints;       //< array of pointers to user-defined enforced constraints/goals in management model

  int              _nEnabledSubBasins;  //< number of enabled subbasins in model 
  int             *_aSBIndices;         //< local index of enabled subbasins (0:_nEnabledSubBasins) [size: _nSubBasins]

  int              _nReservoirs;        //< local storage of number of simulated lakes/reservoirs 
  int             *_aResIndices;        //< storage of enabled reservoir indices (0:_nReservoirs or DOESNT_EXIST) [size:_nSubBasins] 

  //Should probably convert this to a demand class?
  //CWaterDemand   **_pDemands;           //< array of pointers to water demand instances 
  int              _nDemands;           //< local storage of number of demand locations (:IrrigationDemand/:WaterDemand + :ReservoirExtraction time series)
  int             *_aDemandIDs;         //< local storage of demand IDs [size:_nDemands]
  long            *_aDemandSBIDs;       //< local storage of subbasin IDs for each demand [size: _nDemands]
  int             *_aDemandIndices;     //< local storage of demand index ii (counter in each subbasin) [size: _nDemands]
  string          *_aDemandAliases;     //< list of demand aliases [size: _nDemands] 
  double          *_aDemandPenalties;   //< array of priority weights [size: _nDemands]
  bool            *_aDemandUnrestrict;  //< array of restriction status for demands - true if unrestricted, false otherwise [size: _nDemands]
  double          *_aDelivery;          //< array of delivered water for each demand [m3/s] [size: _nDemands]
  double          *_aCumDelivery;       //< array of cumulative deliveries of demand since _aCumDivDate of this year [m3] [size: _nDemands] 
  int             *_aCumDelDate;        //< julian date to calculate cumulative deliveries from {default: Jan 1)[size: _nDemands]

  int            **_aUpstreamDemands;   //< demand indices (d) upstream (inclusive) of subbasin p [size: nSBs][size: _aUpCount] (only restricted demands)
  int             *_aUpCount;           //< number of demands upstream (inclusive) of subbasin p [size: nSBs]
  
  //int            _nDemandGroups;      //< number of demand groups 
  //CDemandGroup **_pDemandGroups;      //< array of pointers to demand groups   
  
  double          *_aSlackValues;       //< array of slack variable values [size: _nSlackVars]
  int              _nSlackVars;         //< number of slack variables 

  int             _nUserDecisionVars;   //< number of user-specified decision variables 

  int             _nUserConstants;      //< number of user-specified named constants
  string         *_aUserConstNames;     //< array of names of user-specified constants
  double         *_aUserConstants;      //< array of values of user-specified constants

  int             _nUserTimeSeries;     //< number of user variable time series
  CTimeSeries   **_pUserTimeSeries;     //< array of pointers to user variable time series

  int             _nUserLookupTables;   //< number of user variable lookup tables 
  CLookupTable  **_pUserLookupTables;   //< array of pointers to user variable lookup tables 

  int             _nHistoryItems;      //< size of flow/demand/stage history that needs to be stored (in time steps) (from :LookbackDuration)
  double        **_aQhist;             //< history of subbasin discharge [size: _nEnabledSBs * _nHistoryItems]
  double        **_aDhist;             //< history of actual diversions [size: _nEnabledSBs * _nHistoryItems]
  double        **_ahhist;             //< history of actual reservoir stages  (or 0 for non-reservoir basins) [size: _nEnabledSBs * _nHistoryItems]

  ofstream        _DEMANDOPT;          //< ofstream for DemandOptimization.csv
  ofstream        _GOALSAT;            //< ofstream for GoalSatisfaction.csv

  int             _do_debug_level;      //< =1 if debug info is to be printed to screen, =2 if LP matrix also printed (full debug), 0 for nothing

  //Called during simualtion
  void         UpdateHistoryArrays();
  void      UpdateControlVariables(const time_struct &tt);
  bool     ConvertToExpressionTerm(const string s, expressionTerm* term, const int lineno, const string filename)  const;
  int               GetDVColumnInd(const dv_type typ, const int counter) const;
  double              EvaluateTerm(expressionTerm **pTerms,const int k, const double &t) const;
  bool        EvaluateConditionExp(const expressionStruct* pE,const double &t) const;

  bool         CheckGoalConditions(const int ii, const int k, const time_struct &tt,const optStruct &Options) const; 

#ifdef _LPSOLVE_
  void           AddConstraintToLP(const int i, const int k, lp_lib::lprec *pLinProg, const time_struct &tt,int *col_ind, double *row_val) const;
#endif 


  //Called during initialization
  bool        UserTimeSeriesExists(string TSname) const;
  void     AddReservoirConstraints();
  void     IdentifyUpstreamDemands(); 
  bool          VariableNameExists(const string &name) const;

public: /*------------------------------------------------------*/
  CDemandOptimizer(CModel *pMod);
  ~CDemandOptimizer();

  int    GetDemandIndexFromName(const string dname) const;
  double GetNamedConstant      (const string s) const;
  int    GetUserDVIndex        (const string s) const;
  double GetControlVariable    (const string s) const;
  //double GetDemandDelivery     (const int p) const;
  int    GetNumUserDVs         () const;
  int    GetDebugLevel         () const; 
  int    GetIndexFromDVString  (string s) const;

  void   SetHistoryLength      (const int n);
  void   SetCumulativeDate     (const int julian_date, const string demandID);
  void   SetDebugLevel         (const int lev);
  void   SetDemandAsUnrestricted(const string dname); 
  
  manConstraint *AddGoalOrConstraint (const string name, const bool soft_constraint);
  
  void   AddDecisionVar        (const decision_var *pDV);
  void   SetDecisionVarBounds  (const string name, const double &min, const double &max);
  void   AddUserConstant       (const string name, const double &val);
  void   AddControlVariable    (const string name, expressionStruct* pExp);
  void   AddUserTimeSeries     (const CTimeSeries *pTS);
  void   AddUserLookupTable    (const CLookupTable *pLUT);
   
  //void MultiplyDemand        (const string dname, const double &mult);
  //void MultiplyGroupDemand   (const string groupname, const double &mult);
  void SetDemandPenalty        (const string dname, const double &pen);
  //void SetDemandPriority     (const string dname, const int &prior);

  expressionStruct *ParseExpression(const char **s, const int Len, const int lineno, const string filename) const;

  void   Initialize            (CModel *pModel, const optStruct &Options);
  void   InitializePostRVMRead (CModel *pModel, const optStruct &Options); 
  void   SolveDemandProblem    (CModel *pModel, const optStruct &Options, const double *aSBrunoff, const time_struct &tt);

  void   WriteOutputFileHeaders(const optStruct &Options);
  void   WriteMinorOutput      (const optStruct &Options,const time_struct &tt);
  void   CloseOutputStreams    ();

  void   Closure               (const optStruct &Options);
};
#endif