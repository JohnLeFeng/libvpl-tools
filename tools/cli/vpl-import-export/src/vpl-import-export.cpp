//==============================================================================
// Copyright Intel Corporation
//
// SPDX-License-Identifier: MIT
//==============================================================================

#include "./util.h"

int main(int argc, char **argv) {
    int err           = -1;
    Params params     = {};
    FileInfo fileInfo = {};

    //-- Parse command line args to params
    if (ParseArgsAndValidate(argc, argv, &params) == false) {
        Usage();
        return 1; // return 1 as error code
    }

    //-- Open input file
    if (!params.infileName.empty()) {
        bool bFailed = false;
        try {
            fileInfo.infile.open(params.infileName.c_str(), std::ios::binary);
        }
        catch (...) {
            bFailed = true;
        }

        if (bFailed || !fileInfo.infile.is_open()) {
            printf("ERROR: could not open input file");
            return -1;
        }
    }

    //-- Open output file
    if (!params.outfileName.empty()) {
        bool bFailed = false;
        try {
            fileInfo.outfile.open(params.outfileName.c_str(), std::ios::binary);
        }
        catch (...) {
            bFailed = true;
        }

        if (bFailed || !fileInfo.outfile.is_open()) {
            printf("ERROR: could not open output file");
            return -1;
        }
    }

    try {
        if (params.testMode == TEST_MODE_DECVPP_FILE || params.testMode == TEST_MODE_RENDER)
            err = RunDecodeVPP(&params, &fileInfo);
        else if (params.testMode == TEST_MODE_ENC_FILE || params.testMode == TEST_MODE_CAPTURE)
            err = RunEncode(&params, &fileInfo);
    }
    catch (...) {
        return -1;
    }

    // print report of hardware resources still in use (if supported, DEBUG builds only)
    DebugDumpHardwareInterface();

    return err;
}
