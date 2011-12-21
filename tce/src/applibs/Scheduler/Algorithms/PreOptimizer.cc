/*
    Copyright (c) 2002-2009 Tampere University of Technology.

    This file is part of TTA-Based Codesign Environment (TCE).

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */
/**
 * @file PreOptimizer.cc
 *
 * Implementation of GuardInverver class.
 *
 * This optimizer removes unneeded predicate arithmetic by using
 * opposite guard instead where the guard is used.
 *
 * @author Heikki Kultala 2009 (hkultala-no.spam-cs.tut.fi)
 * @note rating: red
 */

#include "PreOptimizer.hh"
#include "SchedulerCmdLineOptions.hh"
#include "Procedure.hh"
#include "Instruction.hh"
#include "Move.hh"
#include "ControlFlowGraph.hh"
#include "DataDependenceGraphBuilder.hh"
#include "ProgramOperation.hh"
#include "Terminal.hh"
#include "CodeGenerator.hh"
#include "InstructionReferenceManager.hh"
#include "RegisterFile.hh"
#include "DisassemblyRegister.hh"
#include "Program.hh"
#include "Operation.hh"

static const int DEFAULT_LOWMEM_MODE_THRESHOLD = 200000;

/**
 * Constructor. Initializes interpassData.
 *
 * @param data InterPassData to store.
 */
PreOptimizer::PreOptimizer(InterPassData& data) : ProcedurePass(data),
                                                  ProgramPass(data),
                                                  ControlFlowGraphPass(data) {
}

/**
 * Handles a program.
 * 
 * @param program the program.
 * @param targetMachine targetmachine. Not used at all.
 */
void 
PreOptimizer::handleProgram(
    TTAProgram::Program& program,
    const TTAMachine::Machine& targetMachine)
    throw (Exception) {
    ProgramPass::executeProcedurePass(program, targetMachine, *this);
}

bool 
PreOptimizer::tryToOptimizeAddressReg(
    DataDependenceGraph& ddg, ProgramOperation& po) {
 
    DataDependenceEdge* connectingEdge = NULL;

    if (po.outputMoveCount() != 1 || po.inputMoveCount() != 1) {
        return false;
    }
    MoveNode& address = po.inputMove(0);
    MoveNode& result = po.outputMove(0);

    DataDependenceGraph::EdgeSet iEdges = ddg.inEdges(address);

    if (!result.move().destination().isGPR()) {
        return false;
    }
    MoveNode* src = NULL;
    // loop thru all inedges find who writes the address.
    for (DataDependenceGraph::EdgeSet::iterator i = iEdges.begin();
         i != iEdges.end(); i++) {
        DataDependenceEdge& edge = **i;
        if (edge.dependenceType() == DataDependenceEdge::DEP_RAW &&
            !edge.guardUse()) {
            if (edge.edgeReason() != DataDependenceEdge::EDGE_REGISTER) {
                return false;
            }
/*
            if (edge.isBackEdge()) {
                return false;
            }
*/
            if (src == NULL) {
                src = &ddg.tailNode(edge);
                connectingEdge = &edge;
            } else {
                return false;
            }
        }
    }
    if (src == NULL) {
        // load address missing
        return false;
    }

    if (&ddg.getBasicBlockNode(address) != &ddg.getBasicBlockNode(*src)) {
        return false;
    }

    // TODO: multi-threading might make this fail too often!
    if (src->nodeID() != address.nodeID()-1) {
        // some other moves between these moves.
        return false;
    }

    DataDependenceGraph::EdgeSet oEdges = ddg.outEdges(*src);
    // loop thru all outedges.
    for (DataDependenceGraph::EdgeSet::iterator i = oEdges.begin();
         i != oEdges.end(); i++) {
        DataDependenceEdge& edge = **i;
        if (edge.dependenceType() == DataDependenceEdge::DEP_RAW) {
            if (&ddg.headNode(edge) != &address) {
                // same address used for some other operation.
                return false;
            }
        }
    }

    // cannot use narrower reg for address.
    if (address.move().source().registerFile().width() >
        result.move().destination().registerFile().width()) {
        return false;
    }

    TCEString oldReg = 
        DisassemblyRegister::registerName(address.move().source());
    TCEString newReg = 
        DisassemblyRegister::registerName(result.move().destination());

    address.move().setSource(result.move().destination().copy());
    src->move().setDestination(result.move().destination().copy());

    ddg.renamedSimpleLiveRange(
        *src, address, result, *connectingEdge, oldReg, newReg);
    return true;
}


