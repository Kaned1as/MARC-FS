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

#include "gtest/gtest.h"
#include "../src/marc_rest_client.h"
#include "../src/memory_storage.h"


MarcRestClient* setUpMrc() {
    static MarcRestClient* mrc = nullptr;
    if (!mrc) {
        std::string login = std::getenv("MARCFS_TEST_USERNAME");
        std::string password = std::getenv("MARCFS_TEST_PASSWORD");

        std::cout << "Logging in:" << std::endl;
        std::cout << "Login: " << login << std::endl;
        std::cout << "Password: " << password << std::endl;
        std::cout << std::endl << std::endl;

        mrc = new MarcRestClient;
        Account fake;
        fake.setLogin(login);
        fake.setPassword(password);
        mrc->login(fake);
    }
    return mrc;
}


TEST(ApiIntegrationTesting, JustLogin) {
    setUpMrc();
}

TEST(ApiIntegrationTesting, TestListDir) {
    auto mrc = setUpMrc();
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](std::string arg) {
        return find_if(fVec.cbegin(), fVec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec("Берег.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("Горное озеро.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("Долина реки.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("На отдыхе.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("Полет.mp4"), fVec.cend());

    EXPECT_TRUE(findFileInVec("Полёт") == fVec.cend());
    EXPECT_TRUE(findFileInVec("Берег.") == fVec.cend());
    EXPECT_TRUE(findFileInVec("NonExistingFile.jpg") == fVec.cend());
}

TEST(ApiIntegrationTesting, TestShowUsage) {
    auto mrc = setUpMrc();
    auto stats = mrc->df();

    EXPECT_EQ(stats.total, 8L * 1024 * 1024 * 1024); // Mail.Ru default storage size - 8 GiB
    EXPECT_EQ(stats.used, 492119223); // Mail.Ru default content weight
}

TEST(ApiIntegrationTesting, TestCreateFile) {
    auto mrc = setUpMrc();
    MemoryStorage dummy;
    mrc->upload("/cheshire.cat", dummy);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "cheshire.cat"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "cheshire.cat")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "cheshire.cat")).getType(), S_IFREG);

    // now delete it not to tamper test env
    mrc->remove("/cheshire.cat");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "cheshire.cat") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNonEmptyTinyFile) {
    auto mrc = setUpMrc();
    
    MemoryStorage vpar;
    vpar.append("Here\n", 5);
    mrc->upload("/tiny_file.txt", vpar);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "tiny_file.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "tiny_file.txt")).getSize(), 5);
    EXPECT_EQ((*findFileInVec(fVec, "tiny_file.txt")).getType(), S_IFREG);

    MemoryStorage vpar2;
    mrc->download("/tiny_file.txt", vpar2);
    EXPECT_EQ(vpar2.readFully(), "Here\n");

    // now delete it not to tamper test env
    mrc->remove("/tiny_file.txt");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "tiny_file.txt") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNonEmptyAlmost20File) {
    auto mrc = setUpMrc();
    
    MemoryStorage vpar;
    vpar.append("19  bytes  in  size", 19);
    mrc->upload("/small_file.txt", vpar);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "small_file.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "small_file.txt")).getSize(), 19);
    EXPECT_EQ((*findFileInVec(fVec, "small_file.txt")).getType(), S_IFREG);

    MemoryStorage vpar2;
    mrc->download("/small_file.txt", vpar2);
    EXPECT_EQ(vpar2.readFully(), "19  bytes  in  size");

    // now delete it not to tamper test env
    mrc->remove("/small_file.txt");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "small_file.txt") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNonEmptyExactly20File) {
    auto mrc = setUpMrc();
    
    MemoryStorage vpar;
    vpar.append("20  bytes   in  size", 20);
    mrc->upload("/20_file.txt", vpar);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "20_file.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "20_file.txt")).getSize(), 20);
    EXPECT_EQ((*findFileInVec(fVec, "20_file.txt")).getType(), S_IFREG);

    MemoryStorage vpar2;
    mrc->download("/20_file.txt", vpar2);
    EXPECT_EQ(vpar2.readFully(), "20  bytes   in  size");

    // now delete it not to tamper test env
    mrc->remove("/20_file.txt");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "20_file.txt") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNonEmptyExactly21File) {
    auto mrc = setUpMrc();
    
    MemoryStorage vpar;
    vpar.append("21   bytes   in  size", 21);
    mrc->upload("/21_file.txt", vpar);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "21_file.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "21_file.txt")).getSize(), 21);
    EXPECT_EQ((*findFileInVec(fVec, "21_file.txt")).getType(), S_IFREG);

    MemoryStorage vpar2;
    mrc->download("/21_file.txt", vpar2);
    EXPECT_EQ(vpar2.readFully(), "21   bytes   in  size");

    // now delete it not to tamper test env
    mrc->remove("/21_file.txt");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "21_file.txt") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNonEmptyBigFile) {
    auto mrc = setUpMrc();
    
    MemoryStorage vpar;
    vpar.append("There's one for the money, and two for the sin", 46);
    mrc->upload("/medium_file.txt", vpar);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "medium_file.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "medium_file.txt")).getSize(), 46);
    EXPECT_EQ((*findFileInVec(fVec, "medium_file.txt")).getType(), S_IFREG);

    MemoryStorage vpar2;
    mrc->download("/medium_file.txt", vpar2);
    EXPECT_EQ(vpar2.readFully(), "There's one for the money, and two for the sin");

    // now delete it not to tamper test env
    mrc->remove("/medium_file.txt");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "medium_file.txt") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateDir) {
    auto mrc = setUpMrc();
    mrc->mkdir("/testDir");
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "testDir"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getType(), S_IFDIR);

    // now delete it not to tamper test env
    mrc->remove("/testDir");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "testDir") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNestedDir) {
    auto mrc = setUpMrc();
    mrc->mkdir("/Mail.Ru рекомендует/testDir");
    auto fVec = mrc->ls("/Mail.Ru рекомендует");

    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "testDir"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getType(), S_IFDIR);

    // now delete it not to tamper test env
    mrc->remove("/Mail.Ru рекомендует/testDir");
    auto fVec2 = mrc->ls("/Mail.Ru рекомендует");

    EXPECT_TRUE(findFileInVec(fVec2, "testDir") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestMoveFile) {
    auto mrc = setUpMrc();
    MemoryStorage dummy;
    mrc->upload("/dummyToMove.txt", dummy);
    // now move it
    mrc->rename("/dummyToMove.txt", "/movedDummy.txt");

    auto fVec = mrc->ls("/");
    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "movedDummy.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getType(), S_IFREG);

    // now delete it not to tamper test env
    mrc->remove("/movedDummy.txt");
}


TEST(ApiIntegrationTesting, TestMoveFileIntoNestedDir) {
    auto mrc = setUpMrc();
    MemoryStorage dummy;
    mrc->upload("/dummyToMove.txt", dummy);
    // now move it
    mrc->rename("/dummyToMove.txt", "/Mail.Ru рекомендует/movedDummy.txt");

    auto fVec = mrc->ls("//Mail.Ru рекомендует");
    auto findFileInVec = [&](const auto &vec, std::string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "movedDummy.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getType(), S_IFREG);

    // now delete it not to tamper test env
    mrc->remove("/Mail.Ru рекомендует/movedDummy.txt");
}

TEST(ApiIntegrationTesting, TestFileDownload) {
    auto mrc = setUpMrc();
    MemoryStorage content;
    mrc->download("/Берег.jpg", content);

    EXPECT_EQ(content.size(), 723662); // Cloud default file
    auto magicBytes = content.readFully().substr(0, 4);
    EXPECT_EQ(magicBytes, "\xFF\xD8\xFF\xE1"); // JPEG magic bytes ver. 3, see wiki
}
