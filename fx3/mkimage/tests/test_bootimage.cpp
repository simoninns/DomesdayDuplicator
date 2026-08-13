/************************************************************************

    test_bootimage.cpp

    Tests for FX3 boot image construction
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2026 Simon Inns

    This file is part of Domesday Duplicator.

    Domesday Duplicator is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Email: simon.inns@gmail.com

************************************************************************/

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

extern "C" {
#include "fx3-bootimage.h"
}

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void put32(std::vector<uint8_t> &v, size_t off, uint32_t x)
{
    v[off + 0] = static_cast<uint8_t>(x & 0xFF);
    v[off + 1] = static_cast<uint8_t>((x >> 8) & 0xFF);
    v[off + 2] = static_cast<uint8_t>((x >> 16) & 0xFF);
    v[off + 3] = static_cast<uint8_t>((x >> 24) & 0xFF);
}

void put16(std::vector<uint8_t> &v, size_t off, uint16_t x)
{
    v[off + 0] = static_cast<uint8_t>(x & 0xFF);
    v[off + 1] = static_cast<uint8_t>((x >> 8) & 0xFF);
}

uint32_t get32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

struct LoadSeg {
    uint32_t paddr;
    std::vector<uint8_t> data; // file contents
    uint32_t memsz;            // >= data.size(); the excess is .bss
};

// Build a minimal but valid ELF32 little-endian ARM executable in memory.
// Program headers follow the ELF header; segment data follows those.
std::vector<uint8_t> makeElf(const std::vector<LoadSeg> &segs, uint32_t entry,
                             uint16_t e_type = 2 /* ET_EXEC */, uint16_t e_machine = 40 /* ARM */,
                             uint8_t ei_class = 1, uint8_t ei_data = 1)
{
    const size_t ehsize = 0x34;
    const size_t phentsize = 0x20;
    const size_t phoff = ehsize;
    const size_t dataStart = phoff + phentsize * segs.size();

    size_t total = dataStart;
    for (const auto &s : segs) total += s.data.size();

    std::vector<uint8_t> v(total, 0);

    v[0] = 0x7F;
    v[1] = 'E';
    v[2] = 'L';
    v[3] = 'F';
    v[4] = ei_class;
    v[5] = ei_data;
    v[6] = 1; // EV_CURRENT
    put16(v, 0x10, e_type);
    put16(v, 0x12, e_machine);
    put32(v, 0x14, 1);
    put32(v, 0x18, entry);
    put32(v, 0x1C, static_cast<uint32_t>(phoff));
    put16(v, 0x28, static_cast<uint16_t>(ehsize));
    put16(v, 0x2A, static_cast<uint16_t>(phentsize));
    put16(v, 0x2C, static_cast<uint16_t>(segs.size()));

    size_t off = dataStart;
    for (size_t i = 0; i < segs.size(); i++) {
        const size_t ph = phoff + i * phentsize;
        put32(v, ph + 0x00, 1); // PT_LOAD
        put32(v, ph + 0x04, static_cast<uint32_t>(off));
        put32(v, ph + 0x08, segs[i].paddr); // p_vaddr
        put32(v, ph + 0x0C, segs[i].paddr); // p_paddr
        put32(v, ph + 0x10, static_cast<uint32_t>(segs[i].data.size()));
        put32(v, ph + 0x14, segs[i].memsz);
        put32(v, ph + 0x18, 5);      // PF_R | PF_X
        put32(v, ph + 0x1C, 0x1000); // p_align

        std::memcpy(v.data() + off, segs[i].data.data(), segs[i].data.size());
        off += segs[i].data.size();
    }

    return v;
}

// Owning wrapper so a failed expectation cannot leak the built image.
struct Image {
    uint8_t *data = nullptr;
    size_t size = 0;
    ~Image() { free(data); }
};

// Build straight from an ELF buffer, the way the tool does.
fx3_status buildFromElf(const std::vector<uint8_t> &elf, Image &img, int keepVectors = 0,
                        uint8_t ctl = FX3_IMAGE_CTL_DEFAULT, uint8_t type = FX3_IMAGE_TYPE_NORMAL)
{
    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;

    fx3_status st = fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry);
    if (st != FX3_OK) return st;

    return fx3_build_image(segs, n, entry, ctl, type, keepVectors, &img.data, &img.size);
}

