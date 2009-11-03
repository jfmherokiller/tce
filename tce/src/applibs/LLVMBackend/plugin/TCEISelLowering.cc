
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
 * @file TCETargetLowering.cpp
 *
 * Implementation of TCETargetLowering class.
 *
 * @author Veli-Pekka J��skel�inen 2007 (vjaaskel-no.spam-cs.tut.fi)
 * @author Mikael Lepist� 2009 (mikael.lepisto-no.spam-tut.fi)
 */

#include <assert.h>
#include <string>
#include <llvm/Function.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Target/TargetLowering.h>
#include <llvm/Intrinsics.h>
#include <llvm/CallingConv.h>
#include <llvm/CodeGen/CallingConvLower.h>
#include <llvm/CodeGen/SelectionDAG.h>
#include <llvm/CodeGen/MachineFrameInfo.h>
#include <llvm/CodeGen/MachineRegisterInfo.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/VectorExtras.h>

#include <llvm/Target/TargetLoweringObjectFile.h>

#include "TCEPlugin.hh"
#include "TCERegisterInfo.hh"
#include "TCETargetMachine.hh"
#include "TCETargetObjectFile.hh"
#include "TCESubtarget.hh"
#include "TCEISelLowering.hh"
#include "tce_config.h"

#include <iostream> // DEBUG

using namespace llvm;

//===----------------------------------------------------------------------===//
// Calling Convention Implementation
//===----------------------------------------------------------------------===//

#include "TCEGenCallingConv.inc"

SDValue
TCETargetLowering::LowerReturn(SDValue Chain,
                               unsigned CallConv, bool isVarArg,
                               const SmallVectorImpl<ISD::OutputArg> &Outs,
                               DebugLoc dl, SelectionDAG &DAG) {

  // CCValAssign - represent the assignment of the return value to locations.
  SmallVector<CCValAssign, 16> RVLocs;

  // CCState - Info about the registers and stack slot.
  CCState CCInfo(CallConv, isVarArg, DAG.getTarget(),
                 RVLocs, *DAG.getContext());

  // Analize return values.
  CCInfo.AnalyzeReturn(Outs, RetCC_TCE);

  // If this is the first return lowered for this function, add the regs to the
  // liveout set for the function.
  if (DAG.getMachineFunction().getRegInfo().liveout_empty()) {
    for (unsigned i = 0; i != RVLocs.size(); ++i)
      if (RVLocs[i].isRegLoc())
        DAG.getMachineFunction().getRegInfo().addLiveOut(RVLocs[i].getLocReg());
  }

  SDValue Flag;

  // Copy the result values into the output registers.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    Chain = DAG.getCopyToReg(Chain, dl, VA.getLocReg(), 
                             Outs[i].Val, Flag);

    // Guarantee that all emitted copies are stuck together with flags.
    Flag = Chain.getValue(1);
  }

  if (Flag.getNode())
    return DAG.getNode(TCEISD::RET_FLAG, dl, MVT::Other, Chain, Flag);
  return DAG.getNode(TCEISD::RET_FLAG, dl, MVT::Other, Chain);
}

/**
 * Lowers formal arguments.
 */
