/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  This segment provides common utility functions used throughout
 *  the other [incr Tcl] source files.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclngInt.h"

/*
 *  POOL OF LIST ELEMENTS FOR LINKED LIST
 */
static Itclng_ListElem *listPool = NULL;
static int listPoolLen = 0;

#define ITCLNG_VALID_LIST 0x01face10  /* magic bit pattern for validation */
#define ITCLNG_LIST_POOL_SIZE 200     /* max number of elements in listPool */

/*
 *  This structure is used to take a snapshot of the interpreter
 *  state in Itcl_SaveInterpState.  You can snapshot the state,
 *  execute a command, and then back up to the result or the
 *  error that was previously in progress.
 */
typedef struct InterpState {
    int validate;                   /* validation stamp */
    int status;                     /* return code status */
    Tcl_Obj *objResult;             /* result object */
    char *errorInfo;                /* contents of errorInfo variable */
    char *errorCode;                /* contents of errorCode variable */
} InterpState;

#define TCL_STATE_VALID 0x01233210  /* magic bit pattern for validation */



/*
 * ------------------------------------------------------------------------
 *  Itclng_Assert()
 *
 *  Called whenever an assert() test fails.  Prints a diagnostic
 *  message and abruptly exits.
 * ------------------------------------------------------------------------
 */

void
Itclng_Assert(
    CONST char *testExpr,   /* string representing test expression */
    CONST char *fileName,   /* file name containing this call */
    int lineNumber)	    /* line number containing this call */
{
    Tcl_Panic("Itclng Assertion failed: \"%s\" (line %d of %s)",
	testExpr, lineNumber, fileName);
}



/*
 * ------------------------------------------------------------------------
 *  Itclng_InitStack()
 *
 *  Initializes a stack structure, allocating a certain amount of memory
 *  for the stack and setting the stack length to zero.
 * ------------------------------------------------------------------------
 */
