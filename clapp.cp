/******************************************************************************
 *
 *  File:           clapp.cpp (Command Line APPlication)
 *
 *                  $Id: clapp.cp,v 1.8 2007/03/10 04:13:49 reid Exp $
 *
 *  Function:       basic parent class for handling command line arguments
 *
 *  Author(s):      Reid Woodbury Jr
 *
 *  Copyright:      Copyright (c) 2006 Reid Woodbury Jr
 *                  All Rights Reserved.
 *
 *  Source:         Started anew.
 *
 *  Notes:
 *
 *  Change History:
 *          95_xx_xx_nnn    Started source
 *
 *****************************************************************************/

#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <err.h>
#include <fcntl.h>
#include <sysexits.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <getopt.h>

#include <AvailabilityMacros.h>

#include <iostream>
#include <sstream>

#include "clapp.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::Setup(int argc, char * argv[]) {
    SetOptionStruct();
    InitPrefs();
    HandleArgs(argc, argv);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::SetOptionStruct(void) {
    static option   longOptInit[] = {
        /* { name  has_arg  *flag  val } */
        {"help", no_argument, NULL, 'h'},
        { NULL, 0, NULL, 0 }
    };
    mLongOpts   = longOptInit;

    mShortOpts  = "RDUNh";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::HandleArgs(int argc, char * argv[]) {
    mProgName = ::basename(argv[0]);

    int optc;
    int long_index;
    while ( (optc = getopt_long_only (argc, argv, mShortOpts.c_str(), mLongOpts, &long_index)) != -1 ) {
        switch (optc) {
            case ':':
            MessageMissingOption();
            MessageUsage();
            break;

            case '?':
            MessageAmbiguousOption();
            MessageUsage();
            break;

            //  long option was used for argument
            case 0:
            if (!HandleLongIndex(long_index)) {
                MessageBadLongOptIndex(long_index);
                MessageUsage();
            }
            break;

            default:
            if (!HandleArg(optc)) {
                /* Error message already emitted by getopt_long. */
                MessageUsage();
            }
        }
    }
    mArgc   = argc - optind;
    mArgv   = (char**) argv + optind;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  switch through all options that have a short option
bool clapp::HandleArg(int arg) {
    switch (arg) {
        case 'h':
        MessageHelp();
        break;

        default:
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Handle long options that DON'T have have a corrisponding short option.
bool clapp::HandleLongIndex(int index) {
    switch (index) {
        // case 0:
        // return true;

        default:
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::MessageUsage() {
    cerr << "\n\
usage:  " << mProgName << " [-h]" << endl;

    exit(EX_USAGE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::MessageHelp() {
    cerr << "clapp: Command Line APPlication class" << endl;
    cerr << "\n    -h\n\
    --help        this screen" << endl;

    MessageUsage();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::MessageMissingOption() {
    cerr << "Huh? Missing option argument.\n" << endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::MessageAmbiguousOption() {
    cerr << "Huh? Unknown or ambiguous option.\n" << endl;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clapp::MessageBadLongOptIndex(int long_index) {
    cerr << "Huh? Variable 'long_index' contained: " << long_index << endl;
}
