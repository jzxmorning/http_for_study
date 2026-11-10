#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <sys/time.h>
#include <boost/algorithm/string.hpp>
bool WriteFile(const std::string &file_name, const std::string &content)
{
    std::ofstream out(file_name, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        return false;
    }
    out.write(content.c_str(), content.size());
    out.close();
    return !out.fail();
}
bool IsFileExists(const std::string &file_name)
{
    struct stat st;
    if (stat(file_name.c_str(), &st) == 0)
    {
        return true;
    }
    return false;
}
std::string MakeFileName(int user_id)
{
    std::string filename = "./wwwroot/avaters/" + std::to_string(user_id) + ".png";
    return filename;
}
unsigned ReadBigEndian(const unsigned char *p)
{
    return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) |
           (unsigned(p[2]) << 8) | unsigned(p[3]);
}

bool HasPngHeader(const std::string &body)
{
    const unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (body.size() < 33 || std::memcmp(body.data(), signature, 8) != 0)
        return false;
    auto p = reinterpret_cast<const unsigned char *>(body.data());
    if (ReadBigEndian(p + 8) != 13 || std::memcmp(p + 12, "IHDR", 4) != 0)
        return false;
    unsigned width = ReadBigEndian(p + 16), height = ReadBigEndian(p + 20);
    return width > 0 && height > 0 && width <= 2048 && height <= 2048;
}