SDValue
TCETargetLowering::LowerFormalArguments(
    SDValue Chain,
    unsigned CallConv, bool isVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins,
    DebugLoc dl, SelectionDAG &DAG,
    SmallVectorImpl<SDValue> &InVals) {

    MachineFunction &MF = DAG.getMachineFunction();
    MachineRegisterInfo &RegInfo = MF.getRegInfo();

    // Assign locations to all of the incoming arguments.
    SmallVector<CCValAssign, 16> ArgLocs;
    CCState CCInfo(CallConv, isVarArg, getTargetMachine(),
                   ArgLocs, *DAG.getContext());

    CCInfo.AnalyzeFormalArguments(Ins, CC_TCE);

/*
  static const unsigned ArgRegs[] = {
      SP::I0, SP::I1, SP::I2, SP::I3, SP::I4, SP::I5
  };
  const unsigned *CurArgReg = ArgRegs, *ArgRegEnd = ArgRegs+6;
  unsigned ArgOffset = 68;
*/
    
    unsigned ArgOffset = 0;
  
    for (unsigned i = 0, e = ArgLocs.size(); i != e; ++i) {
        SDValue ArgValue;
        CCValAssign &VA = ArgLocs[i];
        // FIXME: We ignore the register assignments of AnalyzeFormalArguments
        // because it doesn't know how to split a double into two i32 registers.
        EVT ObjectVT = VA.getValVT();
        switch (ObjectVT.getSimpleVT().SimpleTy) {
        default: llvm_unreachable("Unhandled argument type!");
        case MVT::i1:
        case MVT::i8:
        case MVT::i16:
        case MVT::i32: {
            if (!Ins[i].Used) {
                // if (CurArgReg < ArgRegEnd) ++CurArgReg;
                InVals.push_back(DAG.getUNDEF(ObjectVT));
                
                /* we do not have argument registers currently 
                   } else if (CurArgReg < ArgRegEnd) {  // Lives in an incoming GPR
                   unsigned VReg = RegInfo.createVirtualRegister(&SP::IntRegsRegClass);
                   MF.getRegInfo().addLiveIn(*CurArgReg++, VReg);
                   SDValue Arg = DAG.getCopyFromReg(Chain, dl, VReg, MVT::i32);
                   if (ObjectVT != MVT::i32) {
                   unsigned AssertOp = ISD::AssertSext;
                   Arg = DAG.getNode(AssertOp, dl, MVT::i32, Arg,
                   DAG.getValueType(ObjectVT));
                   Arg = DAG.getNode(ISD::TRUNCATE, dl, ObjectVT, Arg);
                   }
                   InVals.push_back(Arg);
                */
            } else {
            int FrameIdx = MF.getFrameInfo()->CreateFixedObject(4, ArgOffset);
            SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
            SDValue Load;
            if (ObjectVT == MVT::i32) {
                Load = DAG.getLoad(MVT::i32, dl, Chain, FIPtr, NULL, 0);
            } else {
                ISD::LoadExtType LoadOp = ISD::SEXTLOAD;
                
                // Sparc is big endian, so add an offset based on the ObjectVT.
                unsigned Offset = 4-std::max(1U, ObjectVT.getSizeInBits()/8);
                FIPtr = DAG.getNode(ISD::ADD, dl, MVT::i32, FIPtr,
                                    DAG.getConstant(Offset, MVT::i32));
                Load = DAG.getExtLoad(LoadOp, dl, MVT::i32, Chain, FIPtr,
                                      NULL, 0, ObjectVT);
                Load = DAG.getNode(ISD::TRUNCATE, dl, ObjectVT, Load);
            }
            InVals.push_back(Load);
            }
            
            ArgOffset += 4;
            break;
        }
            
        case MVT::f32: {
            if (!Ins[i].Used) {                  // Argument is dead.
                //if (CurArgReg < ArgRegEnd) ++CurArgReg;
                InVals.push_back(DAG.getUNDEF(ObjectVT));
                
                /*
                  } else if (CurArgReg < ArgRegEnd) {  // Lives in an incoming GPR
                  // FP value is passed in an integer register.
                  unsigned VReg = RegInfo.createVirtualRegister(&SP::IntRegsRegClass);
                  MF.getRegInfo().addLiveIn(*CurArgReg++, VReg);
                  SDValue Arg = DAG.getCopyFromReg(Chain, dl, VReg, MVT::i32);
                  
                  Arg = DAG.getNode(ISD::BIT_CONVERT, dl, MVT::f32, Arg);
                  InVals.push_back(Arg);
                */
            } else {
                int FrameIdx = MF.getFrameInfo()->CreateFixedObject(4, ArgOffset);
                SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
                SDValue Load = DAG.getLoad(MVT::f32, dl, Chain, FIPtr, NULL, 0);
                InVals.push_back(Load);
            }
            ArgOffset += 4;
            break;
        }
            
        case MVT::i64:
        case MVT::f64: {
            
            if (!Ins[i].Used) {                // Argument is dead.
                //if (CurArgReg < ArgRegEnd) ++CurArgReg;
                //if (CurArgReg < ArgRegEnd) ++CurArgReg;
                InVals.push_back(DAG.getUNDEF(ObjectVT));
            } else {
                SDValue HiVal;
                //if (CurArgReg < ArgRegEnd) {  // Lives in an incoming GPR
                //    unsigned VRegHi = RegInfo.createVirtualRegister(&SP::IntRegsRegClass);
                //    MF.getRegInfo().addLiveIn(*CurArgReg++, VRegHi);
                //    HiVal = DAG.getCopyFromReg(Chain, dl, VRegHi, MVT::i32);
                //} else {
                int FrameIdx = MF.getFrameInfo()->CreateFixedObject(4, ArgOffset);
                SDValue FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
                HiVal = DAG.getLoad(MVT::i32, dl, Chain, FIPtr, NULL, 0);
                //}
                
                SDValue LoVal;
                //if (CurArgReg < ArgRegEnd) {  // Lives in an incoming GPR
                //    unsigned VRegLo = RegInfo.createVirtualRegister(&SP::IntRegsRegClass);
                //    MF.getRegInfo().addLiveIn(*CurArgReg++, VRegLo);
                //    LoVal = DAG.getCopyFromReg(Chain, dl, VRegLo, MVT::i32);
                //} else {
                FrameIdx = MF.getFrameInfo()->CreateFixedObject(4, ArgOffset+4);
                FIPtr = DAG.getFrameIndex(FrameIdx, MVT::i32);
                LoVal = DAG.getLoad(MVT::i32, dl, Chain, FIPtr, NULL, 0);
                //}
                
                // Compose the two halves together into an i64 unit.
                SDValue WholeValue =
                    DAG.getNode(ISD::BUILD_PAIR, dl, MVT::i64, LoVal, HiVal);
                
                // If we want a double, do a bit convert.
                if (ObjectVT == MVT::f64)
                    WholeValue = DAG.getNode(ISD::BIT_CONVERT, dl, MVT::f64, WholeValue);
                
                InVals.push_back(WholeValue);
            }
            ArgOffset += 8;
            break;
        }
        }
        
    }
    
    // inspired from ARM
    if (isVarArg) {        
        // This will point to the next argument passed via stack.
        VarArgsFrameOffset = MF.getFrameInfo()->CreateFixedObject(4, ArgOffset);
    }
    
    return Chain;
}

