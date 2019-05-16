#include <scorep/SCOREP_User.h>
...
int main(int argc, char **argv, char **envp)
{
  MPI_Init(&argc,&argv);
  SCOREP_RECORDING_OFF(); //turn recording off for initialization/regions not of interest
  ...
  SCOREP_RECORDING_ON();
  //use verbatim to facilitate looping over the traces in simulation when simulating multiple jobs
  SCOREP_USER_REGION_BY_NAME_BEGIN("TRACER_Loop", SCOREP_USER_REGION_TYPE_COMMON); 
  // at least add this BEGIN timer call - called from only one rank
  // you can add more calls later with region names TRACER_WallTime_<any string of your choice>
  if(myRank == 0)
    SCOREP_USER_REGION_BY_NAME_BEGIN("TRACER_WallTime_MainLoop", SCOREP_USER_REGION_TYPE_COMMON);
  // Application main work LOOP
  for ( int itscf = 0; itscf < nitscf_; itscf++ )
  {
    ...
  }
  // time call to mark END of work - called from only one rank
  if(myRank == 0)
    SCOREP_USER_REGION_BY_NAME_END("TRACER_WallTime_MainLoop");
  // use verbatim - mark end of trace loop
  SCOREP_USER_REGION_BY_NAME_END("TRACER_Loop"); 
  SCOREP_RECORDING_OFF();//turn off recording again
  ...
}