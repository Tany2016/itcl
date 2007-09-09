/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  [incr Tcl] provides object-oriented extensions to Tcl, much as
 *  C++ provides object-oriented extensions to C.  It provides a means
 *  of encapsulating related procedures together with their shared data
 *  in a local namespace that is hidden from the outside world.  It
 *  promotes code re-use through inheritance.  More than anything else,
 *  it encourages better organization of Tcl applications through the
 *  object-oriented paradigm, leading to code that is easier to
 *  understand and maintain.
 *
 *  These procedures handle class definitions.  Classes are composed of
 *  data members (public/protected/common) and the member functions
 *  (methods/procs) that operate on them.  Each class has its own
 *  namespace which manages the class scope.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *  overhauled version author: Arnulf Wiedemann
 *
 *     RCS:  $Id: itclClass.c,v 1.1.2.4 2007/09/09 20:53:47 wiede Exp $
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclInt.h"

typedef int (Tcl_ResolveCompiledVarProc) (Tcl_Interp *interp,
        CONST84 char *name, int length, Tcl_Namespace *context,
        struct Tcl_ResolvedVarInfo **rPtr);
typedef int (Tcl_ResolveVarProc) (Tcl_Interp *interp, CONST84 char *name,
        Tcl_Namespace *context, int flags, Tcl_Var *rPtr);
typedef int (Tcl_ResolveCmdProc) (Tcl_Interp *interp, CONST84 char *name,
        Tcl_Namespace *context, int flags, Tcl_Command *rPtr);

void Tcl_SetNamespaceResolvers (Tcl_Namespace * namespacePtr,
    Tcl_ResolveCmdProc * cmdProc, Tcl_ResolveVarProc * varProc,
    Tcl_ResolveCompiledVarProc * compiledVarProc);

/*
 *  FORWARD DECLARATIONS
 */
static void ItclDestroyClass _ANSI_ARGS_((ClientData cdata));
static void ItclDestroyClassNamesp _ANSI_ARGS_((ClientData cdata));
static void ItclFreeClass _ANSI_ARGS_((char* cdata));

// FIX ME !!!
/* DUMMY functions to be filled or deleted !! */

void Itcl_DeleteMemberFunc (char *cdata) {
    fprintf(stderr, "Itcl_DeleteMemberFunc not yet implemented\n");
}
/* END DUMMY functions */

static Tcl_NamespaceDeleteProc* _TclOONamespaceDeleteProc = NULL;


/*
 * ------------------------------------------------------------------------
 *  ClassNamespaceDeleted()
 *
 * ------------------------------------------------------------------------
 */
static char *
ClassNamespaceDeleted(
    ClientData clientData,
    Tcl_Interp *interp,
    CONST84 char *part1,
    CONST84 char *part2,
    int flags)
{
    Tcl_DString buffer;
    Tcl_Namespace *nsPtr;
    ItclClass *iclsPtr;
    
    iclsPtr = clientData;

    if (iclsPtr->namesp == NULL) {
        return NULL;
    }
    iclsPtr->namesp = NULL;
    /* delete the namespace for the common variables */
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
    Tcl_DStringAppend(&buffer, Tcl_GetString(iclsPtr->fullname), -1);
    nsPtr = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer), NULL, 0);
    Tcl_DStringFree(&buffer);
    if (nsPtr != NULL) {
        Tcl_DeleteNamespace(nsPtr);
    }
    ItclDestroyClassNamesp(iclsPtr);
    return NULL;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_CreateClass()
 *
 *  Creates a namespace and its associated class definition data.
 *  If a namespace already exists with that name, then this routine
 *  returns TCL_ERROR, along with an error message in the interp.
 *  If successful, it returns TCL_OK and a pointer to the new class
 *  definition.
 * ------------------------------------------------------------------------
 */
