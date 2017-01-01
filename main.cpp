#include <iostream>
#include <unistd.h>

#include "fuse_hooks.h"
#include "account.h"
#include "utils.h"

#define MRUFS_OPT(t, p, v) { t, offsetof(struct marcfs_config, p), v }
#define MARC_FS_VERSION "0.1"

using namespace std;

// global MruData instance
MruData fsMetadata;

struct marcfs_config {
     char *username;
     char *password;
};
enum {
    KEY_HELP,
    KEY_VERSION
};

static struct fuse_opt marcfs_opts[] = {
     MRUFS_OPT("username=%s",   username, 0),
     MRUFS_OPT("password=%s",   password, 0),

     FUSE_OPT_KEY("-V",         KEY_VERSION),
     FUSE_OPT_KEY("--version",  KEY_VERSION),
     FUSE_OPT_KEY("-h",         KEY_HELP),
     FUSE_OPT_KEY("--help",     KEY_HELP),
     FUSE_OPT_END
};

static int marcfs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    switch (key) {
        case KEY_HELP:
            fprintf(stderr,
            "usage: %s mountpoint [options]\n"
            "\n"
            "general options:\n"
            "    -o opt,[opt...]  mount options\n"
            "    -h   --help      print help\n"
            "    -V   --version   print version\n"
            "\n"
            "MARC-FS options:\n"
            "    -o username=STRING - username for your mail.ru mailbox\n"
            "    -o password=STRING - password for the above\n"
            , outargs->argv[0]);
            exit(1);
        case KEY_VERSION:
            cerr << "MARC-FS version " << MARC_FS_VERSION << endl;
            exit(0);
    }
    return 1;
}

int main(int argc, char *argv[])
{
    if (getuid() == 0 || geteuid() == 0) {
        cerr << "Running MARC-FS as root opens unacceptable security holes" << endl;
        return -1;
    }

    if (argc < 2) { // didn't send anything?
        cerr << "No command options specified, ";
        cerr << "please try -h or --help to get comprehensive list" << endl;
        return 1;
    }

    // parse options
    fuse_args args = FUSE_ARGS_INIT(argc, argv);
    marcfs_config conf = {};
    fuse_opt_parse(&args, &conf, marcfs_opts, marcfs_opt_proc);

    // initialize auth and globals
    Account acc;
    acc.setLogin(conf.username);
    acc.setPassword(conf.password);
    fsMetadata.apiPool.initialize(&API::login, acc);

    // initialize FUSE
    static fuse_operations cloudfs_oper = {};
    cloudfs_oper.init = &init_callback;
    cloudfs_oper.read = &read_callback;
    cloudfs_oper.getattr = &getattr_callback;
    cloudfs_oper.readdir = &readdir_callback;
    cloudfs_oper.open = &open_callback;
    cloudfs_oper.release = &release_callback;
    cloudfs_oper.mkdir = &mkdir_callback;
    cloudfs_oper.rmdir = &rmdir_callback;
    cloudfs_oper.write = &write_callback;
    cloudfs_oper.truncate = &truncate_callback;
    cloudfs_oper.unlink = &unlink_callback;
    cloudfs_oper.flush = &flush_callback;
    cloudfs_oper.rename = &rename_callback;
    cloudfs_oper.statfs = &statfs_callback;
    cloudfs_oper.utime = &utime_callback;
    cloudfs_oper.mknod = &mknod_callback;

    //api.upload("863272.jpg", "/home/");
    //api.mkdir("/newfolder");
    //api.ls("/nah");
    //api.download("/10r2005.png", "");

    // start!
    return fuse_main(args.argc, args.argv, &cloudfs_oper, nullptr);
}