// Decode an image into (address, payload) pairs plus the entry and checksum, so
// tests can assert on structure rather than on offsets.
struct Section {
    uint32_t address;
    std::vector<uint8_t> data;
};

struct Decoded {
    uint8_t ctl = 0;
    uint8_t type = 0;
    std::vector<Section> sections;
    uint32_t entry = 0;
    uint32_t checksum = 0;
    bool wellFormed = false;
};

Decoded decode(const Image &img)
{
    Decoded d;
    if (img.size < 16) return d;

    EXPECT_EQ(img.data[0], 'C');
    EXPECT_EQ(img.data[1], 'Y');
    d.ctl = img.data[2];
    d.type = img.data[3];

    size_t pos = 4;
    while (pos + 8 <= img.size) {
        uint32_t words = get32(img.data + pos);
        uint32_t addr = get32(img.data + pos + 4);
        pos += 8;

        if (words == 0) {
            d.entry = addr;
            if (pos + 4 > img.size) return d;
            d.checksum = get32(img.data + pos);
            pos += 4;
            d.wellFormed = (pos == img.size);
            return d;
        }

        const size_t bytes = static_cast<size_t>(words) * 4;
        if (pos + bytes > img.size) return d;

        Section s;
        s.address = addr;
        s.data.assign(img.data + pos, img.data + pos + bytes);
        d.sections.push_back(std::move(s));
        pos += bytes;
    }

    return d;
}

uint32_t sumWords(const std::vector<Section> &sections)
{
    uint32_t sum = 0;
    for (const auto &s : sections)
        for (size_t i = 0; i < s.data.size(); i += 4) sum += get32(s.data.data() + i);
    return sum;
}

std::vector<uint8_t> ramp(size_t n, uint8_t start = 0)
{
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; i++) v[i] = static_cast<uint8_t>(start + i);
    return v;
}

// ---------------------------------------------------------------------------
// Image header
// ---------------------------------------------------------------------------

TEST(Header, CarriesSignatureAndDefaults)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    EXPECT_TRUE(d.wellFormed);
    EXPECT_EQ(d.ctl, FX3_IMAGE_CTL_DEFAULT);
    EXPECT_EQ(d.type, FX3_IMAGE_TYPE_NORMAL);
}

TEST(Header, ControlAndTypeBytesArePassedThrough)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img, 0, 0x0A, 0xB2), FX3_OK);

    auto d = decode(img);
    EXPECT_EQ(d.ctl, 0x0A);
    EXPECT_EQ(d.type, 0xB2);
}

TEST(Header, EntryPointComesFromTheElf)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40007ABC);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    EXPECT_EQ(decode(img).entry, 0x40007ABCu);
}

// ---------------------------------------------------------------------------
// Checksum — AN76405 section 4.4: payload words only, not lengths, addresses or
// the header. A checksum over the wrong range still produces a plausible image
// that the bootloader silently refuses, so this is worth pinning precisely.
// ---------------------------------------------------------------------------