void
Itclng_InitStack(stack)
    Itclng_Stack *stack;     /* stack to be initialized */
{
    stack->values = stack->space;
    stack->max = sizeof(stack->space)/sizeof(ClientData);
    stack->len = 0;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_DeleteStack()
 *
 *  Destroys a stack structure, freeing any memory that may have been
 *  allocated to represent it.
 * ------------------------------------------------------------------------
 */
void
Itclng_DeleteStack(stack)
    Itclng_Stack *stack;     /* stack to be deleted */
{
    /*
     *  If memory was explicitly allocated (instead of using the
     *  built-in buffer) then free it.
     */
    if (stack->values != stack->space) {
        ckfree((char*)stack->values);
    }
    stack->values = NULL;
    stack->len = stack->max = 0;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_PushStack()
 *
 *  Pushes a piece of client data onto the top of the given stack.
 *  If the stack is not large enough, it is automatically resized.
 * ------------------------------------------------------------------------
 */
void
Itclng_PushStack(cdata,stack)
    ClientData cdata;      /* data to be pushed onto stack */
    Itclng_Stack *stack;     /* stack */
{
    ClientData *newStack;

    if (stack->len+1 >= stack->max) {
        stack->max = 2*stack->max;
        newStack = (ClientData*)
            ckalloc((unsigned)(stack->max*sizeof(ClientData)));

        if (stack->values) {
            memcpy((char*)newStack, (char*)stack->values,
                (size_t)(stack->len*sizeof(ClientData)));

            if (stack->values != stack->space)
                ckfree((char*)stack->values);
        }
        stack->values = newStack;
    }
    stack->values[stack->len++] = cdata;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_PopStack()
 *
 *  Pops a bit of client data from the top of the given stack.
 * ------------------------------------------------------------------------
 */
ClientData
Itclng_PopStack(stack)
    Itclng_Stack *stack;  /* stack to be manipulated */
{
    if (stack->values && (stack->len > 0)) {
        stack->len--;
        return stack->values[stack->len];
    }
    return (ClientData)NULL;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_PeekStack()
 *
 *  Gets the current value from the top of the given stack.
 * ------------------------------------------------------------------------
 */
ClientData
Itclng_PeekStack(stack)
    Itclng_Stack *stack;  /* stack to be examined */
{
    if (stack->values && (stack->len > 0)) {
        return stack->values[stack->len-1];
    }
    return (ClientData)NULL;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_GetStackValue()
 *
 *  Gets a value at some index within the stack.  Index "0" is the
 *  first value pushed onto the stack.
 * ------------------------------------------------------------------------
 */
ClientData
Itclng_GetStackValue(stack,pos)
    Itclng_Stack *stack;  /* stack to be examined */
    int pos;            /* get value at this index */
{
    if (stack->values && (stack->len > 0)) {
        assert(pos < stack->len);
        return stack->values[pos];
    }
    return (ClientData)NULL;
}


/*
 * ------------------------------------------------------------------------
 *  Itclng_InitList()
 *
 *  Initializes a linked list structure, setting the list to the empty
 *  state.
 * ------------------------------------------------------------------------
 */
void
Itclng_InitList(listPtr)
    Itclng_List *listPtr;     /* list to be initialized */
{
    listPtr->validate = ITCLNG_VALID_LIST;
    listPtr->num      = 0;
    listPtr->head     = NULL;
    listPtr->tail     = NULL;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_DeleteList()
 *
 *  Destroys a linked list structure, deleting all of its elements and
 *  setting it to an empty state.  If the elements have memory associated
 *  with them, this memory must be freed before deleting the list or it
 *  will be lost.
 * ------------------------------------------------------------------------
 */
void
Itclng_DeleteList(listPtr)
    Itclng_List *listPtr;     /* list to be deleted */
{
    Itclng_ListElem *elemPtr;

    assert(listPtr->validate == ITCLNG_VALID_LIST);

    elemPtr = listPtr->head;
    while (elemPtr) {
        elemPtr = Itclng_DeleteListElem(elemPtr);
    }
    listPtr->validate = 0;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_CreateListElem()
 *
 *  Low-level routined used by procedures like Itcl_InsertList() and
 *  Itcl_AppendList() to create new list elements.  If elements are
 *  available, one is taken from the list element pool.  Otherwise,
 *  a new one is allocated.
 * ------------------------------------------------------------------------
 */
Itclng_ListElem*
Itclng_CreateListElem(listPtr)
    Itclng_List *listPtr;     /* list that will contain this new element */
{
    Itclng_ListElem *elemPtr;

    if (listPoolLen > 0) {
        elemPtr = listPool;
        listPool = elemPtr->next;
        --listPoolLen;
    }
    else {
        elemPtr = (Itclng_ListElem*)ckalloc((unsigned)sizeof(Itclng_ListElem));
    }
    elemPtr->owner = listPtr;
    elemPtr->value = NULL;
    elemPtr->next  = NULL;
    elemPtr->prev  = NULL;

    return elemPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_DeleteListElem()
 *
 *  Destroys a single element in a linked list, returning it to a pool of
 *  elements that can be later reused.  Returns a pointer to the next
 *  element in the list.
 * ------------------------------------------------------------------------
 */
Itclng_ListElem*
Itclng_DeleteListElem(elemPtr)
    Itclng_ListElem *elemPtr;     /* list element to be deleted */
{
    Itclng_List *listPtr;
    Itclng_ListElem *nextPtr;

    nextPtr = elemPtr->next;

    if (elemPtr->prev) {
        elemPtr->prev->next = elemPtr->next;
    }
    if (elemPtr->next) {
        elemPtr->next->prev = elemPtr->prev;
    }

    listPtr = elemPtr->owner;
    if (elemPtr == listPtr->head)
        listPtr->head = elemPtr->next;
    if (elemPtr == listPtr->tail)
        listPtr->tail = elemPtr->prev;
    --listPtr->num;

    if (listPoolLen < ITCLNG_LIST_POOL_SIZE) {
        elemPtr->next = listPool;
        listPool = elemPtr;
        ++listPoolLen;
    }
    else {
        ckfree((char*)elemPtr);
    }
    return nextPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_InsertList()
 *
 *  Creates a new list element containing the given value and returns
 *  a pointer to it.  The element is inserted at the beginning of the
 *  specified list.
 * ------------------------------------------------------------------------
 */
Itclng_ListElem*
Itclng_InsertList(listPtr,val)
    Itclng_List *listPtr;     /* list being modified */
    ClientData val;         /* value associated with new element */
{
    Itclng_ListElem *elemPtr;
    assert(listPtr->validate == ITCLNG_VALID_LIST);

    elemPtr = Itclng_CreateListElem(listPtr);

    elemPtr->value = val;
    elemPtr->next  = listPtr->head;
    elemPtr->prev  = NULL;
    if (listPtr->head) {
        listPtr->head->prev = elemPtr;
    }
    listPtr->head  = elemPtr;
    if (listPtr->tail == NULL) {
        listPtr->tail = elemPtr;
    }
    ++listPtr->num;

    return elemPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_InsertListElem()
 *
 *  Creates a new list element containing the given value and returns
 *  a pointer to it.  The element is inserted in the list just before
 *  the specified element.
 * ------------------------------------------------------------------------
 */
Itclng_ListElem*
Itclng_InsertListElem(pos,val)
    Itclng_ListElem *pos;     /* insert just before this element */
    ClientData val;         /* value associated with new element */
{
    Itclng_List *listPtr;
    Itclng_ListElem *elemPtr;

    listPtr = pos->owner;
    assert(listPtr->validate == ITCLNG_VALID_LIST);
    assert(pos != NULL);

    elemPtr = Itclng_CreateListElem(listPtr);
    elemPtr->value = val;

    elemPtr->prev = pos->prev;
    if (elemPtr->prev) {
        elemPtr->prev->next = elemPtr;
    }
    elemPtr->next = pos;
    pos->prev     = elemPtr;

    if (listPtr->head == pos) {
        listPtr->head = elemPtr;
    }
    if (listPtr->tail == NULL) {
        listPtr->tail = elemPtr;
    }
    ++listPtr->num;

    return elemPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_AppendList()
 *
 *  Creates a new list element containing the given value and returns
 *  a pointer to it.  The element is appended at the end of the
 *  specified list.
 * ------------------------------------------------------------------------
 */
Itclng_ListElem*
Itclng_AppendList(listPtr,val)
    Itclng_List *listPtr;     /* list being modified */
    ClientData val;         /* value associated with new element */
{
    Itclng_ListElem *elemPtr;
    assert(listPtr->validate == ITCLNG_VALID_LIST);

    elemPtr = Itclng_CreateListElem(listPtr);

    elemPtr->value = val;
    elemPtr->prev  = listPtr->tail;
    elemPtr->next  = NULL;
    if (listPtr->tail) {
        listPtr->tail->next = elemPtr;
    }
    listPtr->tail  = elemPtr;
    if (listPtr->head == NULL) {
        listPtr->head = elemPtr;
    }
    ++listPtr->num;

    return elemPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_AppendListElem()
 *
 *  Creates a new list element containing the given value and returns
 *  a pointer to it.  The element is inserted in the list just after
 *  the specified element.
 * ------------------------------------------------------------------------
 */
Itclng_ListElem*
Itclng_AppendListElem(pos,val)
    Itclng_ListElem *pos;     /* insert just after this element */
    ClientData val;         /* value associated with new element */
{
    Itclng_List *listPtr;
    Itclng_ListElem *elemPtr;

    listPtr = pos->owner;
    assert(listPtr->validate == ITCLNG_VALID_LIST);
    assert(pos != NULL);

    elemPtr = Itclng_CreateListElem(listPtr);
    elemPtr->value = val;

    elemPtr->next = pos->next;
    if (elemPtr->next) {
        elemPtr->next->prev = elemPtr;
    }
    elemPtr->prev = pos;
    pos->next     = elemPtr;

    if (listPtr->tail == pos) {
        listPtr->tail = elemPtr;
    }
    if (listPtr->head == NULL) {
        listPtr->head = elemPtr;
    }
    ++listPtr->num;

    return elemPtr;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_SetListValue()
 *
 *  Modifies the value associated with a list element.
 * ------------------------------------------------------------------------
 */
void
Itclng_SetListValue(elemPtr,val)
    Itclng_ListElem *elemPtr; /* list element being modified */
    ClientData val;         /* new value associated with element */
{
    Itclng_List *listPtr = elemPtr->owner;
    assert(listPtr->validate == ITCLNG_VALID_LIST);
    assert(elemPtr != NULL);

    elemPtr->value = val;
}

#ifdef NOTDEF

/*
 * ========================================================================
 *  REFERENCE-COUNTED DATA
 *
 *  The following procedures manage generic reference-counted data.
 *  They are similar in spirit to the Tcl_Preserve/Tcl_Release
 *  procedures defined in the Tcl/Tk core.  But these procedures use
 *  a hash table instead of a linked list to maintain the references,
 *  so they scale better.  Also, the Tcl procedures have a bad behavior
 *  during the "exit" command.  Their exit handler shuts them down
 *  when other data is still being reference-counted and cleaned up.
 *
 * ------------------------------------------------------------------------
 *  Itcl_EventuallyFree()
 *
 *  Registers a piece of data so that it will be freed when no longer
 *  in use.  The data is registered with an initial usage count of "0".
 *  Future calls to Itcl_PreserveData() increase this usage count, and
 *  calls to Itcl_ReleaseData() decrease the count until it reaches
 *  zero and the data is freed.
 * ------------------------------------------------------------------------
 */
void
Itcl_EventuallyFree(
    ClientData cdata,          /* data to be freed when not in use */
    Tcl_FreeProc *fproc)       /* procedure called to free data */
{
    /*
     *  If the clientData value is NULL, do nothing.
     */
    if (cdata == NULL) {
        return;
    }
    Tcl_EventuallyFree(cdata, fproc);
    return;

}

/*
 * ------------------------------------------------------------------------
 *  Itcl_PreserveData()
 *
 *  Increases the usage count for a piece of data that will be freed
 *  later when no longer needed.  Each call to Itcl_PreserveData()
 *  puts one claim on a piece of data, and subsequent calls to
 *  Itcl_ReleaseData() remove those claims.  When Itcl_EventuallyFree()
 *  is called, and when the usage count reaches zero, the data is
 *  freed.
 * ------------------------------------------------------------------------
 */
void
Itcl_PreserveData(cdata)
    ClientData cdata;      /* data to be preserved */
{

    /*
     *  If the clientData value is NULL, do nothing.
     */
    if (cdata == NULL) {
        return;
    }
    Tcl_Preserve(cdata);
    return;
}

/*
 * ------------------------------------------------------------------------
 *  Itcl_ReleaseData()
 *
 *  Decreases the usage count for a piece of data that was registered
 *  previously via Itcl_PreserveData().  After Itcl_EventuallyFree()
 *  is called and the usage count reaches zero, the data is
 *  automatically freed.
 * ------------------------------------------------------------------------
 */
void
Itcl_ReleaseData(cdata)
    ClientData cdata;      /* data to be released */
{

    /*
     *  If the clientData value is NULL, do nothing.
     */
    if (cdata == NULL) {
        return;
    }
    Tcl_Release(cdata);
    return;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_SaveInterpState()
 *
 *  Takes a snapshot of the current result state of the interpreter.
 *  The snapshot can be restored at any point by Itcl_RestoreInterpState.
 *  So if you are in the middle of building a return result, you can
 *  snapshot the interpreter, execute a command that might generate an
 *  error, restore the snapshot, and continue building the result string.
 *
 *  Once a snapshot is saved, it must be restored by calling
 *  Itcl_RestoreInterpState, or discarded by calling
 *  Itcl_DiscardInterpState.  Otherwise, memory will be leaked.
 *
 *  Returns a token representing the state of the interpreter.
 * ------------------------------------------------------------------------
 */
Itcl_InterpState
Itcl_SaveInterpState(interp, status)
    Tcl_Interp* interp;     /* interpreter being modified */
    int status;             /* integer status code for current operation */
{
    return (Itcl_InterpState) Tcl_SaveInterpState(interp, status);
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_RestoreInterpState()
 *
 *  Restores the state of the interpreter to a snapshot taken by
 *  Itcl_SaveInterpState.  This affects variables such as "errorInfo"
 *  and "errorCode".  After this call, the token for the interpreter
 *  state is no longer valid.
 *
 *  Returns the status code that was pending at the time the state was
 *  captured.
 * ------------------------------------------------------------------------
 */
int
Itcl_RestoreInterpState(interp, state)
    Tcl_Interp* interp;       /* interpreter being modified */
    Itcl_InterpState state;   /* token representing interpreter state */
{
    return Tcl_RestoreInterpState(interp, (Tcl_InterpState)state);
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_DiscardInterpState()
 *
 *  Frees the memory associated with an interpreter snapshot taken by
 *  Itcl_SaveInterpState.  If the snapshot is not restored, this
 *  procedure must be called to discard it, or the memory will be lost.
 *  After this call, the token for the interpreter state is no longer
 *  valid.
 * ------------------------------------------------------------------------
 */
void
Itcl_DiscardInterpState(state)
    Itcl_InterpState state;  /* token representing interpreter state */
{
    Tcl_DiscardInterpState((Tcl_InterpState)state);
    return;
}
#endif

/*
 * ------------------------------------------------------------------------
 *  Itclng_Protection()
 *
 *  Used to query/set the protection level used when commands/variables
 *  are defined within a class.  The default protection level (when
 *  no public/protected/private command is active) is ITCL_DEFAULT_PROTECT.
 *  In the default case, new commands are treated as public, while new
 *  variables are treated as protected.
 *
 *  If the specified level is 0, then this procedure returns the
 *  current value without changing it.  Otherwise, it sets the current
 *  value to the specified protection level, and returns the previous
 *  value.
 * ------------------------------------------------------------------------
 */
int
Itclng_Protection(
    Tcl_Interp *interp,  /* interpreter being queried */
    int newLevel)        /* new protection level or 0 */
{
    int oldVal;
    ItclngObjectInfo *infoPtr;

    /*
     *  If a new level was specified, then set the protection level.
     *  In any case, return the protection level as it stands right now.
     */
    infoPtr = (ItclngObjectInfo*) Tcl_GetAssocData(interp, ITCLNG_INTERP_DATA,
        (Tcl_InterpDeleteProc**)NULL);

    assert(infoPtr != NULL);
    oldVal = infoPtr->protection;

    if (newLevel != 0) {
        assert(newLevel == ITCLNG_PUBLIC ||
            newLevel == ITCLNG_PROTECTED ||
            newLevel == ITCLNG_PRIVATE);
        infoPtr->protection = newLevel;
    }
    return oldVal;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_ParseNamespPath()
 *
 *  Parses a reference to a namespace element of the form:
 *
 *      namesp::namesp::namesp::element
 *
 *  Returns pointers to the head part ("namesp::namesp::namesp")
 *  and the tail part ("element").  If the head part is missing,
 *  a NULL pointer is returned and the rest of the string is taken
 *  as the tail.
 *
 *  Both head and tail point to locations within the given dynamic
 *  string buffer.  This buffer must be uninitialized when passed
 *  into this procedure, and it must be freed later on, when the
 *  strings are no longer needed.
 * ------------------------------------------------------------------------
 */
void
Itclng_ParseNamespPath(
    CONST char *name,    /* path name to class member */
    Tcl_DString *buffer, /* dynamic string buffer (uninitialized) */
    char **head,         /* returns "namesp::namesp::namesp" part */
    char **tail)         /* returns "element" part */
{
    register char *sep, *newname;

    Tcl_DStringInit(buffer);

    /*
     *  Copy the name into the buffer and parse it.  Look
     *  backward from the end of the string to the first '::'
     *  scope qualifier.
     */
    Tcl_DStringAppend(buffer, name, -1);
    newname = Tcl_DStringValue(buffer);

    for (sep=newname; *sep != '\0'; sep++)
        ;

    while (--sep > newname) {
        if (*sep == ':' && *(sep-1) == ':') {
            break;
        }
    }

    /*
     *  Found head/tail parts.  If there are extra :'s, keep backing
     *  up until the head is found.  This supports the Tcl namespace
     *  behavior, which allows names like "foo:::bar".
     */
    if (sep > newname) {
        *tail = sep+1;
        while (sep > newname && *(sep-1) == ':') {
            sep--;
        }
        *sep = '\0';
        *head = newname;
    } else {

        /*
         *  No :: separators--the whole name is treated as a tail.
         */
        *tail = newname;
        *head = NULL;
    }
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_CanAccess2()
 *
 *  Checks to see if a class member can be accessed from a particular
 *  namespace context.  Public things can always be accessed.  Protected
 *  things can be accessed if the "from" namespace appears in the
 *  inheritance hierarchy of the class namespace.  Private things
 *  can be accessed only if the "from" namespace is the same as the
 *  class that contains them.
 *
 *  Returns 1/0 indicating true/false.
 * ------------------------------------------------------------------------
 */
int
Itclng_CanAccess2(
    ItclngClass *iclsPtr,        /* class being tested */
    int protection,            /* protection level being tested */
    Tcl_Namespace* fromNsPtr)  /* namespace requesting access */
{
    ItclngClass* fromIclsPtr;
    Tcl_HashEntry *entry;

    /*
     *  If the protection level is "public" or "private", then the
     *  answer is known immediately.
     */
    if (protection == ITCLNG_PUBLIC) {
        return 1;
    } else {
        if (protection == ITCLNG_PRIVATE) {
            return (iclsPtr->nsPtr == fromNsPtr);
        }
    }

    /*
     *  If the protection level is "protected", then check the
     *  heritage of the namespace requesting access.  If cdefnPtr
     *  is in the heritage, then access is allowed.
     */
    assert (protection == ITCLNG_PROTECTED);

    if (Itclng_IsClassNamespace(fromNsPtr)) {
        fromIclsPtr =  (ItclngClass*)Tcl_ObjectGetMetadata(fromNsPtr->clientData,
	        iclsPtr->infoPtr->class_meta_type);
	if (fromIclsPtr == NULL) {
	   return 0;
	}
        entry = Tcl_FindHashEntry(&fromIclsPtr->heritage,
	        (char*)iclsPtr);

        if (entry) {
            return 1;
        }
    }
    return 0;
}

/*
 * ------------------------------------------------------------------------
 *  Itclng_CanAccess()
 *
 *  Checks to see if a class member can be accessed from a particular
 *  namespace context.  Public things can always be accessed.  Protected
 *  things can be accessed if the "from" namespace appears in the
 *  inheritance hierarchy of the class namespace.  Private things
 *  can be accessed only if the "from" namespace is the same as the
 *  class that contains them.
 *
 *  Returns 1/0 indicating true/false.
 * ------------------------------------------------------------------------
 */
int
Itclng_CanAccess(
    ItclngMemberFunc* imPtr,     /* class member being tested */
    Tcl_Namespace* fromNsPtr)  /* namespace requesting access */
{
    return Itclng_CanAccess2(imPtr->iclsPtr, imPtr->protection, fromNsPtr);
}


/*
 * ------------------------------------------------------------------------
 *  Itclng_CanAccessFunc()
 *
 *  Checks to see if a member function with the specified protection
 *  level can be accessed from a particular namespace context.  This
 *  follows the same rules enforced by Itcl_CanAccess, but adds one
 *  special case:  If the function is a protected method, and if the
 *  current context is a base class that has the same method, then
 *  access is allowed.
 *
 *  Returns 1/0 indicating true/false.
 * ------------------------------------------------------------------------
 */
int
Itclng_CanAccessFunc(
    ItclngMemberFunc* imPtr,     /* member function being tested */
    Tcl_Namespace* fromNsPtr)  /* namespace requesting access */
{
    ItclngClass *iclsPtr;
    ItclngClass *fromIclsPtr;
    ItclngMemberFunc *ovlfunc;
    Tcl_HashEntry *entry;

    /*
     *  Apply the usual rules first.
     */
    if (Itclng_CanAccess(imPtr, fromNsPtr)) {
        return 1;
    }

    /*
     *  As a last resort, see if the namespace is really a base
     *  class of the class containing the method.  Look for a
     *  method with the same name in the base class.  If there
     *  is one, then this method overrides it, and the base class
     *  has access.
     */
    if ((imPtr->flags & ITCLNG_COMMON) == 0 &&
            Itclng_IsClassNamespace(fromNsPtr)) {
        Tcl_HashEntry *hPtr;

        iclsPtr = imPtr->iclsPtr;
	hPtr = Tcl_FindHashEntry(&iclsPtr->infoPtr->namespaceClasses,
	        (char *)fromNsPtr);
	if (hPtr == NULL) {
	    return 0;
	}
        fromIclsPtr = Tcl_GetHashValue(hPtr);

        if (Tcl_FindHashEntry(&iclsPtr->heritage, (char*)fromIclsPtr)) {
            entry = Tcl_FindHashEntry(&fromIclsPtr->resolveCmds,
                Tcl_GetString(imPtr->namePtr));

            if (entry) {
                ovlfunc = (ItclngMemberFunc*)Tcl_GetHashValue(entry);
                if ((ovlfunc->flags & ITCLNG_COMMON) == 0 &&
                     ovlfunc->protection < ITCLNG_PRIVATE) {
                    return 1;
                }
            }
        }
    }
    return 0;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_DecodeScopedCommand()
 *
 *  Decodes a scoped command of the form:
 *
 *      namespace inscope <namesp> <command>
 *
 *  If the given string is not a scoped value, this procedure does
 *  nothing and returns TCL_OK.  If the string is a scoped value,
 *  then it is decoded, and the namespace, and the simple command
 *  string are returned as arguments; the simple command should
 *  be freed when no longer in use.  If anything goes wrong, this
 *  procedure returns TCL_ERROR, along with an error message in
 *  the interpreter.
 * ------------------------------------------------------------------------
 */
int
Itclng_DecodeScopedCommand(
    Tcl_Interp *interp,		/* current interpreter */
    CONST char *name,		/* string to be decoded */
    Tcl_Namespace **rNsPtr,	/* returns: namespace for scoped value */
    char **rCmdPtr)		/* returns: simple command word */
{
    Tcl_Namespace *nsPtr = NULL;
    char *cmdName;
    int len = strlen(name);
    CONST char *pos;
    int listc, result;
    CONST char **listv;

    cmdName = ckalloc((unsigned)strlen(name)+1);
    strcpy(cmdName, name);

    if ((*name == 'n') && (len > 17) && (strncmp(name, "namespace", 9) == 0)) {
	for (pos = (name + 9);  (*pos == ' ');  pos++) {
	    /* empty body: skip over spaces */
	}
	if ((*pos == 'i') && ((pos + 7) <= (name + len))
	        && (strncmp(pos, "inscope", 7) == 0)) {

            result = Tcl_SplitList(interp, (CONST84 char *)name, &listc,
		    &listv);
            if (result == TCL_OK) {
                if (listc != 4) {
                    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                        "malformed command \"", name, "\": should be \"",
                        "namespace inscope namesp command\"",
                        (char*)NULL);
                    result = TCL_ERROR;
                } else {
                    nsPtr = Tcl_FindNamespace(interp, listv[2],
                        (Tcl_Namespace*)NULL, TCL_LEAVE_ERR_MSG);

                    if (!nsPtr) {
                        result = TCL_ERROR;
                    } else {
			ckfree(cmdName);
                        cmdName = ckalloc((unsigned)(strlen(listv[3])+1));
                        strcpy(cmdName, listv[3]);
                    }
                }
            }
            ckfree((char*)listv);

            if (result != TCL_OK) {
                char msg[512];
                sprintf(msg, "\n    (while decoding scoped command \"%.400s\")", name);
                Tcl_AddObjErrorInfo(interp, msg, -1);
                return TCL_ERROR;
            }
	}
    }

    *rNsPtr = nsPtr;
    *rCmdPtr = cmdName;
    return TCL_OK;
}