#pragma once

#include <Windows.h>
#include <lstate.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <wincrypt.h>

#include "CryptLibrary/md5.h"
#include "CryptLibrary/sha1.h"
#include "CryptLibrary/sha224.h"
#include "CryptLibrary/sha256.h"
#include "CryptLibrary/sha384.h"
#include "CryptLibrary/sha512.h"
#include "CryptLibrary/sha3_224.h"
#include "CryptLibrary/sha3_256.h"
#include "CryptLibrary/sha3_384.h"
#include "CryptLibrary/sha3_512.h"
#include "CryptLibrary/base64.h"

#include <cryptopp/include/cryptopp/sha.h>
#include <cryptopp/include/cryptopp/sha3.h>
#include <cryptopp/include/cryptopp/hex.h>
#include <cryptopp/include/cryptopp/base64.h>
#pragma comment(lib, "C:\\Users\\qaise\\Downloads\\updated\\updated\\what\\module\\dependencies\\cryptopp\\include\\lib\\cryptlib.lib")

#include <dependencies/lz4/lz4.h>

#include <internal/utils.hpp>
#include <internal/globals.hpp>

#pragma comment(lib, "advapi32.lib")


namespace CryptUtils
{
    std::string random_string(std::size_t length) {
        auto randchar = []() -> char {
            const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
            const std::size_t max_index = (sizeof(charset) - 1);
            return charset[rand() % max_index];
            };

        std::string str(length, 0);
        std::generate_n(str.begin(), length, randchar);
        return str;
    };

    template<typename T>
    static std::string hash_with_algo(const std::string& Input)
    {
        T Hash;
        std::string Digest;

        CryptoPP::StringSource SS(Input, true,
            new CryptoPP::HashFilter(Hash,
                new CryptoPP::HexEncoder(
                    new CryptoPP::StringSink(Digest), false
                )));

        return Digest;
    }
}

namespace Crypt
{
    std::string base64_encode(const unsigned char* data, size_t len)
    {
        if (!data || len == 0) return "";

        static const char* base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        std::string ret;
        ret.reserve(((len + 2) / 3) * 4);

        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (len--) {
            char_array_3[i++] = *(data++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; i < 4; i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (j = 0; j < i + 1; j++)
                ret += base64_chars[char_array_4[j]];

            while (i++ < 3)
                ret += '=';
        }

        return ret;
    }

    std::string base64_decode(const std::string& encoded_string)
    {
        if (encoded_string.empty()) return "";

        static const char base64_chars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        size_t in_len = encoded_string.size();
        size_t i = 0;
        size_t j = 0;
        size_t in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::string ret;

        while (in_len-- && encoded_string[in_] != '=') {
            unsigned char c = encoded_string[in_];

            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                in_++;
                in_len++;
                continue;
            }

            const char* found = strchr(base64_chars, c);
            if (!found) {
                break;
            }

            char_array_4[i++] = c;
            in_++;

            if (i == 4) {
                for (i = 0; i < 4; i++) {
                    const char* pos = strchr(base64_chars, char_array_4[i]);
                    char_array_4[i] = pos ? static_cast<unsigned char>(pos - base64_chars) : 0;
                }

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; i < 3; i++)
                    ret += char_array_3[i];
                i = 0;
            }
        }

        if (i) {
            for (j = 0; j < i; j++) {
                const char* pos = strchr(base64_chars, char_array_4[j]);
                char_array_4[j] = pos ? static_cast<unsigned char>(pos - base64_chars) : 0;
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

            for (j = 0; j < i - 1; j++)
                ret += char_array_3[j];
        }

        return ret;
    }

    int base64encode(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t len;
        const char* data = lua_tolstring(L, 1, &len);
        if (!data || len == 0) {
            lua_pushstring(L, "");
            return 1;
        }

        std::string encoded = base64_encode(reinterpret_cast<const unsigned char*>(data), len);
        lua_pushstring(L, encoded.c_str());
        return 1;
    }

    int base64decode(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t len;
        const char* data = lua_tolstring(L, 1, &len);
        if (!data || len == 0) {
            lua_pushstring(L, "");
            return 1;
        }

        std::string decoded = base64_decode(std::string(data, len));
        lua_pushlstring(L, decoded.c_str(), decoded.length());
        return 1;
    }

