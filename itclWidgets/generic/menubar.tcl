#
# Menubar widget
# ----------------------------------------------------------------------
# The Menubar command creates a new window (given by the pathName 
# argument) and makes it into a Pull down menu widget. Additional 
# options, described above may be specified on the command line or 
# in the option database to configure aspects of the Menubar such 
# as its colors and font. The Menubar command returns its pathName 
# argument. At the time this command is invoked, there must not exist 
# a window named pathName, but pathName's parent must exist.
# 
# A Menubar is a widget that simplifies the task of creating 
# menu hierarchies. It encapsulates a frame widget, as well 
# as menubuttons, menus, and menu entries. The Menubar allows 
# menus to be specified and refer enced in a more consistent 
# manner than using Tk to build menus directly. First, Menubar
# allows a menu tree to be expressed in a hierachical "language". 
# The Menubar accepts a menuButtons option that allows a list of 
# menubuttons to be added to the Menubar. In turn, each menubutton
# accepts a menu option that spec ifies a list of menu entries 
# to be added to the menubutton's menu (as well as an option 
# set for the menu).   Cascade entries in turn, accept a menu 
# option that specifies a list of menu entries to be added to 
# the cascade's menu (as well as an option set for the menu). In 
# this manner, a complete menu grammar can be expressed to the 
# Menubar. Additionally, the Menubar allows each component of 
# the Menubar system to be referenced by a simple componentPathName 
# syntax. Finally, the Menubar extends the option set of menu 
# entries to include the helpStr option used to implement status 
# bar help.
#
# WISH LIST:
#   This section lists possible future enhancements.
# 
# Author: Arnulf P. Wiedemann
# Copyright (c) 2008 for the reimplemented version
#
# see file license.terms in the top directory
#
# ----------------------------------------------------------------------
# This code is derived/reimplemented from the iwidgets package Menubar
# written by:
#  AUTHOR: Bill W. Scott
#  CURRENT MAINTAINER: Chad Smith --> csmith@adc.com or itclguy@yahoo.com
#    Copyright (c) 1995 DSC Technologies Corporation
# ----------------------------------------------------------------------
#
#   @(#) $Id: menubar.tcl,v 1.1.2.5 2009/01/05 19:30:47 wiede Exp $
# ======================================================================

#
# Use option database to override default resources.
#
option add *Menubar*Menu*tearOff       false    widgetDefault
option add *Menubar*Menubutton*relief  flat     widgetDefault
option add *Menubar*Menu*relief        raised   widgetDefault