SDValue
TCETargetLowering::LowerCall(SDValue Chain, SDValue Callee,
                             unsigned CallConv, bool isVarArg,
                             bool isTailCall,
                             const SmallVectorImpl<ISD::OutputArg> &Outs,
                             const SmallVectorImpl<ISD::InputArg> &Ins,
                             DebugLoc dl, SelectionDAG &DAG,
                             SmallVectorImpl<SDValue> &InVals) {
    
    (void)CC_TCE;

    // Count the size of the outgoing arguments.
    unsigned ArgsSize = 0;
    for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
        switch (Outs[i].Val.getValueType().getSimpleVT().SimpleTy) {
        default: llvm_unreachable("Unknown value type!");
        case MVT::i1:
        case MVT::i8:
        case MVT::i16:
        case MVT::i32:
        case MVT::f32:
            ArgsSize += 4;
            break;
        case MVT::i64:
        case MVT::f64:
            ArgsSize += 8;
            break;
        }
    }

    // Keep stack frames 4-byte aligned.
    ArgsSize = (ArgsSize+3) & ~3;

    Chain = DAG.getCALLSEQ_START(Chain, DAG.getIntPtrConstant(ArgsSize, true));
  
    SmallVector<SDValue, 8> MemOpChains;
   
  unsigned ArgOffset = 0;

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    SDValue Val = Outs[i].Val;
    EVT ObjectVT = Val.getValueType();
    SDValue ValToStore(0, 0);
    unsigned ObjSize;
    switch (ObjectVT.getSimpleVT().SimpleTy) {
    default: llvm_unreachable("Unhandled argument type!");
        
    case MVT::i1:
    case MVT::i8:
    case MVT::i16: {
        // TODO: is actually needed (sparc did not have this)?
        // Promote the integer to 32-bits.
//        ISD::NodeType ext = ISD::ANY_EXTEND;
//        if (Ins[i].isSExt) {
//            ext = ISD::SIGN_EXTEND;
//        } else if (Ins[i].isZExt) {
//            ext = ISD::ZERO_EXTEND;
//        }
//        Val = DAG.getNode(ext, dl, MVT::i32, Val);
        // FALL THROUGH
    }
    case MVT::f32:
    case MVT::i32:
        ObjSize = 4;
        ValToStore = Val;
        break;
    case MVT::f64: 
    case MVT::i64: 
        ObjSize = 8;
        ValToStore = Val;    // Whole thing is passed in memory.
        break;
    }
  
    if (ValToStore.getNode()) {
      SDValue StackPtr = DAG.getRegister(TCE::SP, MVT::i32);
      SDValue PtrOff = DAG.getConstant(ArgOffset, MVT::i32);
      PtrOff = DAG.getNode(ISD::ADD, dl, MVT::i32, StackPtr, PtrOff);
      MemOpChains.push_back(DAG.getStore(Chain, dl, ValToStore, 
                                         PtrOff, NULL, 0));
    }
    ArgOffset += ObjSize;
  }

  // Emit all stores, make sure the occur before any copies into physregs.
  if (!MemOpChains.empty()) {
    Chain = DAG.getNode(ISD::TokenFactor, dl, MVT::Other,
                        &MemOpChains[0], MemOpChains.size());
  }

  // Build a sequence of copy-to-reg nodes chained together with token
  // chain and flag operands which copy the outgoing args into registers.
  // The InFlag in necessary since all emited instructions must be
  // stuck together.
  SDValue InFlag;// = Chain.getValue(0);

  // If the callee is a GlobalAddress node (quite common, every direct call is)
  // turn it into a TargetGlobalAddress node so that legalize doesn't hack it.
  // Likewise ExternalSymbol -> TargetExternalSymbol.
  if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(Callee))
    Callee = DAG.getTargetGlobalAddress(G->getGlobal(), MVT::i32);
  else if (ExternalSymbolSDNode *E = dyn_cast<ExternalSymbolSDNode>(Callee))
    Callee = DAG.getTargetExternalSymbol(E->getSymbol(), MVT::i32);

  std::vector<EVT> NodeTys;
  NodeTys.push_back(MVT::Other);   // Returns a chain
  NodeTys.push_back(MVT::Flag);    // Returns a flag for retval copy to use.
  SDValue Ops[] = { Chain, Callee, InFlag };
  Chain = DAG.getNode(TCEISD::CALL, dl, NodeTys, Ops, InFlag.getNode() ? 3 : 2);
  InFlag = Chain.getValue(1);

  Chain = DAG.getCALLSEQ_END(Chain, DAG.getIntPtrConstant(ArgsSize, true),
                             DAG.getIntPtrConstant(0, true), InFlag);
  InFlag = Chain.getValue(1);

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RVInfo(CallConv, isVarArg, DAG.getTarget(),
                 RVLocs, *DAG.getContext());

  RVInfo.AnalyzeCallResult(Ins, RetCC_TCE);

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0; i != RVLocs.size(); ++i) {
    unsigned Reg = RVLocs[i].getLocReg();
   
    /*
    // Remap I0->I7 -> O0->O7.
    if (Reg >= SP::I0 && Reg <= SP::I7) Reg = Reg-SP::I0+SP::O0;
    */

    Chain = DAG.getCopyFromReg(Chain, dl, Reg,
                               RVLocs[i].getValVT(), InFlag).getValue(1);
    InFlag = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}
  /* OLD CODE
    std::vector<llvm::EVT> nodeTys;
    nodeTys.push_back(MVT::Other);
    nodeTys.push_back(MVT::Flag);
    SDValue ops[] = { chain, callee, inFlag };
    chain = dag.getNode(TCEISD::CALL, dl, nodeTys, ops, inFlag.getNode() ? 3 : 2);
    inFlag = chain.getValue(1);

    // Return value
    MVT retTyVT = getValueType(retTy).getSimpleVT();
    SDValue retVal;
    if (retTyVT.SimpleTy != MVT::isVoid) {
        switch (retTyVT.SimpleTy) {
        default: assert(false && "Unsupported return value type.");
        case MVT::i1:
        case MVT::i8:
        case MVT::i16: {
            retVal = dag.getCopyFromReg(chain, dl, TCE::IRES0, MVT::i32, inFlag);
            chain = retVal.getValue(1);
            retVal = dag.getNode(ISD::TRUNCATE, dl, retTyVT, retVal);
            break;
        }
        case MVT::i32: {
            retVal = dag.getCopyFromReg(chain, dl, TCE::IRES0, MVT::i32, inFlag);
            chain = retVal.getValue(1);
            break;
        }
        case MVT::i64: {
            SDValue lo = dag.getCopyFromReg(
                chain, dl, TCE::IRES0, MVT::i32, inFlag);

            SDValue hi = dag.getCopyFromReg(
                lo.getValue(1), dl, TCE::KLUDGE_REGISTER, MVT::i32, 
                lo.getValue(2));

            retVal = dag.getNode(ISD::BUILD_PAIR, dl, MVT::i64, lo, hi);
            chain = hi.getValue(1);
            break;
        }
        case MVT::f32: {
            retVal = dag.getCopyFromReg(chain, dl, TCE::FRES0, MVT::f32, inFlag);
            chain = retVal.getValue(1);
            break;
        }
        case MVT::f64: {
            SDValue lo = dag.getCopyFromReg(
                chain, dl, TCE::IRES0, MVT::i32, inFlag);

            SDValue hi = dag.getCopyFromReg(
                lo.getValue(1), dl, TCE::KLUDGE_REGISTER, MVT::i32, 
                lo.getValue(2));

            retVal = dag.getNode(ISD::BUILD_PAIR, dl, MVT::i64, lo, hi);
            chain = hi.getValue(1);
            break;
        }
        }
    }

    // copied from SPARC backend:
    chain = 
        dag.getCALLSEQ_END(
            chain, dag.getConstant(argsSize, getPointerTy()),
            dag.getConstant(0, MVT::i32), chain.getValue(1));

    return std::make_pair(retVal, chain);
}
  */