TEST(Checksum, SumsPayloadWordsOnly)
{
    auto elf = makeElf({{0x40003000, ramp(64), 64}, {0x40009000, ramp(32, 0x80), 32}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    EXPECT_EQ(d.checksum, sumWords(d.sections));
}

TEST(Checksum, MatchesTheWorkedExampleFromAN76405)
{
    // AN76405 section 4.4.1: two sections, checksum is the plain 32-bit sum
    //   0x12345678 + 0x9ABCDEF1 + 0x23456789 + 0xABCDEF12 + 0xDDCCBBAA + 0x11223344
    const uint32_t words1[] = {0x12345678u, 0x9ABCDEF1u, 0x23456789u, 0xABCDEF12u};
    const uint32_t words2[] = {0xDDCCBBAAu, 0x11223344u};

    std::vector<uint8_t> d1(16), d2(8);
    for (int i = 0; i < 4; i++) put32(d1, i * 4, words1[i]);
    for (int i = 0; i < 2; i++) put32(d2, i * 4, words2[i]);

    auto elf = makeElf({{0x40008000, d1, 16}, {0x40009000, d2, 8}}, 0x40008000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 2u);
    EXPECT_EQ(d.entry, 0x40008000u);
    EXPECT_EQ(d.checksum, 0x6AF37AF2u);
}

TEST(Checksum, WrapsAtThirtyTwoBits)
{
    std::vector<uint8_t> data(8);
    put32(data, 0, 0xFFFFFFFFu);
    put32(data, 4, 0x00000002u);

    auto elf = makeElf({{0x40003000, data, 8}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    EXPECT_EQ(decode(img).checksum, 0x00000001u);
}

// ---------------------------------------------------------------------------
// Vector area — the ARM926EJ-S keeps reset and interrupt vectors below 0x100.
// Loading over them would land on the live bootloader.
// ---------------------------------------------------------------------------

TEST(VectorArea, IsDroppedByDefault)
{
    auto elf = makeElf({{0x00000000, ramp(0x180), 0x180}}, 0x100);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].address, FX3_VECTOR_AREA_END);
    EXPECT_EQ(d.sections[0].data.size(), 0x80u);
    EXPECT_EQ(d.sections[0].data[0], 0x00); // byte at 0x100 of the ramp wraps to 0
}

TEST(VectorArea, IsRetainedWithKeepVectors)
{
    auto elf = makeElf({{0x00000000, ramp(0x180), 0x180}}, 0x100);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img, /*keepVectors=*/1), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].address, 0u);
    EXPECT_EQ(d.sections[0].data.size(), 0x180u);
}

TEST(VectorArea, TrimsAPartiallyOverlappingSegment)
{
    // A segment starting inside the vector area loses only the overlap.
    auto elf = makeElf({{0x00000080, ramp(0x100), 0x100}}, 0x100);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].address, 0x100u);
    EXPECT_EQ(d.sections[0].data.size(), 0x80u);
    EXPECT_EQ(d.sections[0].data[0], 0x80); // ramp value at offset 0x80
}

TEST(VectorArea, DropsASegmentLyingEntirelyBelowIt)
{
    auto elf = makeElf({{0x00000000, ramp(0x40), 0x40}, {0x40003000, ramp(16), 16}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].address, 0x40003000u);
}

TEST(VectorArea, LeavesHigherSegmentsAlone)
{
    auto elf = makeElf({{0x40003000, ramp(0x40), 0x40}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].address, 0x40003000u);
    EXPECT_EQ(d.sections[0].data.size(), 0x40u);
}

// ---------------------------------------------------------------------------
// Section splitting. Not a requirement of AN76405 — it is what the vendor tool
// does, and matching it is what keeps the output byte-identical.
// ---------------------------------------------------------------------------