TTAProgram::CodeSnippet*
PreOptimizer::tryToRemoveXor(
    DataDependenceGraph& ddg, ProgramOperation& po,
    TTAProgram::InstructionReferenceManager* irm) {
    if (po.outputMoveCount() != 1 || po.inputMoveCount() != 2) {
        return false;
    }
    // check that it's or by 1.
    MoveNode& operand1 = po.inputMove(0);
    MoveNode& operand2 = po.inputMove(1);
    TTAProgram::Terminal& src2 = operand2.move().source();
    if (!src2.isImmediate() || src2.value().intValue() != 1) {
        return NULL;
    }
    // now we have a xor op which is a truth value reversal.
    // find where the result is used.
    
    MoveNode& result = po.outputMove(0);
    TTAProgram::Move& resultMove = result.move();
    DataDependenceGraph::EdgeSet oEdges = ddg.outEdges(result);
    bool ok = true;
    
    // loop thru all outedges.
    for (DataDependenceGraph::EdgeSet::iterator i = oEdges.begin();
         i != oEdges.end(); i++) {
        DataDependenceEdge& edge = **i;
        if (!edge.guardUse()) {
            ok = false;
            break;
        }
        
        // them checks that those guard usages cannot have some other
        // movenode writing the guard, if some complex guard 
        // calculation where guard write itself is conditional
        // or multiple guard sources in different BBs.
        int gRawCount = 0;
        MoveNode& head = ddg.headNode(edge);
        DataDependenceGraph::EdgeSet iEdges = ddg.inEdges(head);
        for (DataDependenceGraph::EdgeSet::iterator j = iEdges.begin();
             j != iEdges.end(); j++) {
            if ((**j).guardUse() && 
                (**j).dependenceType() == DataDependenceEdge::DEP_RAW) {
                gRawCount++;
                if (gRawCount > 1) {
                    ok = false;
                    break;
                }
            }
        }
    }
    // some more complex things done with the guard.
    // converting those not yet supported.
    if (!ok) {
        return NULL;
    }
    
    TTAProgram::Instruction& operand1Ins = operand1.move().parent();
    TTAProgram::Instruction& operand2Ins = operand2.move().parent();
    TTAProgram::Instruction& resultIns = resultMove.parent();
    
    // cannot remove if has refs. TODO: move refs to next ins.
    // if we don't have irm, assume we don't have irefs.
    if (irm != NULL && 
        (irm->hasReference(operand1Ins) || irm->hasReference(operand2Ins) ||
         irm->hasReference(resultIns))) {
        return NULL;
    }
    
    bool reversesJump = false;
    for (DataDependenceGraph::EdgeSet::iterator i = oEdges.begin();
         i != oEdges.end(); i++) {
        DataDependenceEdge& edge = **i;
        MoveNode& head = ddg.headNode(edge);
        TTAProgram::Move& guardUseMove = head.move();
        assert(!guardUseMove.isUnconditional());
        guardUseMove.setGuard(
            TTAProgram::CodeGenerator::createInverseGuard(
                guardUseMove.guard()));
        if (guardUseMove.isJump()) {
            reversesJump = true;
        }
    }
    
    // need some copy from one predicate to another?
    TTAProgram::Terminal& src1 = operand1.move().source();
    TTAProgram::Terminal& dst = resultMove.destination();
    if (!src1.equals(dst)) {
        TTAProgram::Move* newMove = new TTAProgram::Move(
            src1.copy(), dst.copy(), operand1.move().bus());
        
        TTAProgram::Instruction* newIns = new TTAProgram::Instruction;
        newIns->addMove(newMove);
        resultMove.parent().parent().insertAfter(
            resultMove.parent(), newIns);
        MoveNode* newMN = new MoveNode(*newMove);
        ddg.addNode(*newMN, operand1);
        ddg.combineNodes(operand1, result, *newMN);

    } else {
        // the op just gets deleted.
        ddg.copyDepsOver(operand1, result, true, true);
    }
    
    // delete the xor operation. (the moves and instructions.)
    TTAProgram::CodeSnippet& parent = operand1Ins.parent();
    
    assert(operand1Ins.moveCount() == 1);
    ddg.deleteNode(operand1);
    parent.remove(operand1Ins);
    delete &operand1Ins;
    
    assert(operand2Ins.moveCount() == 1);
    ddg.deleteNode(operand2);
    parent.remove(operand2Ins);
    delete &operand2Ins;
    
    
    assert(resultIns.moveCount() == 1);
    ddg.deleteNode(result);
    parent.remove(resultIns);
    delete &resultIns;

    if (reversesJump) {
        return &parent;
    } else {
        return NULL;
    }
}

/**
 * Handles a procedure.
 * 
 * @param procedure the procedure.
 * @param targetMachine targetmachine. Not used at all.
 */
void
PreOptimizer::handleProcedure(
    TTAProgram::Procedure& procedure,
    const TTAMachine::Machine& mach)
    throw (Exception) {

    ControlFlowGraph cfg(procedure /*, ProcedurePass::interPassData()*/);
    cfg.updateReferencesFromProcToCfg();

    handleControlFlowGraph(cfg, mach);

    // copy back to the program.
    cfg.copyToProcedure(procedure);
}

void
PreOptimizer::handleCFGDDG(
    ControlFlowGraph& cfg,
    DataDependenceGraph& ddg) {

    TTAProgram::Program* program = cfg.program();
    TTAProgram::InstructionReferenceManager* irm = 
        program == NULL ? NULL :
        &program->instructionReferenceManager();

    // Loop over all programoperations. find XOR's by 1.
    for (int i = 0; i < ddg.programOperationCount(); i++) {
        ProgramOperation& po = ddg.programOperation(i);
        if (po.operation().name() == "XOR") {
            TTAProgram::CodeSnippet* parent = tryToRemoveXor(ddg,po,irm);
            if (parent) {
                bool found = false;
                for (int j = 0; j < cfg.nodeCount(); j++) {
                    BasicBlockNode& bbn = cfg.node(j);
                    if (&bbn.basicBlock() == parent) {
                        cfg.reverseGuardOnOutEdges(bbn);
                        found = true;
                        break;
                    }
                }
                if (found == true) {
                    continue;
                }
                assert(false && "invalid parent on removed xor inst.");
            }
        }
        if (po.operation().readsMemory()) {
            tryToOptimizeAddressReg(ddg, po);
        }
        // TODO: remove also programoperation.
    }
}


void
PreOptimizer::handleControlFlowGraph(
    ControlFlowGraph& cfg,
    const TTAMachine::Machine&)
    throw (Exception) {

    DataDependenceGraphBuilder ddgBuilder(ProcedurePass::interPassData());
    // only RAW register edges and operation edges. no mem edges, 
    // no anti-edges.
    DataDependenceGraph* ddg = ddgBuilder.build(
        cfg, DataDependenceGraph::NO_ANTIDEPS, NULL, false);
    
    handleCFGDDG(cfg, *ddg);

    delete ddg;
}

