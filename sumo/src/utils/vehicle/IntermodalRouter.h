/****************************************************************************/
/// @file    IntermodalRouter.h
/// @author  Jakob Erdmann
/// @date    Mon, 03 March 2014
/// @version $Id$
///
// The Pedestrian Router build a special network and (delegegates to a SUMOAbstractRouter)
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.dlr.de/
// Copyright (C) 2001-2015 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This file is part of SUMO.
//   SUMO is free software: you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/
#ifndef IntermodalRouter_h
#define IntermodalRouter_h


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <string>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <utils/common/MsgHandler.h>
#include <utils/common/SUMOTime.h>
#include <utils/common/ToString.h>
#include "SUMOAbstractRouter.h"
#include "SUMOVehicleParameter.h"
#include "DijkstraRouterTT.h"
#include "IntermodalNetwork.h"
#include "PedestrianRouter.h"

//#define IntermodalRouter_DEBUG_ROUTES


// ===========================================================================
// class definitions
// ===========================================================================
/// @brief the car edge type that is given to the internal router (SUMOAbstractRouter)
template<class E, class L, class N, class V>
class CarEdge : public IntermodalEdge<E, L, N, V> {
public:
    CarEdge(unsigned int numericalID, const E* edge) :
        IntermodalEdge<E, L, N, V>(edge->getID() + "_car", numericalID, edge, "!car") { }

    bool includeInRoute(bool /* allEdges */) const {
        return true;
    }

    /// @name The interface as required by SUMOAbstractRouter routes
    /// @{

    bool prohibits(const IntermodalTrip<E, N, V>* const trip) const {
        return trip->vehicle == 0 || this->myEdge->prohibits(trip->vehicle);
    }

    /// @}

    SUMOReal getTravelTime(const IntermodalTrip<E, N, V>* const trip, SUMOReal time) const {
        const SUMOReal length = this->myEdge->getLength();
        const SUMOReal travelTime = E::getTravelTimeStatic(this->myEdge, trip->vehicle, time);
        if (this->myEdge == trip->from) {
             return travelTime * (length - trip->departPos) / length;
        }
        if (this->myEdge == trip->to) {
            return travelTime * trip->arrivalPos / length;
        }
        return travelTime;
    }

};


/// @brief the stop edge type representing bus and train stops
template<class E, class L, class N, class V>
class StopEdge : public IntermodalEdge<E, L, N, V> {
public:
    StopEdge(const std::string id, unsigned int numericalID, const E* edge) :
        IntermodalEdge<E, L, N, V>(id, numericalID, edge, "!stop") { }

    bool includeInRoute(bool /* allEdges */) const {
        return true;
    }

    bool prohibits(const IntermodalTrip<E, N, V>* const trip) const {
        return trip->modeSet && SVC_BUS == 0;
    }
};


/// @brief the public transport edge type connecting the stop edges
template<class E, class L, class N, class V>
class PublicTransportEdge : public IntermodalEdge<E, L, N, V> {
private:
    struct Schedule {
        Schedule(const SUMOTime _begin, const SUMOTime _end, const SUMOTime _period, const SUMOReal _travelTimeSec)
            : begin(_begin), end(_end), period(_period), travelTimeSec(_travelTimeSec) {}
        const SUMOTime begin;
        const SUMOTime end;
        const SUMOTime period;
        const SUMOReal travelTimeSec;
    private:
        /// @brief Invalidated assignment operator
        Schedule& operator=(const Schedule& src);
    };

public:
    PublicTransportEdge(const std::string id, unsigned int numericalID, const IntermodalEdge<E, L, N, V>* entryStop, const E* endEdge, const std::string& line) :
        IntermodalEdge<E, L, N, V>(line + ":" + (id != "" ? id : endEdge->getID()), numericalID, endEdge, line), myEntryStop(entryStop) { }

    bool includeInRoute(bool /* allEdges */) const {
        return true;
    }