TEST(Splitting, LargeSegmentSplitsAtSixtyFourKiB)
{
    const size_t n = FX3_MAX_SECTION_BYTES + 0x40;
    auto elf = makeElf({{0x40003000, ramp(n), static_cast<uint32_t>(n)}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 2u);
    EXPECT_EQ(d.sections[0].address, 0x40003000u);
    EXPECT_EQ(d.sections[0].data.size(), FX3_MAX_SECTION_BYTES);
    EXPECT_EQ(d.sections[1].address, 0x40003000u + FX3_MAX_SECTION_BYTES);
    EXPECT_EQ(d.sections[1].data.size(), 0x40u);
}

TEST(Splitting, ExactlySixtyFourKiBStaysOneSection)
{
    const size_t n = FX3_MAX_SECTION_BYTES;
    auto elf = makeElf({{0x40003000, ramp(n), static_cast<uint32_t>(n)}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    EXPECT_EQ(decode(img).sections.size(), 1u);
}

TEST(Splitting, SplitPreservesContentAndOrder)
{
    const size_t n = FX3_MAX_SECTION_BYTES + 0x100;
    auto source = ramp(n);
    auto elf = makeElf({{0x40003000, source, static_cast<uint32_t>(n)}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    std::vector<uint8_t> rejoined;
    for (const auto &s : d.sections) rejoined.insert(rejoined.end(), s.data.begin(), s.data.end());

    EXPECT_EQ(rejoined, source);
}

// ---------------------------------------------------------------------------
// .bss and word alignment
// ---------------------------------------------------------------------------

TEST(Bss, MemorySizeBeyondFileSizeIsZeroFilled)
{
    auto elf = makeElf({{0x40030000, ramp(16, 1), 64}}, 0x40030000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    ASSERT_EQ(d.sections[0].data.size(), 64u);

    for (size_t i = 0; i < 16; i++) EXPECT_EQ(d.sections[0].data[i], static_cast<uint8_t>(1 + i));
    for (size_t i = 16; i < 64; i++) EXPECT_EQ(d.sections[0].data[i], 0) << "at offset " << i;
}

TEST(Alignment, SectionLengthIsRoundedUpToWholeWords)
{
    // 13 bytes must become 4 words (16 bytes), zero-padded.
    auto elf = makeElf({{0x40003000, ramp(13, 1), 13}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].data.size(), 16u);
    EXPECT_EQ(d.sections[0].data[13], 0);
    EXPECT_EQ(d.sections[0].data[14], 0);
    EXPECT_EQ(d.sections[0].data[15], 0);
}

// ---------------------------------------------------------------------------
// Multiple segments
// ---------------------------------------------------------------------------

TEST(Segments, AreEmittedInProgramHeaderOrder)
{
    auto elf = makeElf(
            {{0x40003000, ramp(16, 0x10), 16},
             {0x40009000, ramp(16, 0x20), 16},
             {0x4000F000, ramp(16, 0x30), 16}},
            0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 3u);
    EXPECT_EQ(d.sections[0].address, 0x40003000u);
    EXPECT_EQ(d.sections[1].address, 0x40009000u);
    EXPECT_EQ(d.sections[2].address, 0x4000F000u);
    EXPECT_EQ(d.sections[0].data[0], 0x10);
    EXPECT_EQ(d.sections[1].data[0], 0x20);
    EXPECT_EQ(d.sections[2].data[0], 0x30);
}

TEST(Segments, ZeroMemorySizeSegmentsAreSkipped)
{
    auto elf = makeElf({{0x40003000, {}, 0}, {0x40009000, ramp(16), 16}}, 0x40009000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    auto d = decode(img);
    ASSERT_EQ(d.sections.size(), 1u);
    EXPECT_EQ(d.sections[0].address, 0x40009000u);
}

// ---------------------------------------------------------------------------
// ELF validation. A firmware tool that accepts the wrong file and emits a
// plausible-looking image is worse than one that refuses.
// ---------------------------------------------------------------------------

TEST(ElfValidation, RejectsBadMagic)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    elf[1] = 'X';

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_NOT_ELF);
}

TEST(ElfValidation, RejectsSixtyFourBit)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000, 2, 40, /*ei_class=*/2);

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_NOT_ELF32_LSB);
}

TEST(ElfValidation, RejectsBigEndian)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000, 2, 40, 1, /*ei_data=*/2);

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_NOT_ELF32_LSB);
}

TEST(ElfValidation, RejectsNonArm)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000, 2, /*e_machine=*/3 /* x86 */);

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_NOT_ARM);
}

TEST(ElfValidation, RejectsNonExecutable)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000, /*e_type=*/1 /* ET_REL */);

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_NOT_EXEC);
}

TEST(ElfValidation, RejectsATruncatedHeader)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    elf.resize(0x20);

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_TRUNCATED);
}

TEST(ElfValidation, RejectsAProgramHeaderTableOutsideTheFile)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    put32(elf, 0x1C, 0x7FFFFFFFu); // e_phoff far past the end

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_TRUNCATED);
}

TEST(ElfValidation, RejectsSegmentDataOutsideTheFile)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    put32(elf, 0x34 + 0x10, 0x00100000u); // p_filesz far past the end
    put32(elf, 0x34 + 0x14, 0x00100000u); // and p_memsz to match

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_TRUNCATED);
}

TEST(ElfValidation, RejectsFileSizeLargerThanMemorySize)
{
    auto elf = makeElf({{0x40003000, ramp(16), 16}}, 0x40003000);
    put32(elf, 0x34 + 0x14, 8); // p_memsz < p_filesz

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_TRUNCATED);
}