namespace eval ::itcl::widgets {

#
# Provide a lowercase access method for the menubar class
#
proc ::itcl::widgets::menubar { args } {
    uplevel ::itcl::widgets::Menubar $args
}

::itcl::extendedclass Menubar {
    component itcl_hull
    component itcl_interior
    component menubar

    option [list -foreground foreground Foreground] -default Black
    option [list -activebackground activeBackground Foreground] -default "#ececec"
    option [list -activeborderwidth activeBorderWidth BorderWidth] -default 2 
    option [list -activeforeground activeForeground Background] -default black
    option [list -anchor anchor Anchor] -default center
    option [list -borderwidth borderWidth BorderWidth] -default 2
    option [list \
	    -disabledforeground disabledForeground DisabledForeground] -default #a3a3a3 
    option [list \
	    -font font Font] -default "-Adobe-Helvetica-Bold-R-Normal--*-120-*-*-*-*-*-*"
    option [list \
	    -highlightbackground highlightBackground HighlightBackground] -default #d9d9d9
    option [list -highlightcolor highlightColor HighlightColor] -default Black
    option [list \
	    -highlightthickness highlightThickness HighlightThickness] -default 0
    option [list -justify justify Justify] -default center
    option [list -padx padX Pad] -default 4p
    option [list -pady padY Pad] -default 3p
    option [list -wraplength wrapLength WrapLength] -default 0
    option [list -menubuttons menuButtons MenuButtons] -default {} -configuremethod configMenubuttons
    option [list -helpvariable helpVariable HelpVariable] -default {} -configuremethod configHelpvariable

    private variable _parseLevel 0        ;# The parse level depth
    private variable _callerLevel #0      ;# abs level of caller
    private variable _pathMap             ;# Array indexed by Menubar's path
                                          ;# naming, yields tk menu path
    private variable _entryIndex -1       ;# current entry help is displayed
                                          ;# for during help <motion> events
    private variable _tkMenuPath          ;# last tk menu being added to
    private variable _ourMenuPath         ;# our last valid path constructed.
    private variable _menuOption          ;# The -menu option
    private variable _helpString          ;# The -helpstr optio

    constructor {args} {}

    private method menubutton {menuName args}
    private method options {args}
    private method command {cmdName args}
    private method checkbutton {chkName args}
    private method radiobutton {radName args}
    private method separator {sepName args}
    private method cascade {casName args}
    private method _helpHandler {menuPath}
    private method _addMenuButton {buttonName args}
    private method _insertMenuButton {beforeMenuPath buttonName args}
    private method _makeMenuButton {buttonName args}
    private method _makeMenu \
	    {componentName widgetName menuPath menuEvalStr}
    private method _substEvalStr {evalStr}
    private method _deleteMenu {menuPath {menuPath2 {}}}
    private method _deleteAMenu {path}
    private method _addEntry {type path args}
    private method _addCascade {tkMenuPath path args}
    private method _insertEntry {beforeEntryPath type name args}
    private method _insertCascade {bfIndex tkMenuPath path args}
    private method _deleteEntry {entryPath {entryPath2 {}}}
    private method _configureMenu {path tkPath {option {}} args}
    private method _configureMenuOption {type path args}
    private method _configureMenuEntry {path index {option {}} args}
    private method _unsetPaths {parent}
    private method _entryPathToTkMenuPath {entryPath}
    private method _getTkIndex {tkMenuPath tkIndex}
    private method _getPdIndex {tkMenuPath tkIndex}
    private method _getMenuList {}
    private method _getEntryList {menu}
    private method _parsePath {path}
    private method _getSymbolicPath {parent segment}
    private method _getCallerLevel {}

    protected method configMenubuttons {option value}
    protected method configHelpvariable {option value}

    public method add {type path args}
    public method delete {args}
    public method index {path}
    public method insert {beforeComponent type name args}
    public method invoke {entryPath}
    public method menucget {args}
    public method menuconfigure {path args}
    public method path {args}
    public method type {path}
    public method yposition {entryPath}

}

# ------------------------------------------------------------------
#                           CONSTRUCTOR
# ------------------------------------------------------------------
::itcl::body Menubar::constructor {args} {
    set win [createhull frame $this -class [info class] -borderwidth 0]
    set itcl_interior $win
    #
    # Create the Menubar Frame that will hold the menus.
    #
    # might want to make -relief and -bd options with defaults
    setupcomponent menubar using frame $itcl_interior.menubar -relief raised -bd 2
    keepcomponentoption menubar -cursor -background -width -height
    pack $menubar -fill both -expand yes
    # Map our pathname to class to the actual menubar frame
    set _pathMap(.) $menubar
    if {[llength $args] > 0} {
        uplevel 0 configure $args
    }
    #
    # HACK HACK HACK
    # Tk expects some variables to be defined and due to some
    # unknown reason we confuse its normal ordering. So, if
    # the user creates a menubutton with no menu it will fail
    # when clicked on with a "Error: can't read $tkPriv(oldGrab):
    # no such element in array". So by setting it to null we
    # avoid this error.
    uplevel #0 "set tkPriv(oldGrab) {}"

}

# ------------------------------------------------------------------
#                           OPTIONS
# ------------------------------------------------------------------
# This first set of options are for configuring menus and/or menubuttons
# at the menu level.
#

# ------------------------------------------------------------------
# OPTION -menubuttons
#
# The menuButton option is a string which specifies the arrangement 
# of menubuttons on the Menubar frame. Each menubutton entry is 
# delimited by the newline character. Each entry is treated as 
# an add command to the Menubar. 
#
# ------------------------------------------------------------------
::itcl::body Menubar::configMenubuttons {option value} {
    if {$value ne {}} {
	# IF one exists already, delete the old one and create
	# a new one
	if {! [catch {_parsePath .0}]} {
	    delete .0 .last
	} 
	#
	# Determine the context level to evaluate the option string at
	#
	set _callerLevel [_getCallerLevel]
	#
	# Parse the option string in their scope, then execute it in
	# our scope.
	#
	incr _parseLevel
	_substEvalStr value
	if {[catch {uplevel 0 $value} msg]} {
	}
	# reset so that we know we aren't parsing in a scope currently.
	incr _parseLevel -1
    }
    set itcl_options($option) $value
}

# ------------------------------------------------------------------
# OPTION -helpvariable
#
# Specifies the global variable to update whenever the mouse is in 
# motion over a menu entry. This global variable is updated with the 
# current value of the active menu entry's helpStr. Other widgets 
# can "watch" this variable with the trace command, or as is the 
# case with entry or label widgets, they can set their textVariable 
# to the same global variable. This allows for a simple implementation 
# of a help status bar. Whenever the mouse leaves a menu entry, 
# the helpVariable is set to the empty string {}.
# ------------------------------------------------------------------
::itcl::body Menubar::configHelpvariable {option value} {
    if {"" != $value &&
	![string match ::* $value] &&
        ![string match @itcl* $value]} {
        set itcl_options(-helpvariable) "::$itcl_options(-helpvariable)"
    } else {
        set itcl_options($option) $value
    }
}


# -------------------------------------------------------------
#
# METHOD: add type path args
#
# Adds either a menu to the menu bar or a menu entry to a
# menu pane.
#
# If the type is one of  cascade,  checkbutton,  command,
# radiobutton,  or separator it adds a new entry to the bottom
# of the menu denoted by the menuPath prefix of componentPath-
# Name.  The  new entry's type is given by type. If additional
# arguments are present, they  specify  options  available  to
# component  type  Entry. See the man pages for menu(n) in the
# section on Entries. In addition all entries accept an  added
# option, helpStr:
#
#     -helpstr value
#
# Specifes the string to associate with  the  entry.
# When the mouse moves over the associated entry, the variable
# denoted by helpVariable is set. Another widget can  bind  to
# the helpVariable and thus display status help.
#
# If the type is menubutton, it adds a new  menubut-
# ton  to  the  menu bar. If additional arguments are present,
# they specify options available to component type MenuButton.
#
# If the type is menubutton  or  cascade,  the  menu
# option  is  available  in  addition to normal Tk options for
# these to types.
#
#      -menu menuSpec
#
# This is only valid for componentPathNames of  type
# menubutton  or  cascade. Specifes an option set and/or a set
# of entries to place on a menu and associate with  the  menu-
# button or cascade. The option keyword allows the menu widget
# to be configured. Each item in the menuSpec  is  treated  as
# add  commands  (each  with  the  possibility of having other
# -menu options). In this way a menu can be recursively built.
#
# The last segment of  componentPathName  cannot  be
# one  of  the  keywords last, menu, end. Additionally, it may
# not be a number. However the componentPathName may be refer-
# enced  in  this  manner  (see  discussion  of Component Path
# Names).
#
# -------------------------------------------------------------
::itcl::body Menubar::add {type path args} {
    if {![regexp \
            {^(menubutton|command|cascade|separator|radiobutton|checkbutton)$} \
            $type]} {
        error "bad type \"$type\": must be one of the following:\
            \"command\", \"checkbutton\", \"radiobutton\",\
            \"separator\", \"cascade\", or \"menubutton\""
    }
    regexp {[^.]+$} $path segName
    if {[regexp {^(menu|last|end|[0-9]+)$} $segName]} {
        error "bad name \"$segName\": user created component \
                path names may not end with \
		\"end\", \"last\", \"menu\", \
                or be an integer"
    }

    if {$type eq "menubutton"} {
        # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
        # add a menu
        # '''''''''''''''''''''''''''''''''''''''''''''''''''''
	# grab the last component name (the menu name)
	uplevel 0 _addMenuButton $segName $args
    } else {
	# ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
	# add an entry
	# '''''''''''''''''''''''''''''''''''''''''''''''''''''
	uplevel 0 _addEntry $type $path $args
    }
}


# -------------------------------------------------------------
#
# METHOD: delete entryPath ?entryPath2?
#
# If componentPathName is of component type MenuButton or
# Menu,  delete  operates  on menus. If componentPathName is of
# component type Entry, delete operates on menu entries.
#
# This  command  deletes  all  components  between   com-
# ponentPathName  and  componentPathName2  inclusive.  If com-
# ponentPathName2  is  omitted  then  it  defaults   to   com-
# ponentPathName. Returns an empty string.
#
# If componentPathName is of type Menubar, then all menus
# and  the menu bar frame will be destroyed. In this case com-
# ponentPathName2 is ignored.
#
# -------------------------------------------------------------
::itcl::body Menubar::delete {args} {

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Handle out of bounds in arg lengths
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {([llength $args] > 0) && ([llength $args] <= 2)} {

	# Path Conversions
	# '''''''''''''''''''''''''''''''''''''''''''''''''''''
	set path [_parsePath [lindex $args 0]]
	set pathOrIndex $_pathMap($path)
	if {[regexp {^[0-9]+$} $pathOrIndex]} {
	     # Menu Entry
	     # '''''''''''''''''''''''''''''''''''''''''''''''''''''
	    uplevel 0 "_deleteEntry $args"
	} else {
	    # Menu
	    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
	    uplevel 0 "_deleteMenu $args"
	}
    } else {
	error "wrong # args: should be \
		\"[string trimleft $this {:}] delete pathName ?pathName2?\""
    }
    return ""
}

# -------------------------------------------------------------
#
# METHOD: index path
#
# If componentPathName is of type menubutton or menu,  it
# returns  the  position of the menu/menubutton on the Menubar
# frame.
#
# If componentPathName is  of  type  command,  separator,
# radiobutton,  checkbutton,  or  cascade, it returns the menu
# widget's numerical index for the entry corresponding to com-
# ponentPathName. If path is not found or the Menubar frame is
# passed in, -1 is returned.
#
# -------------------------------------------------------------
::itcl::body Menubar::index { path } {

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Path conversions
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {[catch {set fullPath [_parsePath $path]}]} {
	return -1
    }
    if {[catch {set tkPathOrIndex $_pathMap($fullPath)}]} {
	return -1
    }

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # If integer, return the value, otherwise look up the menu position
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {[regexp {^[0-9]+$} $tkPathOrIndex]} {
	set index $tkPathOrIndex
    } else {
	set index [lsearch [_getMenuList] $fullPath]
    }

    return $index
}

# -------------------------------------------------------------
#
# METHOD: insert beforeComponent type name ?option value?
#
# Insert a new component named name before the  component
# specified by componentPathName.
#
# If componentPathName is of type MenuButton or Menu, the
# new  component  inserted  is of type Menu and given the name
# name. In this  case  valid  option  value  pairs  are  those
# accepted by menubuttons.
#
# If componentPathName is of type  Entry,  the  new  com-
# ponent inserted is of type Entry and given the name name. In
# this case valid option value pairs  are  those  accepted  by
# menu entries.
#
# name cannot be one of the  keywords  last,  menu,  end.
# dditionally,  it  may  not  be  a  number. However the com-
# ponentPathName may be referenced in this manner (see discus-
# sion of Component Path Names).
#
# Returns -1 if the menubar frame is passed in.
#
# -------------------------------------------------------------
::itcl::body Menubar::insert {beforeComponent type name args} {
     if {![regexp \
            {^(menubutton|command|cascade|separator|radiobutton|checkbutton)$} \
            $type]} {
        error "bad type \"$type\": must be one of the following:\
 		\"command\", \"checkbutton\", \"radiobutton\",\
 		\"separator\", \"cascade\", or \"menubutton\""
    }
    regexp {[^.]+$} $name segName
    if {[regexp {^(menu|last|end|[0-9]+)$} $segName]} {
	error "bad name \"$name\": user created component \
		path names may not end with \
		\"end\", \"last\", \"menu\", \
		or be an integer"
    }

    set beforeComponent [_parsePath $beforeComponent]

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Choose menu insertion or entry insertion
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {$type == "menubutton"} {
	uplevel 0 _insertMenuButton $beforeComponent $name $args
    } else {
	uplevel 0 _insertEntry $beforeComponent $type $name $args
    }
}


# -------------------------------------------------------------
#
# METHOD: invoke entryPath
#
# Invoke  the  action  of  the  menu  entry  denoted   by
# entryComponentPathName.  See  the sections on the individual
# entries in the menu(n) man pages. If the menu entry is  dis-
# abled  then  nothing  happens.  If  the  entry has a command
# associated with it  then  the  result  of  that  command  is
# returned  as the result of the invoke widget command. Other-
# wise the result is an empty string.
#
# If componentPathName is not a menu entry, an  error  is
# issued.
#
# -------------------------------------------------------------
::itcl::body Menubar::invoke { entryPath } {

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Path Conversions
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    set entryPath [_parsePath $entryPath]
    set index $_pathMap($entryPath)

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Error Processing
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    # first verify that beforeEntryPath is actually a path to
    # an entry and not to menu, menubutton, etc.
    if {![regexp {^[0-9]+$} $index]} {
	error "bad entry path: beforeEntryPath is not an entry"
    }

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Call invoke command
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    # get the tk menu path to call
    set tkMenuPath [_entryPathToTkMenuPath $entryPath]

    # call the menu's invoke command, adjusting index based on tearoff
    $tkMenuPath invoke [_getTkIndex $tkMenuPath $index]
}

# -------------------------------------------------------------
#
# METHOD: menucget componentPath option
#
# Returns the current value of the  configuration  option
# given  by  option.  The  component type of componentPathName
# determines the valid available options.
#
# -------------------------------------------------------------
::itcl::body Menubar::menucget { path opt } {
    return [lindex [menuconfigure $path $opt] 4]
}

# -------------------------------------------------------------
#
# METHOD: menuconfigure componentPath ?option? ?value option value...?
#
# Query or modify the configuration options of  the  sub-
# component  of the Menubar specified by componentPathName. If
# no option is specified, returns a list describing all of the
# available     options     for     componentPathName     (see
# Tk_ConfigureInfo for  information  on  the  format  of  this
# list).  If  option is specified with no value, then the com-
# mand returns a list describing the one  named  option  (this
# list  will  be identical to the corresponding sublist of the
# value returned if no option is specified). If  one  or  more
# option-value  pairs are specified, then the command modifies
# the given widget option(s) to have the  given  value(s);  in
# this case the command returns an empty string. The component
# type of componentPathName  determines  the  valid  available
# options.
#
# -------------------------------------------------------------
::itcl::body Menubar::menuconfigure { path args } {

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Path Conversions
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    set path [_parsePath $path]
    set tkPathOrIndex $_pathMap($path)

    # Case: Menu entry being configured
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {[regexp {^[0-9]+$} $tkPathOrIndex]} {
	uplevel 0 "_configureMenuEntry $path $tkPathOrIndex $args"

	# Case: Menu (button and pane) being configured.
	# '''''''''''''''''''''''''''''''''''''''''''''''''''''
    } else {
	uplevel 0 _configureMenu $path $tkPathOrIndex $args
    }
}

# -------------------------------------------------------------
#
# METHOD: path
#
# SYNOPIS: path ?<mode>? <pattern>
#
# Returns a fully formed component path that matches pat-
# tern.  If no match is found it returns -1. The mode argument
# indicates how the search is to be  matched  against  pattern
# and it must have one of the following values:
#
#     -glob     Pattern is a glob-style pattern which is
#       matched  against each component path using the same rules as
#       the string match command.
#
#     -regexp   Pattern is treated as a regular  expression  
#       and matched against each component path using the same
#       rules as the regexp command.
#
# The default mode is -glob.
#
# -------------------------------------------------------------
::itcl::body Menubar::path { args } {

    set len [llength $args]
    if {$len < 1 || $len > 2} {
	error "wrong # args: should be \
		\"$itcl_hull path ?mode?> <pattern>\""
    }

    set pathList [array names _pathMap]

    set len [llength $args]
    switch -- $len {
    1 {
        # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
        # Case: no search modes given
        # '''''''''''''''''''''''''''''''''''''''''''''''''''''
        set pattern [lindex $args 0]
        set found [lindex $pathList [lsearch -glob $pathList $pattern]]
      } 
    2 {
        # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
        # Case: search modes present (-glob, -regexp)
        # '''''''''''''''''''''''''''''''''''''''''''''''''''''
        set options [lindex $args 0]
        set pattern [lindex $args 1]
        set found \
	        [lindex $pathList [lsearch $options $pathList $pattern]]
      }
    default {
        # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
        # Case: wrong # arguments
        # '''''''''''''''''''''''''''''''''''''''''''''''''''''
        error "wrong # args: \
	    should be \"$itcl_hull path ?-glob? ?-regexp? pattern\""
      }
    }
    return $found
}

# -------------------------------------------------------------
#
# METHOD: type path
#
# Returns the type of the component  given  by  entryCom-
# ponentPathName.  For menu entries, this is the type argument
# passed to the add/insert widget command when the  entry  was
# created, such as command or separator. Othewise it is either
# a menubutton or a menu.
#
# -------------------------------------------------------------
::itcl::body Menubar::type { path } {

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Path Conversions
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    set path [_parsePath $path]

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Error Handling: does the path exist?
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {[catch {set index $_pathMap($path)}]} {
	error "bad path \"$path\""
    }

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # ENTRY, Ask TK for type
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    if {[regexp {^[0-9]+$} $index]} {
	# get the menu path from the entry path name
	set tkMenuPath [_entryPathToTkMenuPath $path]

	# call the menu's type command, adjusting index based on tearoff
	set type [$tkMenuPath type [_getTkIndex $tkMenuPath $index]]
	# ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
	# MENUBUTTON, MENU, or FRAME
	# '''''''''''''''''''''''''''''''''''''''''''''''''''''
    } else {
	# should not happen, but have a path that is not a valid window.
	if {[catch {set className [winfo class $_pathMap($path)]}]} {
	    error "serious error: \"$path\" is not a valid window"
	}
	# get the classname, look it up, get index, us it to look up type
	set type [lindex {frame menubutton menu} \
		[lsearch { Frame Menubutton Menu } $className] \
		]
    }
    return $type
}

# -------------------------------------------------------------
#
# METHOD: yposition entryPath
#
# Returns a decimal string giving the y-coordinate within
# the  menu window of the topmost pixel in the entry specified
# by componentPathName. If the  componentPathName  is  not  an
# entry, an error is issued.
#
# -------------------------------------------------------------
::itcl::body Menubar::yposition { entryPath } {

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Path Conversions
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    set entryPath [_parsePath $entryPath]
    set index $_pathMap($entryPath)

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Error Handling
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    # first verify that entryPath is actually a path to
    # an entry and not to menu, menubutton, etc.
    if {![regexp {^[0-9]+$} $index]} {
	error "bad value: entryPath is not an entry"
    }

    # ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Call yposition command
    # '''''''''''''''''''''''''''''''''''''''''''''''''''''
    # get the menu path from the entry path name
    set tkMenuPath [_entryPathToTkMenuPath $entryPath]

    # call the menu's yposition command, adjusting index based on tearoff
    return [$tkMenuPath yposition [_getTkIndex $tkMenuPath $index]]

}

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# PARSING METHODS
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# -------------------------------------------------------------
#
# PARSING METHOD: menubutton
#
# This method is invoked via an evaluation of the -menubuttons
# option for the Menubar.
#
# It adds a new menubutton and processes any -menu options
# for creating entries on the menu pane associated with the 
# menubutton
# -------------------------------------------------------------
::itcl::body Menubar::menubutton {menuName args} {
    uplevel 0 "add menubutton .$menuName $args"
}

# -------------------------------------------------------------
#
# PARSING METHOD: options
#
# This method is invoked via an evaluation of the -menu
# option for menubutton commands.
#
# It configures the current menu ($_ourMenuPath) with the options
# that follow (args)
#
# -------------------------------------------------------------
::itcl::body Menubar::options { args } {
    uplevel 0 "$_tkMenuPath configure $args"
}


# -------------------------------------------------------------
#
# PARSING METHOD: command
#
# This method is invoked via an evaluation of the -menu
# option for menubutton commands.
#
# It adds a new command entry to the current menu, $_ourMenuPath
# naming it $cmdName. Since this is the most common case when
# creating menus, streamline it by duplicating some code from
# the add{} method.
#
# -------------------------------------------------------------
::itcl::body Menubar::command { cmdName args } {
    set path $_ourMenuPath.$cmdName
    # error checking
    regsub {.*[.]} $path "" segName
    if {[regexp {^(menu|last|end|[0-9]+)$} $segName]} {
 	error "bad name \"$segName\": user created component \
 		path names may not end with \
 		\"end\", \"last\", \"menu\", \
 		or be an integer"
    }
    uplevel 0 _addEntry command $path $args
}

# -------------------------------------------------------------
#
# PARSING METHOD: checkbutton
#
# This method is invoked via an evaluation of the -menu
# option for menubutton/cascade commands.
#
# It adds a new checkbutton entry to the current menu, $_ourMenuPath
# naming it $chkName.
#
# -------------------------------------------------------------
::itcl::body Menubar::checkbutton { chkName args } {
    uplevel 0 "add checkbutton $_ourMenuPath.$chkName $args"
}

# -------------------------------------------------------------
#
# PARSING METHOD: radiobutton
#
# This method is invoked via an evaluation of the -menu
# option for menubutton/cascade commands.
#
# It adds a new radiobutton entry to the current menu, $_ourMenuPath
# naming it $radName.
#
# -------------------------------------------------------------
::itcl::body Menubar::radiobutton { radName args } {
    uplevel 0 "add radiobutton $_ourMenuPath.$radName $args"
}

# -------------------------------------------------------------
#
# PARSING METHOD: separator
#
# This method is invoked via an evaluation of the -menu
# option for menubutton/cascade commands.
#
# It adds a new separator entry to the current menu, $_ourMenuPath
# naming it $sepName.
#
# -------------------------------------------------------------
::itcl::body Menubar::separator {sepName args} {
    uplevel 0 $_tkMenuPath add separator
    set _pathMap($_ourMenuPath.$sepName) [_getPdIndex $_tkMenuPath end]
}

# -------------------------------------------------------------
#
# PARSING METHOD: cascade
#
# This method is invoked via an evaluation of the -menu
# option for menubutton/cascade commands.
#
# It adds a new cascade entry to the current menu, $_ourMenuPath
# naming it $casName. It processes the -menu option if present,
# adding a new menu pane and its associated entries found.
#
# -------------------------------------------------------------
::itcl::body Menubar::cascade { casName args } {
    
    # Save the current menu we are adding to, cascade can change
    # the current menu through -menu options.
    set saveOMP $_ourMenuPath
    set saveTKP $_tkMenuPath

    uplevel 0 "add cascade $_ourMenuPath.$casName $args"

    # Restore the saved menu states so that the next entries of
    # the -menu/-menubuttons we are processing will be at correct level.
    set _ourMenuPath $saveOMP
    set _tkMenuPath $saveTKP
}

# ... A P I   S U P P O R T   M E T H O D S...

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# MENU ADD, INSERT, DELETE
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# -------------------------------------------------------------
#
# PRIVATE METHOD: _addMenuButton
#
# Makes a new menubutton & associated -menu, pack appended
#
# -------------------------------------------------------------
::itcl::body Menubar::_addMenuButton {buttonName args} {
    set realButtonName [uplevel 0 _makeMenuButton $buttonName $args]
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Pack at end, adjust for help buttonName
    # ''''''''''''''''''''''''''''''''''
    if {$buttonName eq "help"} {
	pack $realButtonName -side right
    } else {
	pack $realButtonName -side left
    }
    return $realButtonName
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _insertMenuButton
#
# inserts a menubutton named $buttonName on a menu bar before 
# another menubutton specified by $beforeMenuPath
#
# -------------------------------------------------------------
::itcl::body Menubar::_insertMenuButton {beforeMenuPath buttonName args} {
    uplevel 0 "_makeMenuButton $buttonName $args"
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Pack before the $beforeMenuPath
    # ''''''''''''''''''''''''''''''''
    set beforeTkMenu $_pathMap($beforeMenuPath)
    regsub {[.]menu$} $beforeTkMenu "" beforeTkMenu
    pack $menubar.$buttonName \
	    -side left \
	    -before $beforeTkMenu

    # should make a utility function for the next lines !!!
    set my_prefix ""
    regsub -all {[.]} $menubar {_} my_prefix
    if {[string length $my_prefix] > 0} {
        append my_prefix "_"
    }
    set myButtonName ${my_prefix}$buttonName

    return [set $myButtonName]
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _makeMenuButton
#
# creates a menubutton named buttonName on the menubar with args.
# The -menu option if present will trigger attaching a menu pane.
#
# -------------------------------------------------------------
::itcl::body Menubar::_makeMenuButton {buttonName args} {
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Capture the -menu option if present
    # '''''''''''''''''''''''''''''''''''
    array set temp $args
    if {[::info exists temp(-menu)]} {
	# We only keep this in case of menuconfigure or menucget
	set _menuOption(.$buttonName) $temp(-menu)
	set menuEvalStr $temp(-menu)
    } else {
	set menuEvalStr {}
    }

    # attach the actual menu widget to the menubutton's arg list
    set temp(-menu) $menubar.$buttonName.menu
    set args [array get temp]
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Create menubutton component
    # ''''''''''''''''''''''''''''''''
    set my_prefix ""
    regsub -all {[.]} $menubar {_} my_prefix
    if {[string length $my_prefix] > 0} {
        append my_prefix "_"
    }
    if {![::info exists ${my_prefix}$buttonName]} {
        ::itcl::addcomponent $this ${my_prefix}$buttonName
    }
    setupcomponent ${my_prefix}$buttonName using ::menubutton $menubar.$buttonName {*}$args
    keepcomponentoption ${my_prefix}$buttonName \
                -activebackground \
                -activeforeground \
                -anchor \
                -background \
                -borderwidth \
                -cursor \
                -disabledforeground \
                -font \
                -foreground \
                -highlightbackground \
                -highlightcolor \
                -highlightthickness \
                -justify \
                -padx \
                -pady \
                -wraplength
    set _pathMap(.$buttonName) [set ${my_prefix}$buttonName]
    _makeMenu \
	    $buttonName-menu \
	    [set ${my_prefix}$buttonName].menu \
	    .$buttonName \
	    $menuEvalStr
    return [set ${my_prefix}$buttonName]
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _makeMenu
#
# Creates a menu.
# It then evaluates the $menuEvalStr to create entries on the menu.
#
# Assumes the existence of $$buttonName
#
# -------------------------------------------------------------
::itcl::body Menubar::_makeMenu {componentName widgetName menuPath menuEvalStr} {
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Create menu component
    # ''''''''''''''''''''''''''''''''
    if {![::info exists $componentName]} {
        ::itcl::addcomponent $this $componentName
    }
    setupcomponent $componentName using ::menu $widgetName
    keepcomponentoption $componentName \
		-activebackground \
		-activeborderwidth \
		-activeforeground \
		-background \
		-borderwidth \
		-cursor \
		-disabledforeground \
		-font \
		-foreground 
    set _pathMap($menuPath.menu) [set $componentName]
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Attach help handler to this menu
    # ''''''''''''''''''''''''''''''''
    bind [set $componentName] <<MenuSelect>> \
	    [itcl::code $this _helpHandler $menuPath.menu]
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Handle -menu
    #'''''''''''''''''''''''''''''''''
    set _ourMenuPath $menuPath
    set _tkMenuPath [set $componentName]

    #
    # A zero parseLevel says we are at the top of the parse tree,
    # so get the context scope level and do a subst for the menuEvalStr.
    #
    if {$_parseLevel == 0} {
        set _callerLevel [_getCallerLevel]
    }
    #
    # bump up the parse level, so if we get called via the 'eval $menuEvalStr'
    # we know to skip the above steps...
    #
    incr _parseLevel
    uplevel 0 $menuEvalStr
    #
    # leaving, so done with this parse level, so bump it back down 
    #
    incr _parseLevel -1
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _substEvalStr
#
# This performs the substitution and evaluation of $ [], \ found
# in the -menubutton/-menus options
#
# -------------------------------------------------------------
::itcl::body Menubar::_substEvalStr { evalStr } {
    upvar $evalStr evalStrRef

    set evalStrRef [uplevel $_callerLevel [list subst $evalStrRef]]
}


# -------------------------------------------------------------
#
# PRIVATE METHOD: _deleteMenu
#
# _deleteMenu menuPath ?menuPath2?
#
# deletes menuPath or from menuPath to menuPath2
#
# Menu paths may be formed in one of two ways
#	.MENUBAR.menuName  where menuName is the name of the menu
#	.MENUBAR.menuName.menu  where menuName is the name of the menu
#
# The basic rule is '.menu' is not needed.
# -------------------------------------------------------------
::itcl::body Menubar::_deleteMenu {menuPath {menuPath2 {}}} {
    if {$menuPath2 eq ""} {
	# get a corrected path (subst for number, last, end)
	set path [_parsePath $menuPath]
	_deleteAMenu $path
    } else {
	# gets the list of menus in interface order
	set menuList [_getMenuList]
	# ... get the start menu and the last menu ...
	# get a corrected path (subst for number, last, end)
	set menuStartPath [_parsePath $menuPath]
	regsub {[.]menu$} $menuStartPath "" menuStartPath
	set menuEndPath [_parsePath $menuPath2]
	regsub {[.]menu$} $menuEndPath "" menuEndPath
	# get the menu position (0 based) of the start and end menus.
	set start [lsearch -exact $menuList $menuStartPath]
	if {$start == -1} {
	    error "bad menu path \"$menuStartPath\": \
		    should be one of $menuList"
	}
	set end [lsearch -exact $menuList $menuEndPath]
	if { $end == -1 } {
	    error "bad menu path \"$menuEndPath\": \
		    should be one of $menuList"
	}
	# now create the list from this range of menus
	set delList [lrange $menuList $start $end]
	# walk thru them deleting each menu.
	# this list has no .menu on the end.
	foreach m $delList {
	    _deleteAMenu $m.menu
	}
    }
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _deleteAMenu
#
# _deleteMenu menuPath 
#
# deletes a single Menu (menubutton and menu pane with entries)
#
# -------------------------------------------------------------
::itcl::body Menubar::_deleteAMenu {path} {
    # We will normalize the path to not include the '.menu' if
    # it is on the path already.
    regsub {[.]menu$} $path "" menuButtonPath
    regsub {.*[.]} $menuButtonPath "" buttonName
    set myButtonName [set _pathMap(.$buttonName)]
    regsub -all {[.]} $myButtonName {_} myButtonName
    # Loop through and destroy any cascades, etc on menu.
    set entryList [_getEntryList $menuButtonPath]
    foreach entry $entryList {
	_deleteEntry $entry
    }
    # Delete the menubutton and menu components...
    destroy [set $myButtonName]-menu
    destroy [set $myButtonName]

    # This is because of some itcl bug that doesn't delete
    # the component on the destroy in some cases...
#    catch {delete [set $buttonName]-menu}
#    catch {delete [set $buttonName]}
    
    # unset our paths
    _unsetPaths $menuButtonPath

}

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# ENTRY ADD, INSERT, DELETE
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

# -------------------------------------------------------------
#
# PRIVATE METHOD: _addEntry
#
# Adds an entry to menu.
#
# -------------------------------------------------------------
::itcl::body Menubar::_addEntry { type path args } {
    # Error Checking
    # ''''''''''''''
    # the path should not end with '.menu'
    # Not needed -- already checked by add{}
    # if { [regexp {[.]menu$} $path] } {
    #  error "bad entry path: \"$path\". \
    #    	The name \"menu\" is reserved for menu panes"
    # }

    # get the tkMenuPath
    set tkMenuPath [_entryPathToTkMenuPath $path]
    if {$tkMenuPath eq ""} {
	error "bad entry path: \"$path\". The menu path prefix is not valid"
    }
    # get the -helpstr option if present
    array set temp $args
    if {[::info exists temp(-helpstr)]} {
	set helpStr $temp(-helpstr)
	unset temp(-helpstr)
    } else {
	set helpStr {}
    }
    set args [array get temp]
    # Handle CASCADE
    # ''''''''''''''
    # if this is a cascade go ahead and add in the menu...
    if {$type == "cascade"} {
	uplevel 0 [list _addCascade $tkMenuPath $path] $args
	# Handle Non-CASCADE
	# ''''''''''''''''''
    } else {
	# add the entry if one doesn't already exist with the same
	# command name
	if  {[::info exists _pathMap($path)]} {
	  set cmdname [lindex [split $path .] end]
	  error "Cannot add $type \"$cmdname\". A menu item already\
	    exists with this name."
	}
	uplevel 0 [list $tkMenuPath add $type] $args
	set _pathMap($path) [_getPdIndex $tkMenuPath end]
    }
    # Remember the help string
    set _helpString($path) $helpStr
    return $_pathMap($path)
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _addCascade
#
# Creates a cascade button.  Handles the -menu option
#
# -------------------------------------------------------------
::itcl::body Menubar::_addCascade { tkMenuPath path args } {
    # get the cascade name from our path
    regsub {.*[.]} $path "" cascadeName
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Capture the -menu option if present
    # '''''''''''''''''''''''''''''''''''
    array set temp $args
    if {[::info exists temp(-menu)]} {
	set menuEvalStr $temp(-menu)
    } else {
	set menuEvalStr {}
    }
    # attach the menu pane
    set temp(-menu) $tkMenuPath.$cascadeName
    set args [array get temp]
    # Create the cascade entry
    uplevel 0 $tkMenuPath add cascade $args
    # Keep the -menu string in case of menuconfigure or menucget
    if {$menuEvalStr ne ""} {
	set _menuOption($path) $menuEvalStr
    }
    # update our pathmap
    set _pathMap($path) [_getPdIndex $tkMenuPath end]
    _makeMenu \
	    $cascadeName-menu \
	    $tkMenuPath.$cascadeName \
	    $path \
	    $menuEvalStr
    #return [set $cascadeName]
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _insertEntry
#
# inserts an entry on a menu before entry given by beforeEntryPath.
# The added entry is of type TYPE and its name is NAME. ARGS are
# passed for customization of the entry.
#
# -------------------------------------------------------------
::itcl::body Menubar::_insertEntry {beforeEntryPath type name args} {
    # convert entryPath to an index value
    set bfIndex $_pathMap($beforeEntryPath)
    # first verify that beforeEntryPath is actually a path to
    # an entry and not to menu, menubutton, etc.
    if {![regexp {^[0-9]+$} $bfIndex]} {
	error "bad entry path: $beforeEntryPath is not an entry"
    }

    # get the menu path from the entry path name
    regsub {[.][^.]*$} $beforeEntryPath "" menuPathPrefix
    set tkMenuPath $_pathMap($menuPathPrefix.menu)

    # If this entry already exists in the path map, throw an error.
    if {[::info exists _pathMap($menuPathPrefix.$name)]} {
      error "Cannot insert $type \"$name\". A menu item already\
	exists with this name."
    }
    # INDEX is zero based at this point.
    # ENTRIES is a zero based list...
    set entries [_getEntryList $menuPathPrefix]
    # 
    # Adjust the entries after the inserted item, to have
    # the correct index numbers. Note, we stay zero based 
    # even though tk flips back and forth depending on tearoffs.
    #
    for {set i $bfIndex} {$i < [llength $entries]} {incr i} {
	# path==entry path in numerical order
	set path [lindex $entries $i]
	# add one to each entry after the inserted one.
	set _pathMap($path) [expr {$i + 1}]
    }
    # get the -helpstr option if present
    array set temp $args
    if {[::info exists temp(-helpstr)]} {
	set helpStr $temp(-helpstr)
	unset temp(-helpstr)
    } else {
	set helpStr {}
    }
    set args [array get temp]
    set path $menuPathPrefix.$name
    # Handle CASCADE
    # ''''''''''''''
    # if this is a cascade go ahead and add in the menu...
    if {[string match cascade $type]} {
	if {[catch {eval "_insertCascade \
		$bfIndex $tkMenuPath $path $args"} errMsg]} {
	    for {set i $bfIndex} {$i < [llength $entries]} {incr i} {
		# path==entry path in numerical order
		set path [lindex $entries $i]
		# sub the one we added earlier.
		set _pathMap($path) [expr {$_pathMap($path) - 1}]
		# @@ delete $hs
	    }
	    error $errMsg
	}
	# Handle Entry
	# ''''''''''''''
    } else {
	# give us a zero or 1-based index based on tear-off menu status
	# invoke the menu's insert command
	if {[catch {eval "$tkMenuPath insert \
		[_getTkIndex $tkMenuPath $bfIndex] $type $args"} errMsg]} {
	    for {set i $bfIndex} {$i < [llength $entries]} {incr i} {
		# path==entry path in numerical order
		set path [lindex $entries $i]
		# sub the one we added earlier.
		set _pathMap($path) [expr {$_pathMap($path) - 1}]
		# @@ delete $hs
	    }
	    error $errMsg
	}
	# add the helpstr option to our options list (attach to entry)
	set _helpString($path) $helpStr
	# Insert the new entry path into pathmap giving it an index value
	set _pathMap($menuPathPrefix.$name) $bfIndex
    }
    return [_getTkIndex $tkMenuPath $bfIndex]
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _insertCascade
#
# Creates a cascade button.  Handles the -menu option
#
# -------------------------------------------------------------
::itcl::body Menubar::_insertCascade { bfIndex tkMenuPath path args } {
    # get the cascade name from our path
    regsub {.*[.]} $path "" cascadeName
    #,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
    # Capture the -menu option if present
    # '''''''''''''''''''''''''''''''''''
    array set temp $args
    if {[::info exists temp(-menu)]} {
	# Keep the -menu string in case of menuconfigure or menucget
	set _menuOption($path) $temp(-menu)
	set menuEvalStr $temp(-menu)
    } else {
	set menuEvalStr {}
    }
    # attach the menu pane
    set temp(-menu) $tkMenuPath.$cascadeName
    set args [array get temp]
    # give us a zero or 1-based index based on tear-off menu status
    # invoke the menu's insert command
    uplevel 0 "$tkMenuPath insert \
	    [_getTkIndex $tkMenuPath $bfIndex] cascade $args"
    # Insert the new entry path into pathmap giving it an index value
    set _pathMap($path) $bfIndex
    _makeMenu \
	    $cascadeName-menu \
	    $tkMenuPath.$cascadeName \
	    $path \
	    $menuEvalStr
    #return [set $cascadeName]
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _deleteEntry
#
# _deleteEntry entryPath ?entryPath2?
#
#   either
# deletes the entry entryPath
#   or
# deletes the entries from entryPath to entryPath2 
#
# -------------------------------------------------------------
::itcl::body Menubar::_deleteEntry {entryPath {entryPath2 {}}} {

    if {$entryPath2 eq ""} {
	# get a corrected path (subst for number, last, end)
	set path [_parsePath $entryPath]
	set entryIndex $_pathMap($path)
	if {$entryIndex == -1} {
	    error "bad value for pathName: \
		    $entryPath in call to delet"
	}
	# get the type, if cascade, we will want to delete menu
	set type [type $path]
	# ... munge up the menu name ...
	# the tkMenuPath is looked up with the .menu added to lookup
	# strip off the entry component
	regsub {[.][^.]*$} $path "" menuPath
	set tkMenuPath $_pathMap($menuPath.menu)
	# get the ordered entry list
	set entries [_getEntryList $menuPath]
	# ... Fix up path entry indices ...
	# delete the path from the map
	unset _pathMap([lindex $entries $entryIndex])
	# Subtract off 1 for each entry below the deleted one.
	for {set i [expr {$entryIndex + 1}]} \
		{$i < [llength $entries]} \
		{incr i} {
	    set epath [lindex $entries $i]
	    incr _pathMap($epath) -1
	}
	# ... Delete the menu entry widget ...
	# delete the menu entry, ajusting index for TK
	$tkMenuPath delete [_getTkIndex $tkMenuPath $entryIndex]
	if {$type eq "cascade"} {
	    regsub {.*[.]} $path "" cascadeName
	    destroy [set $cascadeName-menu]
	    # This is because of some itcl bug that doesn't delete
	    # the component on the destroy in some cases...
	    catch {delete $cascadeName-menu}
	    _unsetPaths $path
	}
    } else { 
	# get a corrected path (subst for number, last, end)
	set path1 [_parsePath $entryPath]
	set path2 [_parsePath $entryPath2]
	set fromEntryIndex $_pathMap($path1)
	if {$fromEntryIndex == -1} {
	    error "bad value for entryPath1: \
		    $entryPath in call to delet"
	}
	set toEntryIndex $_pathMap($path2)
	if {$toEntryIndex == -1} {
	    error "bad value for entryPath2: \
		    $entryPath2 in call to delet"
	}
	# ... munge up the menu name ...
	# the tkMenuPath is looked up with the .menu added to lookup
	# strip off the entry component
	regsub {[.][^.]*$} $path1 "" menuPath
	set tkMenuPath $_pathMap($menuPath.menu)
	# get the ordered entry list
	set entries [_getEntryList $menuPath]
	# ... Fix up path entry indices ...
	# delete the range from the pathMap list
	for {set i $fromEntryIndex} {$i <= $toEntryIndex} {incr i} {
	    unset _pathMap([lindex $entries $i])
	}
	# Subtract off 1 for each entry below the deleted range.
	# Loop from one below the bottom delete entry to end list
	for {set i [expr {$toEntryIndex + 1}]} \
		{$i < [llength $entries]} \
		{incr i} {
	    # take this path and sets its index back by size of
	    # deleted range.
	    set path [lindex $entries $i]
	    set _pathMap($path) \
		    [expr {$_pathMap($path) - \
		    (($toEntryIndex - $fromEntryIndex) + 1)}]
	}
	# ... Delete the menu entry widget ...
	# delete the menu entry, ajusting index for TK
	$tkMenuPath delete \
		[_getTkIndex $tkMenuPath $fromEntryIndex] \
		[_getTkIndex $tkMenuPath $toEntryIndex]
    }
}

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# CONFIGURATION SUPPORT
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# -------------------------------------------------------------
#
# PRIVATE METHOD: _configureMenu
#
# This configures a menu. A menu is a true tk widget, thus we
# pass the tkPath variable. This path may point to either a 
# menu button (does not end with the name 'menu', or a menu
# which ends with the name 'menu'
#
# path : our Menubar path name to this menu button or menu pane.
#        if we end with the name '.menu' then it is a menu pane.
# tkPath : the path to the corresponding Tk menubutton or menu.
# args   : the args for configuration
#
# -------------------------------------------------------------
::itcl::body Menubar::_configureMenu {path tkPath {option {}} args} {
    set class [winfo class $tkPath]
    if {$option eq ""} {
	# No arguments: return all options
	set configList [$tkPath configure]
	if {[info exists _menuOption($path)]} {
	    lappend configList [list -menu menu Menu {} $_menuOption($path)]
	} else {
	    lappend configList [list -menu menu Menu {} {}]
	}
	if {[info exists _helpString($path)]} {
	    lappend configList [list -helpstr helpStr HelpStr {} \
		    $_helpString($path)]
	} else {
	    lappend configList [list -helpstr helpStr HelpStr {} {}]
	}
	return $configList
    } elseif {$args eq "" } {
	if {$option eq "-menu"} {
	    if {[info exists _menuOption($path)]} {
		return [list -menu menu Menu {} $_menuOption($path)]
	    } else {
		return [list -menu menu Menu {} {}]
	    }
	} elseif {$option eq "-helpstr"} {
	    if {[info exists _helpString($path)]} {
		return [list -helpstr helpStr HelpStr {} $_helpString($path)]
	    } else {
		return [list -helpstr helpStr HelpStr {} {}]
	    }
	} else {
	    # ... OTHERWISE, let Tk get it.
	    return [$tkPath configure $option]
	}
    } else {
	set args [concat $option $args]
	# If this is a menubutton, and has -menu option, process it
	if {$class eq "Menubutton" && [regexp -- {-menu} $args]} {
	    eval _configureMenuOption menubutton $path $args
	} else {
	    eval $tkPath configure $args
	}
	return ""
    }
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _configureMenuOption
#
# Allows for configuration of the -menu option on
# menubuttons and cascades
#
# find out if we are the last menu, or are before one.
# delete the old menu.
# if we are the last, then add us back at the end
# if we are before another menu, get the beforePath
#
# -------------------------------------------------------------
::itcl::body Menubar::_configureMenuOption {type path args} {
    regsub {[.][^.]*$} $path "" pathPrefix
    if {$type eq "menubutton"} {
	set menuList [_getMenuList]
	set pos [lsearch $menuList $path]
	if {$pos == ([llength $menuList] - 1)} {
	    set insert false
	} else {
	    set insert true
	}
    } elseif {$type eq "cascade"} {
	set lastEntryPath [_parsePath $pathPrefix.last]
	if {$lastEntryPath eq $path} {
	    set insert false
	} else {
	    set insert true
	}
	set pos [index $path]
    }
    uplevel 0 "delete $pathPrefix.$pos"
    if { $insert } {
	# get name from path...
	regsub {.*[.]} $path "" name
	uplevel 0 insert $pathPrefix.$pos $type \
		$name $args
    } else {
	uplevel 0 add $type $path $args
    }
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _configureMenuEntry
#
# This configures a menu entry. A menu entry is either a command,
# radiobutton, separator, checkbutton, or a cascade. These have
# a corresponding Tk index value for the corresponding tk menu
# path.
#
# path   : our Menubar path name to this menu entry.
# index  : the t
# args   : the args for configuration
#
# -------------------------------------------------------------
::itcl::body Menubar::_configureMenuEntry {path index {option {}} args} {
    set type [type $path]
    # set len [llength $args]
    # get the menu path from the entry path name
    set tkMenuPath [_entryPathToTkMenuPath $path]
    if {$option eq ""} {
	set configList [$tkMenuPath entryconfigure \
		[_getTkIndex $tkMenuPath $index]]

	if {$type eq "cascade"} {
	    if {[info exists _menuOption($path)]} {
		lappend configList [list -menu menu Menu {} \
			$_menuOption($path)]
	    } else {
		lappend configList [list -menu menu Menu {} {}]
	    }
	}
	if {[info exists _helpString($path)]} {
	    lappend configList [list -helpstr helpStr HelpStr {} \
		    $_helpString($path)]
	} else {
	    lappend configList [list -helpstr helpStr HelpStr {} {}]
	}
	return $configList
    } elseif {$args eq ""} {
	if {$option eq "-menu"} {
	    if {[info exists _menuOption($path)]} {
		return [list -menu menu Menu {} $_menuOption($path)]
	    } else {
		return [list -menu menu Menu {} {}]
	    }
	} elseif {$option eq "-helpstr"} {
	    if { [info exists _helpString($path)] } {
		return [list -helpstr helpStr HelpStr {} \
			$_helpString($path)]
	    } else {
		return [list -helpstr helpStr HelpStr {} {}]
	    }
	} else {
	    # ... OTHERWISE, let Tk get it.
	    return [$tkMenuPath entryconfigure \
		    [_getTkIndex $tkMenuPath $index] $option]
	}
    } else {
	array set temp [concat $option $args]
	# ... Store -helpstr val,strip out -helpstr val from args
	if {[::info exists temp(-helpstr)]} {
	    set _helpString($path) $temp(-helpstr)
	    unset temp(-helpstr)
	}
	set args [array get temp]
	if {$type eq "cascade" && [::info exists temp(-menu)] } {
	    uplevel 0 "_configureMenuOption cascade $path $args"
	} else {
	    # invoke the menu's entryconfigure command
	    # being careful to ajust the INDEX to be 0 or 1 based 
	    # depending on the tearoff status
	    # if the stripping process brought us down to no options
	    # to set, then forget the configure of widget.
	    if {[llength $args] != 0} {
		uplevel 0 $tkMenuPath entryconfigure \
			[_getTkIndex $tkMenuPath $index] $args
	    }
	}
	return ""
    }
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _unsetPaths
#
# comment
#
# -------------------------------------------------------------
::itcl::body Menubar::_unsetPaths { parent } {
    # first get the complete list of all menu paths
    set pathList [array names _pathMap]
    # for each path that matches parent prefix, unset it.
    foreach path $pathList {
	if {[regexp [subst -nocommands {^$parent}] $path]} {
	    unset _pathMap($path)
	}
    }
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _entryPathToTkMenuPath
#
# Takes an entry path like .mbar.file.new and changes it to
# .mbar.file.menu and performs a lookup in the pathMap to
# get the corresponding menu widget name for tk
#
# -------------------------------------------------------------
::itcl::body Menubar::_entryPathToTkMenuPath {entryPath} {
    # get the menu path from the entry path name
    # by stripping off the entry component of the path
    regsub {[.][^.]*$} $entryPath "" menuPath
    # the tkMenuPath is looked up with the .menu added to lookup
    if {[catch {set tkMenuPath $_pathMap($menuPath.menu)}]} {
	return ""
    } else {
	return $_pathMap($menuPath.menu)
    }
}


# -------------------------------------------------------------
#
# These two methods address the issue of menu entry indices being
# zero-based when the menu is not a tearoff menu and 1-based when
# it is a tearoff menu. Our strategy is to hide this difference.
# 
# _getTkIndex returns the index as tk likes it: 0 based for non-tearoff
# and 1 based for tearoff menus.
# 
# _getPdIndex (get pulldown index) always returns it as 0 based.
# 
# -------------------------------------------------------------

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _getTkIndex
#
# give us a zero or 1-based answer depending on the tearoff
# status of the menu. If the menu denoted by tkMenuPath is a
# tearoff menu it returns a 1-based result, otherwise a 
# zero-based result.
# 
# -------------------------------------------------------------
::itcl::body Menubar::_getTkIndex {tkMenuPath tkIndex} {
    # if there is a tear off make it 1-based index
    if {[$tkMenuPath cget -tearoff]} {
	incr tkIndex
    }
    return $tkIndex
}

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _getPdIndex
#
# Take a tk index and give me a zero based numerical index
#
# Ask the menu widget for the index of the entry denoted by
# 'tkIndex'. Then if the menu is a tearoff adjust the value
# to be zero based.
#
# This method returns the index as if tearoffs did not exist.
# Always returns a zero-based index.
#
# -------------------------------------------------------------
::itcl::body Menubar::_getPdIndex { tkMenuPath tkIndex } {
    # get the index from the tk menu
    # this 0 based for non-tearoff and 1-based for tearoffs
    set pdIndex [$tkMenuPath index $tkIndex]
    # if there is a tear off make it 0-based index
    if {[$tkMenuPath cget -tearoff]} {
	incr pdIndex -1
    }
    return $pdIndex
}

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _getMenuList
#
# Returns the list of menus in the order they are on the interface
# returned list is a list of our menu paths
#
# -------------------------------------------------------------
::itcl::body Menubar::_getMenuList {} {
    # get the menus that are packed
    set tkPathList [pack slaves $menubar]
    regsub -- {[.]} [namespace tail $this] "" mbName
    regsub -all -- "\[.\]$mbName\[.\]menubar\[.\]" $tkPathList "." menuPathList
    return $menuPathList
}

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _getEntryList
#
#
# This method looks at a menupath and gets all the entries and
# returns a list of all the entry path names in numerical order
# based on their index values.
#
# MENU is the path to a menu, like .mbar.file.menu or .mbar.file
# we will calculate a menuPath from this: .mbar.file
# then we will build a list of entries in this menu excluding the
# path .mbar.file.menu
#
# -------------------------------------------------------------
::itcl::body Menubar::_getEntryList {menu} {
    # if it ends with menu, clip it off
    regsub {[.]menu$} $menu "" menuPath
    # first get the complete list of all menu paths
    set pathList [array names _pathMap]
    set numEntries 0
    # iterate over the pathList and put on menuPathList those
    # that match the menuPattern
    foreach path $pathList {
	# if this path is on the menuPath's branch
	if {[regexp [subst -nocommands {$menuPath[.][^.]*$}] $path]} {
	    # if not a menu itself
	    if {![regexp {[.]menu$} $path]} {
		set orderedList($_pathMap($path)) $path
		incr numEntries
	    }
	}
    }
    set entryList {}
    for {set i 0} {$i < $numEntries} {incr i} {
	lappend entryList $orderedList($i)
    }
    return $entryList

}

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _parsePath
#
# given path, PATH, _parsePath splits the path name into its
# component segments. It then puts the name back together one
# segment at a time and calls _getSymbolicPath to replace the
# keywords 'last' and 'end' as well as numeric digits.
#
# -------------------------------------------------------------
::itcl::body Menubar::_parsePath {path} {
    set segments [split [string trimleft $path .] .]
    set concatPath ""
    foreach seg $segments {
	set concatPath [_getSymbolicPath $concatPath $seg]
	if {[catch {set _pathMap($concatPath)} msg]} {
	    error "bad path: \"$path\" does not exist. \"$seg\" not valid"
	}
    }
    return $concatPath
}

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _getSymbolicPath
#
# Given a PATH, _getSymbolicPath looks for the last segment of
# PATH to contain: a number, the keywords last or end. If one
# of these it figures out how to get us the actual pathname
# to the searched widget
#
# Implementor's notes:
#	Surely there is a shorter way to do this. The only diff
#	for non-numeric is getting the llength of the correct list
#	It is hard to know this upfront so it seems harder to generalize.
#
# -------------------------------------------------------------
::itcl::body Menubar::_getSymbolicPath {parent segment} {
    # if the segment is a number, then look it up positionally
    # MATCH numeric index
    if {[regexp {^[0-9]+$} $segment]} {
	# if we have no parent, then we are a menubutton
	if {$parent eq {}} {
	    set returnPath [lindex [_getMenuList] $segment]
	} else {
	    set returnPath [lindex [_getEntryList $parent.menu] $segment]
	}
	# MATCH 'end' or 'last' keywords.
    } elseif {$segment eq "end" || $segment eq "last"} {
	# if we have no parent, then we are a menubutton
	if {$parent eq {}} {
	    set returnPath [lindex [_getMenuList] end]
	} else {
	    set returnPath [lindex [_getEntryList $parent.menu] end]
	}
    } else {
	set returnPath $parent.$segment
    }
    return $returnPath
}

# -------------------------------------------------------------
# 
# PRIVATE METHOD: _helpHandler
#
# Bound to the <Motion> event on a menu pane. This puts the
# help string associated with the menu entry into the 
# status widget help area. If no help exists for the current
# entry, the status widget is cleared.
#
# -------------------------------------------------------------
::itcl::body Menubar::_helpHandler { menuPath } {
    if {$itcl_options(-helpvariable) eq {}} {
	return
    }
    set tkMenuWidget $_pathMap($menuPath)
    set entryIndex [$tkMenuWidget index active]
    # already on this item?
    if {$entryIndex == $_entryIndex} {
	return
    }
    set _entryIndex $entryIndex
    if {$entryIndex ne "none"} {
        set entries [_getEntryList $menuPath]
        set menuEntryHit \
	    [lindex $entries [_getPdIndex $tkMenuWidget $entryIndex]]
        # blank out the old one
        set $itcl_options(-helpvariable) {}
        # if there is a help string for this entry
        if {[::info exists _helpString($menuEntryHit)]} {
	    set $itcl_options(-helpvariable) $_helpString($menuEntryHit)
        }
    } else {
	set $itcl_options(-helpvariable) {}
	set _entryIndex -1
    }
}

# -------------------------------------------------------------
#
# PRIVATE METHOD: _getCallerLevel
#
# Starts at stack frame #0 and works down till we either hit
# a ::Menubar stack frame or an ::itk::Archetype stack frame 
# (the latter happens when a configure is called via the 'component'
# method
#
# Returns the level of the actual caller of the menubar command
# in the form of #num where num is the level number caller stack frame.
#
# -------------------------------------------------------------
::itcl::body Menubar::_getCallerLevel {} {
    set levelName {}
    set levelsAreValid true
    set level 0
    set callerLevel #$level
    while {$levelsAreValid} {
	# Hit the end of the stack frame
	if {[catch {uplevel #$level {namespace current}}]} {
	    set levelsAreValid false
	    set callerLevel #[expr {$level - 1}]
	    # still going
	} else {
	    set newLevelName [uplevel #$level {namespace current}]
	    # See if we have run into the first ::Menubar level
	    if {$newLevelName eq "::itk::Archetype" || \
		    $newLevelName == "::::itcl::widgets::Menubar" } {
		# If so, we are done-- set the callerLevel
		set levelsAreValid false
		set callerLevel #[expr {$level - 1}]
	    } else {
		set levelName $newLevelName
	    }
	}
	incr level
    }
    return $callerLevel
}

} ; # end ::itcl::widgets

#
# The default tkMenuFind proc in menu.tcl only looks for menubuttons
# in frames.  Since our menubuttons are within the Menubar class, the
# default proc won't find them during menu traversal.  This proc
# redefines the default proc to remedy the problem.
#-----------------------------------------------------------
# BUG FIX: csmith (Chad Smith: csmith@adc.com), 3/30/99
#-----------------------------------------------------------
# The line, "set qchild ..." below had a typo.  It should be
# "info command $child" instead of "winfo command $child".
#-----------------------------------------------------------
proc tkMenuFind {w char} {
    global tkPriv
    set char [string tolower $char]

    # Added by csmith, 5/10/01, to fix a bug reported on the itcl mailing list.
    if {$w == "."} {
        foreach child [winfo child $w] {
            set match [tkMenuFind $child $char]
	    if {$match != ""} {
	        return $match
	    }
        }
        return {}
    }
    foreach child [winfo child $w] {
	switch [winfo class $child] {
	Menubutton {
	    set qchild [info command $child]
	    set char2 [string index [$qchild cget -text] \
		    [$qchild cget -underline]]
	    if {([string compare $char [string tolower $char2]] == 0)
	            || ($char eq "")} {
	        if {[$qchild cget -state] != "disabled"} {
		    return $child
	        }
	    }
	  }
	Frame -
	Menubar {
	    set match [tkMenuFind $child $char]
	    if {$match != ""} {
	        return $match
	    }
	  }
	}
    }
    return {}
}