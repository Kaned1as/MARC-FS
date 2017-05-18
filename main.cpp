/*****************************************************************************
 *
 * Copyright (c) 2017, Oleg `Kanedias` Chernovskiy
 *
 * This file is part of MARC-FS.
 *
 * MARC-FS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MARC-FS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MARC-FS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/stat.h> // stat syscall
#include <cstddef> // offsetof macro
#include <string.h>
#include <unistd.h> // getuid
#include <pwd.h>

#include <json/json.h>

#include "fuse_hooks.h"
#include "account.h"
#include "utils.h"

#define MARC_FS_OPT(t, p, v) { t, offsetof(MarcfsConfig, p), v }
#define MARC_FS_VERSION "0.1"

using namespace std;

// global MruData instance definition
MruData fsMetadata;

// config struct declaration for cmdline parsing
struct MarcfsConfig {
     char *username;
     char *password;
     char *cachedir;
     char *conffile;
     char *proxyurl;
};

// non-value options
enum {
    KEY_HELP,
    KEY_VERSION
};

static struct fuse_opt marcfsOpts[] = {
     MARC_FS_OPT("username=%s",   username, 0),
     MARC_FS_OPT("password=%s",   password, 0),
     MARC_FS_OPT("cachedir=%s",   cachedir, 0),
     MARC_FS_OPT("conffile=%s",   conffile, 0),
     MARC_FS_OPT("proxyurl=%s",   proxyurl, 0),

     FUSE_OPT_KEY("-V",         KEY_VERSION),
     FUSE_OPT_KEY("--version",  KEY_VERSION),
     FUSE_OPT_KEY("-h",         KEY_HELP),
     FUSE_OPT_KEY("--help",     KEY_HELP),
     FUSE_OPT_END
};

// handling non-value options
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
            "    -o cachedir=STRING - cache dir for not storing everything in RAM\n"
            "    -o conffile=STRING - json config file location with other params\n"
            "    -o proxyurl=STRING - proxy URL to use for making HTTP calls\n"
            , outargs->argv[0]);
            exit(1);
        case KEY_VERSION:
            cerr << "MARC-FS version " << MARC_FS_VERSION << endl;
            exit(0);
    }
    return 1;
}

static void loadConfigFile(MarcfsConfig *conf) {
    using namespace Json;
    using namespace std;

    Value config;
    Reader reader;
    ifstream configFile;
    if (conf->conffile) {
        // config file location is set, try to load data from there
        configFile.open(conf->conffile, ifstream::in | ifstream::binary);
    } else {
        // try default location
        string homedir = getpwuid(getuid())->pw_dir;
        configFile.open(homedir + "/.config/marcfs/config.json", ifstream::in | ifstream::binary);
    }

    if (configFile.fail())
        return; // no config file

    bool ok = reader.parse(configFile, config, false);
    if (!ok) {
        cerr << "Invalid json in config file, ignoring...";
        return;
    }

    // we can convert it to offsetof+substitution macros in case option count exceeds imagination
    if (!conf->username && config["username"] != Value())
        conf->username = strdup(config["username"].asCString());

    if (!conf->password && config["password"] != Value())
        conf->password = strdup(config["password"].asCString());

    if (!conf->cachedir && config["cachedir"] != Value())
        conf->cachedir = strdup(config["cachedir"].asCString());

    if (!conf->proxyurl && config["proxyurl"] != Value())
        conf->proxyurl = strdup(config["proxyurl"].asCString());
}

/**
 * @brief hideSensitiveData - the part that actually changes all characters
 *        after equal sign into asterisks
 * @param param - parameter, like username=alice
 * @param rest - length of parameter
 */
static void hideSensitiveData(char *param, size_t rest) {
    char *data = strchr(param, '=');
    size_t len;
    if (data && (len = strlen(++data) - rest)) {
        for (size_t j = 0; j < len; ++j) {
            *(data++) = '*';
        }
    }
}

/**
 * @brief hideSensitive - routine to check for credential mapping in arguments
 *        and change any sensitive data found into asterisks.
 * @param argc - argument count, pass directly from main
 * @param argv - argument array, pass directly from main
 */
static void hideSensitive(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        char *user = strstr(argv[i], "username=");
        char *pasw = strstr(argv[i], "password=");
        if (user) {
            hideSensitiveData(user, (pasw && pasw > user) ? (strlen(pasw) + 1) : 0);
        }
        if (pasw) {
            hideSensitiveData(pasw, (user && user > pasw) ? (strlen(user) + 1) : 0);
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
    MarcfsConfig conf = {};
    int res = fuse_opt_parse(&args, &conf, marcfsOpts, marcfs_opt_proc);
    if (res == -1)
        return 2;

    // load config from file
    loadConfigFile(&conf);

    // hide credentials from htop/top/ps output
    hideSensitive(argc, argv);

    // initialize auth and connections
    Account acc;
    acc.setLogin(conf.username);
    acc.setPassword(conf.password);
    MarcRestClient rc;
    if(conf.proxyurl)
        rc.setProxy(conf.proxyurl);

    rc.login(acc); // authenticate one instance to populate pool
    fsMetadata.clientPool.populate(rc, 25);

    // initialize cache dir
    if (conf.cachedir) {
        struct stat info = {};
        stat(conf.cachedir, &info);
        if (!(info.st_mode & S_IFDIR)) { // not a dir
            cout << "cachedir option is invalid: "
                 << conf.cachedir << " directory not found" << endl;
            return 1;
        }

        // cache dir is valid
        fsMetadata.cacheDir = conf.cachedir;
    }

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
