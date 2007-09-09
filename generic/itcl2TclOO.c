/*
 * itcl2TclOO.c --
 *
 *	This file contains code to create and manage methods.
 *
 * Copyright (c) 2007 by Arnulf P. Wiedemann
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id: itcl2TclOO.c,v 1.1.2.4 2007/09/09 20:53:47 wiede Exp $
 */

#include <tclOOInt.h>
#define _TCLOOINT_H
#include "itclInt.h"

int
Itcl_InvokeProcedureMethod(
    ClientData clientData,	/* Pointer to some per-method context. */
    Tcl_Interp *interp,
    TclOO_PreCallProc preCallProc,
    int objc,			/* Number of arguments. */
    Tcl_Obj *const *objv)	/* Arguments as actually seen. */
{
    Method *mPtr = clientData;
    Tcl_Namespace *nsPtr = mPtr->declaringClassPtr->thisPtr->namespacePtr;
    ProcedureMethod *pmPtr = mPtr->clientData;
    Proc *procPtr = pmPtr->procPtr;
    int flags = FRAME_IS_METHOD;
    CallFrame frame;
    CallFrame *framePtr = &frame;
    CallFrame **framePtrPtr1 = &framePtr;
    Tcl_CallFrame **framePtrPtr = (Tcl_CallFrame **)framePtrPtr1;
    Command cmd;
    int result;

    memset(&cmd, 0, sizeof(Command));
    cmd.nsPtr = (Namespace *) nsPtr;
    cmd.clientData = NULL;
    pmPtr->procPtr->cmdPtr = &cmd;

    result = TclProcCompileProc(interp, pmPtr->procPtr,
	    pmPtr->procPtr->bodyPtr, (Namespace *) nsPtr, "body of method",
	    Tcl_GetString(mPtr->namePtr));
    if (result != TCL_OK) {
	return result;
    }
    /*
     * Make the stack frame and fill it out with information about this call.
     * This operation may fail.
     */


    flags |= FRAME_IS_PROC;
    result = TclPushStackFrame(interp, framePtrPtr, nsPtr, flags);
    if (result != TCL_OK) {
	return result;
    }

    framePtr->clientData = NULL;
    framePtr->objc = objc;
    framePtr->objv = objv;
    framePtr->procPtr = procPtr;

    /*
     * Give the pre-call callback a chance to do some setup and, possibly,
     * veto the call.
     */

    if (preCallProc != NULL) {
	int isFinished;

	result = preCallProc(pmPtr->clientData, interp, NULL,
		(Tcl_CallFrame *) framePtr, &isFinished);
	if (isFinished || result != TCL_OK) {
	    Tcl_PopCallFrame(interp);
	    goto done;
	}
    }

    /*
     * Now invoke the body of the method. Note that we need to take special
     * action when doing unknown processing to ensure that the missing method
     * name is passed as an argument.
     */

    result = TclObjInterpProcCore(interp, mPtr->namePtr, 1, ItclProcErrorProc);

done:
    return result;
}


/*
 * ----------------------------------------------------------------------
 *
 * Itcl_PublicObjectCmd, Itcl_PrivateObjectCmd --
 *
 *	Main entry point for object invokations. The Public* and Private*
 *	wrapper functions are just thin wrappers round the main ObjectCmd
 *	function that does call chain creation, management and invokation.
 *
 * ----------------------------------------------------------------------
 */

int
Itcl_PublicObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_Class clsPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *)clientData;
    int result;

    result = TclOOObjectCmdCore(oPtr, interp, objc, objv, PUBLIC_METHOD,
	    &oPtr->publicContextCache, (Class *)clsPtr);
    return result;
}

int
Itcl_PrivateObjectCmd(
    ClientData clientData,
    Tcl_Interp *interp,
    Tcl_Class clsPtr,
    int objc,
    Tcl_Obj *const *objv)
{
    Object *oPtr = (Object *)clientData;
    int result;

    result = TclOOObjectCmdCore(oPtr, interp, objc, objv, PRIVATE_METHOD,
	    &oPtr->publicContextCache, (Class *)clsPtr);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * Itcl_NewProcClassMethod --
 *
 *	Create a new procedure-like method for a class for Itcl.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Itcl_NewProcClassMethod(
    Tcl_Interp *interp,		/* The interpreter containing the class. */
    Tcl_Class clsPtr,		/* The class to modify. */
    TclOO_PreCallProc preCallPtr,
    TclOO_PostCallProc postCallPtr,
    Tcl_ProcErrorProc errProc,
    ClientData clientData,
    Tcl_Obj *nameObj,		/* The name of the method, which may be NULL;
				 * if so, up to caller to manage storage
				 * (e.g., because it is a constructor or
				 * destructor). */
    Tcl_Obj *argsObj,		/* The formal argument list for the method,
				 * which may be NULL; if so, it is equivalent
				 * to an empty list. */
    Tcl_Obj *bodyObj,		/* The body of the method, which must not be
				 * NULL. */
    ClientData *clientData2)
{
    Tcl_Method result;

    result = Tcl_NewProcClassMethod(interp, clsPtr, preCallPtr, postCallPtr,
           errProc, clientData, nameObj, argsObj, bodyObj,
           PUBLIC_METHOD | USE_DECLARER_NS, clientData2);
    return result;
}

/*
 * ----------------------------------------------------------------------
 *
 * Itcl_NewProcMethod --
 *
 *	Create a new procedure-like method for an object for Itcl.
 *
 * ----------------------------------------------------------------------
 */

Tcl_Method
Itcl_NewProcMethod(
    Tcl_Interp *interp,		/* The interpreter containing the object. */
    Tcl_Object oPtr,		/* The object to modify. */
    Tcl_Obj *nameObj,		/* The name of the method, which must not be
				 * NULL. */
    Tcl_Obj *argsObj,		/* The formal argument list for the method,
				 * which must not be NULL. */
    Tcl_Obj *bodyObj,		/* The body of the method, which must not be
				 * NULL. */
    ClientData *clientData)
{
    return Tcl_NewProcMethod(interp, oPtr, nameObj, argsObj, bodyObj,
           PUBLIC_METHOD | USE_DECLARER_NS, clientData);
}