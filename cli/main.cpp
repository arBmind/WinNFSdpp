/*! @file Main procedure definition for WinNFSdpp command line interface
 * */

#include <string>
#include <iostream>
#include <gflags/gflags.h>
#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

DEFINE_bool(log, true, "Enables logging, otherwise prints to stderr");
DEFINE_string(user_id,"0", "User ID");
DEFINE_string(group_id,"0", "Group ID");
DEFINE_string(pathFile,"", "File with local export Paths");
DEFINE_string(cachePath,"./mount_cache", "Mount cache path");

#include "cli.h"

int main(int argc, char *argv[])
{
    const std::string version = WINNFSDPP_VERSION;

    gflags::SetUsageMessage("<flags> <export path> <alias path>");
    gflags::SetVersionString(version);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    google::InitGoogleLogging(argv[0]);
    LOG(INFO) << "WINNFSDPP Version: " << version;

    LOG(INFO) << "Starting windows socket sesstion";
    wsa_session_t wsa_session(2, 2);
    program_t program;
    program.run();
    LOG(INFO) << "Returned from CLI loop, exiting";
}
