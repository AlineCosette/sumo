/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2018 German Aerospace Center (DLR) and others.
// This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v2.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v20.html
// SPDX-License-Identifier: EPL-2.0
/****************************************************************************/
/// @file    GNESelectorFrame.cpp
/// @author  Jakob Erdmann
/// @date    Mar 2011
/// @version $Id$
///
// The Widget for modifying selections of network-elements
// (some elements adapted from GUIDialog_GLChosenEditor)
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <iostream>
#include <utils/foxtools/fxexdefs.h>
#include <utils/foxtools/MFXUtils.h>
#include <utils/common/MsgHandler.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/gui/div/GUIIOGlobals.h>
#include <utils/gui/div/GUIDesigns.h>
#include <utils/gui/images/GUIIconSubSys.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEViewParent.h>
#include <netedit/GNENet.h>
#include <netedit/netelements/GNEJunction.h>
#include <netedit/netelements/GNEEdge.h>
#include <netedit/netelements/GNELane.h>
#include <netedit/netelements/GNEConnection.h>
#include <netedit/netelements/GNECrossing.h>
#include <netedit/additionals/GNEAdditional.h>
#include <netedit/additionals/GNEPoly.h>
#include <netedit/additionals/GNEPOI.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEAttributeCarrier.h>

#include "GNESelectorFrame.h"


// ===========================================================================
// FOX callback mapping
// ===========================================================================
FXDEFMAP(GNESelectorFrame::ModificationMode) ModificationModeMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_CHOOSEN_OPERATION,  GNESelectorFrame::ModificationMode::onCmdSelectModificationMode)
};

FXDEFMAP(GNESelectorFrame::ElementSet) ElementSetMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_CHOOSEN_ELEMENTS,   GNESelectorFrame::ElementSet::onCmdSelectElementSet)
};

FXDEFMAP(GNESelectorFrame::MatchAttribute) MatchAttributeMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_SELECTORFRAME_SELECTTAG,        GNESelectorFrame::MatchAttribute::onCmdSelMBTag),
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_SELECTORFRAME_SELECTATTRIBUTE,  GNESelectorFrame::MatchAttribute::onCmdSelMBAttribute),
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_SELECTORFRAME_PROCESSSTRING,    GNESelectorFrame::MatchAttribute::onCmdSelMBString),
    FXMAPFUNC(SEL_COMMAND,  MID_HELP,                               GNESelectorFrame::MatchAttribute::onCmdHelp)
};

FXDEFMAP(GNESelectorFrame::VisualScaling) VisualScalingMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_GNE_SELECTORFRAME_SELECTSCALE,      GNESelectorFrame::VisualScaling::onCmdScaleSelection)
};

FXDEFMAP(GNESelectorFrame::SelectionOperation) SelectionOperationMap[] = {
    FXMAPFUNC(SEL_COMMAND,  MID_CHOOSEN_LOAD,   GNESelectorFrame::SelectionOperation::onCmdLoad),
    FXMAPFUNC(SEL_COMMAND,  MID_CHOOSEN_SAVE,   GNESelectorFrame::SelectionOperation::onCmdSave),
    FXMAPFUNC(SEL_COMMAND,  MID_CHOOSEN_INVERT, GNESelectorFrame::SelectionOperation::onCmdInvert),
    FXMAPFUNC(SEL_COMMAND,  MID_CHOOSEN_CLEAR,  GNESelectorFrame::SelectionOperation::onCmdClear)
};

// Object implementation
FXIMPLEMENT(GNESelectorFrame::ModificationMode,     FXGroupBox,     ModificationModeMap,    ARRAYNUMBER(ModificationModeMap))
FXIMPLEMENT(GNESelectorFrame::ElementSet,           FXGroupBox,     ElementSetMap,          ARRAYNUMBER(ElementSetMap))
FXIMPLEMENT(GNESelectorFrame::MatchAttribute,       FXGroupBox,     MatchAttributeMap,      ARRAYNUMBER(MatchAttributeMap))
FXIMPLEMENT(GNESelectorFrame::VisualScaling,        FXGroupBox,     VisualScalingMap,       ARRAYNUMBER(VisualScalingMap))
FXIMPLEMENT(GNESelectorFrame::SelectionOperation,   FXGroupBox,     SelectionOperationMap,  ARRAYNUMBER(SelectionOperationMap))

// ===========================================================================
// method definitions
// ===========================================================================

GNESelectorFrame::GNESelectorFrame(FXHorizontalFrame* horizontalFrameParent, GNEViewNet* viewNet):
    GNEFrame(horizontalFrameParent, viewNet, "Selection") {
    // create selectedItems modul
    myLockGLObjectTypes = new LockGLObjectTypes(this);
    // create Modification Mode modul
    myModificationMode = new ModificationMode(this);
    // create ElementSet modul
    myElementSet = new ElementSet(this);
    // create MatchAttribute modul
    myMatchAttribute = new MatchAttribute(this);
    // create VisualScaling modul
    myVisualScaling = new VisualScaling(this);
    // create SelectionOperation modul
    mySelectionOperation = new SelectionOperation(this);
    // Create groupbox for information about selections
    FXGroupBox* selectionHintGroupBox = new FXGroupBox(myContentFrame, "Information", GUIDesignGroupBoxFrame);
    // Create Selection Hint
    new FXLabel(selectionHintGroupBox, " - Hold <SHIFT> for \n   rectangle selection.\n - Press <DEL> to\n   delete selected items.", 0, GUIDesignLabelFrameInformation);
}


GNESelectorFrame::~GNESelectorFrame() {}