    const IntermodalEdge<E, L, N, V>* getEntryStop() const {
        return myEntryStop;
    }

    void addSchedule(const SUMOTime begin, const SUMOTime end, const SUMOTime period, const SUMOReal travelTimeSec) {
        mySchedules.insert(std::make_pair(STEPS2TIME(begin), Schedule(begin, end, period, travelTimeSec)));
    }

    SUMOReal getTravelTime(const IntermodalTrip<E, N, V>* const /* trip */, SUMOReal time) const {
        SUMOReal minArrivalSec = std::numeric_limits<SUMOReal>::max();
        for (typename std::multimap<SUMOReal, Schedule>::const_iterator it = mySchedules.begin(); it != mySchedules.end(); ++it) {
            if (it->first > minArrivalSec) {
                break;
            }
            if (time < STEPS2TIME(it->second.end)) {
                const long long int running = MAX2((SUMOTime)0, TIME2STEPS(time) - it->second.begin) / it->second.period;
                const SUMOTime nextDepart = it->second.begin + running * it->second.period;
                minArrivalSec = MIN2(STEPS2TIME(nextDepart) + it->second.travelTimeSec, minArrivalSec);
            }
        }
        return minArrivalSec - time;
    }

private:
    std::multimap<SUMOReal, Schedule> mySchedules;
    const IntermodalEdge<E, L, N, V>* const myEntryStop;

};


/// @brief the access edge connecting diferent modes that is given to the internal router (SUMOAbstractRouter)
template<class E, class L, class N, class V>
class AccessEdge : public IntermodalEdge<E, L, N, V> {
private:
    typedef IntermodalEdge<E, L, N, V> _IntermodalEdge;

public:
    AccessEdge(unsigned int numericalID, const _IntermodalEdge* inEdge, const _IntermodalEdge* outEdge,
               const SUMOReal inScale, const SUMOReal outScale, const SUMOReal transferTime = NUMERICAL_EPS) :
        _IntermodalEdge(inEdge->getID() + ":" + outEdge->getID(), numericalID, outEdge->getEdge(), "!access"),
        myInEdge(inEdge), myOutEdge(outEdge), myInScale(inScale), myOutScale(outScale), myTransferTime(transferTime) { }

    SUMOReal getTravelTime(const IntermodalTrip<E, N, V>* const trip, SUMOReal time) const {
        return myTransferTime - myInEdge->getTravelTime(trip, time) * myInScale - myOutEdge->getTravelTime(trip, time) * myOutScale;
    }

private:
    const _IntermodalEdge* const myInEdge;
    const _IntermodalEdge* const myOutEdge;
    const SUMOReal myInScale, myOutScale, myTransferTime;

};


/**
 * @class IntermodalRouter
 * The router for pedestrians (on a bidirectional network of sidewalks and crossings)
 */
template<class E, class L, class N, class V, class INTERNALROUTER = DijkstraRouterTT<IntermodalEdge<E, L, N, V>, IntermodalTrip<E, N, V>, prohibited_withPermissions<IntermodalEdge<E, L, N, V>, IntermodalTrip<E, N, V> > > >
class IntermodalRouter : public SUMOAbstractRouter<E, IntermodalTrip<E, N, V> > {
private:
    typedef void(* CreateNetCallback)(IntermodalRouter<E, L, N, V, INTERNALROUTER>&);
    typedef IntermodalEdge<E, L, N, V> _IntermodalEdge;
    typedef PublicTransportEdge<E, L, N, V> _PTEdge;
    typedef AccessEdge<E, L, N, V> _AccessEdge;
    typedef IntermodalNetwork<E, L, N, V> _IntermodalNetwork;
    typedef IntermodalTrip<E, N, V> _IntermodalTrip;

public:
    struct TripItem {
        TripItem(const std::string& _line="") : line(_line) {}
        const std::string line;
        std::string destStop;
        std::vector<const E*> edges;
    private:
        /// @brief Invalidated assignment operator
        TripItem& operator=(const TripItem& src);
    };