int
Itcl_CreateClass(
    Tcl_Interp* interp,		/* interpreter that will contain new class */
    CONST char* path,		/* name of new class */
    ItclObjectInfo *infoPtr,	/* info for all known objects */
    ItclClass **rPtr)		/* returns: pointer to class definition */
{
    char *head;
    char *tail;
    Tcl_DString buffer;
    Tcl_Command cmd;
    Tcl_CmdInfo cmdInfo;
    Tcl_Namespace *classNs;
    Tcl_Object oPtr;
    Tcl_Obj *nameObjPtr;
    Tcl_Obj *namePtr;
    ItclClass *iclsPtr;
    ItclVariable *ivPtr;
    Tcl_HashEntry *entry;
    int newEntry;

    /*
     * check for an empty class name to avoid a crash
     */
    if (strlen(path) == 0) {
	Tcl_AppendResult(interp, "invalid class name \"\"", NULL);
        return TCL_ERROR;
    }
    /*
     *  Make sure that a class with the given name does not
     *  already exist in the current namespace context.  If a
     *  namespace exists, that's okay.  It may have been created
     *  to contain stubs during a "namespace import" operation.
     *  We'll just replace the namespace data below with the
     *  proper class data.
     */
    classNs = Tcl_FindNamespace(interp, (CONST84 char *)path,
	    (Tcl_Namespace*)NULL, /* flags */ 0);

    if (classNs != NULL && Itcl_IsClassNamespace(classNs)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "class \"", path, "\" already exists",
            (char*)NULL);
        return TCL_ERROR;
    }

    oPtr = NULL;
    /*
     *  Make sure that a command with the given class name does not
     *  already exist in the current namespace.  This prevents the
     *  usual Tcl commands from being clobbered when a programmer
     *  makes a bogus call like "class info".
     */
    cmd = Tcl_FindCommand(interp, (CONST84 char *)path,
	    (Tcl_Namespace*)NULL, /* flags */ TCL_NAMESPACE_ONLY);

    if (cmd != NULL && !Itcl_IsStub(cmd)) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "command \"", path, "\" already exists",
            (char*)NULL);

        if (strstr(path,"::") == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                " in namespace \"",
                Tcl_GetCurrentNamespace(interp)->fullName, "\"",
                (char*)NULL);
        }
        return TCL_ERROR;
    }

    /*
     *  Make sure that the class name does not have any goofy
     *  characters:
     *
     *    .  =>  reserved for member access like:  class.publicVar
     */
    Itcl_ParseNamespPath(path, &buffer, &head, &tail);

    if (strstr(tail,".")) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "bad class name \"", tail, "\"",
            (char*)NULL);
        Tcl_DStringFree(&buffer);
        return TCL_ERROR;
    }
    Tcl_DStringFree(&buffer);

    /*
     *  Allocate class definition data.
     */
    iclsPtr = (ItclClass*)ckalloc(sizeof(ItclClass));
    iclsPtr->name = NULL;
    iclsPtr->fullname = NULL;
    iclsPtr->interp = interp;
    iclsPtr->infoPtr = infoPtr;
    Itcl_PreserveData((ClientData)infoPtr);
    iclsPtr->namesp = NULL;
    iclsPtr->accessCmd = NULL;
    iclsPtr->constructor = NULL;
    iclsPtr->destructor = NULL;
    iclsPtr->constructorInit = NULL;

    Tcl_InitObjHashTable(&iclsPtr->variables);
    Tcl_InitObjHashTable(&iclsPtr->functions);

    iclsPtr->numInstanceVars = 0;
    Tcl_InitHashTable(&iclsPtr->classCommons, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&iclsPtr->resolveVars, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&iclsPtr->resolveCmds, TCL_ONE_WORD_KEYS);
    Tcl_InitHashTable(&iclsPtr->contextCache, TCL_ONE_WORD_KEYS);

    Itcl_InitList(&iclsPtr->bases);
    Itcl_InitList(&iclsPtr->derived);

    ItclResolveInfo *resolveInfoPtr = (ItclResolveInfo *)
            ckalloc(sizeof(ItclResolveInfo));
    memset (resolveInfoPtr, 0, sizeof(ItclResolveInfo));
    resolveInfoPtr->flags = ITCL_RESOLVE_CLASS;
    resolveInfoPtr->iclsPtr = iclsPtr;
    iclsPtr->resolvePtr = (Tcl_Resolve *)ckalloc(sizeof(Tcl_Resolve));
    iclsPtr->resolvePtr->cmdProcPtr = Itcl_CmdAliasProc;
    iclsPtr->resolvePtr->varProcPtr = Itcl_VarAliasProc;
    iclsPtr->resolvePtr->clientData = resolveInfoPtr;

    iclsPtr->initCode = NULL;
    iclsPtr->numCommons   = 0;
    iclsPtr->numVariables   = 0;
    iclsPtr->unique   = 0;
    iclsPtr->flags    = 0;

    /*
     *  Initialize the heritage info--each class starts with its
     *  own class definition in the heritage.  Base classes are
     *  added to the heritage from the "inherit" statement.
     */
    Tcl_InitHashTable(&iclsPtr->heritage, TCL_ONE_WORD_KEYS);
    (void) Tcl_CreateHashEntry(&iclsPtr->heritage, (char*)iclsPtr, &newEntry);

    /*
     *  Create a namespace to represent the class.  Add the class
     *  definition info as client data for the namespace.  If the
     *  namespace already exists, then replace any existing client
     *  data with the class data.
     */
    Itcl_PreserveData((ClientData)iclsPtr);

    nameObjPtr = Tcl_NewStringObj("", 0);
    if ((path[0] != ':') && (path[1] != ':')) {
        Tcl_Namespace *currNsPtr = Tcl_GetCurrentNamespace(interp);
        Tcl_AppendToObj(nameObjPtr, currNsPtr->fullName, -1);
        if (currNsPtr->parentPtr != NULL) {
            Tcl_AppendToObj(nameObjPtr, "::", 2);
        }
    }
    Tcl_AppendToObj(nameObjPtr, path, -1);
    if (classNs != NULL) {
        Tcl_Command oldCmd = Tcl_FindCommand(interp,
                Tcl_GetString(nameObjPtr), NULL, 0);
        if (oldCmd != NULL) {
            Tcl_DeleteCommandFromToken(interp, oldCmd);
        }
    }
    oPtr = Tcl_NewObjectInstance(interp, infoPtr->clazzClassPtr,
            path, path, 0, NULL, 0);
    if (oPtr == NULL) {
        Tcl_AppendResult(interp,
                "ITCL: cannot create Tcl_NewObjectInstance for class \"",
                Tcl_GetString(iclsPtr->fullname), "\"", NULL);
       return TCL_ERROR;
    }
    Tcl_ObjectSetMetadata((Tcl_Object) oPtr, infoPtr->class_meta_type, iclsPtr);
    iclsPtr->classPtr = Tcl_GetObjectAsClass(oPtr);
    iclsPtr->oPtr = oPtr;
    Tcl_ObjectSetMapCmdNameProc(iclsPtr->oPtr, ItclMapCmdNameProc);
    cmd = Tcl_GetObjectCommand(oPtr);
    Tcl_GetCommandInfoFromToken(cmd, &cmdInfo);
    cmdInfo.deleteProc = ItclDestroyClass;
    cmdInfo.deleteData = iclsPtr;
    Tcl_SetCommandInfoFromToken(cmd, &cmdInfo);
    classNs = Tcl_FindNamespace(interp, Tcl_GetString(nameObjPtr),
            (Tcl_Namespace*)NULL, /* flags */ 0);
    if (_TclOONamespaceDeleteProc == NULL) {
        _TclOONamespaceDeleteProc = classNs->deleteProc;
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, classNs->fullName, -1);
    Tcl_DStringAppend(&buffer, "::___DO_NOT_DELETE_THIS_VARIABLE", -1);
    Tcl_SetVar(interp, Tcl_DStringValue(&buffer), "1", 0);
    Tcl_TraceVar(interp, Tcl_DStringValue(&buffer), TCL_TRACE_UNSETS,
            ClassNamespaceDeleted, iclsPtr);

    Itcl_EventuallyFree((ClientData)iclsPtr, ItclFreeClass);

    if (classNs == NULL) {
	Tcl_AppendResult(interp,
	        "ITCL: cannot create/get class namespace for class \"",
		Tcl_GetString(iclsPtr->fullname), "\"", NULL);
        Itcl_ReleaseData((ClientData)iclsPtr);
        return TCL_ERROR;
    }

    if (iclsPtr->infoPtr->useOldResolvers) {
        Tcl_SetNamespaceResolvers(classNs,
                (Tcl_ResolveCmdProc*)Itcl_ClassCmdResolver,
                (Tcl_ResolveVarProc*)Itcl_ClassVarResolver,
                (Tcl_ResolveCompiledVarProc*)Itcl_ClassCompiledVarResolver);
    } else {
        Tcl_SetNamespaceResolver(classNs, iclsPtr->resolvePtr);
    }
    iclsPtr->namesp = classNs;


    iclsPtr->name = Tcl_NewStringObj(classNs->name, -1);
    Tcl_IncrRefCount(iclsPtr->name);

    iclsPtr->fullname = Tcl_NewStringObj(classNs->fullName, -1);
    Tcl_IncrRefCount(iclsPtr->fullname);

    entry = Tcl_CreateHashEntry(&infoPtr->classes, (char *)iclsPtr->fullname,
            &newEntry);
    if (entry == NULL) {
	Tcl_AppendResult(interp,
	        "ITCL: cannot create hash entry in infoPtr->classes for class \"",
		Tcl_GetString(iclsPtr->fullname), "\"", NULL);
	return TCL_ERROR;
    }
    Tcl_SetHashValue(entry, (ClientData)iclsPtr);

    entry = Tcl_CreateHashEntry(&infoPtr->namespaceClasses, (char *)classNs,
            &newEntry);
    if (entry == NULL) {
	Tcl_AppendResult(interp,
	        "ITCL: cannot create hash entry in infoPtr->namespaceClasses",
		" for class \"", 
		Tcl_GetString(iclsPtr->fullname), "\"", NULL);
	return TCL_ERROR;
    }
    Tcl_SetHashValue(entry, (ClientData)iclsPtr);

    /*
     * now build the namespace for the common private and protected variables
     * publci variables go directly to the class namespace
     */
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
    Tcl_DStringAppend(&buffer, Tcl_GetString(iclsPtr->fullname), -1);
    if (Tcl_CreateNamespace(interp, Tcl_DStringValue(&buffer),
            NULL, 0) == NULL) {
	Tcl_AppendResult(interp, "ITCL: cannot create variables namespace \"",
	Tcl_DStringValue(&buffer), "\"", NULL);
        return TCL_ERROR;
    }
    /* FIX ME NEED TO FREE that namespace if the class namespace is deleted !!!*/
    /* FIX ME this is only done for namespace delete */


    Tcl_DStringFree(&buffer);
    /*
     *  Add the built-in "this" variable to the list of data members.
     */
    namePtr = Tcl_NewStringObj("this", -1);
    Tcl_IncrRefCount(namePtr);
    (void) Itcl_CreateVariable(interp, iclsPtr, namePtr, (char*)NULL,
            (char*)NULL, &ivPtr);

    ivPtr->protection = ITCL_PROTECTED;  /* always "protected" */
    ivPtr->flags |= ITCL_THIS_VAR;       /* mark as "this" variable */

    entry = Tcl_CreateHashEntry(&iclsPtr->variables, (char *)namePtr, &newEntry);
    Tcl_SetHashValue(entry, (ClientData)ivPtr);

    /*
     *  Create a command in the current namespace to manage the class:
     *    <className>
     *    <className> <objName> ?<constructor-args>?
     */
    Itcl_PreserveData((ClientData)iclsPtr);

    iclsPtr->accessCmd = Tcl_GetObjectCommand(oPtr);
    /* FIX ME should set the class objects unknown command to Itcl_HandleClass */

    *rPtr = iclsPtr;
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  ItclDeleteClassVariablesNamespace()
 *
 * ------------------------------------------------------------------------
 */
