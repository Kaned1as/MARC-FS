/*****************************************************************************
 *
 * Copyright (c) 2017, Oleg `Kanedias` Chernovskiy
 *
 * This file is part of MARC-FS testing suite.
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
#include "../marc_api.h"
#include "../memory_storage.h"

using namespace std;

// template instantiation declarations
extern template void MarcRestClient::upload(string remotePath, AbstractStorage &body, size_t start, size_t count);
extern template void MarcRestClient::download(string remotePath, AbstractStorage& target);

MarcRestClient* setUpMrc() {
    static MarcRestClient* mrc = nullptr;
    if (!mrc) {
        mrc = new MarcRestClient;
        Account fake;
        fake.setLogin("kanedias.the.maker@mail.ru");
        fake.setPassword("thecakeisalie");
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

    auto findFileInVec = [&](string arg) {
        return find_if(fVec.cbegin(), fVec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec("Берег.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("Горное озеро.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("Долина реки.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("На отдыхе.jpg"), fVec.cend());
    EXPECT_NE(findFileInVec("Полет.mp4"), fVec.cend());
    EXPECT_NE(findFileInVec("Mail.Ru рекомендует"), fVec.cend());

    EXPECT_TRUE(findFileInVec("Полёт") == fVec.cend());
    EXPECT_TRUE(findFileInVec("Берег.") == fVec.cend());
    EXPECT_TRUE(findFileInVec("NonExistingFile.jpg") == fVec.cend());
}

TEST(ApiIntegrationTesting, TestListNestedDir) {
    auto mrc = setUpMrc();
    auto fVec = mrc->ls("/Mail.Ru рекомендует");

    auto findFileInVec = [&](string arg) {
        return find_if(fVec.cbegin(), fVec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec("Screenshoter.exe"), fVec.cend());

    EXPECT_TRUE(findFileInVec("Screenshoter") == fVec.cend());
    EXPECT_TRUE(findFileInVec("Screenshoter.ex") == fVec.cend());
    EXPECT_TRUE(findFileInVec("NonExistingFile.jpg") == fVec.cend());
}

TEST(ApiIntegrationTesting, TestShowUsage) {
    auto mrc = setUpMrc();
    auto stats = mrc->df();

    EXPECT_EQ(stats.totalMiB, 16 * 1024); // Mail.Ru default storage size - 16 GiB
    EXPECT_EQ(stats.usedMiB, 491); // Mail.Ru default content weight
}

TEST(ApiIntegrationTesting, TestCreateFile) {
    auto mrc = setUpMrc();
    MemoryStorage dummy;
    mrc->upload<AbstractStorage>("/cheshire.cat", dummy);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "cheshire.cat"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "cheshire.cat")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "cheshire.cat")).getType(), CloudFile::File);

    // now delete it not to tamper test env
    mrc->remove("/cheshire.cat");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "cheshire.cat") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateNonEmptyFile) {
    auto mrc = setUpMrc();
    MemoryStorage vpar;
    vpar.append("Here\n", 5);
    mrc->upload<AbstractStorage>("/virtual_particle.txt", vpar);
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "virtual_particle.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "virtual_particle.txt")).getSize(), 5);
    EXPECT_EQ((*findFileInVec(fVec, "virtual_particle.txt")).getType(), CloudFile::File);

    // now delete it not to tamper test env
    mrc->remove("/virtual_particle.txt");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "virtual_particle.txt") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestCreateDir) {
    auto mrc = setUpMrc();
    mrc->mkdir("/testDir");
    auto fVec = mrc->ls("/");

    auto findFileInVec = [&](const auto &vec, string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "testDir"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getType(), CloudFile::Directory);

    // now delete it not to tamper test env
    mrc->remove("/testDir");
    auto fVec2 = mrc->ls("/");

    EXPECT_TRUE(findFileInVec(fVec2, "testDir") == fVec2.cend());
}


TEST(ApiIntegrationTesting, TestCreateNestedDir) {
    auto mrc = setUpMrc();
    mrc->mkdir("/Mail.Ru рекомендует/testDir");
    auto fVec = mrc->ls("/Mail.Ru рекомендует");

    auto findFileInVec = [&](const auto &vec, string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "testDir"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "testDir")).getType(), CloudFile::Directory);

    // now delete it not to tamper test env
    mrc->remove("/Mail.Ru рекомендует/testDir");
    auto fVec2 = mrc->ls("/Mail.Ru рекомендует");

    EXPECT_TRUE(findFileInVec(fVec2, "testDir") == fVec2.cend());
}

TEST(ApiIntegrationTesting, TestMoveFile) {
    auto mrc = setUpMrc();
    MemoryStorage dummy;
    mrc->upload<AbstractStorage>("/dummyToMove.txt", dummy);
    // now move it
    mrc->rename("/dummyToMove.txt", "/movedDummy.txt");

    auto fVec = mrc->ls("/");
    auto findFileInVec = [&](const auto &vec, string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "movedDummy.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getType(), CloudFile::File);

    // now delete it not to tamper test env
    mrc->remove("/movedDummy.txt");
}


TEST(ApiIntegrationTesting, TestMoveFileIntoNestedDir) {
    auto mrc = setUpMrc();
    MemoryStorage dummy;
    mrc->upload<AbstractStorage>("/dummyToMove.txt", dummy);
    // now move it
    mrc->rename("/dummyToMove.txt", "/Mail.Ru рекомендует/movedDummy.txt");

    auto fVec = mrc->ls("//Mail.Ru рекомендует");
    auto findFileInVec = [&](const auto &vec, string arg) {
        return find_if(vec.cbegin(), vec.cend(), [&arg](auto &f) { return f.getName() == arg; });
    };

    EXPECT_NE(findFileInVec(fVec, "movedDummy.txt"), fVec.cend());
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getSize(), 0);
    EXPECT_EQ((*findFileInVec(fVec, "movedDummy.txt")).getType(), CloudFile::File);

    // now delete it not to tamper test env
    mrc->remove("/Mail.Ru рекомендует/movedDummy.txt");
}

TEST(ApiIntegrationTesting, TestFileDownload) {
    auto mrc = setUpMrc();
    MemoryStorage content;
    mrc->download<AbstractStorage>("/Берег.jpg", content);

    EXPECT_EQ(content.size(), 723662); // Cloud default file
    auto magicBytes = string(content.readFully(), 4);
    EXPECT_EQ(magicBytes, "\xFF\xD8\xFF\xE1"); // JPEG magic bytes ver. 3, see wiki
}