    /// Constructor
    IntermodalRouter(CreateNetCallback callback) :
        SUMOAbstractRouter<E, _IntermodalTrip>(0, "IntermodalRouter"), myAmClone(false), myInternalRouter(0), myIntermodalNet(0), myNumericalID(0), myCallback(callback) {
    }

    /// Destructor
    virtual ~IntermodalRouter() {
        delete myInternalRouter;
        if (!myAmClone) {
            delete myIntermodalNet;
        }
    }

    SUMOAbstractRouter<E, _IntermodalTrip>* clone() {
        createNet();
        return new IntermodalRouter<E, L, N, V, INTERNALROUTER>(myIntermodalNet);
    }

    void addAccess(const std::string& stopId, const E* stopEdge, const SUMOReal pos) {
        assert(stopEdge != 0);
        if (myStopConnections.count(stopId) == 0) {
            myStopConnections[stopId] = new StopEdge<E, L, N, V>(stopId, myNumericalID++, stopEdge);
            myIntermodalNet->addEdge(myStopConnections[stopId]);
        }
        _IntermodalEdge* const stopConn = myStopConnections[stopId];
        if (getSidewalk<E, L>(stopEdge) != 0) {
            const std::pair<_IntermodalEdge*, _IntermodalEdge*>& pair = myIntermodalNet->getBothDirections(stopEdge);
            _AccessEdge* forwardAccess = new _AccessEdge(myNumericalID++, pair.first, stopConn, 1 - pos / pair.first->getEdge()->getLength(), 0);
            myIntermodalNet->addEdge(forwardAccess);
            pair.first->addSuccessor(forwardAccess);
            forwardAccess->addSuccessor(stopConn);
            _AccessEdge* backwardAccess = new _AccessEdge(myNumericalID++, pair.second, stopConn, pos / pair.second->getEdge()->getLength(), 0);
            myIntermodalNet->addEdge(backwardAccess);
            pair.second->addSuccessor(backwardAccess);
            backwardAccess->addSuccessor(stopConn);

            _AccessEdge* forwardExit = new _AccessEdge(myNumericalID++, stopConn, pair.first, 0, pos / pair.first->getEdge()->getLength());
            myIntermodalNet->addEdge(forwardExit);
            stopConn->addSuccessor(forwardExit);
            forwardExit->addSuccessor(pair.first);
            _AccessEdge* backwardExit = new _AccessEdge(myNumericalID++, stopConn, pair.second, 0, 1 - pos / pair.second->getEdge()->getLength());
            myIntermodalNet->addEdge(backwardExit);
            stopConn->addSuccessor(backwardExit);
            backwardExit->addSuccessor(pair.second);

            if (myCarLookup.count(stopEdge) > 0) {
                _IntermodalEdge* const carEdge = myCarLookup[stopEdge];
                _AccessEdge* forward = new _AccessEdge(myNumericalID++, carEdge, pair.first, pos / carEdge->getEdge()->getLength(), 1 - (pos / pair.first->getEdge()->getLength()));
                myIntermodalNet->addEdge(forward);
                carEdge->addSuccessor(forward);
                forward->addSuccessor(pair.first);
                _AccessEdge* backward = new _AccessEdge(myNumericalID++, carEdge, pair.second, pos / carEdge->getEdge()->getLength(), pos / pair.second->getEdge()->getLength());
                myIntermodalNet->addEdge(backward);
                carEdge->addSuccessor(backward);
                backward->addSuccessor(pair.second);
            }
        }
    }