void
ItclDeleteClassVariablesNamespace(
    Tcl_Interp *interp,
    ItclClass *iclsPtr)
{
    Tcl_DString buffer;
    Tcl_Namespace *varNsPtr;

    if (iclsPtr->namesp == NULL) {
        return;
    }
    if (!(iclsPtr->flags & ITCL_CLASS_NO_VARNS_DELETE)) {
        /* free the classes's variables namespace and variables in it */
        Tcl_DStringInit(&buffer);
        Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
        Tcl_DStringAppend(&buffer, iclsPtr->namesp->fullName, -1);
        varNsPtr = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer),
	        NULL, 0);
        if (varNsPtr != NULL) {
            Tcl_DeleteNamespace(varNsPtr);
        }
        Tcl_DStringFree(&buffer);
        iclsPtr->namesp = NULL;
    } else {
        iclsPtr->flags |= ITCL_CLASS_SHOULD_VARNS_DELETE;
    }
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_DeleteClass()
 *
 *  Deletes a class by deleting all derived classes and all objects in
 *  that class, and finally, by destroying the class namespace.  This
 *  procedure provides a friendly way of doing this.  If any errors
 *  are detected along the way, the process is aborted.
 *
 *  Returns TCL_OK if successful, or TCL_ERROR (along with an error
 *  message in the interpreter) if anything goes wrong.
 * ------------------------------------------------------------------------
 */
int
Itcl_DeleteClass(
    Tcl_Interp *interp,     /* interpreter managing this class */
    ItclClass *iclsPtr)    /* class namespace */
{
    ItclClass *iclsPtr2 = NULL;

    Itcl_ListElem *elem;
    ItclObject *contextIoPtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_DString buffer;

    /*
     *  Destroy all derived classes, since these lose their meaning
     *  when the base class goes away.  If anything goes wrong,
     *  abort with an error.
     *
     *  TRICKY NOTE:  When a derived class is destroyed, it
     *    automatically deletes itself from the "derived" list.
     */
    elem = Itcl_FirstListElem(&iclsPtr->derived);
    while (elem) {
        iclsPtr2 = (ItclClass*)Itcl_GetListValue(elem);
        elem = Itcl_NextListElem(elem);  /* advance here--elem will go away */

        if (Itcl_DeleteClass(interp, iclsPtr2) != TCL_OK) {
            goto deleteClassFail;
        }
    }

    /*
     *  Scan through and find all objects that belong to this class.
     *  Note that more specialized objects have already been
     *  destroyed above, when derived classes were destroyed.
     *  Destroy objects and report any errors.
     */
    entry = Tcl_FirstHashEntry(&iclsPtr->infoPtr->objects, &place);
    while (entry) {
        contextIoPtr = (ItclObject*)Tcl_GetHashValue(entry);

        if (contextIoPtr->iclsPtr == iclsPtr) {
            if (Itcl_DeleteObject(interp, contextIoPtr) != TCL_OK) {
                iclsPtr2 = iclsPtr;
                goto deleteClassFail;
            }

	    /*
	     * Fix 227804: Whenever an object to delete was found we
	     * have to reset the search to the beginning as the
	     * current entry in the search was deleted and accessing it
	     * is therefore not allowed anymore.
	     */

	    entry = Tcl_FirstHashEntry(&iclsPtr->infoPtr->objects, &place);
	    continue;
        }

        entry = Tcl_NextHashEntry(&place);
    }

    /*
     *  Destroy the namespace associated with this class.
     *
     *  TRICKY NOTE:
     *    The cleanup procedure associated with the namespace is
     *    invoked automatically.  It does all of the same things
     *    above, but it also disconnects this class from its
     *    base-class lists, and removes the class access command.
     */
    if (iclsPtr->namesp != NULL) {
        Tcl_DeleteNamespace(iclsPtr->namesp);
        ItclDeleteClassVariablesNamespace(interp, iclsPtr);
    }
    return TCL_OK;

deleteClassFail:
    Tcl_DStringInit(&buffer);
    Tcl_DStringAppend(&buffer, "\n    (while deleting class \"", -1);
    Tcl_DStringAppend(&buffer, iclsPtr2->namesp->fullName, -1);
    Tcl_DStringAppend(&buffer, "\")", -1);
    Tcl_AddErrorInfo(interp, Tcl_DStringValue(&buffer));
    Tcl_DStringFree(&buffer);
    return TCL_ERROR;
}


/*
 * ------------------------------------------------------------------------
 *  ItclDestroyClass()
 *
 *  Invoked whenever the access command for a class is destroyed.
 *  Destroys the namespace associated with the class, which also
 *  destroys all objects in the class and all derived classes.
 *  Disconnects this class from the "derived" class lists of its
 *  base classes, and releases any claim to the class definition
 *  data.  If this is the last use of that data, the class will
 *  completely vanish at this point.
 * ------------------------------------------------------------------------
 */
static void
ItclDestroyClass(
    ClientData cdata)  /* class definition to be destroyed */
{
    ItclClass *iclsPtr = (ItclClass*)cdata;
    iclsPtr->accessCmd = NULL;

    if (iclsPtr->namesp != NULL) {
        Tcl_DeleteNamespace(iclsPtr->namesp);
        iclsPtr->namesp = NULL;
    }
    Itcl_ReleaseData((ClientData)iclsPtr);
}