void
GNESelectorFrame::show() {
    // update selected items
    myLockGLObjectTypes->updateLockGLObjectTypes();
    // Show frame
    GNEFrame::show();
}


void
GNESelectorFrame::hide() {
    // hide frame
    GNEFrame::hide();
}


GNESelectorFrame::LockGLObjectTypes* 
GNESelectorFrame::getLockGLObjectTypes() const {
    return myLockGLObjectTypes;
}


void 
GNESelectorFrame::clearCurrentSelection() const {
    std::vector<GNEAttributeCarrier*> selectedACs = myViewNet->getNet()->getSelectedAttributeCarriers();
    for (auto i : selectedACs) {
        if(std::find(GNEAttributeCarrier::allowedNetElementsTags().begin(), GNEAttributeCarrier::allowedNetElementsTags().end(), i->getTag()) != GNEAttributeCarrier::allowedNetElementsTags().end()) {
            dynamic_cast<GNENetElement*>(i)->unselectNetElement();
        } else if(std::find(GNEAttributeCarrier::allowedAdditionalTags().begin(), GNEAttributeCarrier::allowedAdditionalTags().end(), i->getTag()) != GNEAttributeCarrier::allowedAdditionalTags().end()) {
            dynamic_cast<GNEAdditional*>(i)->unselectAdditional();
        } else if(std::find(GNEAttributeCarrier::allowedShapeTags().begin(), GNEAttributeCarrier::allowedShapeTags().end(), i->getTag()) != GNEAttributeCarrier::allowedShapeTags().end()) {
            dynamic_cast<GNEShape*>(i)->unselectShape();
        } else {
            throw ProcessError("Invalid element set");
        }
    }
}


void
GNESelectorFrame::handleIDs(std::vector<GNEAttributeCarrier*> ACs, ModificationMode::SetOperation setop) {
    const ModificationMode::SetOperation setOperation = ((setop == ModificationMode::SET_DEFAULT) ? myModificationMode->getModificationMode() : setop);
    // first save previous selection
    std::vector<GNEAttributeCarrier*> previousACSelection = myViewNet->getNet()->getSelectedAttributeCarriers();
    // start change selection operation
    myViewNet->getUndoList()->p_begin("change selection");
    // if we're replacing or restricting, we need to unselect all previous selected attribute carrier
    if ((setOperation == ModificationMode::SET_REPLACE) || (setOperation == ModificationMode::SET_RESTRICT)) {
        // change attribute GNE_ATTR_SELECTED of all selected items to false
        for (auto i : previousACSelection) {
            i->setAttribute(GNE_ATTR_SELECTED, "false", myViewNet->getUndoList());
        }
    }
    // declare set to keep ACs to select and undselect
    std::vector<GNEAttributeCarrier*> selectACs;
    std::vector<GNEAttributeCarrier*> unselectACs;
    // handle ids
    for (auto i : ACs) {
        // doing the switch outside the loop requires functional techniques. this was deemed to ugly
        switch (setOperation) {
            case GNESelectorFrame::ModificationMode::SET_ADD:
            case GNESelectorFrame::ModificationMode::SET_REPLACE:
                selectACs.push_back(i);
                break;
            case GNESelectorFrame::ModificationMode::SET_SUB:
                unselectACs.push_back(i);
                break;
            case GNESelectorFrame::ModificationMode::SET_RESTRICT:
                if (std::find(previousACSelection.begin(), previousACSelection.end(), i) != previousACSelection.end()) {
                    selectACs.push_back(i);
                } else {
                    unselectACs.push_back(i);
                }
                break;
            default:
                break;
        }
    }
    // change attribute selection of selectACs and unselectACs
    for (auto i : selectACs) {
        i->setAttribute(GNE_ATTR_SELECTED, "true", myViewNet->getUndoList());
    }
    for (auto i : unselectACs) {
        i->setAttribute(GNE_ATTR_SELECTED, "false", myViewNet->getUndoList());
    }
    // finish operation
    myViewNet->getUndoList()->p_end();
    // update view
    myViewNet->update();
}


