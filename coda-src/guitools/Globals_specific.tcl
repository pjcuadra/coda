proc InitSystemAdministrator { } {
    global SysAdmin

    set SysAdmin(Coda) "Henry Pierce at hmpierce@cs.cmu.edu or x8-7557"
}
proc InitPathnamesArray { } {
    global Pathnames
    global user

    set Pathnames(srcarea) /coda/usr/mre/newbuild/src/coda-src/console

    set Pathnames(venus.cache) /usr/coda/venus.cache
    set Pathnames(venus.log) /usr/coda/venus.cache/venus.log
    set Pathnames(myvenus.log) $Pathnames(srcarea)/venus.log
    set Pathnames(passwd) /etc/passwd

    set Pathnames(home) /coda/usr/$user
    set Pathnames(newHoarding) $Pathnames(home)/newHoarding
    set Pathnames(advicerc) $Pathnames(home)/.advicerc

    # The ++ notation below is replaced by the Makefile
    set Pathnames(cfs) /usr/local/bin/cfs
    set Pathnames(ctokens) /usr/local/bin/ctokens
    set Pathnames(clog) /usr/local/bin/clog
    set Pathnames(cunlog) /usr/local/bin/cunlog
    set Pathnames(vutil) /usr/local/bin/vutil
    set Pathnames(hoard) /usr/local/bin/hoard

    set Pathnames(awk) awk
    set Pathnames(cp) /bin/cp
    set Pathnames(df) /bin/df
    set Pathnames(grep) /usr/bin/grep
    set Pathnames(hostname) /bin/hostname
}

proc InitDimensionsArray { } {
    global Dimensions

    set Dimensions(Meter:Height) 50
    set Dimensions(Meter:Gauge:Top) 10
    set Dimensions(Meter:Gauge:Bottom) 30

    set Dimensions(Meter:Width) 300
    set Dimensions(Meter:Gauge:Left) 100
    set Dimensions(Meter:Gauge:Right) 290

    set Dimensions(ProgressBar:Width) 150
    set Dimensions(ProgressBar:Height) 20

}

proc InitColorArray { } {
    global Colors 

    # Initialize a list of possible colors
    set Colors(List) "Red OrangeRed1 DarkOrange1 Orange Gold Yellow LimeGreen ForestGreen DarkGreen SteelBlue CornflowerBlue LightSteelBlue1 MediumPurple1 DarkOrchid grey29 grey45 grey77 grey100"

    #
    # Initialize the colors for the different levels of urgency
    #
    set Colors(Unknown)  gray25
    set Colors(Normal)   ForestGreen
    set Colors(Warning)  Gold
    set Colors(Critical) Red
    set Colors(UnFlash)  gray25

    # Initialize other colors
    set Colors(disabled) gray50

    # Initialize the colors for the different things
    set Colors(meter) SlateGray
}

proc InitDisplayStyleArray { } {
    global DisplayStyle
    global Colors

    set DisplayStyle(Normal) \
	[tixDisplayStyle text -font *times-medium-r-*-*-14* -background gray92]

    set DisplayStyle(Bold) \
	[tixDisplayStyle text -font *times-bold-r-*-*-14* -background gray92]

    set DisplayStyle(Italic) \
	[tixDisplayStyle text -font *times-medium-i-*-*-14* -background gray92]

    set DisplayStyle(BoldItalic) \
	[tixDisplayStyle text -font *times-bold-i-*-*-14* -background gray92]

    foreach color $Colors(List) {
	set DisplayStyle($color) \
	    [tixDisplayStyle text -font *times-bold-r-*-*-14* -foreground $color -background gray92]
    }

    set DisplayStyle(Header) \
	[tixDisplayStyle text \
		-fg black -anchor c \
		-padx 8 -pady 2 \
		-font *times-bold-r-*-*-14* \
		-background gray92]
	
}