/**
 * The Constructor.
 *
 * Initializes the target lowering.
 */
TCETargetLowering::TCETargetLowering(TargetMachine& TM) :
    TargetLowering(TM,  new TCETargetObjectFile()), tm_(dynamic_cast<TCETargetMachine&>(TM)) {

    addRegisterClass(MVT::i1, TCE::I1RegsRegisterClass);
    addRegisterClass(MVT::i32, TCE::I32RegsRegisterClass);
    addRegisterClass(MVT::f32, TCE::F32RegsRegisterClass);

    //setLoadXAction(ISD::EXTLOAD, MVT::f32, Expand);
    //setLoadXAction(ISD::EXTLOAD, MVT::i1 , Promote);
    //setLoadXAction(ISD::ZEXTLOAD, MVT::i1, Expand);

    setOperationAction(ISD::UINT_TO_FP, MVT::i1   , Promote);
    setOperationAction(ISD::UINT_TO_FP, MVT::i8   , Promote);
    setOperationAction(ISD::UINT_TO_FP, MVT::i16  , Promote);

    setOperationAction(ISD::SINT_TO_FP, MVT::i1   , Promote);
    setOperationAction(ISD::SINT_TO_FP, MVT::i8   , Promote);
    setOperationAction(ISD::SINT_TO_FP, MVT::i16  , Promote);

    setOperationAction(ISD::FP_TO_UINT, MVT::i1   , Promote);
    setOperationAction(ISD::FP_TO_UINT, MVT::i8   , Promote);
    setOperationAction(ISD::FP_TO_UINT, MVT::i16  , Promote);

    setOperationAction(ISD::FP_TO_SINT, MVT::i1   , Promote);
    setOperationAction(ISD::FP_TO_SINT, MVT::i8   , Promote);
    setOperationAction(ISD::FP_TO_SINT, MVT::i16  , Promote);

    setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
    setOperationAction(ISD::ConstantPool , MVT::i32, Custom);

    // SELECT is used instead of SELECT_CC
    setOperationAction(ISD::SELECT_CC, MVT::Other, Expand);

    setOperationAction(ISD::SETCC, MVT::i1, Promote);

    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1 , Expand);

    // Expand indirect branches.
    setOperationAction(ISD::BRIND, MVT::Other, Expand);
    // Expand jumptable branches.
    setOperationAction(ISD::BR_JT, MVT::Other, Expand);
    // Expand conditional branches.
    setOperationAction(ISD::BR_CC, MVT::Other, Expand);

    setOperationAction(ISD::MULHU,  MVT::i32, Expand);
    setOperationAction(ISD::MULHS,  MVT::i32, Expand);
    setOperationAction(ISD::SHL_PARTS, MVT::i32, Expand);
    setOperationAction(ISD::SRA_PARTS, MVT::i32, Expand);
    setOperationAction(ISD::SRL_PARTS, MVT::i32, Expand);

    setOperationAction(ISD::DBG_STOPPOINT, MVT::Other, Expand);
    setOperationAction(ISD::DEBUG_LOC, MVT::Other, Expand);
    setOperationAction(ISD::DBG_LABEL, MVT::Other, Expand);

    setOperationAction(ISD::VASTART           , MVT::Other, Custom);

    setOperationAction(ISD::VAARG             , MVT::Other, Expand);
    setOperationAction(ISD::VACOPY            , MVT::Other, Expand);
    setOperationAction(ISD::VAEND             , MVT::Other, Expand);
    setOperationAction(ISD::STACKSAVE         , MVT::Other, Expand);
    setOperationAction(ISD::STACKRESTORE      , MVT::Other, Expand);
    setOperationAction(ISD::DYNAMIC_STACKALLOC, MVT::i32  , Expand);

    setOperationAction(ISD::FCOPYSIGN, MVT::f64, Expand);
    setOperationAction(ISD::FCOPYSIGN, MVT::f32, Expand);

    setOperationAction(ISD::ConstantFP, MVT::f64, Expand);
    setOperationAction(ISD::ConstantFP, MVT::f32, Expand);

    setOperationAction(ISD::SMUL_LOHI, MVT::i32, Expand);
    setOperationAction(ISD::UMUL_LOHI, MVT::i32, Expand);
    setOperationAction(ISD::SMUL_LOHI, MVT::i64, Expand);
    setOperationAction(ISD::UMUL_LOHI, MVT::i64, Expand);

    setOperationAction(ISD::BSWAP, MVT::i32, Expand);

    setOperationAction(ISD::SDIVREM, MVT::i32, Expand);
    setOperationAction(ISD::UDIVREM, MVT::i32, Expand);

    setStackPointerRegisterToSaveRestore(TCE::SP);

    // Set missing operations that can be emulated with emulation function
    // or LLVM built-in emulation pattern to be expanded.
    const std::set<std::pair<unsigned, llvm::MVT::SimpleValueType> >* missingOps = tm_.missingOperations();
    std::set<std::pair<unsigned, llvm::MVT::SimpleValueType> >::const_iterator iter = missingOps->begin();

    std::cerr << "Missing ops: ";

    while (iter != missingOps->end()) {
      unsigned nodetype = (*iter).first;
      llvm::MVT::SimpleValueType valuetype = (*iter).second;
      switch (nodetype) {
      case ISD::SDIV: std::cerr << "SDIV,"; break;
      case ISD::UDIV: std::cerr << "UDIV,"; break;
      case ISD::SREM: std::cerr << "SREM,"; break;
      case ISD::UREM: std::cerr << "UREM,"; break;
      case ISD::ROTL: std::cerr << "ROTL,"; break;
      case ISD::ROTR: std::cerr << "ROTR,"; break;
      case ISD::MUL:  std::cerr << "MUL,"; break;
      case ISD::SIGN_EXTEND_INREG:
        if (valuetype == MVT::i8) std::cerr << "SXQW,";
	if (valuetype == MVT::i16) std::cerr << "SXHW,";
	break;
      default: std::cerr << nodetype << ", "; break;
      };
      setOperationAction(nodetype, valuetype, Expand);
      iter++;
    }
    std::cerr << std::endl;

    computeRegisterProperties();
}