std::vector<GNEAttributeCarrier*>
GNESelectorFrame::getMatches(SumoXMLTag ACTag, SumoXMLAttr ACAttr, char compOp, double val, const std::string& expr) {
    std::vector<GNEAttributeCarrier*> result;
    std::vector<GNEAttributeCarrier*> allACbyTag = myViewNet->getNet()->retrieveAttributeCarriers(ACTag);
    const bool numerical = GNEAttributeCarrier::isNumerical(ACTag, ACAttr);
    for (auto it : allACbyTag) {
        if (expr == "") {
            result.push_back(it);
        } else if (numerical) {
            double acVal;
            std::istringstream buf(it->getAttribute(ACAttr));
            buf >> acVal;
            switch (compOp) {
                case '<':
                    if (acVal < val) {
                        result.push_back(it);
                    }
                    break;
                case '>':
                    if (acVal > val) {
                        result.push_back(it);
                    }
                    break;
                case '=':
                    if (acVal == val) {
                        result.push_back(it);
                    }
                    break;
            }
        } else {
            // string match
            std::string acVal = it->getAttributeForSelection(ACAttr);
            switch (compOp) {
                case '@':
                    if (acVal.find(expr) != std::string::npos) {
                        result.push_back(it);
                    }
                    break;
                case '!':
                    if (acVal.find(expr) == std::string::npos) {
                        result.push_back(it);
                    }
                    break;
                case '=':
                    if (acVal == expr) {
                        result.push_back(it);
                    }
                    break;
                case '^':
                    if (acVal != expr) {
                        result.push_back(it);
                    }
                    break;
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// ModificationMode::LockGLObjectTypes - methods
// ---------------------------------------------------------------------------

GNESelectorFrame::LockGLObjectTypes::LockGLObjectTypes(GNESelectorFrame *selectorFrameParent) :
    FXGroupBox(selectorFrameParent->myContentFrame, "Selected items", GUIDesignGroupBoxFrame),
    mySelectorFrameParent(selectorFrameParent) {
    // create combo box and labels for selected items
    FXMatrix* mLockGLObjectTypes = new FXMatrix(this, 3, (LAYOUT_FILL_X | LAYOUT_BOTTOM | LAYOUT_LEFT | MATRIX_BY_COLUMNS), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    // create typeEntries for the different elements
    myTypeEntries[GLO_JUNCTION] = ObjectTypeEntry(mLockGLObjectTypes, "Junctions", "junction");
    myTypeEntries[GLO_EDGE] = ObjectTypeEntry(mLockGLObjectTypes, "Edges", "edge");
    myTypeEntries[GLO_LANE] = ObjectTypeEntry(mLockGLObjectTypes, "Lanes", "lane");
    myTypeEntries[GLO_CONNECTION] = ObjectTypeEntry(mLockGLObjectTypes, "Connections", "connection");
    myTypeEntries[GLO_ADDITIONAL] = ObjectTypeEntry(mLockGLObjectTypes, "Additionals", "additional");
    myTypeEntries[GLO_CROSSING] = ObjectTypeEntry(mLockGLObjectTypes, "Crossings", "crossing");
    myTypeEntries[GLO_POLYGON] = ObjectTypeEntry(mLockGLObjectTypes, "Polygons", "polygon");
    myTypeEntries[GLO_POI] = ObjectTypeEntry(mLockGLObjectTypes, "POIs", "POI");
}


GNESelectorFrame::LockGLObjectTypes::~LockGLObjectTypes() {}


void 
GNESelectorFrame::LockGLObjectTypes::updateLockGLObjectTypes() {
    myTypeEntries[GLO_JUNCTION].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_JUNCTION).size()).c_str());
    myTypeEntries[GLO_EDGE].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_EDGE).size()).c_str());
    myTypeEntries[GLO_LANE].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_LANE).size()).c_str());
    myTypeEntries[GLO_CONNECTION].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_CONNECTION).size()).c_str());
    myTypeEntries[GLO_ADDITIONAL].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_ADDITIONAL).size()).c_str());
    myTypeEntries[GLO_CROSSING].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_CROSSING).size()).c_str());
    myTypeEntries[GLO_POLYGON].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_POLYGON).size()).c_str());
    myTypeEntries[GLO_POI].count->setText(toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_POI).size()).c_str());
    // show information in debug mode
    if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
    WRITE_WARNING("Current selection: " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_JUNCTION).size()) + " Junctions, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_EDGE).size()) + " Edges, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_LANE).size()) + " Lanes, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_CONNECTION).size()) + " connections, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_ADDITIONAL).size()) + " Additionals, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_CROSSING).size()) + " Crossings, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_POLYGON).size()) + " Polygons, " +
        toString(mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers(GLO_POI).size()) + " POIs");
    }
}


bool 
GNESelectorFrame::LockGLObjectTypes::IsObjectTypeLocked(GUIGlObjectType type) const {
    return myTypeEntries.at(type).locked->getCheck() != FALSE;
}


GNESelectorFrame::LockGLObjectTypes::ObjectTypeEntry::ObjectTypeEntry(FXMatrix* parent, const std::string& label, const std::string& label2) {
    count = new FXLabel(parent, "0", 0, GUIDesignLabelLeft);
    typeName = new FXLabel(parent, label.c_str(), 0, GUIDesignLabelLeft);
    locked = new FXMenuCheck(parent, ("lock\t\tLock " + label2 + " selection").c_str(), 0, 0, LAYOUT_FILL_X | LAYOUT_RIGHT);
}

// ---------------------------------------------------------------------------
// ModificationMode::ModificationMode - methods
// ---------------------------------------------------------------------------

GNESelectorFrame::ModificationMode::ModificationMode(GNESelectorFrame *selectorFrameParent) :
    FXGroupBox(selectorFrameParent->myContentFrame, "Modification Mode", GUIDesignGroupBoxFrame),
    mySelectorFrameParent(selectorFrameParent),
    myModificationModeType(SET_ADD) {
    // Create all options buttons
    myAddRadioButton = new FXRadioButton(this, "add\t\tSelected objects are added to the previous selection",
        this, MID_CHOOSEN_OPERATION, GUIDesignRadioButton);
    myRemoveRadioButton = new FXRadioButton(this, "remove\t\tSelected objects are removed from the previous selection",
        this, MID_CHOOSEN_OPERATION, GUIDesignRadioButton);
    myKeepRadioButton = new FXRadioButton(this, "keep\t\tRestrict previous selection by the current selection",
        this, MID_CHOOSEN_OPERATION, GUIDesignRadioButton);
    myReplaceRadioButton = new FXRadioButton(this, "replace\t\tReplace previous selection by the current selection",
        this, MID_CHOOSEN_OPERATION, GUIDesignRadioButton);
    myAddRadioButton->setCheck(true);
}


GNESelectorFrame::ModificationMode::~ModificationMode() {}


GNESelectorFrame::ModificationMode::SetOperation 
GNESelectorFrame::ModificationMode::getModificationMode() const {
    return myModificationModeType;
}


