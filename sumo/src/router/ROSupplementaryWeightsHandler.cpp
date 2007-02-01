/****************************************************************************/
/// @file    ROSupplementaryWeightsHandler.cpp
/// @author  Christian Roessel
/// @date    Thu Apr 08 2004 15:31 CEST
/// @version $Id: $
///
// / @author  Christian Roessel <christian.roessel@dlr.de>
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// copyright : (C) 2001-2007
//  by DLR (http://www.dlr.de/) and ZAIK (http://www.zaik.uni-koeln.de/AFS)
/****************************************************************************/
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/
// ===========================================================================
// compiler pragmas
// ===========================================================================
#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif


// ===========================================================================
// included modules
// ===========================================================================
#ifdef WIN32
#include <windows_config.h>
#else
#include <config.h>
#endif

#include "ROSupplementaryWeightsHandler.h"

#include <string>
#include <utility>
#include <utils/options/OptionsCont.h>
#include <utils/xml/GenericSAX2Handler.h>
#include <utils/xml/AttributesHandler.h>
#include <utils/common/UtilExceptions.h>
#include <utils/xml/XMLBuildingExceptions.h>
#include <utils/common/MsgHandler.h>
#include <utils/common/ToString.h>
#include <utils/sumoxml/SUMOXMLDefinitions.h>
#include <utils/sumoxml/SUMOSAXHandler.h>
#include <utils/router/FloatValueTimeLine.h>
#include <utils/common/TplConvert.h>
#include "ROEdge.h"
#include "RONet.h"

#ifdef _DEBUG
#include <utils/dev/debug_new.h>
#endif // _DEBUG


// ===========================================================================
// used namespaces
// ===========================================================================
using namespace std;


// ===========================================================================
// method definitions
// ===========================================================================
ROSupplementaryWeightsHandler::ROSupplementaryWeightsHandler(
    OptionsCont& optionCont
    , RONet& net
    , const std::string& filename) :
        SUMOSAXHandler("sumo-supplementary-netweights", filename)
        , optionsM(optionCont)
        , netM(net)
        , hasStartedSupplementaryWeightsM(false)
        , hasStartedIntervalM(false)
        , hasStartedWeightM(false)
        , isEdgeIdSetM(false)
        , isAbsolutValueSetM(false)
        , isMultValueSetM(false)
        , isAddValueSetM(false)
        , intervalStartM(0)
        , intervalEndM(0)
        , edgeIdM("")
        , absolutValueM(0)
        , multValueM(0)
        , addValueM(0)
        , absolutMapM()
        , multMapM()
        , addMapM()
        , weightedEdgesM()
{}


ROSupplementaryWeightsHandler::~ROSupplementaryWeightsHandler()
{
    absolutMapM.clear();
    multMapM.clear();
    addMapM.clear();
    weightedEdgesM.clear();
}

void
ROSupplementaryWeightsHandler::myStartElement(int
        , const std::string& name
        , const Attributes& attrs)
{
    if (name=="supplementary-weights") {
        startParseSupplementaryWeights(attrs);
    } else if (name=="interval") {
        startParseInterval(attrs);
    } else if (name=="weight") {
        startParseWeight(attrs);
    }
}

void
ROSupplementaryWeightsHandler::myEndElement(int, const std::string& name)
{
    if (name=="supplementary-weights") {
        stopParseSupplementaryWeights();
    } else if (name=="interval") {
        stopParseInterval();
    } else if (name=="weight") {
        stopParseWeight();
    }
}

void
ROSupplementaryWeightsHandler::startParseSupplementaryWeights(
    const Attributes& attrs)
{
    assert(attrs.getLength() == 0);
    assert(! hasStartedSupplementaryWeightsM);
    assert(! hasStartedIntervalM);
    assert(! hasStartedWeightM);
    hasStartedSupplementaryWeightsM = true;
}

void
ROSupplementaryWeightsHandler::startParseInterval(const Attributes& attrs)
{
    assert(attrs.getLength() == 2);
    assert(hasStartedSupplementaryWeightsM);
    assert(! hasStartedIntervalM);
    assert(! hasStartedWeightM);
    hasStartedIntervalM = true;

    try {
        intervalStartM = getLong(attrs, SUMO_ATTR_BEGIN);
        intervalEndM   = getLong(attrs, SUMO_ATTR_END);
    } catch (...) {
        MsgHandler::getErrorInstance()->inform(
            "Problems with timestep value.");
    }
}