/**
 * Returns target node opcode names for debugging purposes.
 *
 * @param opcode Opcode to convert to string.
 * @return Opcode name.
 */
const char*
TCETargetLowering::getTargetNodeName(unsigned opcode) const {
    switch (opcode) {
    default: return NULL;
    case TCEISD::CALL: return "TCEISD::CALL";
    case TCEISD::RET_FLAG: return "TCEISD::RET_FLAG";
    case TCEISD::GLOBAL_ADDR: return "TCEISD::GLOBAL_ADDR";
    case TCEISD::CONST_POOL: return "TCEISD::CONST_POOL";
    case TCEISD::Hi: return "TCEISD::Hi";
    case TCEISD::Lo: return "TCEISD::Lo";
    case TCEISD::FTOI: return "TCEISD::FTOI";
    case TCEISD::ITOF: return "TCEISD::ITOF";
    case TCEISD::SELECT_I1: return "TCEISD::SELECT_I1";
    case TCEISD::SELECT_I8: return "TCEISD::SELECT_I8";
    case TCEISD::SELECT_I16: return "TCEISD::SELECT_I16";
    case TCEISD::SELECT_I32: return "TCEISD::SELECT_I32";
    case TCEISD::SELECT_I64: return "TCEISD::SELECT_I64";
    case TCEISD::SELECT_F32: return "TCEISD::SELECT_F32";
    case TCEISD::SELECT_F64: return "TCEISD::SELECT_F64";
    }
}