long
GNESelectorFrame::ModificationMode::onCmdSelectModificationMode(FXObject* obj, FXSelector, void*) {
    if (obj == myAddRadioButton) {
        myModificationModeType = SET_ADD;
        myAddRadioButton->setCheck(true);
        myRemoveRadioButton->setCheck(false);
        myKeepRadioButton->setCheck(false);
        myReplaceRadioButton->setCheck(false);
        return 1;
    } else if (obj == myRemoveRadioButton) {
        myModificationModeType = SET_SUB;
        myAddRadioButton->setCheck(false);
        myRemoveRadioButton->setCheck(true);
        myKeepRadioButton->setCheck(false);
        myReplaceRadioButton->setCheck(false);
        return 1;
    } else if (obj == myKeepRadioButton) {
        myModificationModeType = SET_RESTRICT;
        myAddRadioButton->setCheck(false);
        myRemoveRadioButton->setCheck(false);
        myKeepRadioButton->setCheck(true);
        myReplaceRadioButton->setCheck(false);
        return 1;
    } else if (obj == myReplaceRadioButton) {
        myModificationModeType = SET_REPLACE;
        myAddRadioButton->setCheck(false);
        myRemoveRadioButton->setCheck(false);
        myKeepRadioButton->setCheck(false);
        myReplaceRadioButton->setCheck(true);
        return 1;
    } else {
        return 0;
    }
}

// ---------------------------------------------------------------------------
// ModificationMode::ElementSet - methods
// ---------------------------------------------------------------------------

GNESelectorFrame::ElementSet::ElementSet(GNESelectorFrame *selectorFrameParent) :
    FXGroupBox(selectorFrameParent->myContentFrame, "Element Set", GUIDesignGroupBoxFrame),
    mySelectorFrameParent(selectorFrameParent),
    myCurrentElementSet(ELEMENTSET_NETELEMENT) {
    // Create MatchTagBox for tags and fill it
    mySetComboBox = new FXComboBox(this, GUIDesignComboBoxNCol, this, MID_CHOOSEN_ELEMENTS, GUIDesignComboBox);
    mySetComboBox->appendItem("Net Element");
    mySetComboBox->appendItem("Additional");
    mySetComboBox->appendItem("Shape");
    mySetComboBox->setNumVisible(mySetComboBox->getNumItems());
}


GNESelectorFrame::ElementSet::~ElementSet() {}


GNESelectorFrame::ElementSet::ElementSetType
GNESelectorFrame::ElementSet::getElementSet() const {
    return myCurrentElementSet;
}


long
GNESelectorFrame::ElementSet::onCmdSelectElementSet(FXObject*, FXSelector, void*) {
    if (mySetComboBox->getText() == "Net Element") {
        myCurrentElementSet = ELEMENTSET_NETELEMENT;
        mySetComboBox->setTextColor(FXRGB(0, 0, 0));
        // enable match attribute
        mySelectorFrameParent->myMatchAttribute->enableMatchAttribute();
    } else if (mySetComboBox->getText() == "Additional") {
        myCurrentElementSet = ELEMENTSET_ADDITIONAL;
        mySetComboBox->setTextColor(FXRGB(0, 0, 0));
        // enable match attribute
        mySelectorFrameParent->myMatchAttribute->enableMatchAttribute();
    } else if (mySetComboBox->getText() == "Shape") {
        myCurrentElementSet = ELEMENTSET_SHAPE;
        mySetComboBox->setTextColor(FXRGB(0, 0, 0));
        // enable match attribute
        mySelectorFrameParent->myMatchAttribute->enableMatchAttribute();
    } else {
        myCurrentElementSet = ELEMENTSET_INVALID;
        mySetComboBox->setTextColor(FXRGB(255, 0, 0));
        // disable match attribute
        mySelectorFrameParent->myMatchAttribute->disableMatchAttribute();
    }
    return 1;
}

// ---------------------------------------------------------------------------
// ModificationMode::MatchAttribute - methods
// ---------------------------------------------------------------------------

GNESelectorFrame::MatchAttribute::MatchAttribute(GNESelectorFrame *selectorFrameParent) :
    FXGroupBox(selectorFrameParent->myContentFrame, "Match Attribute", GUIDesignGroupBoxFrame),
    mySelectorFrameParent(selectorFrameParent),
    myCurrentTag(SUMO_TAG_EDGE),
    myCurrentAttribute(SUMO_ATTR_ID) {
    // Create MatchTagBox for tags
    myMatchTagComboBox = new FXComboBox(this, GUIDesignComboBoxNCol, this, MID_GNE_SELECTORFRAME_SELECTTAG, GUIDesignComboBox);
    // Create listBox for Attributes
    myMatchAttrComboBox = new FXComboBox(this, GUIDesignComboBoxNCol, this, MID_GNE_SELECTORFRAME_SELECTATTRIBUTE, GUIDesignComboBox);
    // Create TextField for Match string
    myMatchString = new FXTextField(this, GUIDesignTextFieldNCol, this, MID_GNE_SELECTORFRAME_PROCESSSTRING, GUIDesignTextField);
    // Create help button
    new FXButton(this, "Help", 0, this, MID_HELP, GUIDesignButtonRectangular);
    // Fill list of sub-items (first element will be "edge")
    enableMatchAttribute();
    // Set speed of edge as default attribute
    myMatchAttrComboBox->setText("speed");
    myCurrentAttribute = SUMO_ATTR_SPEED;
    // Set default value for Match string
    myMatchString->setText(">10.0");
}


