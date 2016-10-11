// Copyright (c) 2016, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <linux/kdev_t.h>
#include <set>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <glog/logging.h>

namespace baidu {
namespace galaxy {

std::string GenerateTaskId(const std::string& podid) {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::stringstream sm_uuid;
    sm_uuid << uuid;
    std::string str_uuid = podid;
    str_uuid.append("_");
    str_uuid.append(sm_uuid.str());
    return str_uuid;
}

int DownloadByDirectWrite(const std::string& binary,
                          const std::string& tar_packet) {
    const int WRITE_FLAG = O_CREAT | O_WRONLY;
    const int WRITE_MODE = S_IRUSR | S_IWUSR
                           | S_IRGRP | S_IRWXO;
    int fd = ::open(tar_packet.c_str(),
                    WRITE_FLAG, WRITE_MODE);

    if (fd == -1) {
        LOG(WARNING)
                << "open download file " << tar_packet << " failed, "
                << "err: " <<  errno << ", " << strerror(errno);
        return -1;
    }

    int write_len = ::write(fd,
                            binary.c_str(), binary.size());

    if (write_len == -1) {
        LOG(WARNING)
                << "write download " << tar_packet << " file failed, "
                << "err: " << errno << ", " << strerror(errno);
        ::close(fd);
        return -1;
    }

    ::close(fd);
    return 0;
}

int RandRange(int min, int max) {
    srand(time(NULL));
    return min + rand() % (max - min + 1);
}

void GetStrFTime(std::string* time_str) {
    const int TIME_BUFFER_LEN = 100;
    char time_buffer[TIME_BUFFER_LEN];
    time_t seconds = time(NULL);
    struct tm t;
    localtime_r(&seconds, &t);
    size_t len = strftime(time_buffer,
                          TIME_BUFFER_LEN - 1,
                          "%Y%m%d%H%M%S",
                          &t);
    time_buffer[len] = '\0';
    time_str->clear();
    time_str->append(time_buffer, len);
    return ;
}

void ReplaceEmptyChar(std::string& str) {
    size_t index = str.find_first_of(" ");

    while (index != std::string::npos) {
        str.replace(index, 1, "_");
        index = str.find_first_of(" ");
    }
}

namespace process {

bool GetCwd(std::string* dir) {
    if (dir == NULL) {
        return false;
    }

    const int TEMP_PATH_LEN = 1024;
    char* path_buffer = NULL;
    int path_len = TEMP_PATH_LEN;
    bool ret = false;

    for (int i = 1; i < 10; i++) {
        path_len = i * TEMP_PATH_LEN;

        if (path_buffer != NULL) {
            delete []path_buffer;
            path_buffer = NULL;
        }

        path_buffer = new char[path_len];
        char* path = ::getcwd(path_buffer, path_len);

        if (path != NULL) {
            ret = true;
            break;
        }

        if (errno == ERANGE) {
            continue;
        }

        LOG(WARNING)
                << "get cwd failed, "
                << "err: " << errno << ", " << strerror(errno);
        break;
    }

    if (ret) {
        dir->clear();
        dir->append(path_buffer, strlen(path_buffer));
    }

    if (path_buffer != NULL) {
        delete []path_buffer;
    }

    return ret;
}

bool PrepareStdFds(const std::string& pwd,
                   const std::string& process_id,
                   int* stdout_fd,
                   int* stderr_fd) {
    if (stdout_fd == NULL || stderr_fd == NULL) {
        LOG(WARNING) << "prepare stdout_fd or stderr_fd NULL";
        return false;
    }

    baidu::galaxy::file::MkdirRecur(pwd);
    std::string now_str_time;
    GetStrFTime(&now_str_time);
    // std::string stdout_file = pwd + "/" + process_id + "_" + now_str_time + "_stdout";
    // std::string stderr_file = pwd + "/" + process_id + "_" + now_str_time + "_stderr";
    std::string stdout_file = pwd + "/" + process_id + "_stdout";
    std::string stderr_file = pwd + "/" + process_id + "_stderr";

    const int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
    const int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
    *stdout_fd = ::open(stdout_file.c_str(),
                        STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    *stderr_fd = ::open(stderr_file.c_str(),
                        STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);

    if (*stdout_fd == -1 || *stderr_fd == -1) {
        LOG(WARNING)
                << "fail to open file: " << stdout_file << ", "
                << "err: " << errno << ", " << strerror(errno);
        return false;
    }

    return true;
}

void GetProcessOpenFds(const pid_t pid,
                       std::vector<int>* fd_vector) {
    if (fd_vector == NULL) {
        return;
    }

    std::string pid_path = "/proc/";
    pid_path.append(boost::lexical_cast<std::string>(pid));
    pid_path.append("/fd/");

    std::vector<std::string> fd_files;

    if (!file::ListFiles(pid_path, &fd_files)) {
        LOG(WARNING) << "list " << pid_path << " failed";
        return;
    }

    for (size_t i = 0; i < fd_files.size(); i++) {
        if (fd_files[i] == "." || fd_files[i] == "..") {
            continue;
        }

        fd_vector->push_back(::atoi(fd_files[i].c_str()));
    }

    LOG(INFO) << "list pid " << pid << " fds size " << fd_vector->size();
    return;
}

void PrepareChildProcessEnvStep1(pid_t pid, const char* work_dir) {
    int ret = ::setpgid(pid, pid);

    if (ret != 0) {
        assert(0);
    }

    ret = ::chdir(work_dir);

    if (ret != 0) {
        assert(0);
    }
}

void PrepareChildProcessEnvStep2(const int stdin_fd,
                                 const int stdout_fd,
                                 const int stderr_fd,
                                 const std::vector<int>& fd_vector) {
    while (::dup2(stdout_fd, STDOUT_FILENO) == -1
            && errno == EINTR) {}

    while (::dup2(stderr_fd, STDERR_FILENO) == -1
            && errno == EINTR) {}

    while (stdin_fd >= 0
            && ::dup2(stdin_fd, STDIN_FILENO) == -1
            && errno == EINTR) {}

    for (size_t i = 0; i < fd_vector.size(); i++) {
        if (fd_vector[i] == STDOUT_FILENO
                || fd_vector[i] == STDERR_FILENO
                || fd_vector[i] == STDIN_FILENO) {
            // not close std fds
            continue;
        }

        ::close(fd_vector[i]);
    }
}

} // ending namespace process

namespace user {

bool Su(const std::string& user_name) {
    uid_t uid;
    gid_t gid;

    if (!GetUidAndGid(user_name, &uid, &gid)) {
        return false;
    }

    if (::setgid(gid) != 0) {
        return false;
    }

    if (::setuid(uid) != 0) {
        return false;
    }

    return true;
}

bool GetUidAndGid(const std::string& user_name, uid_t* uid, gid_t* gid) {
    if (user_name.empty() || uid == NULL || gid == NULL) {
        return false;
    }

    bool rs = false;
    struct passwd user_passd_info;
    struct passwd* user_passd_rs;
    long int user_passd_buf_len_temp = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    size_t user_passd_buf_len;
    if (user_passd_buf_len_temp == -1) {
        return false;
    } else {
        user_passd_buf_len = (size_t)user_passd_buf_len_temp;
    }

    for (int i = 0; i < 2; i++) {
        char* user_passd_buf = new char[user_passd_buf_len];
        int ret = ::getpwnam_r(user_name.c_str(), &user_passd_info,
                               user_passd_buf, user_passd_buf_len, &user_passd_rs);

        if (ret == 0 && user_passd_rs != NULL) {
            *uid = user_passd_rs->pw_uid;
            *gid = user_passd_rs->pw_gid;
            rs = true;
            delete[] user_passd_buf;
            break;
        } else if (errno == ERANGE) {
            user_passd_buf_len *= 2;
        }
        delete[] user_passd_buf;
    }

    return rs;
}

}   // ending namespace user

namespace file {

bool Traverse(const std::string& path, const OptFunc& opt) {
    bool rs = false;

    if (!IsDir(path, rs)) {
        return false;
    }

    if (!rs) {
        if (0 != opt(path.c_str())) {
            LOG(WARNING)
                    << "opt " << path << " failed, "
                    << "err: " << errno << ", " << strerror(errno);
            return false;
        }

        return true;
    }

    std::vector<std::string> stack;
    stack.push_back(path);
    std::set<std::string> visited;
    std::string cur_path;

    while (stack.size() != 0) {
        cur_path = stack.back();

        bool is_dir;

        if (!IsDir(cur_path, is_dir)) {
            LOG(WARNING) << "IsDir " << path << " failed err";
            return false;
        }

        if (is_dir) {
            if (visited.find(cur_path) != visited.end()) {
                stack.pop_back();

                if (0 != opt(cur_path.c_str())) {
                    LOG(WARNING)
                            << "opt " << cur_path << " failed, "
                            << "err: " << errno << ", " << strerror(errno);
                    return false;
                }

                continue;
            }

            visited.insert(cur_path);
            DIR* dir_desc = ::opendir(cur_path.c_str());

            if (dir_desc == NULL) {
                LOG(WARNING)
                        << "open dir " << cur_path << " failed, "
                        << "err: " << errno << ", " << strerror(errno);
                return false;
            }

            bool ret = true;
            struct dirent* dir_entity;

            while ((dir_entity = ::readdir(dir_desc)) != NULL) {
                if (IsSpecialDir(dir_entity->d_name)) {
                    continue;
                }

                std::string tmp_path = cur_path + "/";
                tmp_path.append(dir_entity->d_name);
                is_dir = false;

                if (!IsDir(tmp_path, is_dir)) {
                    ret = false;
                    break;
                }

                if (is_dir) {
                    stack.push_back(tmp_path);
                } else {
                    if (opt(tmp_path.c_str()) != 0) {
                        LOG(WARNING)
                                << "opt " << tmp_path << " failed, "
                                << "err: " << errno << ", " << strerror(errno);
                        ret = false;
                        break;
                    }
                }
            }

            ::closedir(dir_desc);

            if (!ret) {
                LOG(WARNING) << "opt " << path << " failed err";
                return ret;
            }
        }
    }

    return true;

}

bool ListFiles(const std::string& dir_path,
               std::vector<std::string>* files) {
    if (files == NULL) {
        return false;
    }

    DIR* dir = ::opendir(dir_path.c_str());

    if (dir == NULL) {
        LOG(WARNING)
                << "opendir " << dir_path << " failed, "
                << "err: " << errno << ", " << strerror(errno);
        return false;
    }

    struct dirent* entry;

    while ((entry = ::readdir(dir)) != NULL) {
        files->push_back(entry->d_name);
    }

    ::closedir(dir);
    return true;
}

bool IsExists(const std::string& path) {
    struct stat stat_buf;
    int ret = ::lstat(path.c_str(), &stat_buf);

    if (ret < 0) {
        return false;
    }

    return true;
}

bool Mkdir(const std::string& dir_path) {
    const int dir_mode = 0777;
    int ret = ::mkdir(dir_path.c_str(), dir_mode);

    if (ret == 0 || errno == EEXIST) {
        return true;
    }

    LOG(WARNING)
            << "mkdir " << dir_path << " failed, "
            << "err: " << errno << ", " << strerror(errno);
    return false;
}

bool MkdirRecur(const std::string& dir_path) {
    size_t beg = 0;
    size_t seg = dir_path.find('/', beg);

    while (seg != std::string::npos) {
        if (seg + 1 >= dir_path.size()) {
            break;
        }

        if (!Mkdir(dir_path.substr(0, seg + 1))) {
            return false;
        }

        beg = seg + 1;
        seg = dir_path.find('/', beg);
    }

    return Mkdir(dir_path);
}

bool IsFile(const std::string& path, bool& is_file) {
    struct stat stat_buf;
    int ret = lstat(path.c_str(), &stat_buf);

    if (ret == -1) {
        LOG(WARNING)
                << "stat path " << path << " failed, "
                << "err: " << errno << ", " << strerror(errno);
        return false;
    }

    if (S_ISREG(stat_buf.st_mode)) {
        is_file = true;
    } else {
        is_file = false;
    }

    return true;
}

bool IsDir(const std::string& path, bool& is_dir) {
    struct stat stat_buf;
    int ret = ::lstat(path.c_str(), &stat_buf);

    if (ret == -1) {
        LOG(WARNING)
                << "stat path " << path << " failed, "
                << "err: " << errno << ", " << strerror(errno);
        return false;
    }

    if (S_ISDIR(stat_buf.st_mode)) {
        is_dir = true;
    } else {
        is_dir = false;
    }

    return true;
}

bool IsSpecialDir(const char* path) {
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

bool Remove(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    return Traverse(path, boost::bind(::remove, _1));
}

bool Chown(const std::string& path, uid_t uid, gid_t gid) {
    if (path.empty() || !IsExists(path)) {
        return false;
    }

    return Traverse(path, boost::bind(::lchown, _1, uid, gid));
}

bool SymbolLink(const std::string& old_path, const std::string& new_path) {
    if (IsExists(new_path)) {
        return false;
    }

    if (0 != ::symlink(old_path.c_str(), new_path.c_str())) {
        LOG(WARNING)
                << "symlink failed for old: " << old_path << ", "
                << "new: " << new_path << ", "
                << "err: " << errno << ", " << strerror(errno);
        return false;
    }

    return true;
}

bool Write(const std::string& path, const std::string& content) {
    FILE* fd = ::fopen(path.c_str(), "we");

    if (fd == NULL) {
        fprintf(stderr, "open file %s failed", path.c_str());
        return false;
    }

    int ret = ::fprintf(fd, "%s", content.c_str());
    ::fclose(fd);

    if (ret <= 0) {
        fprintf(stderr, "write file %s failed", path.c_str());
        return false;
    }

    return true;
}

bool GetDeviceMajorNumberByPath(const std::string& path, int32_t& major_number) {
    struct stat sb;

    if (stat(path.c_str(), &sb) == -1) {
        return false;
    }

    major_number = major(sb.st_dev);
    return true;
}

} // ending namespace file

namespace md5 {

#define F(x, y, z)   ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)   ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)   ((x) ^ (y) ^ (z))
#define I(x, y, z)   ((y) ^ ((x) | ~(z)))
#define STEP(f, a, b, c, d, x, t, s) \
        (a) += f((b), (c), (d)) + (x) + (t); \
        (a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
        (a) += (b);

#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define SET(n) \
            (*(MD5_u32 *)&ptr[(n) * 4])
#define GET(n) \
            SET(n)
#else
#define SET(n) \
            (ctx->block[(n)] = \
            (MD5_u32)ptr[(n) * 4] | \
            ((MD5_u32)ptr[(n) * 4 + 1] << 8) | \
            ((MD5_u32)ptr[(n) * 4 + 2] << 16) | \
            ((MD5_u32)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
            (ctx->block[(n)])
#endif

typedef unsigned int MD5_u32;

typedef struct {
    MD5_u32 lo, hi;
    MD5_u32 a, b, c, d;
    unsigned char buffer[64];
    MD5_u32 block[16];
} MD5_CTX;

static void MD5_Init(MD5_CTX* ctx);
static void MD5_Update(MD5_CTX* ctx, const void* data, unsigned long size);
static void MD5_Final(unsigned char* result, MD5_CTX* ctx);

static const void* body(MD5_CTX* ctx, const void* data, unsigned long size) {
    const unsigned char* ptr;
    MD5_u32 a, b, c, d;
    MD5_u32 saved_a, saved_b, saved_c, saved_d;

    ptr = (const unsigned char*)data;
    a = ctx->a;
    b = ctx->b;
    c = ctx->c;
    d = ctx->d;

    do {
        saved_a = a;
        saved_b = b;
        saved_c = c;
        saved_d = d;

        STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
        STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
        STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
        STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
        STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
        STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
        STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
        STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
        STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
        STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
        STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
        STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
        STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
        STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
        STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
        STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)
        STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
        STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
        STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
        STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
        STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
        STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
        STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
        STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
        STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
        STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
        STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
        STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
        STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
        STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
        STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
        STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)
        STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
        STEP(H, d, a, b, c, GET(8), 0x8771f681, 11)
        STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
        STEP(H, b, c, d, a, GET(14), 0xfde5380c, 23)
        STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
        STEP(H, d, a, b, c, GET(4), 0x4bdecfa9, 11)
        STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
        STEP(H, b, c, d, a, GET(10), 0xbebfbc70, 23)
        STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
        STEP(H, d, a, b, c, GET(0), 0xeaa127fa, 11)
        STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
        STEP(H, b, c, d, a, GET(6), 0x04881d05, 23)
        STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
        STEP(H, d, a, b, c, GET(12), 0xe6db99e5, 11)
        STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
        STEP(H, b, c, d, a, GET(2), 0xc4ac5665, 23)
        STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
        STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
        STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
        STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
        STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
        STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
        STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
        STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
        STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
        STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
        STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
        STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
        STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
        STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
        STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
        STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

        a += saved_a;
        b += saved_b;
        c += saved_c;
        d += saved_d;

        ptr += 64;
    } while (size -= 64);

    ctx->a = a;
    ctx->b = b;
    ctx->c = c;
    ctx->d = d;

    return ptr;
}

void MD5_Init(MD5_CTX* ctx) {
    ctx->a = 0x67452301;
    ctx->b = 0xefcdab89;
    ctx->c = 0x98badcfe;
    ctx->d = 0x10325476;

    ctx->lo = 0;
    ctx->hi = 0;
}

void MD5_Update(MD5_CTX* ctx, const void* data, unsigned long size) {
    MD5_u32 saved_lo;
    unsigned long used, free;

    saved_lo = ctx->lo;

    if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo) {
        ctx->hi++;
    }

    ctx->hi += size >> 29;
    used = saved_lo & 0x3f;

    if (used) {
        free = 64 - used;

        if (size < free) {
            memcpy(&ctx->buffer[used], data, size);
            return;
        }

        memcpy(&ctx->buffer[used], data, free);
        data = (unsigned char*)data + free;
        size -= free;
        body(ctx, ctx->buffer, 64);
    }

    if (size >= 64) {
        data = body(ctx, data, size & ~(unsigned long)0x3f);
        size &= 0x3f;
    }

    memcpy(ctx->buffer, data, size);
}

void MD5_Final(unsigned char* result, MD5_CTX* ctx) {
    unsigned long used, free;
    used = ctx->lo & 0x3f;
    ctx->buffer[used++] = 0x80;
    free = 64 - used;

    if (free < 8) {
        memset(&ctx->buffer[used], 0, free);
        body(ctx, ctx->buffer, 64);
        used = 0;
        free = 64;
    }

    memset(&ctx->buffer[used], 0, free - 8);
    ctx->lo <<= 3;
    ctx->buffer[56] = ctx->lo;
    ctx->buffer[57] = ctx->lo >> 8;
    ctx->buffer[58] = ctx->lo >> 16;
    ctx->buffer[59] = ctx->lo >> 24;
    ctx->buffer[60] = ctx->hi;
    ctx->buffer[61] = ctx->hi >> 8;
    ctx->buffer[62] = ctx->hi >> 16;
    ctx->buffer[63] = ctx->hi >> 24;
    body(ctx, ctx->buffer, 64);
    result[0] = ctx->a;
    result[1] = ctx->a >> 8;
    result[2] = ctx->a >> 16;
    result[3] = ctx->a >> 24;
    result[4] = ctx->b;
    result[5] = ctx->b >> 8;
    result[6] = ctx->b >> 16;
    result[7] = ctx->b >> 24;
    result[8] = ctx->c;
    result[9] = ctx->c >> 8;
    result[10] = ctx->c >> 16;
    result[11] = ctx->c >> 24;
    result[12] = ctx->d;
    result[13] = ctx->d >> 8;
    result[14] = ctx->d >> 16;
    result[15] = ctx->d >> 24;
    memset(ctx, 0, sizeof(*ctx));
}

/**
 * Return Calculated raw result(always little-endian),
 * the size is always 16
 */
void Md5Bin(const void* dat, size_t len, unsigned char out[16]) {
    MD5_CTX c;
    MD5_Init(&c);
    MD5_Update(&c, dat, len);
    MD5_Final(out, &c);
}

static char hb2hex(unsigned char hb) {
    hb = hb & 0xF;
    return hb < 10 ? '0' + hb : hb - 10 + 'a';
}

std::string Md5File(const char* filename) {
    FILE* file = std::fopen(filename, "rb");
    std::string res = Md5File(file);
    std::fclose(file);
    return res;
}

std::string Md5File(FILE* file) {
    MD5_CTX c;
    MD5_Init(&c);

    char buff[BUFSIZ];
    unsigned char out[16];
    size_t len = 0;

    while ((len = std::fread(buff , sizeof(char), BUFSIZ, file)) > 0) {
        MD5_Update(&c, buff, len);
    }

    MD5_Final(out, &c);

    std::string res;

    for (size_t i = 0; i < 16; ++ i) {
        res.push_back(hb2hex(out[i] >> 4));
        res.push_back(hb2hex(out[i]));
    }

    return res;
}

std::string Md5(const void* dat, size_t len) {
    std::string res;
    unsigned char out[16];
    Md5Bin(dat, len, out);

    for (size_t i = 0; i < 16; ++ i) {
        res.push_back(hb2hex(out[i] >> 4));
        res.push_back(hb2hex(out[i]));
    }

    return res;
}

std::string Md5(std::string dat) {
    return Md5(dat.c_str(), dat.length());
}

/**
 * Generate shorter md5sum by something like base62
 * instead of base16 or base10.
 * 0~61 are represented by 0-9a-zA-Z
 */
std::string Md5Sum6(const void* dat, size_t len) {
    static const char* tbl = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string res;
    unsigned char out[16];
    Md5Bin(dat, len, out);

    for (size_t i = 0; i < 6; ++i) {
        res.push_back(tbl[out[i] % 62]);
    }

    return res;
}

std::string Md5Sum6(std::string dat) {
    return Md5Sum6(dat.c_str(), dat.length());
}

} // ending namespace md5

namespace net {

template <typename ElemT>
struct HexTo {
    ElemT value;
    operator ElemT() const {return value;}
    friend std::istream& operator>>(std::istream& in, HexTo& out) {
        in >> std::hex >> out.value;
        return in;
    }
};

bool IsPortInProcFile(const int32_t& port, const std::string& file_name) {
    bool ret = false;
    std::ifstream net_file(file_name.c_str());
    if (!net_file) {
        return ret;
    }

    std::string line;
    std::vector<std::string> parts;
    while (getline(net_file, line)) {
        boost::split(parts,
                     line,
                     boost::is_any_of(": "),
                     boost::token_compress_on);
        if (parts.size() > 3) {
            try {
                if (boost::lexical_cast<HexTo<int32_t> >(parts[3]) == port) {
                    ret = true;
                    break;
                }
            } catch (boost::bad_lexical_cast& e) {
            }
        }
    }

    return ret;
}

bool IsPortOpen(int32_t port) {
    bool ret = false;
    std::string proc_net_tcp = "/proc/net/tcp";
    std::string proc_net_udp = "/proc/net/udp";

    if (IsPortInProcFile(port, proc_net_tcp)) {
        ret = true;
    } else {
        ret = IsPortInProcFile(port, proc_net_udp);
    }

    return ret;
}

} // ending namespace net

} // ending namespace galaxy
} // ending namespace baidu
