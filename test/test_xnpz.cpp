/***************************************************************************
* Copyright (c) Wolf Vollprecht, Sylvain Corlay and Johan Mabille          *
* Copyright (c) QuantStack                                                 *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#include <cstdint>
#include <sstream>
#include <exception>

#include "gtest/gtest.h"
#include "xtensor-io/xnpz.hpp"

namespace xt
{
    TEST(xnpz, load)
    {
        auto npz_map = xt::load_npz("files/uncompressed.npz");
        auto arr_0 = npz_map["arr_0"].cast<double>();
        auto arr_1 = npz_map["arr_1"].cast<uint64_t>(false);
        xt::xarray<uint64_t> xarr_1 = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
        EXPECT_TRUE(xt::all(xt::isclose(arr_0, linspace<double>(0, 100))));
        EXPECT_TRUE(xt::all(xt::equal(arr_1, xarr_1)));
    }

    TEST(xnpz, load_compressed)
    {
        auto npz_map = xt::load_npz("files/compressed.npz");
        auto arr_0 = npz_map["arr_0"].cast<uint64_t>(false);
        auto arr_1 = npz_map["arr_1"].cast<double>();
        xt::xarray<uint64_t> xarr_0 = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
        EXPECT_TRUE(xt::all(xt::isclose(arr_1, linspace<double>(0, 100))));
        EXPECT_TRUE(xt::all(xt::equal(arr_0, xarr_0)));
    }

    TEST(xnpz, load64)
    {
        auto npz_map = xt::load_npz("files/uncompressed64.npz");
        auto d3 = npz_map["d3"].cast<int32_t>();
        auto eye5 = npz_map["eye5"].cast<float>(false);
        xt::xarray<int32_t> xarr_1 = {{{0, 1}, {2, 3}}, {{4, 5}, {6, 7}}};
        EXPECT_TRUE(xt::all(xt::isclose(eye5, eye<float>(5))));
        EXPECT_TRUE(xt::all(xt::equal(d3, xarr_1)));
    }

    TEST(xnpz, load64_compressed)
    {
        auto npz_map = xt::load_npz("files/compressed64.npz");
        auto d3 = npz_map["d3"].cast<int32_t>();
        auto eye5 = npz_map["eye5"].cast<float>(false);
        xt::xarray<int32_t> xarr_1 = {{{0, 1}, {2, 3}}, {{4, 5}, {6, 7}}};
        EXPECT_TRUE(xt::all(xt::isclose(eye5, eye<float>(5))));
        EXPECT_TRUE(xt::all(xt::equal(d3, xarr_1)));
    }

    bool compare_binary_files(std::string fn1, std::string fn2, std::size_t n_zipped_files)
    {
        std::ifstream stream1(fn1, std::ios::in | std::ios::binary);
        std::vector<uint8_t> fn1_contents((std::istreambuf_iterator<char>(stream1)),
                                           std::istreambuf_iterator<char>());

        std::ifstream stream2(fn2, std::ios::in | std::ios::binary);
        std::vector<uint8_t> fn2_contents((std::istreambuf_iterator<char>(stream2)),
                                           std::istreambuf_iterator<char>());
        std::size_t unequal = 0;

        if (fn1_contents.size() != fn2_contents.size())
        {
            throw std::runtime_error("Content sizes are not the same!");
        }

        for (auto iter_fn1 = fn1_contents.begin(), iter_fn2 = fn2_contents.begin();
             iter_fn1 != fn1_contents.end();
             iter_fn1++, iter_fn2++)
        {
            if (*iter_fn1 != *iter_fn2)
            {
                unequal += 1;
            }
        }
        // this is date + time == 4 bytes per file + once in global header
        std::size_t unequal_allowed = (n_zipped_files + 2) * 4;
        if (unequal > unequal_allowed)
        {
            std::stringstream ss;
            ss << "Number of unequal elements not allowed size: " << unequal << " vs allowed: " << unequal_allowed << std::endl;
            throw std::runtime_error(ss.str());
        }
        return true;
    }

    /*TEST(xnpz, save_uncompressed)
    {
        dump_npz("files/dump_uncompressed.npz", "arr_0", linspace<double>(0, 100), false, false);
        xt::xarray<int64_t> arr = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
        dump_npz("files/dump_uncompressed.npz", "arr_1", arr);
        EXPECT_TRUE(compare_binary_files("files/dump_uncompressed.npz", "files/uncompressed.npz", 2));
    }

    TEST(xnpz, save_compressed)
    {
        xt::xarray<int64_t> arr = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
        dump_npz("files/dump_compressed.npz", "arr_1", linspace<double>(0, 100), true, false );
        dump_npz("files/dump_compressed.npz", "arr_0", arr, true);
        EXPECT_TRUE(compare_binary_files("files/dump_compressed.npz", "files/compressed.npz", 2));
    }*/

    namespace test
    {
        void dump_hex_buf(const std::string& file_name,
                          const std::vector<uint8_t>& buf)
        {
            std::ofstream out(file_name.c_str());
            out << std::hex;
            int i = 0;
            for (auto c : buf)
            {
                out << uint32_t(c) << ' ';
                if (i++%32 == 0)
                {
                    out << std::endl;
                }
            }
        }
    }
    TEST(xnpz, exploring)
    {
        /*std::string fn1 = "files/new_numpy.npz";
        std::string fn2 = "files/dump_uncompressed.npz";

        std::ifstream stream1(fn1, std::ios::in | std::ios::binary);
        std::vector<uint8_t> fn1_contents((std::istreambuf_iterator<char>(stream1)),
                                           std::istreambuf_iterator<char>());

        std::ofstream out1("new_numpy.txt");
        std::copy(fn1_contents.cbegin(), fn1_contents.cend(), std::ostreambuf_iterator<char>(out1));

        test::dump_hex_buf("new_numpy_int.txt", fn1_contents);


        std::ifstream stream2(fn2, std::ios::in | std::ios::binary);
        std::vector<uint8_t> fn2_contents((std::istreambuf_iterator<char>(stream2)),
                                           std::istreambuf_iterator<char>());

        std::ofstream out2("dump_uncompressed.txt");
        std::copy(fn2_contents.cbegin(), fn2_contents.cend(), std::ostreambuf_iterator<char>(out2));

        test::dump_hex_buf("dump_uncompressed_int.txt", fn2_contents);*/

        auto npz_map = xt::load_npz("files/new_numpy.npz");
        auto arr_0 = npz_map["arr_0"].cast<double>();
        auto arr_1 = npz_map["arr_1"].cast<uint64_t>(false);
        xt::xarray<uint64_t> xarr_1 = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
        EXPECT_TRUE(xt::all(xt::isclose(arr_0, linspace<double>(0, 100))));
        EXPECT_TRUE(xt::all(xt::equal(arr_1, xarr_1)));
    }
}
