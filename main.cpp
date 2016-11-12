#include <iostream>
#include <unistd.h>

#include "fuse_hooks.h"
#include "account.h"
#include "api.h"

using namespace std;

API api;
static fuse_operations cloudfs_oper = {};

int main(int argc, char *argv[])
{
    if ((getuid() == 0) || (geteuid() == 0)) {
        cerr << "Running MRCFS as root opens unnacceptable security holes" << endl;
        return 1;
    }

    cloudfs_oper.read = &read_callback;
    cloudfs_oper.getattr = &getattr_callback;
    cloudfs_oper.getdir = &readdir_callback;
    cloudfs_oper.open = &open_callback;
    cloudfs_oper.release = &release_callback;
    cloudfs_oper.mkdir = &mkdir_callback;
    cloudfs_oper.rmdir = &rmdir_callback;
    cloudfs_oper.write = &write_callback;
    cloudfs_oper.truncate = nullptr;
    cloudfs_oper.unlink = &unlink_callback;
    cloudfs_oper.flush = &flush_callback;
    cloudfs_oper.rename = &rename_callback;
    cloudfs_oper.statfs = &statfs_callback;
    cloudfs_oper.chmod = nullptr;
    cloudfs_oper.utime = &utime_callback;

    Account acc;
    acc.setLogin("kairllur@mail.ru");
    acc.setPassword("REDACTED");

    api.login(acc);
    //api.upload("863272.jpg", "/home/");
    //api.mkdir("/newfolder");
    //api.ls("/");
    //api.download("/10r2005.png", "");

    return fuse_main(argc, argv, &cloudfs_oper);
}
