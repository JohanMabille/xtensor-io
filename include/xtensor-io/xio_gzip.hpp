/***************************************************************************
* Copyright (c) Wolf Vollprecht, Sylvain Corlay and Johan Mabille          *
* Copyright (c) QuantStack                                                 *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XTENSOR_IO_GZIP_HPP
#define XTENSOR_IO_GZIP_HPP

#include <fstream>

#include "zlib.h"

#include "xtensor/xadapt.hpp"
#include "xtensor-io.hpp"
#include "xfile_array.hpp"
#include "xio_stream_wrapper.hpp"

#define GZIP_CHUNK 0x4000
#define GZIP_WINDOWBITS 15
#define GZIP_ENCODING 16
#define ENABLE_ZLIB_GZIP 32

namespace xt
{
    namespace detail
    {
        template <typename T, class I>
        inline xt::svector<T> load_gzip(I& stream, bool as_big_endian)
        {
            xt::svector<T> uncompressed_buffer;
            z_stream zs = {0};
            unsigned char in[GZIP_CHUNK];
            T out[GZIP_CHUNK / sizeof(T)];
            zs.zalloc = Z_NULL;
            zs.zfree = Z_NULL;
            zs.opaque = Z_NULL;
            zs.next_in = in;
            zs.avail_in = 0;
            inflateInit2(&zs, GZIP_WINDOWBITS | ENABLE_ZLIB_GZIP);
            while (true) {
                int bytes_read;
                int zlib_status;

                stream.read((char*)in, sizeof(in));
                bytes_read = stream.gcount();
                zs.avail_in = bytes_read;
                zs.next_in = in;
                do
                {
                    unsigned have;
                    zs.avail_out = GZIP_CHUNK;
                    zs.next_out = (Bytef*)out;
                    zlib_status = inflate(&zs, Z_NO_FLUSH);
                    switch (zlib_status)
                    {
                        case Z_OK:
                        case Z_STREAM_END:
                        case Z_BUF_ERROR:
                            break;
                        default:
                            inflateEnd(&zs);
                            return uncompressed_buffer;
                    }
                    have = GZIP_CHUNK - zs.avail_out;
                    // not possible to insert in a xt::svector currently
                    //uncompressed_buffer.insert(std::end(uncompressed_buffer), out, out + have / sizeof(T));
                    // so just loop for now
                    for (int i = 0; i < have / sizeof(T); i++)
                    {
                        uncompressed_buffer.push_back(out[i]);
                    }
                }
                while (zs.avail_out == 0);
                if (stream.eof())
                {
                    inflateEnd(&zs);
                    break;
                }
            }
            if ((sizeof(T) > 1) && (as_big_endian != is_big_endian()))
            {
                swap_endianness(uncompressed_buffer);
            }
            return uncompressed_buffer;
        }

        template <class O, class E>
        inline void dump_gzip(O& stream, const xexpression<E>& e, bool as_big_endian, int level)
        {
            using value_type = typename E::value_type;
            const E& ex = e.derived_cast();
            auto&& eval_ex = eval(ex);
            auto shape = eval_ex.shape();
            std::size_t size = compute_size(shape);
            std::size_t uncompressed_size = size * sizeof(value_type);
            const char* uncompressed_buffer;
            xt::svector<value_type> swapped_buffer;
            if ((sizeof(value_type) > 1) && (as_big_endian != is_big_endian()))
            {
                swapped_buffer.resize(size);
                std::copy(eval_ex.data(), eval_ex.data() + size, swapped_buffer.begin());
                swap_endianness(swapped_buffer);
                uncompressed_buffer = reinterpret_cast<const char*>(swapped_buffer.data());
            }
            else
            {
                uncompressed_buffer = reinterpret_cast<const char*>(eval_ex.data());
            }
            unsigned char out[GZIP_CHUNK];
            z_stream zs;
            zs.zalloc = Z_NULL;
            zs.zfree = Z_NULL;
            zs.opaque = Z_NULL;
            deflateInit2(&zs, level, Z_DEFLATED, GZIP_WINDOWBITS | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY);
            zs.avail_in = uncompressed_size;
            zs.next_in = (Bytef*)uncompressed_buffer;
            do
            {
                int have;
                zs.avail_out = GZIP_CHUNK;
                zs.next_out = out;
                deflate(&zs, Z_FINISH);
                have = GZIP_CHUNK - zs.avail_out;
                stream.write((const char*)out, std::streamsize(have));
            }
            while (zs.avail_out == 0);
            deflateEnd(&zs);
            stream.flush();
        }
    }  // namespace detail

    /**
     * Save xexpression to GZIP format
     *
     * @param stream An output stream to which to dump the data
     * @param e the xexpression
     */
    template <typename E, class O>
    inline void dump_gzip(O& stream, const xexpression<E>& e, bool as_big_endian=is_big_endian(), int level=1)
    {
        detail::dump_gzip(stream, e, as_big_endian, level);
    }

    template <typename E>
    inline void dump_gzip(std::ostream& stream, const xexpression<E>& e, bool as_big_endian=is_big_endian(), int level=1)
    {
        auto s = xostream_wrapper(stream);
        detail::dump_gzip(s, e, as_big_endian, level);
    }

    /**
     * Save xexpression to GZIP format
     *
     * @param filename The filename or path to dump the data
     * @param e the xexpression
     */
    template <typename E>
    inline void dump_gzip(const char* filename, const xexpression<E>& e, bool as_big_endian=is_big_endian(), int level=1)
    {
        std::ofstream stream(filename, std::ofstream::binary);
        if (!stream.is_open())
        {
            std::runtime_error("IO Error: failed to open file");
        }
        auto s = xostream_wrapper(stream);
        detail::dump_gzip(s, e, as_big_endian, level);
    }

    template <typename E>
    inline void dump_gzip(const std::string& filename, const xexpression<E>& e, bool as_big_endian=is_big_endian(), int level=1)
    {
        dump_gzip<E>(filename.c_str(), e, as_big_endian, level);
    }

    /**
     * Save xexpression to GZIP format in a string
     *
     * @param e the xexpression
     */
    template <typename E>
    inline std::string dump_gzip(const xexpression<E>& e, bool as_big_endian=is_big_endian(), int level=1)
    {
        std::stringstream stream;
        auto s = xostream_wrapper(stream);
        detail::dump_gzip(s, e, as_big_endian, level);
        return stream.str();
    }

    /**
     * Loads a GZIP file
     *
     * @param stream An input stream from which to load the file
     * @tparam T select the type of the GZIP file
     * @tparam L select layout_type::column_major if you stored data in
     *           Fortran format
     * @return xarray with contents from GZIP file
     */
    template <typename T, layout_type L = layout_type::dynamic, class I>
    inline auto load_gzip(I& stream, bool as_big_endian=is_big_endian())
    {
        xt::svector<T> uncompressed_buffer = detail::load_gzip<T>(stream, as_big_endian);
        std::vector<std::size_t> shape = {uncompressed_buffer.size()};
        auto array = adapt(std::move(uncompressed_buffer), shape);
        return array;
    }

    /**
     * Loads a GZIP file
     *
     * @param filename The filename or path to the file
     * @tparam T select the type of the GZIP file
     * @tparam L select layout_type::column_major if you stored data in
     *           Fortran format
     * @return xarray with contents from GZIP file
     */
    template <typename T, layout_type L = layout_type::dynamic>
    inline auto load_gzip(const char* filename, bool as_big_endian=is_big_endian())
    {
        std::ifstream stream(filename, std::ifstream::binary);
        if (!stream.is_open())
        {
            std::runtime_error(std::string("load_gzip: failed to open file ") + filename);
        }
        auto s = xistream_wrapper(stream);
        return load_gzip<T, L>(s, as_big_endian);
    }

    template <typename T, layout_type L = layout_type::dynamic>
    inline auto load_gzip(const std::string& filename, bool as_big_endian=is_big_endian())
    {
        return load_gzip<T, L>(filename.c_str(), as_big_endian);
    }

    struct xio_gzip_config
    {
        std::string name;
        std::string version;
        bool big_endian;
        int level;

        xio_gzip_config()
            : name("gzip")
            , version(ZLIB_VERSION)
            , big_endian(is_big_endian())
            , level(1)
        {
        }

        template <class T>
        void write_to(T& j) const
        {
            j["level"] = level;
        }

        template <class T>
        void read_from(T& j)
        {
            level = j["level"];
        }

        bool will_dump(xfile_dirty dirty)
        {
            return dirty.data_dirty;
        }
    };

    template <class E, class I>
    void load_file(I& stream, xexpression<E>& e, const xio_gzip_config& config)
    {
        E& ex = e.derived_cast();
        auto shape = ex.shape();
        ex = load_gzip<typename E::value_type>(stream, config.big_endian);
        if (!shape.empty())
        {
            if (compute_size(shape) != ex.size())
            {
                XTENSOR_THROW(std::runtime_error, "load_file: size mismatch");
            }
            ex.reshape(shape);
        }
    }

    template <class E, class O>
    void dump_file(O& stream, const xexpression<E> &e, const xio_gzip_config& config)
    {
        dump_gzip(stream, e, config.big_endian, config.level);
    }
}  // namespace xt

#endif