#if 0
// isMaskedValueZeroForTargetNode - Return true if 'Op & Mask' is known to
/// be zero. Op is expected to be a target specific node. Used by DAG
/// combiner.
void TCETargetLowering::computeMaskedBitsForTargetNode(const SDValue Op,
                                                       const APInt &Mask,
                                                       APInt &KnownZero,
                                                       APInt &KnownOne,
                                                       const SelectionDAG &DAG,
                                                       unsigned Depth) const {
  APInt KnownZero2, KnownOne2;
  KnownZero = KnownOne = APInt(Mask.getBitWidth(), 0);   // Don't know anything.

  switch (Op.getOpcode()) {
  default: break;
/*
  case SPISD::SELECT_ICC:
  case SPISD::SELECT_FCC:
    DAG.ComputeMaskedBits(Op.getOperand(1), Mask, KnownZero, KnownOne,
                          Depth+1);
    DAG.ComputeMaskedBits(Op.getOperand(0), Mask, KnownZero2, KnownOne2,
                          Depth+1);
    assert((KnownZero & KnownOne) == 0 && "Bits known to be one AND zero?");
    assert((KnownZero2 & KnownOne2) == 0 && "Bits known to be one AND zero?");

    // Only known if known in both the LHS and RHS.
    KnownOne &= KnownOne2;
    KnownZero &= KnownZero2;
    break;
*/
  }
}
#endif

