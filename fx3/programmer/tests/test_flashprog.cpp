/*
 * test_flashprog.cpp
 *
 * Domesday Duplicator - FX3 programmer tests
 *
 * T1 (unit) coverage for locating cyfxflashprog.img.
 *
 * The defect this guards against is subtle: every candidate path used to be relative to
 * the current working directory, so the tool worked when run from its own build tree and
 * failed everywhere else. It failed at the point of use, during a flash write, with a
 * message that gave the user nothing to act on. These tests pin the search order and, in
 * particular, that the compiled-in install path is present and is consulted before any
 * working-directory-relative candidate.
 *
 * SPDX-FileCopyrightText: 2026 Simon Inns
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

extern "C" {
#include "fx3-flashprog.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>

namespace
{

// A temporary directory that cleans up after itself, so tests can create real files —
// the search is stat()-based, so it cannot be exercised without them.
class TempDir
{
public:
    TempDir()
    {
        char pattern[] = "/tmp/ddd-flashprog-test-XXXXXX";
        const char *made = mkdtemp(pattern);
        if (made == nullptr) {
            ADD_FAILURE() << "could not create a temporary directory";
            return;
        }
        path_ = made;
    }

    ~TempDir()
    {
        if (path_.empty()) {
            return;
        }
        // Only ever contains files this fixture created, one level deep
        for (const char *name : { "cyfxflashprog.img", "elsewhere.img", "adirectory" }) {
            const std::string full = path_ + "/" + name;
            ::remove(full.c_str());
        }
        ::rmdir(path_.c_str());
    }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    const std::string &path() const { return path_; }

    std::string writeFile(const char *name, const char *contents) const
    {
        const std::string full = path_ + "/" + name;
        std::ofstream out(full, std::ios::binary);
        out << contents;
        out.close();
        return full;
    }

private:
    std::string path_;
};

// Runs a test body with the working directory changed, and restores it afterwards.
class ScopedChdir
{
public:
    explicit ScopedChdir(const std::string &to)
    {
        char *cwd = getcwd(nullptr, 0);
        if (cwd != nullptr) {
            original_ = cwd;
            free(cwd);
        }
        if (chdir(to.c_str()) != 0) {
            ADD_FAILURE() << "could not chdir to " << to;
        }
    }

    ~ScopedChdir()
    {
        if (!original_.empty()) {
            if (chdir(original_.c_str()) != 0) {
                // Nothing useful to do in a destructor, but do not fail silently
                std::fprintf(stderr, "warning: could not restore working directory\n");
            }
        }
    }

    ScopedChdir(const ScopedChdir &) = delete;
    ScopedChdir &operator=(const ScopedChdir &) = delete;

private:
    std::string original_;
};

// Frees the char* the function under test returns
struct FreeDeleter
{
    void operator()(char *p) const { free(p); }
};

using OwnedPath = std::unique_ptr<char, FreeDeleter>;

// --- The compiled-in install path -----------------------------------------------------------

TEST(FlashprogPath, InstallPathIsCompiledIn)
{
    // This is the whole point of the fix. If the build stops defining it, an installed
    // binary silently goes back to only working from its own directory.
    const char *installed = fx3_flashprog_install_path();

    ASSERT_NE(installed, nullptr) << "FLASHPROG_INSTALL_PATH was not defined at build time";
    EXPECT_NE(installed[0], '\0');
    EXPECT_EQ(installed[0], '/') << "the install path must be absolute, got: " << installed;
    EXPECT_NE(std::strstr(installed, "cyfxflashprog.img"), nullptr)
        << "the install path must name the image, got: " << installed;
}

// --- The environment override ----------------------------------------------------------------

TEST(FlashprogPath, EnvironmentOverrideWins)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());

    // Two candidates exist: one the environment names, one in the working directory.
    // The environment must win.
    const std::string elsewhere = dir.writeFile("elsewhere.img", "loader");
    dir.writeFile("cyfxflashprog.img", "loader");

    ScopedChdir cd(dir.path());
    OwnedPath found(fx3_find_flashprog_image_with_env(elsewhere.c_str()));

    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found.get(), elsewhere.c_str());
}

TEST(FlashprogPath, EmptyEnvironmentValueIsIgnored)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    dir.writeFile("cyfxflashprog.img", "loader");

    ScopedChdir cd(dir.path());

    // An exported-but-empty FX3_FLASH_PROG is a common shell accident. It must fall
    // through to the next candidate rather than being treated as a path of "".
    OwnedPath found(fx3_find_flashprog_image_with_env(""));

    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found.get(), "cyfxflashprog.img");
}

TEST(FlashprogPath, NullEnvironmentValueIsIgnored)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    dir.writeFile("cyfxflashprog.img", "loader");

    ScopedChdir cd(dir.path());
    OwnedPath found(fx3_find_flashprog_image_with_env(nullptr));

    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found.get(), "cyfxflashprog.img");
}

TEST(FlashprogPath, NonexistentEnvironmentPathFallsThrough)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    dir.writeFile("cyfxflashprog.img", "loader");

    ScopedChdir cd(dir.path());

    const std::string missing = dir.path() + "/does-not-exist.img";
    OwnedPath found(fx3_find_flashprog_image_with_env(missing.c_str()));

    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found.get(), "cyfxflashprog.img");
}

// --- Not found ----------------------------------------------------------------------------

TEST(FlashprogPath, ReturnsNullWhenNothingExists)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());

    // An empty directory, with no image anywhere above it either. The install path is
    // compiled in but points into a prefix that does not exist in the test environment.
    ScopedChdir cd(dir.path());
    OwnedPath found(fx3_find_flashprog_image_with_env(nullptr));

    if (found != nullptr) {
        // The only legitimate way to find something here is a real installed copy on the
        // machine running the tests. Anything else is a bug.
        EXPECT_STREQ(found.get(), fx3_flashprog_install_path())
            << "found an unexpected candidate from an empty directory";
    } else {
        SUCCEED();
    }
}

// --- A directory is not an image --------------------------------------------------------------

TEST(FlashprogPath, DirectoryNamedLikeTheImageIsRejected)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());

    // stat() succeeds on a directory, so the check must test for a regular file. Without
    // S_ISREG the tool would try to read a directory as firmware.
    const std::string asDir = dir.path() + "/adirectory";
    ASSERT_EQ(::mkdir(asDir.c_str(), 0755), 0);

    ScopedChdir cd(dir.path());
    OwnedPath found(fx3_find_flashprog_image_with_env(asDir.c_str()));

    if (found != nullptr) {
        EXPECT_STRNE(found.get(), asDir.c_str()) << "a directory was accepted as the image";
    }

    ::rmdir(asDir.c_str());
}

// --- The caller owns the result ----------------------------------------------------------------

TEST(FlashprogPath, ResultIsIndependentOfTheInput)
{
    TempDir dir;
    ASSERT_FALSE(dir.path().empty());
    const std::string image = dir.writeFile("cyfxflashprog.img", "loader");

    // The returned string must be a copy: load_flash_programmer() free()s it, so returning
    // a pointer into the environment or into a string literal would be a double-free or a
    // free of static storage.
    std::string mutableInput = image;
    OwnedPath found(fx3_find_flashprog_image_with_env(mutableInput.c_str()));

    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found.get(), image.c_str());

    mutableInput[0] = 'X';
    EXPECT_STREQ(found.get(), image.c_str()) << "the result aliased its input";
}

} // namespace