void
ROSupplementaryWeightsHandler::startParseWeight(const Attributes& attrs)
{
    assert(attrs.getLength() >= 2);
    assert(hasStartedSupplementaryWeightsM);
    assert(hasStartedIntervalM);
    assert(! hasStartedWeightM);
    hasStartedWeightM = true;

    // Check attributes and assign them to members
    for (unsigned index = 0; index < attrs.getLength(); ++index) {
        const string attrName(
            TplConvert<XMLCh>::_2str(attrs.getLocalName(index)));
        const string attrValue(
            TplConvert<XMLCh>::_2str(attrs.getValue(index)));

        if (attrName=="edge-id") {
            edgeIdM      = attrValue;
            isEdgeIdSetM = true;
        } else if (attrName=="absolut") {
            absolutValueM      = TplConvert<char>::_2SUMOReal(attrValue.c_str());
            isAbsolutValueSetM = true;
        } else if (attrName=="mult") {
            multValueM      = TplConvert<char>::_2SUMOReal(attrValue.c_str());
            isMultValueSetM = true;
        } else if (attrName=="add") {
            addValueM      = TplConvert<char>::_2SUMOReal(attrValue.c_str());
            isAddValueSetM = true;
        }
    }

    assert(isEdgeIdSetM);
    if (isAbsolutValueSetM) {
        insertValuedTimeRangeIntoMap(absolutMapM, absolutValueM);
    }
    if (isMultValueSetM) {
        insertValuedTimeRangeIntoMap(multMapM, multValueM);
    }
    if (isAddValueSetM) {
        insertValuedTimeRangeIntoMap(addMapM, addValueM);
    }
    weightedEdgesM.insert(edgeIdM);
}

void
ROSupplementaryWeightsHandler::insertValuedTimeRangeIntoMap(
    WeightsMap& aMap
    , SUMOReal aValue)
{
    WeightsMapIt iter = aMap.find(edgeIdM);
    if (iter == aMap.end()) {
        FloatValueTimeLine* valueTimeLine = new FloatValueTimeLine();
        iter = aMap.insert(make_pair(edgeIdM, valueTimeLine)).first;
    }
    iter->second->add(intervalStartM, intervalEndM, (SUMOReal) aValue);
}


void
ROSupplementaryWeightsHandler::stopParseSupplementaryWeights(void)
{
    assert(hasStartedSupplementaryWeightsM);
    assert(! hasStartedIntervalM);
    assert(! hasStartedWeightM);
    // Do not allow several "supplementary-weights" tags, therefore don't
    // reset hasStartedSupplementaryWeights.

    // Pass timeValueLines to edges
    for (EdgeSetIt edgeIt = weightedEdgesM.begin();
            edgeIt != weightedEdgesM.end(); ++edgeIt) {

        string edgeId = *edgeIt;
        FloatValueTimeLine* absolut =
            getFloatValueTimeLine(absolutMapM, edgeId);
        FloatValueTimeLine* mult = getFloatValueTimeLine(multMapM, edgeId);
        FloatValueTimeLine* add = getFloatValueTimeLine(addMapM, edgeId);

        ROEdge *e = netM.getEdge(edgeId);
        if (e!=0) {
            e->setSupplementaryWeights(absolut, add, mult);
        } else {
            MsgHandler::getErrorInstance()->inform(
                "Could not add weight to the unknown edge '" + edgeId + "'.");

        }
    }
}

FloatValueTimeLine*
ROSupplementaryWeightsHandler::getFloatValueTimeLine(WeightsMap& aMap,
        string aEdgeId)
{
    WeightsMapIt it = aMap.find(aEdgeId);
    FloatValueTimeLine* retVal = 0;
    if (it == aMap.end()) {
        retVal = new FloatValueTimeLine();
    } else {
        retVal = it->second;
    }
    /*
       if(retVal!=0) {
           retVal->sort();
       }
    */
    return retVal;
}


void
ROSupplementaryWeightsHandler::stopParseInterval(void)
{
    assert(hasStartedSupplementaryWeightsM);
    assert(hasStartedIntervalM);
    assert(! hasStartedWeightM);
    hasStartedIntervalM = false;
}

void
ROSupplementaryWeightsHandler::stopParseWeight(void)
{
    assert(hasStartedSupplementaryWeightsM);
    assert(hasStartedIntervalM);
    assert(hasStartedWeightM);
    hasStartedWeightM   = false;
    isEdgeIdSetM       = false;
    isAbsolutValueSetM = false;
    isMultValueSetM    = false;
    isAddValueSetM     = false;
}



/****************************************************************************/