static SDValue LowerSELECT(
    SDValue op, SelectionDAG& dag) {

    SDValue cond = op.getOperand(0);
    SDValue trueVal = op.getOperand(1);
    SDValue falseVal = op.getOperand(2);

    unsigned opcode = 0;
    switch(trueVal.getValueType().getSimpleVT().SimpleTy) {
    default: assert(0 && "Unknown type to select.");
    case MVT::i1: {
        opcode = TCEISD::SELECT_I1;
        break;
    }
    case MVT::i8: {
        opcode = TCEISD::SELECT_I8;
        break;
    }
    case MVT::i16: {
        opcode = TCEISD::SELECT_I16;
        break;
    }
    case MVT::i32: {
        opcode = TCEISD::SELECT_I32;
        break;
    }
    case MVT::i64: {
        opcode = TCEISD::SELECT_I64;
        break;
    }
    case MVT::f32: {
        opcode = TCEISD::SELECT_F32;
        break;
    }
    case MVT::f64: {
        opcode = TCEISD::SELECT_F64;
        break;
    }
    }

    return dag.getNode(
        opcode, op.getDebugLoc(), trueVal.getValueType(), trueVal, falseVal, cond);
}

static SDValue LowerGLOBALADDRESS(SDValue Op, SelectionDAG &DAG) {

    GlobalValue* gv = cast<GlobalAddressSDNode>(Op)->getGlobal();
    SDValue ga = DAG.getTargetGlobalAddress(gv, MVT::i32);
    return DAG.getNode(TCEISD::GLOBAL_ADDR, Op.getDebugLoc(), MVT::i32, ga);

/* Check if this code should work as well
  GlobalValue *GV = cast<GlobalAddressSDNode>(Op)->getGlobal();
  // FIXME there isn't really any debug info here
  DebugLoc dl = Op.getDebugLoc();
  SDValue GA = DAG.getTargetGlobalAddress(GV, MVT::i32);
  SDValue Hi = DAG.getNode(SPISD::Hi, dl, MVT::i32, GA);
  SDValue Lo = DAG.getNode(SPISD::Lo, dl, MVT::i32, GA);
  return DAG.getNode(ISD::ADD, dl, MVT::i32, Lo, Hi);
*/
}