/*
 * ------------------------------------------------------------------------
 *  ItclDestroyClassNamesp()
 *
 *  Invoked whenever the namespace associated with a class is destroyed.
 *  Destroys all objects associated with this class and all derived
 *  classes.  Disconnects this class from the "derived" class lists
 *  of its base classes, and removes the class access command.  Releases
 *  any claim to the class definition data.  If this is the last use
 *  of that data, the class will completely vanish at this point.
 * ------------------------------------------------------------------------
 */
static void
ItclDestroyClassNamesp(
    ClientData cdata)  /* class definition to be destroyed */
{
    ItclClass *iclsPtr = (ItclClass*)cdata;
    ItclObject *contextObj;
    Itcl_ListElem *elem, *belem;
    ItclClass *iclsPtr2;
    ItclClass *basePtr;
    ItclClass *derivedPtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;

    /*
     *  Destroy all derived classes, since these lose their meaning
     *  when the base class goes away.
     *
     *  TRICKY NOTE:  When a derived class is destroyed, it
     *    automatically deletes itself from the "derived" list.
     */
    elem = Itcl_FirstListElem(&iclsPtr->derived);
    while (elem) {
        iclsPtr2 = (ItclClass*)Itcl_GetListValue(elem);
	if (iclsPtr2->namesp != NULL) {
            Tcl_DeleteNamespace(iclsPtr2->namesp);
            iclsPtr2->namesp = NULL;
        }

	/* As the first namespace is now destroyed we have to get the
         * new first element of the hash table. We cannot go to the
         * next element from the current one, because the current one
         * is deleted. itcl Patch #593112, for Bug #577719.
	 */

        elem = Itcl_FirstListElem(&iclsPtr->derived);
    }

    /*
     *  Scan through and find all objects that belong to this class.
     *  Destroy them quietly by deleting their access command.
     */
    entry = Tcl_FirstHashEntry(&iclsPtr->infoPtr->objects, &place);
    while (entry) {
        contextObj = (ItclObject*)Tcl_GetHashValue(entry);
        if (contextObj->iclsPtr == iclsPtr) {
            Tcl_DeleteCommandFromToken(iclsPtr->interp, contextObj->accessCmd);
	    /*
	     * Fix 227804: Whenever an object to delete was found we
	     * have to reset the search to the beginning as the
	     * current entry in the search was deleted and accessing it
	     * is therefore not allowed anymore.
	     */

	    entry = Tcl_FirstHashEntry(&iclsPtr->infoPtr->objects, &place);
	    continue;
        }
        entry = Tcl_NextHashEntry(&place);
    }

    /*
     *  Next, remove this class from the "derived" list in
     *  all base classes.
     */
    belem = Itcl_FirstListElem(&iclsPtr->bases);
    while (belem) {
        basePtr = (ItclClass*)Itcl_GetListValue(belem);

        elem = Itcl_FirstListElem(&basePtr->derived);
        while (elem) {
            derivedPtr = (ItclClass*)Itcl_GetListValue(elem);
            if (derivedPtr == iclsPtr) {
                Itcl_ReleaseData( Itcl_GetListValue(elem) );
                elem = Itcl_DeleteListElem(elem);
            } else {
                elem = Itcl_NextListElem(elem);
            }
        }
        belem = Itcl_NextListElem(belem);
    }

    /*
     *  Next, destroy the access command associated with the class.
     */
    if (iclsPtr->accessCmd) {
        Tcl_CmdInfo cmdInfo;
	if (Tcl_GetCommandInfoFromToken(iclsPtr->accessCmd, &cmdInfo) == 1) {
            cmdInfo.deleteProc = Itcl_ReleaseData;
	    Tcl_SetCommandInfoFromToken(iclsPtr->accessCmd, &cmdInfo);
            Tcl_DeleteCommandFromToken(iclsPtr->interp, iclsPtr->accessCmd);
        }
    }

    /*
     *  Release the namespace's claim on the class definition.
     */
    Itcl_ReleaseData((ClientData)iclsPtr);
}


/*
 * ------------------------------------------------------------------------
 *  ItclFreeClass()
 *
 *  Frees all memory associated with a class definition.  This is
 *  usually invoked automatically by Itcl_ReleaseData(), when class
 *  data is no longer being used.
 * ------------------------------------------------------------------------
 */
