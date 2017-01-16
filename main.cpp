#include <cstddef> // offsetof macro
#include <string.h>
#include <unistd.h>

#include "fuse_hooks.h"
#include "account.h"
#include "utils.h"

#define MRUFS_OPT(t, p, v) { t, offsetof(marcfs_config, p), v }
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

static int marcfs_opt_proc(void */*data*/, const char */*arg*/, int key, struct fuse_args *outargs)
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

void hide_sensitive_data(char *param, size_t rest) {
    char *data = strchr(param, '=');
    size_t j, len;
    if (data && (len = strlen(++data) - rest)) {
        for (j = 0; j < len; ++j) {
            *(data++) = '*';
        }
    }
}

void hide_sensitive(int argc, char *argv[]) {
    int i;
    char *user, *pasw;
    for (i = 1; i < argc; ++i) {
        user = strstr(argv[i], "username=");
        pasw = strstr(argv[i], "password=");
        if (user) {
            hide_sensitive_data(user, (pasw && pasw > user) ? (strlen(pasw) + 1) : 0);
        }
        if (pasw) {
            hide_sensitive_data(pasw, (user && user > pasw) ? (strlen(user) + 1) : 0);
        }
    }
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
    int res = fuse_opt_parse(&args, &conf, marcfs_opts, marcfs_opt_proc);
    if (res == -1) {
        return 2;
    }

    hide_sensitive(argc, argv);

    // initialize auth and globals
    Account acc;
    acc.setLogin(conf.username);
    acc.setPassword(conf.password);
    MarcRestClient rc;
    rc.login(acc);
    fsMetadata.clientPool.populate(rc, 5);

    // initialize FUSE
    static fuse_operations cloudfs_oper = {};
    cloudfs_oper.init = &initCallback;
    cloudfs_oper.read = &readCallback;
    cloudfs_oper.getattr = &getattrCallback;
    cloudfs_oper.readdir = &readdirCallback;
    cloudfs_oper.open = &openCallback;
    cloudfs_oper.release = &releaseCallback;
    cloudfs_oper.mkdir = &mkdirCallback;
    cloudfs_oper.rmdir = &rmdirCallback;
    cloudfs_oper.write = &writeCallback;
    cloudfs_oper.truncate = &truncateCallback;
    cloudfs_oper.unlink = &unlinkCallback;
    cloudfs_oper.flush = &flushCallback;
    cloudfs_oper.rename = &renameCallback;
    cloudfs_oper.statfs = &statfsCallback;
    cloudfs_oper.utime = &utimeCallback;
    cloudfs_oper.mknod = &mknodCallback;
    cloudfs_oper.create = &createCallback;
    cloudfs_oper.chmod = &chmodCallback;

    // start!
    return fuse_main(args.argc, args.argv, &cloudfs_oper, nullptr);
}
