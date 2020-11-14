#include <math.h>
#include <inttypes.h>
#include "pid.h"
#include "stm32f4xx_hal.h"

typedef struct{
	double kp;                  // * (P)roportional Tuning Parameter
	double ki;                  // * (I)ntegral Tuning Parameter
	double kd;                  // * (D)erivative Tuning Parameter
	int8_t controllerDirection;
	double setpoint;
} PIDController_t;


double *myInput;              // * Pointers to the Input, Output, and Setpoint variables
double *myOutput;             //   This creates a hard link between the variables and the
double *mySetpoint;           //   PID, freeing the user from having to constantly tell us
							  //   what these values are.  with pointers we'll just know.

uint32_t lastTime;
double ITerm, lastInput;

uint32_t SampleTime;
double outMin, outMax;
const uint8_t inAuto=1;


uint8_t PID_Compute(PIDController_t *this)
{
   uint32_t now = HAL_GetTick();
   uint32_t timeChange = (now - lastTime);
   if(timeChange>=SampleTime)
   {
      /*Compute all the working error variables*/
	  double input = *myInput;
      double error = *mySetpoint - input;
      ITerm+= (this->ki * error);
      if(ITerm > outMax) ITerm= outMax;
      else if(ITerm < outMin) ITerm= outMin;
      double dInput = (input - lastInput);

      /*Compute PID Output*/
      double output = this->kp * error + ITerm- this->kd * dInput;

	  if(output > outMax) output = outMax;
      else if(output < outMin) output = outMin;
	  *myOutput = output;

      /*Remember some variables for next time*/
      lastInput = input;
      lastTime = now;
	  return 1;
   }
   else return 0;
}


/* SetTunings(...)*************************************************************
 * This function allows the controller's dynamic performance to be adjusted.
 * it's called automatically from the constructor, but tunings can also
 * be adjusted on the fly during normal operation
 ******************************************************************************/
void PID_SetTunings(PIDController_t* this, double Kp, double Ki, double Kd)
{
   if (Kp<0 || Ki<0 || Kd<0) return;

   double SampleTimeInSec = ((double)SampleTime)/1000;
   this->kp = Kp;
   this->ki = Ki * SampleTimeInSec;
   this->kd = Kd / SampleTimeInSec;

  if(this->controllerDirection ==REVERSE)
   {
      this->kp = (0 - this->kp);
      this->ki = (0 - this->ki);
      this->kd = (0 - this->kd);
   }
}

/* SetSampleTime(...) *********************************************************
 * sets the period, in Milliseconds, at which the calculation is performed
 ******************************************************************************/
void PID_SetSampleTime(PIDController_t* this, int NewSampleTime)
{
   if (NewSampleTime > 0)
   {
      double ratio  = (double)NewSampleTime
                      / (double)SampleTime;
      this->ki *= ratio;
      this->kd /= ratio;
      SampleTime = (unsigned long)NewSampleTime;
   }
}

/* SetOutputLimits(...)****************************************************
 *     This function will be used far more often than SetInputLimits.  while
 *  the input to the controller will generally be in the 0-1023 range (which is
 *  the default already,)  the output will be a little different.  maybe they'll
 *  be doing a time window and will need 0-8000 or something.  or maybe they'll
 *  want to clamp it from 0-125.  who knows.  at any rate, that can all be done
 *  here.
 **************************************************************************/
void PID_SetOutputLimits(double Min, double Max)
{
   if(Min >= Max) return;
   outMin = Min;
   outMax = Max;

   if(inAuto)
   {
	   if(*myOutput > outMax) *myOutput = outMax;
	   else if(*myOutput < outMin) *myOutput = outMin;

	   if(ITerm > outMax) ITerm= outMax;
	   else if(ITerm < outMin) ITerm= outMin;
   }
}



/* Initialize()****************************************************************
 *	does all the things that need to happen to ensure a bumpless transfer
 *  from manual to automatic mode.
 ******************************************************************************/
void PID_Initialize()
{
   ITerm = *myOutput;
   lastInput = *myInput;
   if(ITerm > outMax) ITerm = outMax;
   else if(ITerm < outMin) ITerm = outMin;
}

/* SetControllerDirection(...)*************************************************
 * The PID will either be connected to a DIRECT acting process (+Output leads
 * to +Input) or a REVERSE acting process(+Output leads to -Input.)  we need to
 * know which one, because otherwise we may increase the output when we should
 * be decreasing.  This is called from the constructor.
 ******************************************************************************/
void PID_SetControllerDirection(PIDController_t* this, int Direction)
{
   if(inAuto && Direction !=this->controllerDirection)
   {
	  this->kp = (0 - this->kp);
      this->ki = (0 - this->ki);
      this->kd = (0 - this->kd);
   }
   this->controllerDirection = Direction;
}


