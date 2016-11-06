#ifndef CLOUDFILE_H
#define CLOUDFILE_H

#include <string>

class CloudFile
{
    enum Type {
        File,
        Directory
    };

public:
    CloudFile();

    std::string getPath() const;
    void setPath(const std::string &value);

    std::string getName() const;
    void setName(const std::string &value);

    uint64_t getSize() const;
    void setSize(const uint64_t &value);

    Type getType() const;
    void setType(const Type &value);

private:
    Type type;
    std::string path; // remote path on server
    std::string name;
    uint64_t size;
};

#endif // CLOUDFILE_H
