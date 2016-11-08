#ifndef CLOUDFILE_H
#define CLOUDFILE_H

#include <string>
#include <json/value.h>

/**
 * @brief The CloudFile class representation of the structure returned by ls API call
 */
class CloudFile
{
    enum Type {
        File,
        Directory
    };

public:
    CloudFile();
    CloudFile(const Json::Value &val) {
        type = val["type"].asString() == "file" ? File : Directory;
        kind = val["kind"].asString();
        home = val["home"].asString();
        name = val["name"].asString();
        hash = val["hash"].asString();
        size = val["size"].asUInt64();
        mtime = val["mtime"].asUInt64();
        virusScan = val["virus_scan"].asString();
    }

    Type getType() const;
    void setType(const Type &value);

    std::string getKind() const;
    void setKind(const std::string &value);

    std::string getHome() const;
    void setHome(const std::string &value);

    std::string getName() const;
    void setName(const std::string &value);

    std::string getHash() const;
    void setHash(const std::string &value);

    uint64_t getMtime() const;
    void setMtime(const uint64_t &value);

    uint64_t getSize() const;
    void setSize(const uint64_t &value);

    std::string getVirusScan() const;
    void setVirusScan(const std::string &value);

private:
    Type type;
    std::string kind;       // seems to be the same as type
    std::string home;       // full remote path on server (with fname)
    std::string name;       // name of file
    std::string hash;       // hash of file
    uint64_t mtime;         // modification time
    uint64_t size;          // size of file
    std::string virusScan;  // "pass" usually
};

#endif // CLOUDFILE_H