    void addSchedule(const SUMOVehicleParameter& pars) {
        SUMOTime lastUntil = 0;
        std::vector<SUMOVehicleParameter::Stop> validStops;
        for (std::vector<SUMOVehicleParameter::Stop>::const_iterator s = pars.stops.begin(); s != pars.stops.end(); ++s) {
            if (myStopConnections.count(s->busstop) > 0 && s->until >= lastUntil) {
                validStops.push_back(*s);
                lastUntil = s->until;
            }
        }
        if (validStops.size() < 2) {
            WRITE_WARNING("Ignoring public transport line '" + pars.line + "' with less than two usable stops.");
            return;
        }

        typename std::vector<_PTEdge*>& lineEdges = myPTLines[pars.line];
        if (lineEdges.empty()) {
            _IntermodalEdge* lastStop = 0;
            SUMOTime lastTime = 0;
            for (std::vector<SUMOVehicleParameter::Stop>::const_iterator s = validStops.begin(); s != validStops.end(); ++s) {
                _IntermodalEdge* currStop = myStopConnections[s->busstop];
                if (lastStop != 0) {
                    _PTEdge* const newEdge = new _PTEdge(s->busstop, myNumericalID++, lastStop, currStop->getEdge(), pars.line);
                    myIntermodalNet->addEdge(newEdge);
                    newEdge->addSchedule(lastTime, pars.repetitionEnd + lastTime - pars.depart, pars.repetitionOffset, STEPS2TIME(s->until - lastTime));
                    lastStop->addSuccessor(newEdge);
                    newEdge->addSuccessor(currStop);
                    lineEdges.push_back(newEdge);
                }
                lastTime = s->until;
                lastStop = currStop;
            }
        } else {
            if (validStops.size() != lineEdges.size() + 1) {
                WRITE_WARNING("Number of stops for public transport line '" + pars.line + "' does not match earlier definitions, ignoring schedule.");
                return;
            }
            if (lineEdges.front()->getEntryStop() != myStopConnections[validStops.front().busstop]) {
                WRITE_WARNING("Different stop for '" + pars.line + "' compared to earlier definitions, ignoring schedule.");
                return;
            }
            typename std::vector<_PTEdge*>::const_iterator lineEdge = lineEdges.begin();
            typename std::vector<SUMOVehicleParameter::Stop>::const_iterator s = validStops.begin() + 1;
            for (; s != validStops.end(); ++s, ++lineEdge) {
                if ((*lineEdge)->getSuccessors(SVC_IGNORING)[0] != myStopConnections[s->busstop]) {
                    WRITE_WARNING("Different stop for '" + pars.line + "' compared to earlier definitions, ignoring schedule.");
                    return;
                }
            }
            SUMOTime lastTime = validStops.front().until;
            for (lineEdge = lineEdges.begin(), s = validStops.begin() + 1; lineEdge != lineEdges.end(); ++lineEdge, ++s) {
                (*lineEdge)->addSchedule(lastTime, pars.repetitionEnd + lastTime - pars.depart, pars.repetitionOffset, STEPS2TIME(s->until - lastTime));
                lastTime = s->until;
            }
        }
    }

    /** @brief Builds the route between the given edges using the minimum effort at the given time
        The definition of the effort depends on the wished routing scheme */
    bool compute(const E* from, const E* to, SUMOReal departPos, SUMOReal arrivalPos, SUMOReal speed,
                 const V* const vehicle, const SVCPermissions modeSet, SUMOTime msTime,
                 std::vector<TripItem>& into) {
        createNet();
        _IntermodalTrip trip(from, to, departPos, arrivalPos, speed, msTime, 0, vehicle, modeSet);
        std::vector<const _IntermodalEdge*> intoPed;
        const bool success = myInternalRouter->compute(myIntermodalNet->getDepartEdge(from),
                                                       myIntermodalNet->getArrivalEdge(to),
                                                       &trip, msTime, intoPed);
        if (success) {
            std::string lastLine = "";
            for (size_t i = 0; i < intoPed.size(); ++i) {
                if (intoPed[i]->includeInRoute(false)) {
                    if (intoPed[i]->getLine() == "!stop") {
                        into.back().destStop = intoPed[i]->getID();
                    } else {
                        if (intoPed[i]->getLine() != lastLine) {
                            lastLine = intoPed[i]->getLine();
                            if (lastLine == "!car") {
                                into.push_back(TripItem(vehicle->getID()));
                            } else if (lastLine == "!ped") {
                                into.push_back(TripItem());
                            } else {
                                into.push_back(TripItem(lastLine));
                            }
                        }
                        into.back().edges.push_back(intoPed[i]->getEdge());
                    }
                }
            }
        }
#ifdef IntermodalRouter_DEBUG_ROUTES
        SUMOReal time = STEPS2TIME(msTime);
        for (size_t i = 0; i < intoPed.size(); ++i) {
            time += myInternalRouter->getEffort(intoPed[i], &trip, time);
        }
        std::cout << TIME2STEPS(msTime) << " trip from " << from->getID() << " to " << to->getID()
                  << " departPos=" << departPos
                  << " arrivalPos=" << arrivalPos
                  << " edges=" << toString(intoPed)
//                  << " resultEdges=" << toString(into)
                  << " time=" << time
                  << "\n";
#endif
        return success;
    }