static void
ItclFreeClass(cdata)
    char *cdata;  /* class definition to be destroyed */
{
/* FIX ME !!! */
#ifdef NOTDEF
    ItclClass *iclsPtr = (ItclClass*)cdata;

    int newEntry;
    Itcl_ListElem *elem;
    Tcl_HashSearch place;
    Tcl_HashEntry *entry, *hPtr;
    ItclVariable *ivPtr;
    ItclVarLookup *vlookup;
    Var *varPtr;
    Tcl_HashTable varTable;

    /*
     *  Tear down the list of derived classes.  This list should
     *  really be empty if everything is working properly, but
     *  release it here just in case.
     */
    elem = Itcl_FirstListElem(&iclsPtr->derived);
    while (elem) {
        Itcl_ReleaseData( Itcl_GetListValue(elem) );
        elem = Itcl_NextListElem(elem);
    }
    Itcl_DeleteList(&iclsPtr->derived);

    /*
     *  Tear down the variable resolution table.  Some records
     *  appear multiple times in the table (for x, foo::x, etc.)
     *  so each one has a reference count.
     */
    Tcl_InitHashTable(&varTable, TCL_STRING_KEYS);

    entry = Tcl_FirstHashEntry(&iclsPtr->resolveVars, &place);
    while (entry) {
        vlookup = (ItclVarLookup*)Tcl_GetHashValue(entry);
        if (--vlookup->usage == 0) {
            /*
             *  If this is a common variable owned by this class,
             *  then release the class's hold on it.  If it's no
             *  longer being used, move it into a variable table
             *  for destruction.
             */
            if ( (vlookup->ivPtr->flags & ITCL_COMMON) != 0 &&
                 vlookup->ivPtr->classPtr == iclsPtr ) {
                varPtr = (Var*)vlookup->var.common;
                if (--varPtr->refCount == 0) {
                    hPtr = Tcl_CreateHashEntry(&varTable,
                        Tcl-GetString(vlookup->ivPtr->fullname), &newEntry);
                    Tcl_SetHashValue(hPtr, (ClientData) varPtr);
                }
            }
            ckfree((char*)vlookup);
        }
        entry = Tcl_NextHashEntry(&place);
    }

    TclDeleteVars((Interp*)iclsPtr->interp, &varTable);
    Tcl_DeleteHashTable(&iclsPtr->resolveVars);

    /*
     *  Tear down the virtual method table...
     */
    Tcl_DeleteHashTable(&iclsPtr->resolveCmds);

    /*
     *  Delete all variable definitions.
     */
    entry = Tcl_FirstHashEntry(&iclsPtr->variables, &place);
    while (entry) {
        ivPtr = (ItclVariable*)Tcl_GetHashValue(entry);
        Itcl_DeleteVariable(ivPtr);
        entry = Tcl_NextHashEntry(&place);
    }
    Tcl_DeleteHashTable(&iclsPtr->variables);

    /*
     *  Delete all function definitions.
     */
    entry = Tcl_FirstHashEntry(&iclsPtr->functions, &place);
    while (entry) {
        Itcl_ReleaseData( Tcl_GetHashValue(entry) );
        entry = Tcl_NextHashEntry(&place);
    }
    Tcl_DeleteHashTable(&iclsPtr->functions);

    /*
     *  Release the claim on all base classes.
     */
    elem = Itcl_FirstListElem(&iclsPtr->bases);
    while (elem) {
        Itcl_ReleaseData( Itcl_GetListValue(elem) );
        elem = Itcl_NextListElem(elem);
    }
    Itcl_DeleteList(&iclsPtr->bases);
    Tcl_DeleteHashTable(&iclsPtr->heritage);

    /*
     *  Free up the object initialization code.
     */
    if (iclsPtr->initCode) {
        Tcl_DecrRefCount(iclsPtr->initCode);
    }

    Itcl_ReleaseData((ClientData)iclsPtr->infoPtr);

    ckfree(iclsPtr->name);
    ckfree(iclsPtr->fullname);

    ckfree((char*)iclsPtr);
#endif
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_IsClassNamespace()
 *
 *  Checks to see whether or not the given namespace represents an
 *  [incr Tcl] class.  Returns non-zero if so, and zero otherwise.
 * ------------------------------------------------------------------------
 */
int
Itcl_IsClassNamespace(
    Tcl_Namespace *nsPtr)  /* namespace being tested */
{
    if (nsPtr != NULL) {
        if (nsPtr->deleteProc == NULL) {
	   return 0;
	}
        return (nsPtr->deleteProc == _TclOONamespaceDeleteProc);
    }
    return 0;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_IsClass()
 *
 *  Checks the given Tcl command to see if it represents an itcl class.
 *  Returns non-zero if the command is associated with a class.
 * ------------------------------------------------------------------------
 */
int
Itcl_IsClass(
    Tcl_Command cmd)         /* command being tested */
{
    Tcl_CmdInfo cmdInfo;

    if (Tcl_GetCommandInfoFromToken(cmd, &cmdInfo) == 0) {
        return 0;
    }
    if (cmdInfo.deleteProc == ItclDestroyClass) {
        return 1;
    }

    /*
     *  This may be an imported command.  Try to get the real
     *  command and see if it represents a class.
     */
    cmd = Tcl_GetOriginalCommand(cmd);
    if (cmd != NULL) {
	if (Tcl_GetCommandInfoFromToken(cmd, &cmdInfo) == 0) {
            return 0;
        }
        if (cmdInfo.deleteProc == ItclDestroyClass) {
            return 1;
        }
    }
    return 0;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_FindClass()
 *
 *  Searches for the specified class in the active namespace.  If the
 *  class is found, this procedure returns a pointer to the class
 *  definition.  Otherwise, if the autoload flag is non-zero, an
 *  attempt will be made to autoload the class definition.  If it
 *  still can't be found, this procedure returns NULL, along with an
 *  error message in the interpreter.
 * ------------------------------------------------------------------------
 */
ItclClass*
Itcl_FindClass(interp, path, autoload)
    Tcl_Interp* interp;      /* interpreter containing class */
    CONST char* path;              /* path name for class */
{
    Tcl_Namespace* classNs;

    /*
     *  Search for a namespace with the specified name, and if
     *  one is found, see if it is a class namespace.
     */
    classNs = Itcl_FindClassNamespace(interp, path);

    if (classNs && Itcl_IsClassNamespace(classNs)) {
	ItclObjectInfo *infoPtr;
	infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, NULL);
        return (ItclClass*)Tcl_ObjectGetMetadata(classNs->clientData,
	        infoPtr->class_meta_type);
    }

    /*
     *  If the autoload flag is set, try to autoload the class
     *  definition.
     */
    if (autoload) {
        if (Tcl_VarEval(interp, "::auto_load ", path, (char*)NULL) != TCL_OK) {
            char msg[256];
            sprintf(msg, "\n    (while attempting to autoload class \"%.200s\")", path);
            Tcl_AddErrorInfo(interp, msg);
            return NULL;
        }
        Tcl_ResetResult(interp);

        classNs = Itcl_FindClassNamespace(interp, path);
        if (classNs && Itcl_IsClassNamespace(classNs)) {
	    ItclObjectInfo *infoPtr;
	    Tcl_HashEntry *hPtr;

	    infoPtr = Tcl_GetAssocData(interp, ITCL_INTERP_DATA, NULL);
	    hPtr = Tcl_FindHashEntry(&infoPtr->namespaceClasses, (char *)
	            classNs);
	    if (hPtr == NULL) {
	        char msg[256];
		sprintf(msg,
		        "\n    (while attempting to autoload class \"%.200s\")"
		        , path);
                Tcl_AddErrorInfo(interp, msg);
                return NULL;
	    }
	    return (ItclClass *)Tcl_GetHashValue(hPtr);
        }
    }

    Tcl_AppendResult(interp, "class \"", path, "\" not found in context \"",
        Tcl_GetCurrentNamespace(interp)->fullName, "\"",
        (char*)NULL);

    return NULL;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_FindClassNamespace()
 *
 *  Searches for the specified class namespace.  The normal Tcl procedure
 *  Tcl_FindNamespace also searches for namespaces, but only in the
 *  current namespace context.  This makes it hard to find one class
 *  from within another.  For example, suppose. you have two namespaces
 *  Foo and Bar.  If you're in the context of Foo and you look for
 *  Bar, you won't find it with Tcl_FindNamespace.  This behavior is
 *  okay for namespaces, but wrong for classes.
 *
 *  This procedure search for a class namespace.  If the name is
 *  absolute (i.e., starts with "::"), then that one name is checked,
 *  and the class is either found or not.  But if the name is relative,
 *  it is sought in the current namespace context and in the global
 *  context, just like the normal command lookup.
 *
 *  This procedure returns a pointer to the desired namespace, or
 *  NULL if the namespace was not found.
 * ------------------------------------------------------------------------
 */
Tcl_Namespace*
Itcl_FindClassNamespace(interp, path)
    Tcl_Interp* interp;        /* interpreter containing class */
    CONST char* path;                /* path name for class */
{
    Tcl_Namespace* contextNs = Tcl_GetCurrentNamespace(interp);
    Tcl_Namespace* classNs;
    Tcl_DString buffer;

    /*
     *  Look up the namespace.  If the name is not absolute, then
     *  see if it's the current namespace, and try the global
     *  namespace as well.
     */
    classNs = Tcl_FindNamespace(interp, (CONST84 char *)path,
	    (Tcl_Namespace*)NULL, /* flags */ 0);

    if ( !classNs && contextNs->parentPtr != NULL &&
         (*path != ':' || *(path+1) != ':') ) {

        if (strcmp(contextNs->name, path) == 0) {
            classNs = contextNs;
        }
        else {
            Tcl_DStringInit(&buffer);
            Tcl_DStringAppend(&buffer, "::", -1);
            Tcl_DStringAppend(&buffer, path, -1);

            classNs = Tcl_FindNamespace(interp, Tcl_DStringValue(&buffer),
                (Tcl_Namespace*)NULL, /* flags */ 0);

            Tcl_DStringFree(&buffer);
        }
    }
    return classNs;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_HandleClass()
 *
 *  first argument is ::itcl::parser::handleClass 
 *  Invoked by Tcl whenever the user issues the command associated with
 *  a class name.  Handles the following syntax:
 *
 *    <className>
 *    <className> <objName> ?<args>...?
 *
 *  Without any arguments, the command does nothing.  In the olden days,
 *  this allowed the class name to be invoked by itself to prompt the
 *  autoloader to load the class definition.  Today, this behavior is
 *  retained for backward compatibility with old releases.
 *
 *  If arguments are specified, then this procedure creates a new
 *  object named <objName> in the appropriate class.  Note that if
 *  <objName> contains "#auto", that part is automatically replaced
 *  by a unique string built from the class name.
 * ------------------------------------------------------------------------
 */
int
Itcl_HandleClass(
    ClientData clientData,   /* class definition */
    Tcl_Interp *interp,      /* current interpreter */
    int objc,                /* number of arguments */
    Tcl_Obj *CONST objv[])   /* argument objects */
{
    ItclClass *iclsPtr;
    ItclObjectInfo *infoPtr = (ItclObjectInfo *)clientData;
    Tcl_HashEntry *hPtr;
    int result = TCL_OK;

    char unique[256];    /* buffer used for unique part of object names */
    Tcl_DString buffer;  /* buffer used to build object names */
    char *token;
    char *objName;
    char tmp;
    char *start;
    char *pos;
    char *match;

    /*
     *  If the command is invoked without an object name, then do nothing.
     *  This used to support autoloading--that the class name could be
     *  invoked as a command by itself, prompting the autoloader to
     *  load the class definition.  We retain the behavior here for
     *  backward-compatibility with earlier releases.
     */
    if (objc <= 3) {
        return TCL_OK;
    }

    hPtr = Tcl_FindHashEntry(&infoPtr->classes, (char *)objv[2]);
    if (hPtr == NULL) {
        Tcl_AppendResult(interp, "no such class: \"",
	        Tcl_GetString(objv[1]), "\"", NULL);
	return TCL_ERROR;
    }
    iclsPtr = Tcl_GetHashValue(hPtr);

    /*
     *  If the object name is "::", and if this is an old-style class
     *  definition, then treat the remaining arguments as a command
     *  in the class namespace.  This used to be the way of invoking
     *  a class proc, but the new syntax is "class::proc" (without
     *  spaces).
     */
    token = Tcl_GetString(objv[3]);
    if ((*token == ':') && (strcmp(token,"::") == 0) && (objc > 4)) {
        /*
         *  If this is not an old-style class, then return an error
         *  describing the syntax change.
         */
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "syntax \"class :: proc\" is an anachronism\n",
            "[incr Tcl] no longer supports this syntax.\n",
            "Instead, remove the spaces from your procedure invocations:\n",
            "  ",
            Tcl_GetString(objv[1]), "::",
            Tcl_GetString(objv[4]), " ?args?",
            (char*)NULL);
        return TCL_ERROR;
    }

    /*
     *  Otherwise, we have a proper object name.  Create a new instance
     *  with that name.  If the name contains "#auto", replace this with
     *  a uniquely generated string based on the class name.
     */
    Tcl_DStringInit(&buffer);
    objName = NULL;

    match = "#auto";
    start = token;
    for (pos=start; *pos != '\0'; pos++) {
        if (*pos == *match) {
            if (*(++match) == '\0') {
                tmp = *start;
                *start = '\0';  /* null-terminate first part */

                /*
                 *  Substitute a unique part in for "#auto", and keep
                 *  incrementing a counter until a valid name is found.
                 */
                do {
		    Tcl_CmdInfo dummy;

                    sprintf(unique,"%.200s%d", Tcl_GetString(iclsPtr->name),
                        iclsPtr->unique++);
                    unique[0] = tolower(unique[0]);

                    Tcl_DStringTrunc(&buffer, 0);
                    Tcl_DStringAppend(&buffer, token, -1);
                    Tcl_DStringAppend(&buffer, unique, -1);
                    Tcl_DStringAppend(&buffer, start+5, -1);

                    objName = Tcl_DStringValue(&buffer);

		    /*
		     * [Fix 227811] Check for any command with the
		     * given name, not only objects.
		     */

                    if (Tcl_GetCommandInfo (interp, objName, &dummy) == 0) {
                        break;  /* if an error is found, bail out! */
                    }
                } while (1);

                *start = tmp;       /* undo null-termination */
                objName = Tcl_DStringValue(&buffer);
                break;              /* object name is ready to go! */
            }
        }
        else {
            match = "#auto";
            pos = start++;
        }
    }

    /*
     *  If "#auto" was not found, then just use object name as-is.
     */
    if (objName == NULL) {
        objName = token;
    }

    /*
     *  Try to create a new object.  If successful, return the
     *  object name as the result of this command.
     */
    result = ItclCreateObject(interp, objName, iclsPtr, objc-4, objv+4);

    if (result == TCL_OK) {
        Tcl_SetObjResult(interp, Tcl_NewStringObj(objName, -1));
    }

    Tcl_DStringFree(&buffer);
    if (result == TCL_ERROR) {
        return Itcl_ProcessReturn(interp, result, 2, NULL);
    }
    return result;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_BuildVirtualTables()
 *
 *  Invoked whenever the class heritage changes or members are added or
 *  removed from a class definition to rebuild the member lookup
 *  tables.  There are two tables:
 *
 *  METHODS:  resolveCmds
 *    Used primarily in Itcl_ClassCmdResolver() to resolve all
 *    command references in a namespace.
 *
 *  DATA MEMBERS:  resolveVars
 *    Used primarily in Itcl_ClassVarResolver() to quickly resolve
 *    variable references in each class scope.
 *
 *  These tables store every possible name for each command/variable
 *  (member, class::member, namesp::class::member, etc.).  Members
 *  in a derived class may shadow members with the same name in a
 *  base class.  In that case, the simple name in the resolution
 *  table will point to the most-specific member.
 * ------------------------------------------------------------------------
 */
void
Itcl_BuildVirtualTables(
    ItclClass* iclsPtr)       /* class definition being updated */
{
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    ItclVarLookup *vlookup;
    ItclVariable *ivPtr;
    ItclMemberFunc *imPtr;
    ItclHierIter hier;
    ItclClass *iclsPtr2;
    Tcl_Namespace* nsPtr;
    Tcl_DString buffer, buffer2;
    int newEntry;

    Tcl_DStringInit(&buffer);
    Tcl_DStringInit(&buffer2);

    /*
     *  Clear the variable resolution table.
     */
    entry = Tcl_FirstHashEntry(&iclsPtr->resolveVars, &place);
    while (entry) {
        vlookup = (ItclVarLookup*)Tcl_GetHashValue(entry);
        if (--vlookup->usage == 0) {
            ckfree((char*)vlookup);
        }
        entry = Tcl_NextHashEntry(&place);
    }
    Tcl_DeleteHashTable(&iclsPtr->resolveVars);
    Tcl_InitHashTable(&iclsPtr->resolveVars, TCL_STRING_KEYS);
    iclsPtr->numInstanceVars = 0;

    /*
     *  Set aside the first object-specific slot for the built-in
     *  "this" variable.  Only allocate one of these, even though
     *  there is a definition for "this" in each class scope.
     */
    iclsPtr->numInstanceVars++;

    /*
     *  Scan through all classes in the hierarchy, from most to
     *  least specific.  Add a lookup entry for each variable
     *  into the table.
     */
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
        entry = Tcl_FirstHashEntry(&iclsPtr2->variables, &place);
        while (entry) {
            ivPtr = (ItclVariable*)Tcl_GetHashValue(entry);

            vlookup = (ItclVarLookup*)ckalloc(sizeof(ItclVarLookup));
            vlookup->ivPtr = ivPtr;
            vlookup->usage = 0;
            vlookup->leastQualName = NULL;

            /*
             *  If this variable is PRIVATE to another class scope,
             *  then mark it as "inaccessible".
             */
            vlookup->accessible = (ivPtr->protection != ITCL_PRIVATE ||
	            ivPtr->iclsPtr == iclsPtr);

/* FIX ME !!! sould use for var lookup !! */
#ifdef NOTDEF
            /*
             *  If this is a common variable, then keep a reference to
             *  the variable directly.  Otherwise, keep an index into
             *  the object's variable table.
             */
            if ((ivPtr->flags & ITCL_COMMON) != 0) {
                nsPtr = (Namespace*)iclsPtr2->namesp;
                hPtr = Tcl_FindHashEntry(&nsPtr->varTable, ivPtr->name);
                assert(hPtr != NULL);

                vlookup->var.common = (Tcl_Var)Tcl_GetHashValue(hPtr);
            }
            else {
                /*
                 *  If this is a reference to the built-in "this"
                 *  variable, then its index is "0".  Otherwise,
                 *  add another slot to the end of the table.
                 */
                if ((ivPtr->flags & ITCL_THIS_VAR) != 0) {
                    vlookup->var.index = 0;
                }
                else {
                    vlookup->var.index = iclsPtr->numInstanceVars++;
                }
            }
#endif

            /*
             *  Create all possible names for this variable and enter
             *  them into the variable resolution table:
             *     var
             *     class::var
             *     namesp1::class::var
             *     namesp2::namesp1::class::var
             *     ...
             */
            Tcl_DStringSetLength(&buffer, 0);
            Tcl_DStringAppend(&buffer, Tcl_GetString(ivPtr->namePtr), -1);
            nsPtr = iclsPtr2->namesp;

            while (1) {
                entry = Tcl_CreateHashEntry(&iclsPtr->resolveVars,
                    Tcl_DStringValue(&buffer), &newEntry);

                if (newEntry) {
                    Tcl_SetHashValue(entry, (ClientData)vlookup);
                    vlookup->usage++;

                    if (!vlookup->leastQualName) {
                        vlookup->leastQualName =
                            Tcl_GetHashKey(&iclsPtr->resolveVars, entry);
                    }
                }

                if (nsPtr == NULL) {
                    break;
                }
                Tcl_DStringSetLength(&buffer2, 0);
                Tcl_DStringAppend(&buffer2, Tcl_DStringValue(&buffer), -1);
                Tcl_DStringSetLength(&buffer, 0);
                Tcl_DStringAppend(&buffer, nsPtr->name, -1);
                Tcl_DStringAppend(&buffer, "::", -1);
                Tcl_DStringAppend(&buffer, Tcl_DStringValue(&buffer2), -1);

                nsPtr = nsPtr->parentPtr;
            }

            /*
             *  If this record is not needed, free it now.
             */
            if (vlookup->usage == 0) {
                ckfree((char*)vlookup);
            }
            entry = Tcl_NextHashEntry(&place);
        }
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Itcl_DeleteHierIter(&hier);

    /*
     *  Clear the command resolution table.
     */
    Tcl_DeleteHashTable(&iclsPtr->resolveCmds);
    Tcl_InitHashTable(&iclsPtr->resolveCmds, TCL_STRING_KEYS);

    /*
     *  Scan through all classes in the hierarchy, from most to
     *  least specific.  Look for the first (most-specific) definition
     *  of each member function, and enter it into the table.
     */
    Itcl_InitHierIter(&hier, iclsPtr);
    iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    while (iclsPtr2 != NULL) {
        entry = Tcl_FirstHashEntry(&iclsPtr2->functions, &place);
        while (entry) {
            imPtr = (ItclMemberFunc*)Tcl_GetHashValue(entry);

            /*
             *  Create all possible names for this function and enter
             *  them into the command resolution table:
             *     func
             *     class::func
             *     namesp1::class::func
             *     namesp2::namesp1::class::func
             *     ...
             */
            Tcl_DStringSetLength(&buffer, 0);
            Tcl_DStringAppend(&buffer, Tcl_GetString(imPtr->namePtr), -1);
            nsPtr = iclsPtr2->namesp;

            while (1) {
                entry = Tcl_CreateHashEntry(&iclsPtr->resolveCmds,
                    Tcl_DStringValue(&buffer), &newEntry);

                if (newEntry) {
                    Tcl_SetHashValue(entry, (ClientData)imPtr);
                }

                if (nsPtr == NULL) {
                    break;
                }
                Tcl_DStringSetLength(&buffer2, 0);
                Tcl_DStringAppend(&buffer2, Tcl_DStringValue(&buffer), -1);
                Tcl_DStringSetLength(&buffer, 0);
                Tcl_DStringAppend(&buffer, nsPtr->name, -1);
                Tcl_DStringAppend(&buffer, "::", -1);
                Tcl_DStringAppend(&buffer, Tcl_DStringValue(&buffer2), -1);

                nsPtr = nsPtr->parentPtr;
            }
            entry = Tcl_NextHashEntry(&place);
        }
        iclsPtr2 = Itcl_AdvanceHierIter(&hier);
    }
    Itcl_DeleteHierIter(&hier);

    Tcl_DStringFree(&buffer);
    Tcl_DStringFree(&buffer2);
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_CreateVariable()
 *
 *  Creates a new class variable definition.  If this is a public
 *  variable, it may have a bit of "config" code that is used to
 *  update the object whenever the variable is modified via the
 *  built-in "configure" method.
 *
 *  Returns TCL_ERROR along with an error message in the specified
 *  interpreter if anything goes wrong.  Otherwise, this returns
 *  TCL_OK and a pointer to the new variable definition in "ivPtr".
 * ------------------------------------------------------------------------
 */
int
Itcl_CreateVariable(
    Tcl_Interp *interp,       /* interpreter managing this transaction */
    ItclClass* iclsPtr,       /* class containing this variable */
    Tcl_Obj* namePtr,         /* variable name */
    char* init,               /* initial value */
    char* config,             /* code invoked when variable is configured */
    ItclVariable** ivPtrPtr)  /* returns: new variable definition */
{
    int newEntry;
    ItclVariable *ivPtr;
    ItclMemberCode *mcode;
    Tcl_HashEntry *entry;

    /*
     *  Add this variable to the variable table for the class.
     *  Make sure that the variable name does not already exist.
     */
    entry = Tcl_CreateHashEntry(&iclsPtr->variables, (char *)namePtr, &newEntry);
    if (!newEntry) {
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "variable name \"", Tcl_GetString(namePtr),
	    "\" already defined in class \"",
            iclsPtr->fullname, "\"",
            (char*)NULL);
        return TCL_ERROR;
    }
    Tcl_IncrRefCount(namePtr);

    /*
     *  If this variable has some "config" code, try to capture
     *  its implementation.
     */
    if (config) {
        if (Itcl_CreateMemberCode(interp, iclsPtr, (char*)NULL, config,
                &mcode) != TCL_OK) {
            Tcl_DeleteHashEntry(entry);
            return TCL_ERROR;
        }
        Itcl_PreserveData((ClientData)mcode);
        Itcl_EventuallyFree((ClientData)mcode, Itcl_DeleteMemberCode);
    } else {
        mcode = NULL;
    }
        

    /*
     *  If everything looks good, create the variable definition.
     */
    ivPtr = (ItclVariable*)ckalloc(sizeof(ItclVariable));
    memset(ivPtr, 0, sizeof(ItclVariable));
    ivPtr->iclsPtr      = iclsPtr;
    ivPtr->protection   = Itcl_Protection(interp, 0);
    ivPtr->codePtr      = mcode;
    ivPtr->namePtr      = namePtr;
    Tcl_IncrRefCount(ivPtr->namePtr);
    ivPtr->fullNamePtr = Tcl_NewStringObj(Tcl_GetString(iclsPtr->fullname), -1);
    Tcl_AppendToObj(ivPtr->fullNamePtr, "::", 2);
    Tcl_AppendToObj(ivPtr->fullNamePtr, Tcl_GetString(namePtr), -1);
    Tcl_IncrRefCount(ivPtr->fullNamePtr);

    if (ivPtr->protection == ITCL_DEFAULT_PROTECT) {
        ivPtr->protection = ITCL_PROTECTED;
    }

    if (init) {
        ivPtr->init = Tcl_NewStringObj(init, -1);
        Tcl_IncrRefCount(ivPtr->init);
    } else {
        ivPtr->init = NULL;
    }

    Tcl_SetHashValue(entry, (ClientData)ivPtr);

    *ivPtrPtr = ivPtr;
    return TCL_OK;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_DeleteVariable()
 *
 *  Destroys a variable definition created by Itcl_CreateVariable(),
 *  freeing all resources associated with it.
 * ------------------------------------------------------------------------
 */
void
Itcl_DeleteVariable(ivPtr)
    ItclVariable *ivPtr;   /* variable definition to be destroyed */
{
    Tcl_DecrRefCount(ivPtr->namePtr);
    Tcl_DecrRefCount(ivPtr->fullNamePtr);

    if (ivPtr->codePtr) {
        Itcl_ReleaseData((ClientData)ivPtr->codePtr);
    }
    ivPtr->codePtr = NULL;

    if (ivPtr->init) {
        Tcl_DecrRefCount(ivPtr->init);
    }
    ckfree((char*)ivPtr);
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_GetCommonVar()
 *
 *  Returns the current value for a common class variable.  The member
 *  name is interpreted with respect to the given class scope.  That
 *  scope is installed as the current context before querying the
 *  variable.  This by-passes the protection level in case the variable
 *  is "private".
 *
 *  If successful, this procedure returns a pointer to a string value
 *  which remains alive until the variable changes it value.  If
 *  anything goes wrong, this returns NULL.
 * ------------------------------------------------------------------------
 */
CONST char*
Itcl_GetCommonVar(
    Tcl_Interp *interp,        /* current interpreter */
    CONST char *name,          /* name of desired instance variable */
    ItclClass *contextIclsPtr) /* name is interpreted in this scope */
{
    CONST char *val = NULL;
    Tcl_HashEntry *hPtr;
    Tcl_DString buffer;
    Tcl_Obj *namePtr;
    ItclVariable *ivPtr;
    const char *cp;
    const char *lastCp;

    lastCp = name;
    cp = name;
    while (cp != NULL) {
        cp = strstr(lastCp, "::");
        if (cp != NULL) {
	    lastCp = cp + 2;
	}
    }
    namePtr = Tcl_NewStringObj(lastCp, -1);
    Tcl_IncrRefCount(namePtr);
    hPtr = Tcl_FindHashEntry(&contextIclsPtr->variables, (char *)namePtr);
    Tcl_DecrRefCount(namePtr);
    if (hPtr == NULL) {
        return NULL;
    }
    ivPtr = Tcl_GetHashValue(hPtr);
    /*
     *  Activate the namespace for the given class.  That installs
     *  the appropriate name resolution rules and by-passes any
     *  security restrictions.
     */
    Tcl_DStringInit(&buffer);
    if (ivPtr->protection != ITCL_PUBLIC) {
        Tcl_DStringAppend(&buffer, ITCL_VARIABLES_NAMESPACE, -1);
    }
    Tcl_DStringAppend(&buffer, name, -1);

    val = Tcl_GetVar2(interp, (CONST84 char *)Tcl_DStringValue(&buffer),
            (char*)NULL, 0);
    Tcl_DStringFree(&buffer);
    return val;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_InitHierIter()
 *
 *  Initializes an iterator for traversing the hierarchy of the given
 *  class.  Subsequent calls to Itcl_AdvanceHierIter() will return
 *  the base classes in order from most-to-least specific.
 * ------------------------------------------------------------------------
 */
void
Itcl_InitHierIter(iter,iclsPtr)
    ItclHierIter *iter;   /* iterator used for traversal */
    ItclClass *iclsPtr;     /* class definition for start of traversal */
{
    Itcl_InitStack(&iter->stack);
    Itcl_PushStack((ClientData)iclsPtr, &iter->stack);
    iter->current = iclsPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_DeleteHierIter()
 *
 *  Destroys an iterator for traversing class hierarchies, freeing
 *  all memory associated with it.
 * ------------------------------------------------------------------------
 */
void
Itcl_DeleteHierIter(iter)
    ItclHierIter *iter;  /* iterator used for traversal */
{
    Itcl_DeleteStack(&iter->stack);
    iter->current = NULL;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_AdvanceHierIter()
 *
 *  Moves a class hierarchy iterator forward to the next base class.
 *  Returns a pointer to the current class definition, or NULL when
 *  the end of the hierarchy has been reached.
 * ------------------------------------------------------------------------
 */
ItclClass*
Itcl_AdvanceHierIter(
    ItclHierIter *iter)  /* iterator used for traversal */
{
    register Itcl_ListElem *elem;
    ItclClass *iclsPtr;

    iter->current = (ItclClass*)Itcl_PopStack(&iter->stack);

    /*
     *  Push classes onto the stack in reverse order, so that
     *  they will be popped off in the proper order.
     */
    if (iter->current) {
        iclsPtr = (ItclClass*)iter->current;
        elem = Itcl_LastListElem(&iclsPtr->bases);
        while (elem) {
            Itcl_PushStack(Itcl_GetListValue(elem), &iter->stack);
            elem = Itcl_PrevListElem(elem);
        }
    }
    return iter->current;
}