GNESelectorFrame::MatchAttribute::~MatchAttribute() {}


void 
GNESelectorFrame::MatchAttribute::enableMatchAttribute() {
    // enable comboboxes and text field
    myMatchTagComboBox->enable();
    myMatchAttrComboBox->enable();
    myMatchString->enable();
    // Clear items of myMatchTagComboBox
    myMatchTagComboBox->clearItems();
    // Set items depending of current item set
    if (mySelectorFrameParent->myElementSet->getElementSet() == ElementSet::ELEMENTSET_NETELEMENT) {
        for (auto i : GNEAttributeCarrier::allowedNetElementsTags()) {
            myMatchTagComboBox->appendItem(toString(i).c_str());
        }
    } else if (mySelectorFrameParent->myElementSet->getElementSet() == ElementSet::ELEMENTSET_ADDITIONAL) {
        for (auto i : GNEAttributeCarrier::allowedAdditionalTags()) {
            myMatchTagComboBox->appendItem(toString(i).c_str());
        }
    } else if (mySelectorFrameParent->myElementSet->getElementSet() == ElementSet::ELEMENTSET_SHAPE) {
        for (auto i : GNEAttributeCarrier::allowedShapeTags()) {
            myMatchTagComboBox->appendItem(toString(i).c_str());
        }
    } else {
        throw ProcessError("Invalid element set");
    }
    // set first item as current item
    myMatchTagComboBox->setCurrentItem(0);
    myMatchTagComboBox->setNumVisible(myMatchTagComboBox->getNumItems());
    // Fill attributes with the current element type
    onCmdSelMBTag(0, 0, 0);
}


void 
GNESelectorFrame::MatchAttribute::disableMatchAttribute() {
    // disable comboboxes and text field
    myMatchTagComboBox->disable();
    myMatchAttrComboBox->disable();
    myMatchString->disable();
    // change colors to black (even if there are invalid values)
    myMatchTagComboBox->setTextColor(FXRGB(0, 0, 0));
    myMatchAttrComboBox->setTextColor(FXRGB(0, 0, 0));
    myMatchString->setTextColor(FXRGB(0, 0, 0));
}


long
GNESelectorFrame::MatchAttribute::onCmdSelMBTag(FXObject*, FXSelector, void*) {
    // First check what type of elementes is being selected
    myCurrentTag = SUMO_TAG_NOTHING;
    // find current element tag
    if (mySelectorFrameParent->myElementSet->getElementSet() == ElementSet::ELEMENTSET_NETELEMENT) {
        for (auto i : GNEAttributeCarrier::allowedNetElementsTags()) {
            if (toString(i) == myMatchTagComboBox->getText().text()) {
                myCurrentTag = i;
            }
        }
    } else if (mySelectorFrameParent->myElementSet->getElementSet() == ElementSet::ELEMENTSET_ADDITIONAL) {
        for (auto i : GNEAttributeCarrier::allowedAdditionalTags()) {
            if (toString(i) == myMatchTagComboBox->getText().text()) {
                myCurrentTag = i;
            }
        }
    } else if (mySelectorFrameParent->myElementSet->getElementSet() == ElementSet::ELEMENTSET_SHAPE) {
        for (auto i : GNEAttributeCarrier::allowedShapeTags()) {
            if (toString(i) == myMatchTagComboBox->getText().text()) {
                myCurrentTag = i;
            }
        }
    } else {
        throw ProcessError("Unkown set");
    }
    // check that typed-by-user value is correct
    if (myCurrentTag != SUMO_TAG_NOTHING) {
        // set color and enable items
        myMatchTagComboBox->setTextColor(FXRGB(0, 0, 0));
        myMatchAttrComboBox->enable();
        myMatchString->enable();
        myMatchAttrComboBox->clearItems();
        // fill attribute combo box
        for (auto it : GNEAttributeCarrier::allowedAttributes(myCurrentTag)) {
            myMatchAttrComboBox->appendItem(toString(it.first).c_str());
        }
        // check if item can block movement
        if(GNEAttributeCarrier::canBlockMovement(myCurrentTag)) {
            myMatchAttrComboBox->appendItem(toString(GNE_ATTR_BLOCK_MOVEMENT).c_str());
        }
        // check if item can block shape
        if(GNEAttributeCarrier::canBlockShape(myCurrentTag)) {
            myMatchAttrComboBox->appendItem(toString(GNE_ATTR_BLOCK_SHAPE).c_str());
        }
        // check if item can close shape
        if(GNEAttributeCarrier::canCloseShape(myCurrentTag)) {
            myMatchAttrComboBox->appendItem(toString(GNE_ATTR_CLOSE_SHAPE).c_str());
        }
        // check if item can have parent
        if(GNEAttributeCarrier::canHaveParent(myCurrentTag)) {
            myMatchAttrComboBox->appendItem(toString(GNE_ATTR_PARENT).c_str());
        }
        // @ToDo: Here can be placed a button to set the default value
        myMatchAttrComboBox->setNumVisible(myMatchAttrComboBox->getNumItems());
        onCmdSelMBAttribute(0, 0, 0);
    } else {
        // change color to red and disable items
        myMatchTagComboBox->setTextColor(FXRGB(255, 0, 0));
        myMatchAttrComboBox->disable();
        myMatchString->disable();
    }
    update();
    return 1;
}