static SDValue LowerCONSTANTPOOL(SDValue Op, SelectionDAG &DAG) {
    // TODO: Check this.
    llvm::MVT ptrVT = Op.getValueType().getSimpleVT();
    ConstantPoolSDNode* cp = cast<ConstantPoolSDNode>(Op);
    SDValue res;
    if (cp->isMachineConstantPoolEntry()) {
        res = DAG.getTargetConstantPool(
            cp->getMachineCPVal(), ptrVT,
            cp->getAlignment());
    } else {
        res = DAG.getTargetConstantPool(
            cp->getConstVal(), ptrVT,
            cp->getAlignment());
    }
    return DAG.getNode(TCEISD::CONST_POOL, Op.getDebugLoc(), MVT::i32, res);
}

static SDValue LowerVASTART(SDValue Op, SelectionDAG &DAG,
                              TCETargetLowering &TLI) {

    /* ARM ripoff */
    // vastart just stores the address of the VarArgsFrameIndex slot into the
    // memory location argument.
    DebugLoc dl = Op.getDebugLoc();
    EVT PtrVT = DAG.getTargetLoweringInfo().getPointerTy();
    SDValue FR = DAG.getFrameIndex(TLI.getVarArgsFrameOffset(), PtrVT);
    const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
    return DAG.getStore(Op.getOperand(0), dl, FR, Op.getOperand(1), SV, 0);
}


/**
 * Returns the preferred comparison result type.
 */
MVT::SimpleValueType 
TCETargetLowering::getSetCCResultType(llvm::EVT VT) const { 
    return llvm::MVT::i1;
}

/**
 * Handles custom operation lowerings.
 */
SDValue
TCETargetLowering::LowerOperation(SDValue op, SelectionDAG& dag) {

    switch(op.getOpcode()) {
    case ISD::GlobalAddress: return LowerGLOBALADDRESS(op, dag);
    case ISD::SELECT: return LowerSELECT(op, dag);
    case ISD::VASTART: return LowerVASTART(op, dag, *this);
    case ISD::ConstantPool: return LowerCONSTANTPOOL(op, dag);
    
    case ISD::DYNAMIC_STACKALLOC: {
        assert(false && "Dynamic stack allocation not yet implemented.");
    }
    }

    op.getNode()->dump(&dag);
    assert(0 && "Custom lowerings not implemented!");
}


//===----------------------------------------------------------------------===//
//                         Inline Assembly Support
//===----------------------------------------------------------------------===//

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
TCETargetLowering::ConstraintType
TCETargetLowering::getConstraintType(const std::string &Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:  break;
    case 'r': return C_RegisterClass;
    }
  }

  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass*>
TCETargetLowering::getRegForInlineAsmConstraint(const std::string &Constraint,
                                                  EVT VT) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'r':
        return std::make_pair(0U, TCE::I32RegsRegisterClass);
    case 'f':
        if (VT == MVT::f32) {
            return std::make_pair(0U, TCE::F32RegsRegisterClass);
        } else if (VT == MVT::f64) {
            return std::make_pair(0U, TCE::F64RegsRegisterClass);
        }
    }
  }

  return TargetLowering::getRegForInlineAsmConstraint(Constraint, VT);
}


std::vector<unsigned> TCETargetLowering::
getRegClassForInlineAsmConstraint(const std::string &Constraint,
                                  EVT VT) const {
  if (Constraint.size() != 1)
    return std::vector<unsigned>();

  switch (Constraint[0]) {
  default: break;
  case 'r':
      // TODO: WHAT TO DO WITH THESE?!?
    return make_vector<unsigned>(/*SP::L0, SP::L1, SP::L2, SP::L3,
                                 SP::L4, SP::L5, SP::L6, SP::L7,
                                 SP::I0, SP::I1, SP::I2, SP::I3,
                                 SP::I4, SP::I5,
                                 SP::O0, SP::O1, SP::O2, SP::O3,
                                 SP::O4, SP::O5, SP::O7,*/ 0);
  }

  return std::vector<unsigned>();
}

bool
TCETargetLowering::isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const {
  // The TCE target isn't yet aware of offsets.
  return false;
}

/// getFunctionAlignment - Return the Log2 alignment of this function.
unsigned TCETargetLowering::getFunctionAlignment(const Function *) const {
  return 4;
}