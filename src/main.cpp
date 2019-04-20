/*****************************************************************************
 *
 * Copyright (c) 2018-2019, Oleg `Kanedias` Chernovskiy
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

#include <sys/stat.h>   // stat syscall
#include <cstddef>      // offsetof macro
#include <string.h>
#include <unistd.h>     // getuid
#include <signal.h>     // disabling signal handlers
#include <pwd.h>

#include <json/json.h>

#include "fuse_hooks.h"
#include "account.h"
#include "utils.h"

#define MARC_FS_OPT(t, p, v) { t, offsetof(MarcfsConfig, p), v }
#define MARC_FS_VERSION "0.1"

extern ObjectPool<MarcRestClient> clientPool;
extern std::string cacheDir;

// config struct declaration for cmdline parsing
struct MarcfsConfig {
     char *username = nullptr; // username, full login@domain.tld
     char *password = nullptr; // password for username
     char *cachedir = nullptr; // cache directory on local filesystem
     char *conffile = nullptr; // config file, default is ~/.config/marcfs/config.json
     char *proxyurl = nullptr; // proxy url, default is taken from http(s)_proxy env var

     long maxDownloadRate = 0; // rate limit on download, in KiB/s
     long maxUploadRate = 0; // rate limit on upload, in KiB/s
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
     MARC_FS_OPT("max-download-rate=%l",   maxDownloadRate, 0),
     MARC_FS_OPT("max-upload-rate=%l",   maxUploadRate, 0),

     FUSE_OPT_KEY("-V",         KEY_VERSION),
     FUSE_OPT_KEY("--version",  KEY_VERSION),
     FUSE_OPT_KEY("-h",         KEY_HELP),
     FUSE_OPT_KEY("--help",     KEY_HELP),
     FUSE_OPT_END
};

// handling non-value options
static int marcfs_opt_proc(void */*data*/, const char */*arg*/, int key, struct fuse_args *outargs)
{
    using namespace std;

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
            "    -o max-download-rate=INTEGER - rate limit on download, in KiB/s\n"
            "    -o max-upload-rate=INTEGER - rate limit on upload, in KiB/s\n"
            , outargs->argv[0]);
            exit(1);
        case KEY_VERSION:
            cerr << "MARC-FS version " << MARC_FS_VERSION << endl;
            exit(0);
    }
    return 1;
}

static void loadConfigFile(MarcfsConfig *conf) {
    using namespace std;

    Json::Value config;
    Json::CharReaderBuilder reader;

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

    string parseErrors;
    bool ok = parseFromStream(reader, configFile, &config, &parseErrors);
    if (!ok) {
        cerr << "Errors parsing config file: " << parseErrors << endl;
        cerr << "Invalid json in config file, ignoring..." << endl;
        return;
    }

    // we can convert it to offsetof+substitution macros in case option count exceeds imagination
    if (!conf->username && config["username"] != Json::Value())
        conf->username = strdup(config["username"].asCString());

    if (!conf->password && config["password"] != Json::Value())
        conf->password = strdup(config["password"].asCString());

    if (!conf->cachedir && config["cachedir"] != Json::Value())
        conf->cachedir = strdup(config["cachedir"].asCString());

    if (!conf->proxyurl && config["proxyurl"] != Json::Value())
        conf->proxyurl = strdup(config["proxyurl"].asCString());
    
    if (!conf->maxDownloadRate && config["max-download-rate"] != Json::Value())
        conf->maxDownloadRate = config["max-download-rate"].asInt64();

    if (!conf->maxUploadRate && config["max-upload-rate"] != Json::Value())
        conf->maxUploadRate = config["max-upload-rate"].asInt64();
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
    using namespace std;

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
    using namespace std;

    // disable SIGPIPE that may come from openssl internals
    signal(SIGPIPE, SIG_IGN);

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

    // check config validity
    if (!conf.username || !conf.password) {
        cerr << "Both 'username' and 'password' parameters must be specified" << endl;
        return 3;
    }

    // hide credentials from htop/top/ps output
    hideSensitive(argc, argv);

    // initialize auth and connections
    Account acc;
    acc.setLogin(conf.username);
    acc.setPassword(conf.password);
    MarcRestClient rc;
    if(conf.proxyurl)
        rc.setProxy(conf.proxyurl);

    // setup speed limits
    if (conf.maxDownloadRate) {
        rc.setMaxDownloadRate(conf.maxDownloadRate * 1024);
    }
    if (conf.maxUploadRate) {
        rc.setMaxUploadRate(conf.maxUploadRate * 1024);
    }

    rc.login(acc); // authenticate one instance to populate pool
    clientPool.populate(rc, 25);

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
        cacheDir = conf.cachedir;
    }

    // initialize FUSE
    static fuse_operations cloudfs_oper = {};
    cloudfs_oper.init = &initCallback;
    cloudfs_oper.getattr = &getattrCallback;
    cloudfs_oper.opendir = &opendirCallback;
    cloudfs_oper.readdir = &readdirCallback;
    cloudfs_oper.releasedir = &releasedirCallback;
    cloudfs_oper.open = &openCallback;
    cloudfs_oper.read = &readCallback;
    cloudfs_oper.write = &writeCallback;
    cloudfs_oper.flush = &flushCallback;
    cloudfs_oper.release = &releaseCallback;
    cloudfs_oper.mkdir = &mkdirCallback;
    cloudfs_oper.rmdir = &rmdirCallback;
    cloudfs_oper.truncate = &truncateCallback;
    cloudfs_oper.unlink = &unlinkCallback;
    cloudfs_oper.rename = &renameCallback;
    cloudfs_oper.statfs = &statfsCallback;
    cloudfs_oper.utimens = &utimensCallback;
    cloudfs_oper.mknod = &mknodCallback;
    cloudfs_oper.chmod = &chmodCallback;

    // start!
    return fuse_main(args.argc, args.argv, &cloudfs_oper, nullptr);
}