long
GNESelectorFrame::MatchAttribute::onCmdSelMBAttribute(FXObject*, FXSelector, void*) {
    // first obtain all item attributes vinculated with current tag
    std::vector<std::pair <SumoXMLAttr, std::string> > itemAttrs = GNEAttributeCarrier::allowedAttributes(myCurrentTag);
    // add extra attribute if item can block movement
    if(GNEAttributeCarrier::canBlockMovement(myCurrentTag)) {
        itemAttrs.push_back(std::pair<SumoXMLAttr, std::string>(GNE_ATTR_BLOCK_MOVEMENT, "false"));
    }
    // add extra attribute if item can block shape
    if(GNEAttributeCarrier::canBlockShape(myCurrentTag)) {
        itemAttrs.push_back(std::pair<SumoXMLAttr, std::string>(GNE_ATTR_BLOCK_SHAPE, "false"));
    }
    // add extra attribute if item can close shape
    if(GNEAttributeCarrier::canCloseShape(myCurrentTag)) {
        itemAttrs.push_back(std::pair<SumoXMLAttr, std::string>(GNE_ATTR_CLOSE_SHAPE, "true"));
    }
    // add extra attribute if item can have parent
    if(GNEAttributeCarrier::canHaveParent(myCurrentTag)) {
        itemAttrs.push_back(std::pair<SumoXMLAttr, std::string>(GNE_ATTR_PARENT, ""));
    }
    // set current selected attribute
    myCurrentAttribute = SUMO_ATTR_NOTHING;
    for (auto i : itemAttrs) {
        if (toString(i.first) == myMatchAttrComboBox->getText().text()) {
            myCurrentAttribute = i.first;
        }
    }
    // check if selected attribute is valid
    if (myCurrentAttribute != SUMO_ATTR_NOTHING) {
        myMatchAttrComboBox->setTextColor(FXRGB(0, 0, 0));
        myMatchString->enable();
    } else {
        myMatchAttrComboBox->setTextColor(FXRGB(255, 0, 0));
        myMatchString->disable();
    }
    return 1;
}


long
GNESelectorFrame::MatchAttribute::onCmdSelMBString(FXObject*, FXSelector, void*) {
    // obtain expresion
    std::string expr(myMatchString->getText().text());
    bool valid = true;
    if (expr == "") {
        // the empty expression matches all objects
        mySelectorFrameParent->handleIDs(mySelectorFrameParent->getMatches(myCurrentTag, myCurrentAttribute, '@', 0, expr));
    } else if (GNEAttributeCarrier::isNumerical(myCurrentTag, myCurrentAttribute)) {
        // The expression must have the form
        //  <val matches if attr < val
        //  >val matches if attr > val
        //  =val matches if attr = val
        //  val matches if attr = val
        char compOp = expr[0];
        if (compOp == '<' || compOp == '>' || compOp == '=') {
            expr = expr.substr(1);
        } else {
            compOp = '=';
        }
        try {
            mySelectorFrameParent->handleIDs(mySelectorFrameParent->getMatches(myCurrentTag, myCurrentAttribute, compOp, GNEAttributeCarrier::parse<double>(expr.c_str()), expr));
        } catch (EmptyData&) {
            valid = false;
        } catch (NumberFormatException&) {
            valid = false;
        }
    } else {
        // The expression must have the form
        //   =str: matches if <str> is an exact match
        //   !str: matches if <str> is not a substring
        //   ^str: matches if <str> is not an exact match
        //   str: matches if <str> is a substring (sends compOp '@')
        // Alternatively, if the expression is empty it matches all objects
        char compOp = expr[0];
        if (compOp == '=' || compOp == '!' || compOp == '^') {
            expr = expr.substr(1);
        } else {
            compOp = '@';
        }
        mySelectorFrameParent->handleIDs(mySelectorFrameParent->getMatches(myCurrentTag, myCurrentAttribute, compOp, 0, expr));
    }
    if (valid) {
        myMatchString->setTextColor(FXRGB(0, 0, 0));
        myMatchString->killFocus();
    } else {
        myMatchString->setTextColor(FXRGB(255, 0, 0));
    }
    return 1;
}