    /** @brief Builds the route between the given edges using the minimum effort at the given time
        The definition of the effort depends on the wished routing scheme */
    bool compute(const E*, const E*, const _IntermodalTrip* const,
                 SUMOTime, std::vector<const E*>&) {
        throw ProcessError("Do not use this method");
    }

    SUMOReal recomputeCosts(const std::vector<const E*>&, const _IntermodalTrip* const, SUMOTime) const {
        throw ProcessError("Do not use this method");
    }

    void prohibit(const std::vector<E*>& toProhibit) {
        std::vector<_IntermodalEdge*> toProhibitPE;
        for (typename std::vector<E*>::const_iterator it = toProhibit.begin(); it != toProhibit.end(); ++it) {
            toProhibitPE.push_back(myIntermodalNet->getBothDirections(*it).first);
            toProhibitPE.push_back(myIntermodalNet->getBothDirections(*it).second);
            toProhibitPE.push_back(getCarEdge(*it));
        }
        myInternalRouter->prohibit(toProhibitPE);
    }

private:
    IntermodalRouter(_IntermodalNetwork* net):
        SUMOAbstractRouter<E, _IntermodalTrip>(0, "PedestrianRouter"), myAmClone(true),
        myInternalRouter(new INTERNALROUTER(net->getAllEdges(), true, &_IntermodalEdge::getTravelTimeStatic)),
        myIntermodalNet(net), myNumericalID((int)net->getAllEdges().size()) {}

    void addCarEdges(const std::vector<E*>& edges) {
        for (typename std::vector<E*>::const_iterator i = edges.begin(); i != edges.end(); ++i) {
            const E* const edge = *i;
            if (!edge->isInternal()) {
                myCarLookup[edge] = new CarEdge<E, L, N, V>(myNumericalID++, edge);
                myIntermodalNet->addEdge(myCarLookup[edge]);
            }
        }
        for (typename std::vector<E*>::const_iterator i = edges.begin(); i != edges.end(); ++i) {
            const E* const edge = *i;
            if (!edge->isInternal()) {
                _IntermodalEdge* startConnector = myIntermodalNet->getDepartEdge(edge);
                _IntermodalEdge* endConnector = myIntermodalNet->getArrivalEdge(edge);
                _IntermodalEdge* carEdge = getCarEdge(edge);
                if (getSidewalk<E, L>(edge) != 0) {
                    const std::pair<_IntermodalEdge*, _IntermodalEdge*>& pair = myIntermodalNet->getBothDirections(edge);
                    _IntermodalEdge* forwardAccess = new _AccessEdge(myNumericalID++, carEdge, pair.first, 0, 1);
                    myIntermodalNet->addEdge(forwardAccess);
                    carEdge->addSuccessor(forwardAccess);
                    forwardAccess->addSuccessor(pair.first);

                    _IntermodalEdge* backwardAccess = new _AccessEdge(myNumericalID++, carEdge, pair.second, 1, 1);
                    myIntermodalNet->addEdge(backwardAccess);
                    carEdge->addSuccessor(backwardAccess);
                    backwardAccess->addSuccessor(pair.second);
                }
                const std::vector<E*>& successors = edge->getSuccessors();
                for (typename std::vector<E*>::const_iterator it = successors.begin(); it != successors.end(); ++it) {
                    carEdge->addSuccessor(getCarEdge(*it));
                }
                startConnector->addSuccessor(carEdge);
                carEdge->addSuccessor(endConnector);
            }
        }
    }