    int crypt_encrypt(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1) || !lua_isstring(L, 2)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t data_len;
        const char* data = lua_tolstring(L, 1, &data_len);

        size_t key_len;
        const char* key = lua_tolstring(L, 2, &key_len);

        if (!data || !key || key_len == 0) {
            lua_pushstring(L, "");
            return 1;
        }

        const int IV_SIZE = 16;
        std::vector<BYTE> iv(IV_SIZE);

        HCRYPTPROV hProvider = 0;
        if (!CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
            lua_pushstring(L, "");
            return 1;
        }

        CryptGenRandom(hProvider, IV_SIZE, &iv[0]);
        CryptReleaseContext(hProvider, 0);

        std::string result;
        result.reserve(IV_SIZE + data_len);

        result.append(reinterpret_cast<const char*>(&iv[0]), IV_SIZE);

        for (size_t i = 0; i < data_len; i++) {
            unsigned char iv_byte = iv[i % IV_SIZE];
            result += static_cast<char>(data[i] ^ key[i % key_len] ^ iv_byte);
        }

        lua_pushlstring(L, result.c_str(), result.length());
        return 1;
    }

    int crypt_decrypt(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1) || !lua_isstring(L, 2)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t encrypted_len;
        const char* encrypted = lua_tolstring(L, 1, &encrypted_len);

        size_t key_len;
        const char* key = lua_tolstring(L, 2, &key_len);

        if (!encrypted || !key || key_len == 0) {
            lua_pushstring(L, "");
            return 1;
        }

        const int IV_SIZE = 16;

        if (encrypted_len < IV_SIZE) {
            lua_pushstring(L, "");
            return 1;
        }

        std::vector<BYTE> iv(IV_SIZE);
        memcpy(&iv[0], encrypted, IV_SIZE);

        size_t data_len = encrypted_len - IV_SIZE;
        const char* data = encrypted + IV_SIZE;

        std::string result;
        result.reserve(data_len);

        for (size_t i = 0; i < data_len; i++) {
            unsigned char iv_byte = iv[i % IV_SIZE];
            result += static_cast<char>(data[i] ^ key[i % key_len] ^ iv_byte);
        }

        lua_pushlstring(L, result.c_str(), result.length());
        return 1;
    }

    int generatebytes(lua_State* L)
    {
        int size = luaL_optinteger(L, 1, 32);

        if (size <= 0 || size > 1024 * 1024) {
            lua_pushstring(L, "");
            return 1;
        }

        HCRYPTPROV hProvider = 0;
        if (!CryptAcquireContextW(&hProvider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
            lua_pushstring(L, "");
            return 1;
        }

        std::vector<BYTE> bytes(size);
        BOOL success = CryptGenRandom(hProvider, static_cast<DWORD>(size), &bytes[0]);
        CryptReleaseContext(hProvider, 0);

        if (!success) {
            lua_pushstring(L, "");
            return 1;
        }

        std::string encoded = base64_encode(&bytes[0], size);
        lua_pushstring(L, encoded.c_str());
        return 1;
    }

    int generatekey(lua_State* L)
    {
        lua_pushinteger(L, 32);
        return generatebytes(L);
    }

    int hash(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t len;
        const char* data = lua_tolstring(L, 1, &len);
        if (!data) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t hash = 5381;
        for (size_t i = 0; i < len; i++) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(data[i]);
        }

        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << hash;
        lua_pushstring(L, ss.str().c_str());
        return 1;
    }

    int derive(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1) || !lua_isnumber(L, 2)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t value_len;
        const char* value = lua_tolstring(L, 1, &value_len);
        int length = lua_tointeger(L, 2);

        if (!value || value_len == 0 || length <= 0 || length > 1024 * 1024) {
            lua_pushstring(L, "");
            return 1;
        }

        std::string result;
        result.reserve(length);

        for (int i = 0; i < length; i++) {
            unsigned char base = value[i % value_len];
            unsigned char derived = base ^ (i & 0xFF) ^ ((i >> 8) & 0xFF);
            result += static_cast<char>(derived);
        }

        lua_pushlstring(L, result.c_str(), result.length());
        return 1;
    }

    inline int lz4compress(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t src_size;
        const char* src = lua_tolstring(L, 1, &src_size);
        if (!src || src_size == 0) {
            lua_pushstring(L, "");
            return 1;
        }

        int max_dst_size = LZ4_compressBound(static_cast<int>(src_size));
        if (max_dst_size <= 0) {
            lua_pushstring(L, "");
            return 1;
        }

        std::vector<char> compressed(max_dst_size);

        int compressed_size = LZ4_compress_default(
            src,
            compressed.data(),
            static_cast<int>(src_size),
            max_dst_size
        );

        if (compressed_size <= 0) {
            lua_pushstring(L, "");
            return 1;
        }

        lua_pushlstring(L, compressed.data(), compressed_size);
        return 1;
    }

    inline int lz4decompress(lua_State* L)
    {
        if (!L || !lua_isstring(L, 1)) {
            lua_pushstring(L, "");
            return 1;
        }

        size_t compressed_size;
        const char* compressed = lua_tolstring(L, 1, &compressed_size);
        if (!compressed || compressed_size == 0) {
            lua_pushstring(L, "");
            return 1;
        }

        int uncompressed_size = luaL_optinteger(L, 2, 8192);

        std::vector<char> decompressed(uncompressed_size);

        int result = LZ4_decompress_safe(
            compressed,
            decompressed.data(),
            static_cast<int>(compressed_size),
            uncompressed_size
        );

        if (result < 0) {
            lua_pushstring(L, "");
            return 1;
        }

        lua_pushlstring(L, decompressed.data(), result);
        return 1;
    }

    int getfunctionhash(lua_State* L)
    {

        luaL_checktype(L, 1, LUA_TFUNCTION);

        Closure* cl = (Closure*)lua_topointer(L, 1);

        uint8_t nupvalues = cl->nupvalues;
        Proto* p = (Proto*)cl->l.p;

        std::string result =
            std::to_string((int)p->sizep) + "," +
            std::to_string((int)p->sizelocvars) + "," +
            std::to_string((int)p->sizeupvalues) + "," +
            std::to_string((int)p->sizek) + "," +
            std::to_string((int)p->sizelineinfo) + "," +
            std::to_string((int)p->linegaplog2) + "," +
            std::to_string((int)p->linedefined) + "," +
            std::to_string((int)p->bytecodeid) + "," +
            std::to_string((int)p->sizetypeinfo) + "," +
            std::to_string(nupvalues);

        std::string hash = SHA256::hash(result);

        lua_pushstring(L, hash.c_str());

        return 1;
    }

    void RegisterLibrary(lua_State* L)
    {
        if (!L) return;

        lua_newtable(L);
        Utils::AddTableFunction(L, "base64encode", base64encode);
        Utils::AddTableFunction(L, "base64decode", base64decode);
        Utils::AddTableFunction(L, "base64_encode", base64encode);
        Utils::AddTableFunction(L, "base64_decode", base64decode);
        Utils::AddTableFunction(L, "encrypt", crypt_encrypt);
        Utils::AddTableFunction(L, "decrypt", crypt_decrypt);
        Utils::AddTableFunction(L, "generatebytes", generatebytes);
        Utils::AddTableFunction(L, "generatekey", generatekey);
        Utils::AddTableFunction(L, "hash", hash);
        Utils::AddTableFunction(L, "derive", derive);

        lua_newtable(L);
        Utils::AddTableFunction(L, "encode", base64encode);
        Utils::AddTableFunction(L, "decode", base64decode);
        lua_setfield(L, -2, "base64");

        lua_setglobal(L, "crypt");

        lua_newtable(L);
        Utils::AddTableFunction(L, "encode", base64encode);
        Utils::AddTableFunction(L, "decode", base64decode);
        lua_setglobal(L, "base64");
        Utils::AddFunction(L, "getfunctionhash", getfunctionhash);
        Utils::AddFunction(L, "base64_encode", base64encode);
        Utils::AddFunction(L, "base64_decode", base64decode);
        Utils::AddFunction(L, "base64encode", base64encode);
        Utils::AddFunction(L, "base64decode", base64decode);
        Utils::AddFunction(L, "lz4compress", lz4compress);
        Utils::AddFunction(L, "lz4decompress", lz4decompress);
    }
}