long
GNESelectorFrame::MatchAttribute::onCmdHelp(FXObject*, FXSelector, void*) {
    // Create dialog box
    FXDialogBox* additionalNeteditAttributesHelpDialog = new FXDialogBox(this, "Netedit Parameters Help", GUIDesignDialogBox);
    additionalNeteditAttributesHelpDialog->setIcon(GUIIconSubSys::getIcon(ICON_MODEADDITIONAL));
    // set help text
    std::ostringstream help;
    help
        << "- The 'Match Attribute' controls allow to specify a set of objects which are then applied to the current selection\n"
        << "  according to the current 'Modification Mode'.\n"
        << "     1. Select an object type from the first input box\n"
        << "     2. Select an attribute from the second input box\n"
        << "     3. Enter a 'match expression' in the third input box and press <return>\n"
        << "\n"
        << "- The empty expression matches all objects\n"
        << "- For numerical attributes the match expression must consist of a comparison operator ('<', '>', '=') and a number.\n"
        << "- An object matches if the comparison between its attribute and the given number by the given operator evaluates to 'true'\n"
        << "\n"
        << "- For string attributes the match expression must consist of a comparison operator ('', '=', '!', '^') and a string.\n"
        << "     '' (no operator) matches if string is a substring of that object'ts attribute.\n"
        << "     '=' matches if string is an exact match.\n"
        << "     '!' matches if string is not a substring.\n"
        << "     '^' matches if string is not an exact match.\n"
        << "\n"
        << "- Examples:\n"
        << "     junction; id; 'foo' -> match all junctions that have 'foo' in their id\n"
        << "     junction; type; '=priority' -> match all junctions of type 'priority', but not of type 'priority_stop'\n"
        << "     edge; speed; '>10' -> match all edges with a speed above 10\n";
    // Create label with the help text
    new FXLabel(additionalNeteditAttributesHelpDialog, help.str().c_str(), 0, GUIDesignLabelFrameInformation);
    // Create horizontal separator
    new FXHorizontalSeparator(additionalNeteditAttributesHelpDialog, GUIDesignHorizontalSeparator);
    // Create frame for OK Button
    FXHorizontalFrame* myHorizontalFrameOKButton = new FXHorizontalFrame(additionalNeteditAttributesHelpDialog, GUIDesignAuxiliarHorizontalFrame);
    // Create Button Close (And two more horizontal frames to center it)
    new FXHorizontalFrame(myHorizontalFrameOKButton, GUIDesignAuxiliarHorizontalFrame);
    new FXButton(myHorizontalFrameOKButton, "OK\t\tclose", GUIIconSubSys::getIcon(ICON_ACCEPT), additionalNeteditAttributesHelpDialog, FXDialogBox::ID_ACCEPT, GUIDesignButtonOK);
    new FXHorizontalFrame(myHorizontalFrameOKButton, GUIDesignAuxiliarHorizontalFrame);
    // Write Warning in console if we're in testing mode
    if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
        WRITE_WARNING("Opening help dialog of selector frame");
    }
    // create Dialog
    additionalNeteditAttributesHelpDialog->create();
    // show in the given position
    additionalNeteditAttributesHelpDialog->show(PLACEMENT_CURSOR);
    // refresh APP
    getApp()->refresh();
    // open as modal dialog (will block all windows until stop() or stopModal() is called)
    getApp()->runModalFor(additionalNeteditAttributesHelpDialog);
    // Write Warning in console if we're in testing mode
    if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
        WRITE_WARNING("Close help dialog of selector frame");
    }
    return 1;
}

// ---------------------------------------------------------------------------
// ModificationMode::VisualScaling - methods
// ---------------------------------------------------------------------------

GNESelectorFrame::VisualScaling::VisualScaling(GNESelectorFrame *selectorFrameParent) :
    FXGroupBox(selectorFrameParent->myContentFrame, "Visual Scaling", GUIDesignGroupBoxFrame),
    mySelectorFrameParent(selectorFrameParent) {
    // Create spin button and configure it
    mySelectionScaling = new FXRealSpinner(this, 7, this, MID_GNE_SELECTORFRAME_SELECTSCALE, GUIDesignSpinDial);
    //mySelectionScaling->setNumberFormat(1);
    //mySelectionScaling->setIncrements(0.1, .5, 1);
    mySelectionScaling->setIncrement(0.5);
    mySelectionScaling->setRange(1, 100);
    mySelectionScaling->setValue(1);
    mySelectionScaling->setHelpText("Enlarge selected objects");
}


GNESelectorFrame::VisualScaling::~VisualScaling() {}


long
GNESelectorFrame::VisualScaling::onCmdScaleSelection(FXObject*, FXSelector, void*) {
    // set scale in viewnet
    mySelectorFrameParent->getViewNet()->setSelectionScaling(mySelectionScaling->getValue());
    mySelectorFrameParent->getViewNet()->update();
    return 1;
}

// ---------------------------------------------------------------------------
// ModificationMode::SelectionOperation - methods
// ---------------------------------------------------------------------------

GNESelectorFrame::SelectionOperation::SelectionOperation(GNESelectorFrame *selectorFrameParent) :
    FXGroupBox(selectorFrameParent->myContentFrame, "Operations for selections", GUIDesignGroupBoxFrame),
    mySelectorFrameParent(selectorFrameParent) {
    // Create "Clear List" Button
    new FXButton(this, "Clear\t\t", 0, this, MID_CHOOSEN_CLEAR, GUIDesignButton);
    // Create "Invert" Button
    new FXButton(this, "Invert\t\t", 0, this, MID_CHOOSEN_INVERT, GUIDesignButton);
    // Create "Save" Button
    new FXButton(this, "Save\t\tSave ids of currently selected objects to a file.", 0, this, MID_CHOOSEN_SAVE, GUIDesignButton);
    // Create "Load" Button
    new FXButton(this, "Load\t\tLoad ids from a file according to the current modfication mode.", 0, this, MID_CHOOSEN_LOAD, GUIDesignButton);
}


GNESelectorFrame::SelectionOperation::~SelectionOperation() {}


long
GNESelectorFrame::SelectionOperation::onCmdLoad(FXObject*, FXSelector, void*) {
    // get the new file name
    FXFileDialog opendialog(this, "Open List of Selected Items");
    opendialog.setIcon(GUIIconSubSys::getIcon(ICON_EMPTY));
    opendialog.setSelectMode(SELECTFILE_EXISTING);
    opendialog.setPatternList("Selection files (*.txt)\nAll files (*)");
    if (gCurrentFolder.length() != 0) {
        opendialog.setDirectory(gCurrentFolder);
    }
    if (opendialog.execute()) {
        std::vector<GNEAttributeCarrier*> loadedACs;
        gCurrentFolder = opendialog.getDirectory();
        std::string file = opendialog.getFilename().text();
        std::ostringstream msg;
        std::ifstream strm(file.c_str());
        // check if file can be opened
        if (!strm.good()) {
            WRITE_ERROR("Could not open '" + file + "'.");
            return 0;
        }
        while (strm.good()) {
            std::string line;
            strm >> line;
            // check if line isn't empty
            if (line.length() != 0) {
                // obtain GLObject
                GUIGlObject* object = GUIGlObjectStorage::gIDStorage.getObjectBlocking(line);
                // check if GUIGlObject doesn't exist
                if(object != nullptr) {
                    // obtain GNEAttributeCarrier
                    GNEAttributeCarrier *AC = mySelectorFrameParent->getViewNet()->getNet()->retrieveAttributeCarrier(object->getGlID(), false);
                    // check if AC exist and if is selectable
                    if((AC != nullptr) && GNEAttributeCarrier::canBeSelected(AC->getTag())) {
                        loadedACs.push_back(AC);
                    }
                }
            }
        }
        // change selected attribute in loaded ACs
        if (loadedACs.size() > 0) {
            mySelectorFrameParent->getViewNet()->getUndoList()->p_begin("load selection");
            for (auto i : loadedACs) {
                i->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
            }
            mySelectorFrameParent->getViewNet()->getUndoList()->p_end();
        }
    }
    mySelectorFrameParent->getViewNet()->update();
    return 1;
}