    inline void createNet() {
        if (myIntermodalNet == 0) {
            myIntermodalNet = new _IntermodalNetwork(E::getAllEdges(), myNumericalID);
            myNumericalID = (int)myIntermodalNet->getAllEdges().size();
            addCarEdges(E::getAllEdges());
            myCallback(*this);
            myInternalRouter = new INTERNALROUTER(myIntermodalNet->getAllEdges(), true, &_IntermodalEdge::getTravelTimeStatic);
        }
    }

    /// @brief Returns the associated car edge
    _IntermodalEdge* getCarEdge(const E* e) {
        typename std::map<const E*, _IntermodalEdge*>::const_iterator it = myCarLookup.find(e);
        if (it == myCarLookup.end()) {
            throw ProcessError("Car edge '" + e->getID() + "' not found in pedestrian network.");
        }
        return it->second;
    }

private:
    const bool myAmClone;
    INTERNALROUTER* myInternalRouter;
    _IntermodalNetwork* myIntermodalNet;
    int myNumericalID;
    CreateNetCallback myCallback;

    /// @brief retrieve the car edge for the given input edge E
    std::map<const E*, _IntermodalEdge*> myCarLookup;

    /// @brief retrieve the public transport edges for the given line
    std::map<std::string, std::vector<_PTEdge*> > myPTLines;

    /// @brief retrieve the connecting edges for the given "bus" stop
    std::map<std::string, _IntermodalEdge*> myStopConnections;


private:
    /// @brief Invalidated assignment operator
    IntermodalRouter& operator=(const IntermodalRouter& s);

};



/**
 * @class RouterProvider
 * The encapsulation of the routers for vehicles and pedestrians
 */
template<class E, class L, class N, class V>
class RouterProvider {
public:
    RouterProvider(SUMOAbstractRouter<E, V>* vehRouter,
                   PedestrianRouterDijkstra<E, L, N, V>* pedRouter,
                   IntermodalRouter<E, L, N, V>* interRouter)
        : myVehRouter(vehRouter), myPedRouter(pedRouter), myInterRouter(interRouter) {}

    RouterProvider(const RouterProvider& original)
        : myVehRouter(original.getVehicleRouter().clone()),
        myPedRouter(static_cast<PedestrianRouterDijkstra<E, L, N, V>*>(original.getPedestrianRouter().clone())),
        myInterRouter(static_cast<IntermodalRouter<E, L, N, V>*>(original.getIntermodalRouter().clone())) {}

    SUMOAbstractRouter<E, V>& getVehicleRouter() const {
        return *myVehRouter;
    }

    PedestrianRouterDijkstra<E, L, N, V>& getPedestrianRouter() const {
        return *myPedRouter;
    }

    IntermodalRouter<E, L, N, V>& getIntermodalRouter() const {
        return *myInterRouter;
    }

    virtual ~RouterProvider() {
        delete myVehRouter;
        delete myPedRouter;
        delete myInterRouter;
    }


private:
    SUMOAbstractRouter<E, V>* const myVehRouter;
    PedestrianRouterDijkstra<E, L, N, V>* const myPedRouter;
    IntermodalRouter<E, L, N, V>* const myInterRouter;


private:
    /// @brief Invalidated assignment operator
    RouterProvider& operator=(const RouterProvider& src);

};


#endif

/****************************************************************************/