TEST(ElfValidation, RejectsAnElfWithNothingToLoad)
{
    auto elf = makeElf({{0x40003000, {}, 0}}, 0x40003000);

    fx3_segment segs[FX3_MAX_SEGMENTS];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, FX3_MAX_SEGMENTS, &n, &entry),
              FX3_E_NO_SEGMENTS);
}

TEST(ElfValidation, ReportsTooManySegments)
{
    std::vector<LoadSeg> many;
    for (int i = 0; i < 4; i++)
        many.push_back({static_cast<uint32_t>(0x40003000 + i * 0x1000), ramp(16), 16});
    auto elf = makeElf(many, 0x40003000);

    fx3_segment segs[2];
    size_t n = 0;
    uint32_t entry = 0;
    EXPECT_EQ(fx3_elf_parse(elf.data(), elf.size(), segs, 2, &n, &entry),
              FX3_E_TOO_MANY_SEGMENTS);
}

TEST(ElfValidation, EveryStatusHasAMessage)
{
    const fx3_status all[] = {FX3_OK,
                              FX3_E_TRUNCATED,
                              FX3_E_NOT_ELF,
                              FX3_E_NOT_ELF32_LSB,
                              FX3_E_NOT_ARM,
                              FX3_E_NOT_EXEC,
                              FX3_E_TOO_MANY_SEGMENTS,
                              FX3_E_NO_SEGMENTS,
                              FX3_E_NOMEM};

    for (fx3_status s : all) {
        const char *m = fx3_strerror(s);
        ASSERT_NE(m, nullptr);
        EXPECT_NE(std::strlen(m), 0u);
        EXPECT_STRNE(m, "unknown error") << "status " << static_cast<int>(s);
    }
}

// ---------------------------------------------------------------------------
// Golden vectors. Byte-for-byte reference output, so a refactor that keeps the
// structure but changes the encoding is caught.
// ---------------------------------------------------------------------------

TEST(Golden, SingleSectionImageIsByteExact)
{
    std::vector<uint8_t> data(8);
    put32(data, 0, 0x11223344u);
    put32(data, 4, 0x55667788u);

    auto elf = makeElf({{0x40003000, data, 8}}, 0x40003000);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    const uint8_t expected[] = {
            'C',  'Y',  0x1C, 0xB0,                   // signature, bImageCTL, bImageType
            0x02, 0x00, 0x00, 0x00,                   // dLength 0 = 2 long words
            0x00, 0x30, 0x00, 0x40,                   // dAddress 0 = 0x40003000
            0x44, 0x33, 0x22, 0x11,                   // payload word 0
            0x88, 0x77, 0x66, 0x55,                   // payload word 1
            0x00, 0x00, 0x00, 0x00,                   // dLength N = 0, termination
            0x00, 0x30, 0x00, 0x40,                   // dAddress N = program entry
            0xCC, 0xAA, 0x88, 0x66                    // checksum
    };

    ASSERT_EQ(img.size, sizeof(expected));
    EXPECT_EQ(std::memcmp(img.data, expected, sizeof(expected)), 0);
}

TEST(Golden, VectorTrimmedImageIsByteExact)
{
    // 0x104 bytes at address 0: the first 0x100 are the vector area and go, and
    // the surviving 4 bytes land at 0x100.
    std::vector<uint8_t> data(0x104, 0xEE);
    put32(data, 0x100, 0x0000000Fu);

    auto elf = makeElf({{0x00000000, data, 0x104}}, 0x00000104);
    Image img;
    ASSERT_EQ(buildFromElf(elf, img), FX3_OK);

    const uint8_t expected[] = {
            'C',  'Y',  0x1C, 0xB0,                   //
            0x01, 0x00, 0x00, 0x00,                   // one long word survives
            0x00, 0x01, 0x00, 0x00,                   // at 0x00000100
            0x0F, 0x00, 0x00, 0x00,                   // the payload
            0x00, 0x00, 0x00, 0x00,                   // termination
            0x04, 0x01, 0x00, 0x00,                   // program entry 0x104
            0x0F, 0x00, 0x00, 0x00                    // checksum
    };

    ASSERT_EQ(img.size, sizeof(expected));
    EXPECT_EQ(std::memcmp(img.data, expected, sizeof(expected)), 0);
}

} // namespace