long
GNESelectorFrame::SelectionOperation::onCmdSave(FXObject*, FXSelector, void*) {
    FXString file = MFXUtils::getFilename2Write(
                        this, "Save List of selected Items", ".txt", GUIIconSubSys::getIcon(ICON_EMPTY), gCurrentFolder);
    if (file == "") {
        return 1;
    }
    try {
        OutputDevice& dev = OutputDevice::getDevice(file.text());
        for (auto i : mySelectorFrameParent->myViewNet->getNet()->getSelectedAttributeCarriers()) {
            GUIGlObject* object = dynamic_cast<GUIGlObject*>(i);
            if(object) {
                dev << GUIGlObject::TypeNames.getString(object->getType()) << ":" << i->getID() << "\n";
            }
        }
        dev.close();
    } catch (IOError& e) {
        // write warning if netedit is running in testing mode
        if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
            WRITE_WARNING("Opening FXMessageBox 'error storing selection'");
        }
        // open message box error
        FXMessageBox::error(this, MBOX_OK, "Storing Selection failed", "%s", e.what());
        // write warning if netedit is running in testing mode
        if (OptionsCont::getOptions().getBool("gui-testing-debug")) {
            WRITE_WARNING("Closed FXMessageBox 'error storing selection' with 'OK'");
        }
    }
    return 1;
}


long
GNESelectorFrame::SelectionOperation::onCmdClear(FXObject*, FXSelector, void*) {
    // for clear selection, simply change all GNE_ATTR_SELECTED attribute of current selected elements
    mySelectorFrameParent->getViewNet()->getUndoList()->p_begin("clear selection");
    std::vector<GNEAttributeCarrier*> selectedAC = mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers();
    // change attribute GNE_ATTR_SELECTED of all selected items to false
    for (auto i : selectedAC) {
        i->setAttribute(GNE_ATTR_SELECTED, "false", mySelectorFrameParent->getViewNet()->getUndoList());
    }
    // finish clear selection
    mySelectorFrameParent->getViewNet()->getUndoList()->p_end();
    // update view
    mySelectorFrameParent->getViewNet()->update();
    return 1;
}


long
GNESelectorFrame::SelectionOperation::onCmdInvert(FXObject*, FXSelector, void*) {
    // first make a copy of current selected elements
    std::vector<GNEAttributeCarrier*> copyOfSelectedAC = mySelectorFrameParent->getViewNet()->getNet()->getSelectedAttributeCarriers();
    // for invert selection, first clean current selection and next select elements of set "unselectedElements"
    mySelectorFrameParent->getViewNet()->getUndoList()->p_begin("invert selection");
    // select junctions, edges, lanes connections and crossings
    std::vector<GNEJunction*> junctions = mySelectorFrameParent->getViewNet()->getNet()->retrieveJunctions();
    for (auto i : junctions) {
        i->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
        // due we iterate over all junctions, only it's neccesary iterate over incoming edges
        for (auto j : i->getGNEIncomingEdges()) {
            // only select edges if "select edges" flag is enabled. In other case, select only lanes
            if(mySelectorFrameParent->getViewNet()->selectEdges()) {
                j->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
            } else {
                for (auto k : j->getLanes()) {
                    k->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
                }
            }
            // select connections
            for (auto k : j->getGNEConnections()) {
                k->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
            }
        }
        // select crossings
        for (auto j : i->getGNECrossings()) {
            j->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
        }
    }
    // select additionals
    std::vector<GNEAdditional*> additionals = mySelectorFrameParent->getViewNet()->getNet()->getAdditionals();
    for (auto i : additionals) {
        i->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
    }
    // select polygons
    for (auto i : mySelectorFrameParent->getViewNet()->getNet()->getPolygons()) {
        dynamic_cast<GNEPoly*>(i.second)->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
    }
    // select POIs
    for (auto i : mySelectorFrameParent->getViewNet()->getNet()->getPOIs()) {
        dynamic_cast<GNEPOI*>(i.second)->setAttribute(GNE_ATTR_SELECTED, "true", mySelectorFrameParent->getViewNet()->getUndoList());
    }
    // now iterate over all elements of "copyOfSelectedAC" and undselect it 
    for (auto i : copyOfSelectedAC) {
        i->setAttribute(GNE_ATTR_SELECTED, "false", mySelectorFrameParent->getViewNet()->getUndoList());
    }
    // finish selection operation
    mySelectorFrameParent->getViewNet()->getUndoList()->p_end();
    // update view
    mySelectorFrameParent->getViewNet()->update();
    return 1;
}


/****************************************************************************/
