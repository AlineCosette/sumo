/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    MSCFModel_TCI.cpp
/// @author  Leonhard Luecken
/// @date    Tue, 5 Feb 2018
/// @version $Id$
///
// Task Capability Interface car-following model.
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <microsim/MSVehicle.h>
#include <microsim/MSLane.h>
#include <microsim/MSGlobals.h>
#include <microsim/MSNet.h>
#include "MSCFModel_TCI.h"
#include <microsim/lcmodels/MSAbstractLaneChangeModel.h>
#include <utils/common/RandHelper.h>
#include <utils/common/SUMOTime.h>


// ===========================================================================
// DEBUG constants
// ===========================================================================
//#define DEBUG_COND (true)


// ===========================================================================
// method definitions
// ===========================================================================
MSCFModel_TCI::OUProcess::OUProcess(double initialState, double timeScale, double noiseIntensity)
    : myState(initialState),
      myTimeScale(timeScale),
      myNoiseIntensity(noiseIntensity) {}


MSCFModel_TCI::OUProcess::~OUProcess() {}


void
MSCFModel_TCI::OUProcess::step(double dt) {
    myState = exp(-dt/myTimeScale)*myState + myNoiseIntensity*sqrt(2*dt/myTimeScale)*RandHelper::randNorm(0, 1);
}


double
MSCFModel_TCI::OUProcess::getState() const {
    return myState;
}


MSCFModel_TCI::MSCFModel_TCI(const MSVehicleType* vtype, double accel, double decel,
                                   double emergencyDecel, double apparentDecel,
                                   double headwayTime) :
    MSCFModel(vtype, accel, decel, emergencyDecel, apparentDecel, headwayTime),
    myAccelerationError(0., 1.,1.),
    myHeadwayError(0., 1.,1.),
    myRelativeSpeedError(0., 1.,1.) {
}


MSCFModel_TCI::~MSCFModel_TCI() {}


double 
MSCFModel_TCI::patchSpeedBeforeLC(const MSVehicle* veh, double vMin, double vMax) const {
    const double sigma = (veh->passingMinor()
                          ? veh->getVehicleType().getParameter().getJMParam(SUMO_ATTR_JM_SIGMA_MINOR, 0.)
                          : 0.);
//    const double vDawdle = MAX2(vMin, dawdle2(vMax, sigma));
//    return vDawdle;
    return vMax;
}


double
MSCFModel_TCI::stopSpeed(const MSVehicle* const veh, const double speed, double gap) const {
    // NOTE: This allows return of smaller values than minNextSpeed().
    // Only relevant for the ballistic update: We give the argument headway=veh->getActionStepLengthSecs(), to assure that
    // the stopping position is approached with a uniform deceleration also for tau!=veh->getActionStepLengthSecs().
    return MIN2(maximumSafeStopSpeed(gap, speed, false, veh->getActionStepLengthSecs()), maxNextSpeed(speed, veh));
}


double
MSCFModel_TCI::followSpeed(const MSVehicle* const veh, double speed, double gap, double predSpeed, double predMaxDecel) const {
    const double vsafe = maximumSafeFollowSpeed(gap, speed, predSpeed, predMaxDecel);
    const double vmin = minNextSpeed(speed);
    const double vmax = maxNextSpeed(speed, veh);
    // ballistic
    return MAX2(MIN2(vsafe, vmax), vmin);
}


MSCFModel*
MSCFModel_TCI::duplicate(const MSVehicleType* vtype) const {
    return new MSCFModel_TCI(vtype, myAccel, myDecel, myEmergencyDecel, myApparentDecel, myHeadwayTime);
}


void
MSCFModel_TCI::updateStepDuration() {
    myStepDuration = SIMTIME - myLastUpdateTime;
    myLastUpdateTime = SIMTIME;
}


void
MSCFModel_TCI::calculateDrivingDifficulty(double capability, double demand) {
    assert(capability > 0.);
    assert(demand >= 0.);
    myCurrentDrivingDifficulty = difficultyFunction(demand/capability);
}


double
MSCFModel_TCI::difficultyFunction(double demandCapabilityQuotient) const {
    double difficulty;
    if (demandCapabilityQuotient <= 1) {
        // demand does not exceed capability -> we are in the region for a slight ascend of difficulty
        difficulty = mySubCriticalDifficultyCoefficient*demandCapabilityQuotient;
    } else {// demand exceeds capability -> we are in the region for a steeper ascend of the effect of difficulty
        difficulty = mySubCriticalDifficultyCoefficient + (demandCapabilityQuotient - 1)*mySuperCriticalDifficultyCoefficient;
    }
    return MIN2(myMaxDifficulty, difficulty);
}


void
MSCFModel_TCI::adaptTaskCapability() {
    myTaskCapability = myTaskCapability + myCapabilityTimeScale*myStepDuration*(myTaskDemand - myHomeostasisDifficulty*myTaskCapability);
}


void
MSCFModel_TCI::updateAccelerationError() {
    updateErrorProcess(myAccelerationError, myAccelerationErrorTimeScaleCoefficient, myAccelerationErrorNoiseIntensityCoefficient);
}

void
MSCFModel_TCI::updateRelativeSpeedError() {
    updateErrorProcess(myRelativeSpeedError, myRelativeSpeedErrorTimeScaleCoefficient, myRelativeSpeedErrorNoiseIntensityCoefficient);
}

void
MSCFModel_TCI::updateHeadwayError() {
    updateErrorProcess(myHeadwayError, myHeadwayErrorTimeScaleCoefficient, myHeadwayErrorNoiseIntensityCoefficient);
}

void
MSCFModel_TCI::updateActionStepLength() {
    myActionStepLength;
}


void
MSCFModel_TCI::updateErrorProcess(OUProcess& errorProcess, double timeScaleCoefficient, double noiseIntensityCoefficient) const {
    if (myCurrentDrivingDifficulty == 0) {
        errorProcess.setState(0.);
    } else {
        errorProcess.setTimeScale(timeScaleCoefficient/myCurrentDrivingDifficulty);
        errorProcess.setNoiseIntensity(myCurrentDrivingDifficulty*noiseIntensityCoefficient);
        errorProcess.step(myStepDuration);
    }
}







/****************************************************************************/
